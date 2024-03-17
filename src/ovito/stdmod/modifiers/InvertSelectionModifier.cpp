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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "InvertSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(InvertSelectionModifier);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
InvertSelectionModifier::InvertSelectionModifier(ObjectInitializationFlags flags) : GenericPropertyModifier(flags)
{
    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> InvertSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No data element type set."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    if(!container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        throw Exception(tr("Cannot invert selection, because property container type %1 does not support element selections.").arg(container->getOOMetaClass().name()));

    ConstPropertyPtr inputSelection = container->getProperty(Property::GenericSelectionProperty);
    PropertyPtr outputSelection = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);

    // The actual computation can be performed in a separate worker thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([
            state = std::move(state),
            inputSelection = std::move(inputSelection),
            outputSelection = std::move(outputSelection)]() mutable
    {
        if(inputSelection) {
#ifdef OVITO_USE_SYCL
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<SelectionIntType, access_mode::read> inputAcc(inputSelection, cgh);
                SyclBufferAccess<SelectionIntType, access_mode::discard_write> outputAcc(outputSelection, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, InvertSelection_kernel)(sycl::range(inputAcc.size()), [=](size_t i) {
                    outputAcc[i] = !inputAcc[i];
                });
            });
#else
            BufferReadAccess<SelectionIntType> inputAcc(inputSelection);
            auto i = inputAcc.begin();
            for(auto& o : BufferWriteAccess<SelectionIntType, access_mode::discard_write>(outputSelection))
                o = !(*i++);
#endif
        }
        else {
            outputSelection->fill(SelectionIntType{1});
        }

        return std::move(state);
    });
}

}   // End of namespace
