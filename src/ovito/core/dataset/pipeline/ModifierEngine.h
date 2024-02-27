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
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ModifierEvaluationRequest.h"

namespace Ovito {

/**
 * Abstract base class for algorithm engines performing the modifier's computation in a background thread.
 */
class OVITO_CORE_EXPORT ModifierEngine : public AsynchronousTask<>
{
public:

    /// Constructor.
    explicit ModifierEngine(const ModifierEvaluationRequest& request, const TimeInterval& validityInterval = TimeInterval::infinite()) :
        _request(request), _validityInterval(validityInterval) {}

#ifdef Q_OS_LINUX
    /// Destructor.
    virtual ~ModifierEngine();
#endif

    /// Injects the computed results into the data pipeline.
    virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) {
        state.intersectStateValidity(validityInterval());
    }

    /// This method is called by the system whenever a parameter of the modifier changes.
    /// The method can be overridden by subclasses to indicate to the caller whether the engine object should be
    /// discarded (false) or may be kept in the cache, because the computation results are not affected by the changing parameter (true).
    virtual bool modifierChanged(const PropertyFieldEvent& event) { return false; }

    /// This method is called by the system whenever the preliminary pipeline input changes.
    /// The method should indicate to the caller whether the cached engine object can be
    /// kept around in a transient phase until a full evaluation is started (return true) or
    /// should rather be immediately discarded (return false).
    virtual bool pipelineInputChanged() { return true; }

    /// Decides whether the computation is sufficiently short to perform
    /// it synchronously within the GUI thread. The default implementation returns false,
    /// which means the computation will be performed asynchronously in a worker thread.
    virtual bool preferSynchronousExecution() { return false; }

    /// Returns the validity interval of the stored computation results.
    const TimeInterval& validityInterval() const { return _validityInterval; }

    /// Changes the validity interval of the computation results.
    void setValidityInterval(const TimeInterval& iv) { _validityInterval = iv; }

private:

    /// The modifier evaludation request this engine was launched for.
    const ModifierEvaluationRequest _request;

    /// The validity time interval of the stored computation results.
    TimeInterval _validityInterval;
};

#ifndef Core_EXPORTS
// Export this class template specialization from the DLL under Windows.
extern template class OVITO_CORE_EXPORT Future<ModifierEnginePtr>;
#endif

}   // End of namespace
