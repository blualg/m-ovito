////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/core/Core.h>
#include "ModifierEvaluationTask.h"

namespace Ovito {

/**
 * \brief An asynchronous task that takes care of evaluating the modifier associated with a modification
 *        pipeline node. The task additionally waits for some auxiliary input of the modifier to become available (in addition to the upstream pipeline data).
 *        The auxiliary input data for the modifier must be provided in the form of a future.
 */
template<typename ModifierClass, typename AuxiliaryFutureType>
class ComplexModifierEvaluationTask : public ModifierEvaluationTask<>
{
public:

    /// Inherit constructor from base class.
    using ModifierEvaluationTask::ModifierEvaluationTask;

    /// Starts the execution of this task. This gets called by the launchTask() helper function.
    void operator()(SharedFuture<PipelineFlowState> inputFuture, AuxiliaryFutureType auxiliaryFuture) noexcept {

        // Schedule callback upon completion of the future that yields the input state from the upstream pipeline.
        ModifierEvaluationTask::operator()(std::move(inputFuture));

        // Schedule callback upon completion of the future that yields the auxiliary input data.
        _auxiliaryAwaiter.whenTaskFinishes<ComplexModifierEvaluationTask, &ComplexModifierEvaluationTask::auxiliaryInputAvailable>(
            std::move(auxiliaryFuture),
            ObjectExecutor(modificationNode()),
            shared_from_this());
    }

protected:

    /// Asks the modifier to compute its results based on the now available upstream pipeline data.
    virtual void evaluateModifier(PromiseBase promise) noexcept override {
        OVITO_ASSERT(resultStorage()); // Upstream data must already be stored in this task's results storage.
        evaluateModifierIfReady(std::move(promise));
    }

private:

    /// This callback gets invoked once the auxiliary data becomes ready.
    void auxiliaryInputAvailable(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
        _auxiliaryFuture = AuxiliaryFutureType{std::move(finishedTask)};
        evaluateModifierIfReady(std::move(promise));
    }

    /// Performs the actual modifier computation if all necessary inputs (upstream pipeline data and auxiliary data) are available.
    void evaluateModifierIfReady(PromiseBase promise) noexcept {
        if(_auxiliaryFuture && resultStorage()) {
            Task::Scope taskScope(this);

            Future<PipelineFlowState> modifierFuture;
            handleModifierExceptions([&]() {
                modifierFuture = static_object_cast<ModifierClass>(modifier())->evaluateComplexModifier(request(), PipelineFlowState{resultStorage()}, std::move(_auxiliaryFuture).result());
                OVITO_ASSERT(modifierFuture);

                // Register the task to indicate in the UI that the pipeline node is currently doing some work.
                if(!request().interactiveMode())
                    modificationNode()->registerActiveFuture(modifierFuture);
            });

            // Schedule callback to be invoked once the modifier yields its results.
            if(modifierFuture) {
                whenTaskFinishes<ModifierEvaluationTask, &ComplexModifierEvaluationTask::modifierResultsAvailable>(
                    std::move(modifierFuture),
                    ObjectExecutor(modificationNode()),
                    std::move(promise));
            }
        }
        else {
            // Keep waiting for remaining input data.
            // Discard promise to make sure this task does not get canceled prematurely.
            promise.takeTask();
        }
    }

private:

    /// Takes care of waiting for the auxiliary input of the modifier to become available.
    TaskAwaiter _auxiliaryAwaiter{static_cast<Task&>(*this)};

    /// Holds the auxiliary modifier input once it becomes available.
    AuxiliaryFutureType _auxiliaryFuture;
};

}   // End of namespace
