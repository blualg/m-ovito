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


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Lets the user edit the simulation cell parameters and boundary conditions.
 */
class OVITO_STDMOD_EXPORT EditSimulationCellModifier : public Modifier
{
    OVITO_CLASS(EditSimulationCellModifier)

public:

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The three cell vectors and the cell origin.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(AffineTransformation{AffineTransformation::Zero()}, cellMatrix, setCellMatrix);

    /// Specifies whether the modifier will override the cell geometry.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, replaceCell, setReplaceCell);

    /// Specifies periodic boundary condition in the X direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcX, setPbcX);
    /// Specifies periodic boundary condition in the Y direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcY, setPbcY);
    /// Specifies periodic boundary condition in the Z direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcZ, setPbcZ);

    /// The dimensionality of the system.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, is2D, setIs2D);

    /// Flags indicating that the PBC or dimensionality parameters have not been set by the user (or Python API)
    /// before the modifier is inserted into a pipeline. In this case, they are initialized from the upstream input cell
    /// in the initializeModifier() method.
    bool _uninitializedPBC = true;
    bool _uninitializedDimensionality = true;
};

}   // End of namespace
