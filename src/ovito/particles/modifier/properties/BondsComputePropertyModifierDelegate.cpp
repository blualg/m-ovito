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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/dataset/DataSet.h>
#include "BondsComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondsComputePropertyModifierDelegate);
OVITO_CLASSINFO(BondsComputePropertyModifierDelegate, "DisplayName", "Bonds");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds())
            return { DataObjectReference(&Particles::OOClass()) };
    }
    return {};
}

/******************************************************************************
* Initializes an expression evaluator.
******************************************************************************/
std::unique_ptr<PropertyExpressionEvaluator> BondsComputePropertyModifierDelegate::initializeExpressionEvaluator(const ComputePropertyModifier* modifier, const PipelineFlowState& originalState, int frame) const
{
    auto evaluator = std::make_unique<BondExpressionEvaluator>();
    evaluator->initialize(modifier->expressions(), originalState, originalState.expectObject(inputContainerRef()), frame);
    return evaluator;
}

}   // End of namespace
