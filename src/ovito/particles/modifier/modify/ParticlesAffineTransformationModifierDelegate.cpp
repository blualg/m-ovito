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
#include <ovito/particles/objects/VectorVis.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/SyclBufferAccess.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ParticlesAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesAffineTransformationModifierDelegate);
OVITO_CLASSINFO(ParticlesAffineTransformationModifierDelegate, "DisplayName", "Particles");
IMPLEMENT_CREATABLE_OVITO_CLASS(VectorParticlePropertiesAffineTransformationModifierDelegate);
OVITO_CLASSINFO(VectorParticlePropertiesAffineTransformationModifierDelegate, "DisplayName", "Vector properties");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState),
            selectionOnly = modifier->selectionOnly()]() mutable {

        if(const Particles* inputParticles = state.getObject<Particles>()) {
            inputParticles->verifyIntegrity();

            // Get the input particle coordinates (as strong reference to force creation of a mutable clone below).
            ConstPropertyPtr inputPositionProperty = inputParticles->expectProperty(Particles::PositionProperty);

            // Make sure we can safely modify the particles object.
            Particles* outputParticles = state.makeMutable(inputParticles);

            // Create an uninitialized copy of the particle position property.
            Property* outputPositionProperty = outputParticles->makePropertyMutable(inputPositionProperty, DataBuffer::Uninitialized);

            // Let the modifier class do the actual coordinate transformation work.
            AffineTransformationModifier::transformCoordinates(tm, selectionOnly, inputPositionProperty, outputPositionProperty, inputParticles->getProperty(Particles::SelectionProperty));
        }

        return std::move(state);
    });
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> VectorParticlePropertiesAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all properties in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Property::OOClass())) {
        if(isTransformableProperty(path.lastAs<Property>()))
            objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Decides if the given particle property is one that should be transformed.
******************************************************************************/
bool VectorParticlePropertiesAffineTransformationModifierDelegate::isTransformableProperty(const Property* property)
{
    OVITO_ASSERT(property);

    // Transfer any property that has a VectorVis element attached and which has the right data type.
    return property->visElement<VectorVis>() != nullptr && (property->dataType() == DataBuffer::Float32 || property->dataType() == DataBuffer::Float64) && property->componentCount() == 3;
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> VectorParticlePropertiesAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState),
            selectionOnly = modifier->selectionOnly()]() mutable {

        CloneHelper cloneHelper;
        for(const ConstDataObjectPath& objectPath : state.getObjectsRecursive(Property::OOClass())) {
            const Property* inputProperty = objectPath.lastAs<Property>();
            if(inputProperty && isTransformableProperty(inputProperty)) {
                // Get the parent property container.
                const PropertyContainer* container = objectPath.lastAs<PropertyContainer>(1);
                if(!container)
                    throw Exception(tr("Cannot transform vector property '%1' because it is not part of a property container.").arg(inputProperty->name()));
                container->verifyIntegrity();

                // Check if there is a selection property present.
                const Property* selProperty = nullptr;
                if(container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
                    selProperty = container->getProperty(Property::GenericSelectionProperty);
                }

                // Strong reference to the input vectors to force creation of a mutable clone below.
                ConstPropertyPtr inputPropertyRef = inputProperty;

                // Make the property container mutable.
                DataObjectPath mutableContainerPath = state.makeMutable(objectPath.parentPath(), cloneHelper);
                PropertyContainer* mutableContainer = static_object_cast<PropertyContainer>(mutableContainerPath.last());

                // Create an uninitialized copy of the vector property.
                Property* outputProperty = mutableContainer->makePropertyMutable(inputPropertyRef, DataBuffer::Uninitialized);

                // Let the modifier class do the actual vector transformation work.
                AffineTransformationModifier::transformVectors(tm, selectionOnly, inputPropertyRef, outputProperty, selProperty);
            }
        }

        return std::move(state);
    });
}

}   // End of namespace
