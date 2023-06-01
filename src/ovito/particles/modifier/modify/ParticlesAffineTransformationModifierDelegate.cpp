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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/VectorVis.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/SyclBufferAccess.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "ParticlesAffineTransformationModifierDelegate.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParticlesAffineTransformationModifierDelegate);
IMPLEMENT_OVITO_CLASS(VectorParticlePropertiesAffineTransformationModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<ParticlesObject>())
        return { DataObjectReference(&ParticlesObject::OOClass()) };
    return {};
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus ParticlesAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    using namespace cl::sycl;

    if(const ParticlesObject* inputParticles = state.getObject<ParticlesObject>()) {
        inputParticles->verifyIntegrity();

        // Make sure we can safely modify the particles object.
        ParticlesObject* outputParticles = state.makeMutable(inputParticles);

        // Create a modifiable copy of the particle position.
        PropertyObject* posProperty = outputParticles->expectMutableProperty(ParticlesObject::PositionProperty);
        if(posProperty->size() != 0) {

            // Determine transformation matrix.
            AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(request.modifier());
            const AffineTransformation tm = mod->effectiveAffineTransformation(inputState);

            if(mod->selectionOnly()) {
                if(const PropertyObject* selProperty = inputParticles->getProperty(ParticlesObject::SelectionProperty)) {
#ifdef OVITO_USE_SYCL
                    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](handler& cgh) {
                        SyclBufferAccess<Point3> posAccessor(posProperty, cgh);
                        SyclBufferAccess<const SelectionIntType> selAccessor(selProperty, cgh);
                        cgh.parallel_for<class particles_affine_transformation_selection>(range<>(posProperty->size()), [=](size_t i) {
                            if(selAccessor[i])
                                posAccessor[i] = tm * posAccessor[i];
                        });
                    });
#else
                    BufferReadAccess<SelectionIntType> selAccess;
                    const auto* s = selAccess.cbegin();
                    for(Point3& p : BufferWriteAccess<Point3, access_mode::read_write>(posProperty)) {
                        if(*s++)
                            p = tm * p;
                    }
#endif
                }
            }
            else {
                // Check if the matrix describes a pure translation. If yes, we can
                // simply add vectors instead of computing full matrix products.
                if(tm.isTranslationMatrix()) {
                    const Vector3 translation = tm.translation();
#ifdef OVITO_USE_SYCL
                    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](handler& cgh) {
                        SyclBufferAccess<Point3> posAccessor(posProperty, cgh);
                        cgh.parallel_for<class particles_affine_transformation_simple_translation>(range<>(posProperty->size()), [=](size_t i) {
                            posAccessor[i] += translation;
                        });
                    });
#else
                    for(Point3& p : BufferWriteAccess<Point3, access_mode::read_write>(posProperty))
                        p += translation;
#endif
                }
                else {
#ifdef OVITO_USE_SYCL
                    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](handler& cgh) {
                        SyclBufferAccess<Point3> posAccessor(posProperty, cgh);
                        cgh.parallel_for<class particles_affine_transformation_full_xform>(range<>(posProperty->size()), [=](size_t i) {
                            posAccessor[i] = tm * posAccessor[i];
                        });
                    });
#else
                    for(Point3& p : BufferAccess<Point3>(posProperty))
                        p = tm * p;
#endif
                }
            }
        }
        outputParticles->verifyIntegrity();
    }

    return PipelineStatus::Success;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VectorParticlePropertiesAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all properties in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(PropertyObject::OOClass())) {
        if(isTransformableProperty(path.lastAs<PropertyObject>()))
            objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Decides if the given particle property is one that should be transformed.
******************************************************************************/
bool VectorParticlePropertiesAffineTransformationModifierDelegate::isTransformableProperty(const PropertyObject* property)
{
    OVITO_ASSERT(property);

    // Transfer any property that has a VectorVis element attached and which has the right data type.
    return property->visElement<VectorVis>() != nullptr && (property->dataType() == DataBuffer::Float32 || property->dataType() == DataBuffer::Float64) && property->componentCount() == 3;
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus VectorParticlePropertiesAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    CloneHelper cloneHelper;
    for(const ConstDataObjectPath& objectPath : state.getObjectsRecursive(PropertyObject::OOClass())) {
        const PropertyObject* inputProperty = objectPath.lastAs<PropertyObject>();
        if(isTransformableProperty(inputProperty)) {
            DataObjectPath mutableObjectPath = state.makeMutable(objectPath, cloneHelper);

            // Determine transformation matrix.
            AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(request.modifier());

            const PropertyContainer* container = mutableObjectPath.lastAs<PropertyContainer>(1);
            PropertyObject* property = mutableObjectPath.lastAs<PropertyObject>();
            if(property->dataType() == DataBuffer::Float32) {
                const auto tm = mod->effectiveAffineTransformation(inputState).toDataType<float>();
                BufferWriteAccess<Vector_3<float>, access_mode::read_write> propertyAccess(property);
                if(!mod->selectionOnly() || !container || !container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
                    for(auto& v : propertyAccess)
                        v = tm * v;
                }
                else {
                    if(BufferReadAccess<SelectionIntType> selProperty = container->getProperty(PropertyObject::GenericSelectionProperty)) {
                        const auto* s = selProperty.cbegin();
                        for(auto& v : propertyAccess) {
                            if(*s++)
                                v = tm * v;
                        }
                    }
                }
            }
            else {
                const auto tm = mod->effectiveAffineTransformation(inputState).toDataType<double>();
                BufferWriteAccess<Vector_3<double>, access_mode::read_write> propertyAccess(property);
                if(!mod->selectionOnly() || !container || !container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
                    for(auto& v : propertyAccess)
                        v = tm * v;
                }
                else {
                    if(BufferReadAccess<SelectionIntType> selProperty = container->getProperty(PropertyObject::GenericSelectionProperty)) {
                        const auto* s = selProperty.cbegin();
                        for(auto& v : propertyAccess) {
                            if(*s++)
                                v = tm * v;
                        }
                    }
                }
            }
        }
    }

    return PipelineStatus::Success;
}

}   // End of namespace
