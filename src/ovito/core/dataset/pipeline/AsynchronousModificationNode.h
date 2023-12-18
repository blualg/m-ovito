////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "ModificationNode.h"
#include "AsynchronousModifier.h"

namespace Ovito {

/**
 * \brief Represents the use of an AsynchronousModifier in a data pipeline.
 */
class OVITO_CORE_EXPORT AsynchronousModificationNode : public ModificationNode
{
    OVITO_CLASS(AsynchronousModificationNode)
    OVITO_CLASSINFO("ClassNameAlias", "AsynchronousModifierApplication");  // For backward compatibility with OVITO 3.9.2

public:

    /// \brief Constructs a modifier application.
    explicit AsynchronousModificationNode(ObjectInitializationFlags flags) : ModificationNode(flags) {}

    /// Returns the sequence of compute engines from a recent successfully completed modifier evaluation which are still valid.
    const std::vector<AsynchronousModifier::EnginePtr>& validStages() const { return _validStages; }

    /// Stores the sequence of compute engines from a recent successfully completed modifier evaluation.
    void setValidStages(std::vector<AsynchronousModifier::EnginePtr> validStages) { _validStages = std::move(validStages); }

    /// Returns a compute engine containing the results of a fully completed algorithm, which may be outdated.
    const AsynchronousModifier::EnginePtr& completedEngine() const { return _completedEngine; }

    /// Stores the compute engine containing the results of a fully completed algorithm.
    void setCompletedEngine(AsynchronousModifier::EnginePtr eng) { _completedEngine = std::move(eng); }

protected:

    /// \brief Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// The sequence of compute engines from a recent successfully completed modifier evaluation which are still valid.
    std::vector<AsynchronousModifier::EnginePtr> _validStages;

    /// A compute engine containing the results of a fully completed algorithm, which may be outdated.
    AsynchronousModifier::EnginePtr _completedEngine;
};

}   // End of namespace
