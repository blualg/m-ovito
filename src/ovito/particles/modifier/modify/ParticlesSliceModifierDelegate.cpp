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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ParticlesSliceModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesSliceModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());

    const Particles* inputParticles = state.expectObject<Particles>();
    inputParticles->verifyIntegrity();

    // Get the required input properties.
    const Property* posProperty = inputParticles->expectProperty(Particles::PositionProperty);
    const Property* selProperty = modifier->applyToSelection() ? inputParticles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    // The actual work can be performed in a separate thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([
            state = std::move(state),
            plane,
            sliceWidth,
            invert = modifier->inverse(),
            createSelection = modifier->createSelection(),
            posProperty,
            selProperty,
            inputParticles]() mutable
    {
        // Number of marked/selected particles.
        size_t numMarked = 0;

        // Create mask array to be computed.
        PropertyPtr mask = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, inputParticles->elementCount(), Particles::SelectionProperty);
        OVITO_ASSERT(posProperty->size() == mask->size());
        OVITO_ASSERT(!selProperty || selProperty->size() == mask->size());

#ifdef OVITO_USE_SYCL
        if(mask->size() != 0) {
            // This is a single-element counter variable that will be incremented by the kernel for each marked element.
            sycl::buffer<size_t> numMarkedBuf(&numMarked, 1);

            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                // Access the input coordinates.
                SyclBufferAccess<Point3, access_mode::read> posAcc(posProperty, cgh);
                // Access the input selection flags.
                SyclBufferAccess<SelectionIntType, access_mode::read> selAcc(selProperty, cgh);
                // Access output flags array.
                SyclBufferAccess<SelectionIntType, access_mode::write> maskAcc(mask, cgh, DataBuffer::Uninitialized);
#ifdef OVITO_USE_SYCL_ACPP
                auto reduction = sycl::reduction(sycl::accessor{numMarkedBuf, cgh, sycl::no_init}, size_t{0}, sycl::plus<size_t>());
#else
                auto reduction = sycl::reduction(numMarkedBuf, cgh, size_t{0}, sycl::plus<size_t>(), sycl::property::reduction::initialize_to_identity{});
#endif
                if(sliceWidth <= 0) {
                    OVITO_SYCL_PARALLEL_FOR(cgh, SliceModifier_particles_kernel1)(sycl::range(mask->size()), reduction, [=](size_t i, auto& red) {
                        if(!selAcc || selAcc[i]) {
                            if(plane.pointDistance(posAcc[i]) > 0) {
                                maskAcc[i] = 1;
                                red += (size_t)1;
                            }
                            else maskAcc[i] = 0;
                        }
                        else maskAcc[i] = 0;
                    });
                }
                else {
                    OVITO_SYCL_PARALLEL_FOR(cgh, SliceModifier_particles_kernel2)(sycl::range(mask->size()), reduction, [=](size_t i, auto& red) {
                        if(!selAcc || selAcc[i]) {
                            if(invert == (plane.classifyPoint(posAcc[i], sliceWidth) == 0)) {
                                maskAcc[i] = 1;
                                red += (size_t)1;
                            }
                            else maskAcc[i] = 0;
                        }
                        else maskAcc[i] = 0;
                    });
                }
            });
        }
#else
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> maskAcc(mask);
        BufferReadAccess<Point3> posAcc = posProperty;
        BufferReadAccess<SelectionIntType> selAcc = selProperty;

        auto m = maskAcc.begin();
        if(sliceWidth <= 0) {
            if(selAcc) {
                const auto* s = selAcc.cbegin();
                for(const Point3& p : posAcc) {
                    if(*s++ && plane.pointDistance(p) > 0) {
                        *m = 1;
                        numMarked++;
                    }
                    else *m = 0;
                    ++m;
                }
            }
            else {
                for(const Point3& p : posAcc) {
                    if(plane.pointDistance(p) > 0) {
                        *m = 1;
                        numMarked++;
                    }
                    else *m = 0;
                    ++m;
                }
            }
        }
        else {
            if(selAcc) {
                const auto* s = selAcc.cbegin();
                for(const Point3& p : posAcc) {
                    if(*s++ && invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                        *m = 1;
                        numMarked++;
                    }
                    else *m = 0;
                    ++m;
                }
            }
            else {
                for(const Point3& p : posAcc) {
                    if(invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                        *m = 1;
                        numMarked++;
                    }
                    else *m = 0;
                    ++m;
                }
            }
        }
        OVITO_ASSERT(m == maskAcc.end());
        posAcc.reset();
        selAcc.reset();
        maskAcc.reset();
#endif

        QString statusMessage = tr("%1 input particles").arg(inputParticles->elementCount());

        // Make sure we can safely modify the particles object.
        Particles* outputParticles = state.makeMutable(inputParticles);
        if(createSelection == false) {
            // Delete the marked particles.
            outputParticles->deleteElements(std::move(mask), numMarked);
            statusMessage += tr("\n%1 particles deleted").arg(numMarked);
            statusMessage += tr("\n%1 particles remaining").arg(outputParticles->elementCount());
        }
        else {
            // Create or replace the selection particle property.
            outputParticles->createProperty(std::move(mask));
            statusMessage += tr("\n%1 particles selected").arg(numMarked);
            statusMessage += tr("\n%1 particles unselected").arg(outputParticles->elementCount() - numMarked);
        }
        outputParticles->verifyIntegrity();

        state.setStatus(std::move(statusMessage));

        return std::move(state);
    });
}

}   // End of namespace
