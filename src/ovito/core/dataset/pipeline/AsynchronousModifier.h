////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "Modifier.h"
#include "ModifierApplication.h"

namespace Ovito {

/**
 * \brief Base class for modifiers that compute their results in a background thread.
 */
class OVITO_CORE_EXPORT AsynchronousModifier : public Modifier
{
    OVITO_CLASS(AsynchronousModifier)

public:

    /**
     * Abstract base class for algorithm engines performing the modifier's computation in a background thread.
     */
    class OVITO_CORE_EXPORT Engine : public AsynchronousTask<>
    {
    public:

        /// Constructor.
        explicit Engine(const ModifierEvaluationRequest& request, const TimeInterval& validityInterval = TimeInterval::infinite()) :
            _request(request), _validityInterval(validityInterval) {}

#ifdef Q_OS_LINUX
        /// Destructor.
        virtual ~Engine();
#endif

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) = 0;

        /// This method is called by the system whenever a parameter of the modifier changes.
        /// The method can be overridden by subclasses to indicate to the caller whether the engine object should be 
        /// discarded (false) or may be kept in the cache, because the computation results are not affected by the changing parameter (true). 
        virtual bool modifierChanged(const PropertyFieldEvent& event) { return false; }

        /// This method is called by the system whenever the preliminary pipeline input changes.
        /// The method should indicate to the caller whether the cached engine object can be 
        /// kept around in a transient phase until a full evaluation is started (return true) or 
        /// should rather be immediately discarded (return false).
        virtual bool pipelineInputChanged() { return true; }

        /// Creates another engine that performs the next stage of the computation. 
        virtual std::shared_ptr<Engine> createContinuationEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) { return {}; }

        /// Decides whether the computation is sufficiently short to perform
        /// it synchronously within the GUI thread. The default implementation returns false,
        /// which means the computation will be performed asynchronously in a worker thread.
        virtual bool preferSynchronousExecution() { return false; }

        /// Returns the validity interval of the stored computation results.
        const TimeInterval& validityInterval() const { return _validityInterval; }

        /// Changes the validity interval of the computation results.
        void setValidityInterval(const TimeInterval& iv) { _validityInterval = iv; }

        /// Returns the object to be set as data source of data objects newly created by the engine.
        const PipelineObject* dataSource() const { return _request.modApp(); }

    private:

        /// The modifier evaludation request this engine was launched for.
        const ModifierEvaluationRequest _request;

        /// The validity time interval of the stored computation results.
        TimeInterval _validityInterval;
    };

    /// A managed pointer to an Engine instance.
    using EnginePtr = std::shared_ptr<Engine>;

public:

    /// Constructor.
    using Modifier::Modifier;

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Suppress preliminary viewport updates when a parameter of the asynchronous modifier changes.
    virtual bool performPreliminaryUpdateAfterChange() override { return false; }

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;
    
    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Asks the object for the result of the data pipeline.
    virtual Future<PipelineFlowState> evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) = 0;

    /// This function is called from AsynchronousModifier::evaluateSynchronous() to apply the results from the last 
    /// asycnhronous compute engine during a synchronous pipeline evaluation.
    virtual bool applyCachedResultsSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state);
};

#ifndef Core_EXPORTS
// Export this class template specialization from the DLL under Windows.
extern template class OVITO_CORE_EXPORT Future<AsynchronousModifier::EnginePtr>;
#endif

}   // End of namespace
