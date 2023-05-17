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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "InvertSelectionModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(InvertSelectionModifier);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
InvertSelectionModifier::InvertSelectionModifier(ObjectInitializationFlags flags) : GenericPropertyModifier(flags)
{
    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("ParticlesObject"));
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void InvertSelectionModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    if(!subject())
        throw Exception(tr("No data element type set."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    DataBufferAccess<SelectionIntType> selProperty = container->createProperty(DataBuffer::Initialized, PropertyObject::GenericSelectionProperty);
    for(auto& s : selProperty)
        s = !s;
}

}   // End of namespace
