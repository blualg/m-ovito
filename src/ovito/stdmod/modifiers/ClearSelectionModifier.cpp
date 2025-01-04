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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ClearSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ClearSelectionModifier);
OVITO_CLASSINFO(ClearSelectionModifier, "DisplayName", "Clear selection");
OVITO_CLASSINFO(ClearSelectionModifier, "Description", "Reset the selection state of all elements.");
OVITO_CLASSINFO(ClearSelectionModifier, "ModifierCategory", "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
void ClearSelectionModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ClearSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No input element type selected."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    if(const Property* selProperty = container->getProperty(Property::GenericSelectionProperty))
        container->removeProperty(selProperty);

    return std::move(state);
}

}   // End of namespace
