////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
	if(const ParticlesObject* inputParticles = state.getObject<ParticlesObject>()) {
		inputParticles->verifyIntegrity();

		// Make sure we can safely modify the particles object.
		ParticlesObject* outputParticles = state.makeMutable(inputParticles);

		// Create a modifiable copy of the particle position.
		PropertyAccess<Point3> posProperty = outputParticles->expectMutableProperty(ParticlesObject::PositionProperty);

		// Determine transformation matrix.
		AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(request.modifier());
		const AffineTransformation tm = mod->effectiveAffineTransformation(inputState);

		if(mod->selectionOnly()) {
			if(ConstPropertyAccess<int> selProperty = inputParticles->getProperty(ParticlesObject::SelectionProperty)) {
				const int* s = selProperty.cbegin();
				for(Point3& p : posProperty) {
					if(*s++)
						p = tm * p;
				}
			}
		}
		else {
			// Check if the matrix describes a pure translation. If yes, we can
			// simply add vectors instead of computing full matrix products.
			Vector3 translation = tm.translation();
			if(tm == AffineTransformation::translation(translation)) {
				for(Point3& p : posProperty)
					p += translation;
			}
			else {
				for(Point3& p : posProperty)
					p = tm * p;
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
	return property->visElement<VectorVis>() != nullptr && property->dataType() == DataBuffer::Float && property->componentCount() == 3;
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
			const AffineTransformation tm = mod->effectiveAffineTransformation(inputState);

			const PropertyContainer* container = mutableObjectPath.lastAs<PropertyContainer>(1);
			PropertyAccess<Vector3> property = mutableObjectPath.lastAs<PropertyObject>();

			if(!mod->selectionOnly() || !container || !container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
				for(Vector3& v : property)
					v = tm * v;
			}
			else {
				if(ConstPropertyAccess<int> selProperty = container->getProperty(PropertyObject::GenericSelectionProperty)) {
					const int* s = selProperty.cbegin();
					for(Vector3& v : property) {
						if(*s++)
							v = tm * v;
					}
				}
			}
		}
	}

	return PipelineStatus::Success;
}

}	// End of namespace
