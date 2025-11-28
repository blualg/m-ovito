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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "EditSimulationCellModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(EditSimulationCellModifier);
OVITO_CLASSINFO(EditSimulationCellModifier, "DisplayName", "Edit simulation cell");
OVITO_CLASSINFO(EditSimulationCellModifier, "Description", "Edit the simulation cell parameters and boundary conditions.");
OVITO_CLASSINFO(EditSimulationCellModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, cellMatrix);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, replaceCell);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcX);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcY);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, pbcZ);
DEFINE_PROPERTY_FIELD(EditSimulationCellModifier, is2D);
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, cellMatrix, "Cell matrix");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, replaceCell, "Override cell");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcX, "Periodic boundary conditions (X)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcY, "Periodic boundary conditions (Y)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, pbcZ, "Periodic boundary conditions (Z)");
SET_PROPERTY_FIELD_LABEL(EditSimulationCellModifier, is2D, "2D");
SET_PROPERTY_FIELD_UNITS(EditSimulationCellModifier, cellMatrix, WorldParameterUnit);

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void EditSimulationCellModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(cellMatrix() == AffineTransformation::Zero()) {
        // When the modifier is first inserted, automatically adopt the input cell matrix and boundary conditions.
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const SimulationCell* inputCell = input.getObject<SimulationCell>()) {
            setCellMatrix(inputCell->cellMatrix());
            if(_uninitializedPBC) {
                setPbcX(inputCell->pbcX());
                setPbcY(inputCell->pbcY());
                setPbcZ(inputCell->pbcZ());
            }
            if(_uninitializedDimensionality) {
                setIs2D(inputCell->is2D());
            }
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void EditSimulationCellModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(pbcX) || field == PROPERTY_FIELD(pbcY) || field == PROPERTY_FIELD(pbcZ)) {
        _uninitializedPBC = false; // User has explicitly set PBC parameters.
    }
    if(field == PROPERTY_FIELD(is2D)) {
        _uninitializedDimensionality = false; // User has explicitly set dimensionality parameter.
    }

    Modifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> EditSimulationCellModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    SimulationCell* cell = state.expectMutableObject<SimulationCell>();

    cell->setIs2D(is2D());
    cell->setPbcFlags(pbcX(), pbcY(), !is2D() && pbcZ());
    if(replaceCell())
        cell->setCellMatrix(cellMatrix());

    return std::move(state);
}

}   // End of namespace
