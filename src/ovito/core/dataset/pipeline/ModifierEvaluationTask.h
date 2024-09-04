////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/core/utilities/concurrent/detail/ContinuationTask.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief An asynchronous task object that takes care of evaluating the modifier associated with a modification pipeline node.
 */
class OVITO_CORE_EXPORT ModifierEvaluationTask : public detail::ContinuationTask<PipelineFlowState>
{
public:

    /// The type of future associated with this task type. This is used by the launchTask() function.
    using future_type = Future<PipelineFlowState>;

    /// Constructor.
    ModifierEvaluationTask(ModifierEvaluationRequest&& request) :
        detail::ContinuationTask<PipelineFlowState>(Task::NoState),
        _request(std::move(request)) {}

    /// Returns the evaluation request descriptor.
    const ModifierEvaluationRequest& request() const { return _request; }

    /// Returns the modification node this task is associated with.
    decltype(auto) modificationNode() const noexcept {
        return request().modificationNode();
    }

    /// Returns the modifier this task is associated with.
    decltype(auto) modifier() const noexcept {
        return request().modifier();
    }

    /// Starts the execution of this task. This gets called by the launchTask() helper function.
    void operator()(SharedFuture<PipelineFlowState> inputFuture) noexcept {
        OVITO_ASSERT(inputFuture.isValid());

        // Schedule callback upon completion of the future that yields the input pipeline state.
        whenTaskFinishes<ModifierEvaluationTask, &ModifierEvaluationTask::inputStateAvailable>(
            inputFuture.takeTaskDependency(),
            *modificationNode(),
            shared_from_this());
    }

protected:

    /// This callback gets invoked once the input pipeline state has been computed.
    void inputStateAvailable(PromiseBase promise) noexcept {
        // Lock access to this task object.
        Task::MutexLock lock(*this);

        // Get the task that did just finish.
        auto inputTask = takeAwaitedTask();

        // Stop if the awaited task got canceled.
        if(!inputTask || inputTask->isCanceled())
            return; // The promise's destructor will take care of cancelling this task too.

        // Check if the awaited task completed with an error.
        if(inputTask->exceptionStore()) {
            // Forward the pipeline error state.
            exceptionLocked(inputTask->exceptionStore());
            finishLocked(lock);
            return;
        }

        // Get the input pipeline state.
        PipelineFlowState state = inputTask->template getResult<PipelineFlowState>();

        // Sanity check: With the throwOnError option set, the input data must never be in an error state.
        OVITO_ASSERT(!request().throwOnError() || state.status().type() != PipelineStatus::Error);

        // Clear the status of the input.
        state.setStatus(PipelineStatus::Success);

        // Store the input state for later.
        setResult<PipelineFlowState>(std::move(state));

        // Evaluation becomes a no-op if
        //  - the pipeline node doesn't have a modifier,
        //  - the modifier is disabled,
        //  - the upstream pipeline did not yield any data, or
        //  - the modifier cannot be evaluated in interactive mode (this is handled by the modifier directly).
        if(!modificationNode()->modifierAndGroupEnabled() || !resultStorage()) {
            finishLocked(lock);
            return;
        }
        OVITO_ASSERT(modifier());

        lock.unlock();

        // Let the modifier operate on the input pipeline state.
        evaluateModifier(std::move(promise));
    }

    /// Asks the modifier to compute its results based on the now available input.
    void evaluateModifier(PromiseBase promise) noexcept {

        Future<PipelineFlowState> modifierFuture;
        handleModifierExceptions([&]() {
            Task::Scope taskScope(this);
            modifierFuture = modifier()->evaluateModifier(request(), PipelineFlowState{resultStorage()});
            OVITO_ASSERT(modifierFuture.isValid());

            // Register the task to indicate in the UI that the pipeline node is currently doing some work.
            if(!request().interactiveMode())
                modificationNode()->registerActiveFuture(modifierFuture);
        });

        // Schedule callback to be invoked once the modifier yields its results.
        if(modifierFuture.isValid()) {
            whenTaskFinishes<ModifierEvaluationTask, &ModifierEvaluationTask::modifierResultsAvailable>(
                modifierFuture.takeTaskDependency(),
                *modificationNode(),
                std::move(promise));
        }
    }

    /// This callback gets invoked once the modifier has computed its results.
    void modifierResultsAvailable(PromiseBase promise) noexcept {
        // Lock access to this task object.
        Task::MutexLock lock(*this);

        // Get the task that did just finish.
        auto modifierTask = takeAwaitedTask();

        // Stop if the awaited task got canceled.
        if(!modifierTask || modifierTask->isCanceled())
            return; // The promise's destructor will take care of cancelling this task too.

        lock.unlock();

        // Check if the awaited task completed with an error.
        if(modifierTask->exceptionStore()) {
            // Process the error output state.
            handleModifierExceptions([&]() {
                std::rethrow_exception(modifierTask->exceptionStore());
            });
            return;
        }

        // Get the modifier's output pipeline state.
        setEvaluationResults(modifierTask->template takeResult<PipelineFlowState>());
    }

    /// Sets the final output of this evaluation task.
    void setEvaluationResults(PipelineFlowState&& state) noexcept {
        // Move the output pipeline state into the task.
        resultStorage() = std::move(state);

        // Indicate the outcome of the calculation in the GUI.
        if(!request().interactiveMode())
            modificationNode()->setStatus(resultStorage().status());

        // Return results to the caller.
        setFinished();
    }

    /// Helper function that takes care of handling (and converting) various exception types that may be thrown by a modifier function.
    template<typename Function>
    void handleModifierExceptions(Function&& func) noexcept {
        try {
            try {
                func();
            }
            catch(const OperationCanceled&) {
                throw;  // Pass through regular exceptions.
            }
            catch(const Exception&) {
                throw;  // Pass through regular exceptions.
            }
            catch(const std::bad_alloc&) {
                throw Exception(ModificationNode::tr("Not enough memory."));
            }
            catch(const std::exception& ex) {
                OVITO_ASSERT_MSG(false, "ModificationNode::evaluateInternal()", "Caught an unexpected exception type during modifier evaluation.");
                throw Exception(ModificationNode::tr("A non-standard exception occurred: %1").arg(QString::fromLatin1(ex.what())));
            }
            catch(...) {
                OVITO_ASSERT_MSG(false, "ModificationNode::evaluateInternal()", "Caught an unknown exception type during modifier evaluation.");
                throw Exception(ModificationNode::tr("An unknown type of exception occurred."));
            }
        }
        catch(Exception& ex) {
            // Indicate the failure of the modifier calculation in the GUI.
            if(!request().interactiveMode())
                modificationNode()->setStatus(ex);

            // In a Python environment, it's useful if the error message indicates which modifier has failed.
            ex.prependToMessage(ModificationNode::tr("Modifier '%1' reported: ").arg(modificationNode()->objectTitle()));

            // Forward the exception to the caller if error propagation along the pipeline has been requested.
            if(request().throwOnError()) {
                captureExceptionAndFinish();
                return;
            }

            // Fall back to the results produced by the upstream pipeline.
            resultStorage().setStatus(PipelineStatus(ex, QStringLiteral(" ")));
            setFinished();
        }
    }

private:

    /// The evaluation request descriptor.
    ModifierEvaluationRequest _request;
};

}   // End of namespace

