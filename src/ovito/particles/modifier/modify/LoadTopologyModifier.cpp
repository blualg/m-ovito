////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/particles/objects/Particles.h>
#include "LoadTopologyModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LoadTopologyModifier);
OVITO_CLASSINFO(LoadTopologyModifier, "DisplayName", "Load topology");
OVITO_CLASSINFO(LoadTopologyModifier, "Description", "Load static topology and particle properties from a separate topology file.");
OVITO_CLASSINFO(LoadTopologyModifier, "ModifierCategory", "Visualization");

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool LoadTopologyModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* This modifier only provides a UI action; the pipeline rewrite is done by its editor.
******************************************************************************/
Future<PipelineFlowState> LoadTopologyModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    return std::move(state);
}

}   // End of namespace
