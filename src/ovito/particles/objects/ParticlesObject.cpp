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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/BondType.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "ParticlesObject.h"
#include "ParticlesVis.h"
#include "BondsVis.h"
#include "VectorVis.h"
#include "ParticleBondMap.h"

#if QT_FEATURE_static > 0
	// This no-op helper function is called by the QML/Gui module 
	// to make sure the Particles plugin and its dependencies 
	// get linked into the static WASM executable and are not eliminated
	// by the linker.
	void ovito_static_plugin_Particles() {}
#endif

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParticlesObject);
DEFINE_REFERENCE_FIELD(ParticlesObject, bonds);
DEFINE_REFERENCE_FIELD(ParticlesObject, angles);
DEFINE_REFERENCE_FIELD(ParticlesObject, dihedrals);
DEFINE_REFERENCE_FIELD(ParticlesObject, impropers);
SET_PROPERTY_FIELD_LABEL(ParticlesObject, bonds, "Bonds");
SET_PROPERTY_FIELD_LABEL(ParticlesObject, angles, "Angles");
SET_PROPERTY_FIELD_LABEL(ParticlesObject, dihedrals, "Dihedrals");
SET_PROPERTY_FIELD_LABEL(ParticlesObject, impropers, "Impropers");

/******************************************************************************
* Constructor.
******************************************************************************/
ParticlesObject::ParticlesObject(ObjectCreationParams params) : PropertyContainer(params)
{
	// Assign the default data object identifier.
	setIdentifier(OOClass().pythonName());

	// Create and attach a default visualization element for rendering the particles.
	if(params.createVisElement())
		setVisElement(OORef<ParticlesVis>::create(params));
}

/******************************************************************************
* Duplicates the BondsObject if it is shared with other particle objects.
* After this method returns, the BondsObject is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
BondsObject* ParticlesObject::makeBondsMutable()
{
    OVITO_ASSERT(bonds());
    return makeMutable(bonds());
}

/******************************************************************************
* Duplicates the AnglesObject if it is shared with other particle objects.
* After this method returns, the AnglesObject is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
AnglesObject* ParticlesObject::makeAnglesMutable()
{
    OVITO_ASSERT(angles());
    return makeMutable(angles());
}

/******************************************************************************
* Duplicates the DihedralsObject if it is shared with other particle objects.
* After this method returns, the DihedralsObject is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
DihedralsObject* ParticlesObject::makeDihedralsMutable()
{
    OVITO_ASSERT(dihedrals());
    return makeMutable(dihedrals());
}

/******************************************************************************
* Duplicates the ImpropersObject if it is shared with other particle objects.
* After this method returns, the ImpropersObject is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
ImpropersObject* ParticlesObject::makeImpropersMutable()
{
    OVITO_ASSERT(impropers());
    return makeMutable(impropers());
}

/******************************************************************************
* Convinience method that makes sure that there is a BondsObject.
* Throws an exception if there isn't.
******************************************************************************/
const BondsObject* ParticlesObject::expectBonds() const
{
    if(!bonds())
		throwException(tr("This operation requires bonds as input, but the particle system has no bond topology defined."));
	return bonds();
}

/******************************************************************************
* Convinience method that makes sure that there is a BondsObject and the
* bond topology property. Throws an exception if there isn't.
******************************************************************************/
const PropertyObject* ParticlesObject::expectBondsTopology() const
{
    return expectBonds()->expectProperty(BondsObject::TopologyProperty);
}

/******************************************************************************
* Deletes the particles for which bits are set in the given bit-mask.
* Returns the number of deleted particles.
******************************************************************************/
size_t ParticlesObject::deleteElements(const boost::dynamic_bitset<>& mask)
{
	OVITO_ASSERT(mask.size() == elementCount());

	size_t deleteCount = mask.count();
	size_t oldParticleCount = elementCount();
	size_t newParticleCount = oldParticleCount - deleteCount;
	if(deleteCount == 0)
		return 0;	// Nothing to delete.

	// Delete the particles.
	PropertyContainer::deleteElements(mask);

	// Delete dangling bonds, i.e. those that are incident on deleted particles.
    if(bonds()) {
        // Make sure we can safely modify the bonds object.
        BondsObject* mutableBonds = makeBondsMutable();

        size_t oldBondCount = mutableBonds->elementCount();
        boost::dynamic_bitset<> deletedBondsMask(oldBondCount);

        // Build map from old particle indices to new indices.
        std::vector<size_t> indexMap(oldParticleCount);
        auto index = indexMap.begin();
        size_t count = 0;
        for(size_t i = 0; i < oldParticleCount; i++)
            *index++ = mask.test(i) ? std::numeric_limits<size_t>::max() : count++;

        // Remap particle indices of stored bonds and remove dangling bonds.
        if(const PropertyObject* topologyProperty = mutableBonds->getTopology()) {
            PropertyAccess<ParticleIndexPair> mutableTopology = mutableBonds->makeMutable(topologyProperty);
			for(size_t bondIndex = 0; bondIndex < oldBondCount; bondIndex++) {
                size_t index1 = mutableTopology[bondIndex][0];
                size_t index2 = mutableTopology[bondIndex][1];

                // Remove invalid bonds, i.e. whose particle indices are out of bounds.
                if(index1 >= oldParticleCount || index2 >= oldParticleCount) {
                    deletedBondsMask.set(bondIndex);
                    continue;
                }

                // Remove dangling bonds whose particles have gone.
                if(mask.test(index1) || mask.test(index2)) {
                    deletedBondsMask.set(bondIndex);
                    continue;
                }

                // Keep bond and remap particle indices.
                mutableTopology[bondIndex][0] = indexMap[index1];
                mutableTopology[bondIndex][1] = indexMap[index2];
            }
			mutableTopology.reset();

            // Delete the marked bonds.
            mutableBonds->deleteElements(deletedBondsMask);
        }
    }

	// Delete dangling angles, i.e. those that are incident on deleted particles.
    if(angles()) {
        // Make sure we can safely modify the angles object.
        AnglesObject* mutableAngles = makeAnglesMutable();

        size_t oldAngleCount = mutableAngles->elementCount();
        boost::dynamic_bitset<> deletedAnglesMask(oldAngleCount);

        // Build map from old particle indices to new indices.
        std::vector<size_t> indexMap(oldParticleCount);
        auto index = indexMap.begin();
        size_t count = 0;
        for(size_t i = 0; i < oldParticleCount; i++)
            *index++ = mask.test(i) ? std::numeric_limits<size_t>::max() : count++;

        // Remap particle indices of angles and remove dangling angles.
        if(const PropertyObject* topologyProperty = mutableAngles->getTopology()) {
            PropertyAccess<ParticleIndexTriplet> mutableTopology = mutableAngles->makeMutable(topologyProperty);
			for(size_t angleIndex = 0; angleIndex < oldAngleCount; angleIndex++) {
                size_t index1 = mutableTopology[angleIndex][0];
                size_t index2 = mutableTopology[angleIndex][1];
                size_t index3 = mutableTopology[angleIndex][2];

                // Remove invalid angles, i.e. whose particle indices are out of bounds.
                if(index1 >= oldParticleCount || index2 >= oldParticleCount || index3 >= oldParticleCount) {
                    deletedAnglesMask.set(angleIndex);
                    continue;
                }

                // Remove dangling angles whose particles have gone.
                if(mask.test(index1) || mask.test(index2) || mask.test(index3)) {
                    deletedAnglesMask.set(angleIndex);
                    continue;
                }

                // Keep angle and remap particle indices.
                mutableTopology[angleIndex][0] = indexMap[index1];
                mutableTopology[angleIndex][1] = indexMap[index2];
                mutableTopology[angleIndex][2] = indexMap[index3];
            }
			mutableTopology.reset();

            // Delete the marked angles.
            mutableAngles->deleteElements(deletedAnglesMask);
        }
    }

	// Delete dangling dihedrals, i.e. those that are incident on deleted particles.
    if(dihedrals()) {
        // Make sure we can safely modify the dihedrals object.
        DihedralsObject* mutableDihedrals = makeDihedralsMutable();

        size_t oldDihedralCount = mutableDihedrals->elementCount();
        boost::dynamic_bitset<> deletedDihedralsMask(oldDihedralCount);

        // Build map from old particle indices to new indices.
        std::vector<size_t> indexMap(oldParticleCount);
        auto index = indexMap.begin();
        size_t count = 0;
        for(size_t i = 0; i < oldParticleCount; i++)
            *index++ = mask.test(i) ? std::numeric_limits<size_t>::max() : count++;

        // Remap particle indices of angles and remove dangling dihedrals.
        if(const PropertyObject* topologyProperty = mutableDihedrals->getTopology()) {
            PropertyAccess<ParticleIndexQuadruplet> mutableTopology = mutableDihedrals->makeMutable(topologyProperty);
			for(size_t dihedralIndex = 0; dihedralIndex < oldDihedralCount; dihedralIndex++) {
                size_t index1 = mutableTopology[dihedralIndex][0];
                size_t index2 = mutableTopology[dihedralIndex][1];
                size_t index3 = mutableTopology[dihedralIndex][2];
                size_t index4 = mutableTopology[dihedralIndex][3];

                // Remove invalid dihedrals, i.e. whose particle indices are out of bounds.
                if(index1 >= oldParticleCount || index2 >= oldParticleCount || index3 >= oldParticleCount || index4 >= oldParticleCount) {
                    deletedDihedralsMask.set(dihedralIndex);
                    continue;
                }

                // Remove dangling dihedrals whose particles have gone.
                if(mask.test(index1) || mask.test(index2) || mask.test(index3) || mask.test(index4)) {
                    deletedDihedralsMask.set(dihedralIndex);
                    continue;
                }

                // Keep dihedral and remap particle indices.
                mutableTopology[dihedralIndex][0] = indexMap[index1];
                mutableTopology[dihedralIndex][1] = indexMap[index2];
                mutableTopology[dihedralIndex][2] = indexMap[index3];
                mutableTopology[dihedralIndex][3] = indexMap[index4];
            }
			mutableTopology.reset();

            // Delete the marked dihedrals.
            mutableDihedrals->deleteElements(deletedDihedralsMask);
        }
    }

	// Delete dangling impropers, i.e. those that are incident on deleted particles.
    if(impropers()) {
        // Make sure we can safely modify the impropers object.
        ImpropersObject* mutableImpropers = makeImpropersMutable();

        size_t oldImproperCount = mutableImpropers->elementCount();
        boost::dynamic_bitset<> deletedImpropersMask(oldImproperCount);

        // Build map from old particle indices to new indices.
        std::vector<size_t> indexMap(oldParticleCount);
        auto index = indexMap.begin();
        size_t count = 0;
        for(size_t i = 0; i < oldParticleCount; i++)
            *index++ = mask.test(i) ? std::numeric_limits<size_t>::max() : count++;

        // Remap particle indices of angles and remove dangling impropers.
        if(const PropertyObject* topologyProperty = mutableImpropers->getTopology()) {
            PropertyAccess<ParticleIndexQuadruplet> mutableTopology = mutableImpropers->makeMutable(topologyProperty);
			for(size_t improperIndex = 0; improperIndex < oldImproperCount; improperIndex++) {
                size_t index1 = mutableTopology[improperIndex][0];
                size_t index2 = mutableTopology[improperIndex][1];
                size_t index3 = mutableTopology[improperIndex][2];
                size_t index4 = mutableTopology[improperIndex][3];

                // Remove invalid impropers, i.e. whose particle indices are out of bounds.
                if(index1 >= oldParticleCount || index2 >= oldParticleCount || index3 >= oldParticleCount || index4 >= oldParticleCount) {
                    deletedImpropersMask.set(improperIndex);
                    continue;
                }

                // Remove dangling impropers whose particles have gone.
                if(mask.test(index1) || mask.test(index2) || mask.test(index3) || mask.test(index4)) {
                    deletedImpropersMask.set(improperIndex);
                    continue;
                }

                // Keep improper and remap particle indices.
                mutableTopology[improperIndex][0] = indexMap[index1];
                mutableTopology[improperIndex][1] = indexMap[index2];
                mutableTopology[improperIndex][2] = indexMap[index3];
                mutableTopology[improperIndex][3] = indexMap[index4];
            }
			mutableTopology.reset();

            // Delete the marked impropers.
            mutableImpropers->deleteElements(deletedImpropersMask);
        }
    }	

	return deleteCount;
}

/******************************************************************************
* Sorts the particles list with respect to particle IDs.
* Does nothing if particles do not have IDs.
******************************************************************************/
std::vector<size_t> ParticlesObject::sortById()
{
	std::vector<size_t> invertedPermutation = PropertyContainer::sortById();

	// If the storage order of particles has changed, we need to update other topological 
	// structures that refer to the particle indices.
	if(!invertedPermutation.empty()) {

		// Update bond topology data to match new particle ordering.
		if(bonds()) {
			if(PropertyAccess<ParticleIndexPair> bondTopology = makeBondsMutable()->getMutableProperty(BondsObject::TopologyProperty)) {
				for(ParticleIndexPair& bond : bondTopology) {
					for(qlonglong& idx : bond) {
						if(idx >= 0 && idx < (qlonglong)invertedPermutation.size())
							idx = invertedPermutation[idx];
					}
				}
			}
		}

		// Update angle topology data to match new particle ordering.
		if(angles()) {
			if(PropertyAccess<ParticleIndexTriplet> angleTopology = makeAnglesMutable()->getMutableProperty(AnglesObject::TopologyProperty)) {
				for(ParticleIndexTriplet& angle : angleTopology) {
					for(qlonglong& idx : angle) {
						if(idx >= 0 && idx < (qlonglong)invertedPermutation.size())
							idx = invertedPermutation[idx];
					}
				}
			}
		}

		// Update dihedral topology data to match new particle ordering.
		if(dihedrals()) {
			if(PropertyAccess<ParticleIndexQuadruplet> dihedralTopology = makeDihedralsMutable()->getMutableProperty(DihedralsObject::TopologyProperty)) {
				for(ParticleIndexQuadruplet& dihedral : dihedralTopology) {
					for(qlonglong& idx : dihedral) {
						if(idx >= 0 && idx < (qlonglong)invertedPermutation.size())
							idx = invertedPermutation[idx];
					}
				}
			}
		}

		// Update improper topology data to match new particle ordering.
		if(impropers()) {
			if(PropertyAccess<ParticleIndexQuadruplet> improperTopology = makeImpropersMutable()->getMutableProperty(ImpropersObject::TopologyProperty)) {
				for(ParticleIndexQuadruplet& improper : improperTopology) {
					for(qlonglong& idx : improper) {
						if(idx >= 0 && idx < (qlonglong)invertedPermutation.size())
							idx = invertedPermutation[idx];
					}
				}
			}
		}
	}
	return invertedPermutation;
}

/******************************************************************************
* Adds a set of new bonds to the particle system.
******************************************************************************/
void ParticlesObject::addBonds(const std::vector<Bond>& newBonds, BondsVis* bondsVis, const std::vector<PropertyPtr>& bondProperties, DataOORef<const BondType> bondType)
{
	OVITO_ASSERT(isSafeToModify());

	// Check if there is an existing bonds object.
	BondsObject* mutableBonds;
	if(!bonds()) {
		// Create the bonds object.
		DataOORef<BondsObject> bonds = DataOORef<BondsObject>::create(dataset());
		mutableBonds = bonds.get();
		setBonds(std::move(bonds));
	}
	else {
		mutableBonds = makeBondsMutable();
	}

	// Create new bonds making sure bonds are not created twice.
	mutableBonds->addBonds(newBonds, bondsVis, this, bondProperties, std::move(bondType));
}

/******************************************************************************
* Returns a property array with the input particle colors.
******************************************************************************/
ConstPropertyPtr ParticlesObject::inputParticleColors() const
{
	// Access the particles vis element.
	if(ParticlesVis* particleVis = visElement<ParticlesVis>()) {
		// Query particle colors from vis element.
		return particleVis->particleColors(this, false);
	}

	// Return an array with uniform colors if there is no vis element attached to the particles object.
	PropertyPtr colors = ParticlesObject::OOClass().createStandardProperty(dataset(), elementCount(), ParticlesObject::ColorProperty);
	colors->fill(Color(1,1,1));
	return colors;
}

/******************************************************************************
* Returns a property array with the input bond colors.
******************************************************************************/
ConstPropertyPtr ParticlesObject::inputBondColors(bool ignoreExistingColorProperty) const
{
	// Access the bonds vis element.
    if(bonds()) {
		if(BondsVis* bondsVis = bonds()->visElement<BondsVis>()) {

			// Request half-bond colors from vis element.
			std::vector<Color> halfBondColors = bondsVis->halfBondColors(this, false, bondsVis->coloringMode(), ignoreExistingColorProperty);
			OVITO_ASSERT(bonds()->elementCount() * 2 == halfBondColors.size());

			// Map half-bond colors to full bond colors.
			PropertyAccessAndRef<Color> colors = BondsObject::OOClass().createStandardProperty(dataset(), bonds()->elementCount(), BondsObject::ColorProperty);
			auto ci = halfBondColors.cbegin();
			for(Color& co : colors) {
				co = *ci;
				ci += 2;
			}
			return colors.take();
		}

		// If no vis element is available, create an array filled with the default bond color.
		PropertyPtr colors = BondsObject::OOClass().createStandardProperty(dataset(), bonds()->elementCount(), BondsObject::ColorProperty);
		colors->fill<Color>(Color(1,1,1));
		return colors;
    }
	return {};
}

/******************************************************************************
* Returns a property array with the input particle radii.
******************************************************************************/
ConstPropertyPtr ParticlesObject::inputParticleRadii() const
{
	// Access the particles vis element.
	if(ParticlesVis* particleVis = visElement<ParticlesVis>()) {

		// Query particle radii from vis element.
		return particleVis->particleRadii(this, false);
	}

	// Return uniform default radius for all particles.
	PropertyPtr buffer = OOClass().createStandardProperty(dataset(), elementCount(), ParticlesObject::RadiusProperty);
	buffer->fill<FloatType>(1);
	return buffer;
}

/******************************************************************************
* Returns a property array with the input particle masses.
******************************************************************************/
ConstPropertyPtr ParticlesObject::inputParticleMasses() const
{
	// Take masses directly from the 'Mass' property if available.
	if(const PropertyObject* massProperty = getProperty(ParticlesObject::MassProperty))
		return massProperty;

	if(const PropertyObject* typeProperty = getProperty(ParticlesObject::TypeProperty)) {
		// Assign masses based on particle types.
		// Build a lookup map for particle type masses.
		std::map<int,FloatType> massMap = ParticleType::typeMassMap(typeProperty);

		// Skip the following loop if all per-type masses are zero. In this case, simply use the default mass for all particles.
		if(boost::algorithm::any_of(massMap, [](const std::pair<int,FloatType>& it) { return it.second != 0; })) {

			// Allocate output array.
			PropertyAccessAndRef<FloatType> massProperty = ParticlesObject::OOClass().createStandardProperty(dataset(), elementCount(), ParticlesObject::MassProperty);

			// Fill output array using lookup table.
			ConstPropertyAccess<int> typeData(typeProperty);
			boost::transform(typeData, massProperty.begin(), [&](int t) {
				auto it = massMap.find(t);
				if(it != massMap.end())
					return it->second;
				else
					return 0.0;
			});

			return massProperty.take();
		}
	}

	// Return uniform default mass 0 for all particles.
	return OOClass().createStandardProperty(dataset(), elementCount(), ParticlesObject::MassProperty, DataBuffer::InitializeMemory);
}

/******************************************************************************
* Creates a storage object for standard particle properties.
******************************************************************************/
PropertyPtr ParticlesObject::OOMetaClass::createStandardPropertyInternal(DataSet* dataset, size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
	// Certain standard properties need to be initialized with default values determined by the visual element attached to the property container.
	if(flags.testFlag(DataBuffer::InitializeMemory) && !containerPath.empty()) {
		if(type == ColorProperty) {
			if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(containerPath.back())) {
				OVITO_ASSERT(particles->elementCount() == elementCount);
				ConstPropertyPtr property = particles->inputParticleColors();
				OVITO_ASSERT(property);
				OVITO_ASSERT(property->size() == elementCount);
				OVITO_ASSERT(property->type() == ColorProperty);
				return std::move(property).makeMutable();
			}
		}
		else if(type == RadiusProperty) {
			if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(containerPath.back())) {
				OVITO_ASSERT(particles->elementCount() == elementCount);
				ConstPropertyPtr property = particles->inputParticleRadii();
				OVITO_ASSERT(property);
				OVITO_ASSERT(property->size() == elementCount);
				OVITO_ASSERT(property->type() == RadiusProperty);
				return std::move(property).makeMutable();
			}
		}
	}

	int dataType;
	size_t componentCount;

	switch(type) {
	case TypeProperty:
	case StructureTypeProperty:
	case SelectionProperty:
	case CoordinationProperty:
	case MoleculeTypeProperty:
	case NucleobaseTypeProperty:
	case DNAStrandProperty:
		dataType = PropertyObject::Int;
		componentCount = 1;
		break;
	case IdentifierProperty:
	case ClusterProperty:
	case MoleculeProperty:
		dataType = PropertyObject::Int64;
		componentCount = 1;
		break;
	case PositionProperty:
	case DisplacementProperty:
	case VelocityProperty:
	case ForceProperty:
	case DipoleOrientationProperty:
	case AngularVelocityProperty:
	case AngularMomentumProperty:
	case TorqueProperty:
	case AsphericalShapeProperty:
	case NucleotideAxisProperty:
	case NucleotideNormalProperty:
		dataType = PropertyObject::Float;
		componentCount = 3;
		break;
	case ColorProperty:
	case VectorColorProperty:
		dataType = PropertyObject::Float;
		componentCount = 3;
		break;
	case PotentialEnergyProperty:
	case KineticEnergyProperty:
	case TotalEnergyProperty:
	case RadiusProperty:
	case MassProperty:
	case ChargeProperty:
	case TransparencyProperty:
	case SpinProperty:
	case DipoleMagnitudeProperty:
	case CentroSymmetryProperty:
	case DisplacementMagnitudeProperty:
	case VelocityMagnitudeProperty:
		dataType = PropertyObject::Float;
		componentCount = 1;
		break;
	case StressTensorProperty:
	case StrainTensorProperty:
	case ElasticStrainTensorProperty:
	case StretchTensorProperty:
		dataType = PropertyObject::Float;
		componentCount = 6;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(SymmetricTensor2));
		break;
	case DeformationGradientProperty:
	case ElasticDeformationGradientProperty:
		dataType = PropertyObject::Float;
		componentCount = 9;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Matrix3));
		break;
	case OrientationProperty:
	case RotationProperty:
		dataType = PropertyObject::Float;
		componentCount = 4;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Quaternion));
		break;
	case PeriodicImageProperty:
		dataType = PropertyObject::Int;
		componentCount = 3;
		break;
	case SuperquadricRoundnessProperty:
		dataType = PropertyObject::Float;
		componentCount = 2;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Vector2));
		break;
	default:
		OVITO_ASSERT_MSG(false, "ParticlesObject::createStandardProperty()", "Invalid standard property type");
		throw Exception(tr("This is not a valid standard property type: %1").arg(type));
	}

	const QStringList& componentNames = standardPropertyComponentNames(type);
	const QString& propertyName = standardPropertyName(type);

	OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

	// Allocate the storage array.
	PropertyPtr property = PropertyPtr::create(dataset, elementCount, dataType, componentCount, propertyName, flags & ~DataBuffer::InitializeMemory, type, componentNames);

	// Initialize memory if requested.
	if(flags.testFlag(DataBuffer::InitializeMemory) && !containerPath.empty()) {
		// Certain standard properties need to be initialized with default values determined by the attached visual elements.
		if(type == MassProperty) {
			if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(containerPath.back())) {
				if(const PropertyObject* typeProperty = particles->getProperty(ParticlesObject::TypeProperty)) {
					// Use per-type mass information and initialize the per-particle mass array from it.
					std::map<int,FloatType> massMap = ParticleType::typeMassMap(typeProperty);
					if(!massMap.empty()) {
						boost::transform(ConstPropertyAccess<int>(typeProperty), PropertyAccess<FloatType>(property).begin(), [&](int t) {
							auto iter = massMap.find(t);
							return iter != massMap.end() ? iter->second : FloatType(0);
						});
						flags.setFlag(DataBuffer::InitializeMemory, false);
					}
				}				
			}
		}
		else if(type == VectorColorProperty) {
			if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(containerPath.back())) {
				for(const PropertyObject* p : particles->properties()) {
					if(VectorVis* vectorVis = dynamic_object_cast<VectorVis>(p->visElement())) {
						property->fill(vectorVis->arrowColor());
						flags.setFlag(DataBuffer::InitializeMemory, false);
						break;
					}
				}
			}
		}
	}

	// Some properties get an attached visual element.
	if(type == ParticlesObject::DisplacementProperty) {
		OORef<VectorVis> vis = OORef<VectorVis>::create(dataset);
		vis->setObjectTitle(tr("Displacements"));
		vis->setEnabled(false);
		vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled)});
		property->addVisElement(std::move(vis));
	}
	else if(type == ParticlesObject::ForceProperty) {
		OORef<VectorVis> vis = OORef<VectorVis>::create(dataset);
		vis->setObjectTitle(tr("Forces"));
		vis->setEnabled(false);
		vis->setReverseArrowDirection(false);
		vis->setArrowPosition(VectorVis::Base);
		vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
		property->addVisElement(std::move(vis));
	}
	else if(type == ParticlesObject::VelocityProperty) {
		OORef<VectorVis> vis = OORef<VectorVis>::create(dataset);
		vis->setObjectTitle(tr("Velocities"));
		vis->setEnabled(false);
		vis->setReverseArrowDirection(false);
		vis->setArrowPosition(VectorVis::Base);
		vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
		property->addVisElement(std::move(vis));
	}
	else if(type == ParticlesObject::DipoleOrientationProperty) {
		OORef<VectorVis> vis = OORef<VectorVis>::create(dataset);
		vis->setObjectTitle(tr("Dipoles"));
		vis->setEnabled(false);
		vis->setReverseArrowDirection(false);
		vis->setArrowPosition(VectorVis::Center);
		vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
		property->addVisElement(std::move(vis));
	}

	if(flags.testFlag(DataBuffer::InitializeMemory)) {
		// Default-initialize property values with zeros.
		property->fillZero();
	}

	return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void ParticlesObject::OOMetaClass::initialize()
{
	PropertyContainerClass::initialize();

	// Enable automatic conversion of a ParticlePropertyReference to a generic PropertyReference and vice versa.
	QMetaType::registerConverter<ParticlePropertyReference, PropertyReference>();
	QMetaType::registerConverter<PropertyReference, ParticlePropertyReference>();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QMetaType::registerComparators<ParticlePropertyReference>();
#endif

	setPropertyClassDisplayName(tr("Particles"));
	setElementDescriptionName(QStringLiteral("particles"));
	setPythonName(QStringLiteral("particles"));

	const QStringList emptyList;
	const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
	const QStringList rgbList = QStringList() << "R" << "G" << "B";
	const QStringList symmetricTensorList = QStringList() << "XX" << "YY" << "ZZ" << "XY" << "XZ" << "YZ";
	const QStringList tensorList = QStringList() << "XX" << "YX" << "ZX" << "XY" << "YY" << "ZY" << "XZ" << "YZ" << "ZZ";
	const QStringList quaternionList = QStringList() << "X" << "Y" << "Z" << "W";

	registerStandardProperty(TypeProperty, tr("Particle Type"), PropertyObject::Int, emptyList, &ParticleType::OOClass(), tr("Particle types"));
	registerStandardProperty(SelectionProperty, tr("Selection"), PropertyObject::Int, emptyList);
	registerStandardProperty(ClusterProperty, tr("Cluster"), PropertyObject::Int64, emptyList);
	registerStandardProperty(CoordinationProperty, tr("Coordination"), PropertyObject::Int, emptyList);
	registerStandardProperty(PositionProperty, tr("Position"), PropertyObject::Float, xyzList, nullptr, tr("Particle positions"));
	registerStandardProperty(ColorProperty, tr("Color"), PropertyObject::Float, rgbList, nullptr, tr("Particle colors"));
	registerStandardProperty(DisplacementProperty, tr("Displacement"), PropertyObject::Float, xyzList, nullptr, tr("Displacements"));
	registerStandardProperty(DisplacementMagnitudeProperty, tr("Displacement Magnitude"), PropertyObject::Float, emptyList);
	registerStandardProperty(VelocityProperty, tr("Velocity"), PropertyObject::Float, xyzList, nullptr, tr("Velocities"));
	registerStandardProperty(PotentialEnergyProperty, tr("Potential Energy"), PropertyObject::Float, emptyList);
	registerStandardProperty(KineticEnergyProperty, tr("Kinetic Energy"), PropertyObject::Float, emptyList);
	registerStandardProperty(TotalEnergyProperty, tr("Total Energy"), PropertyObject::Float, emptyList);
	registerStandardProperty(RadiusProperty, tr("Radius"), PropertyObject::Float, emptyList, nullptr, tr("Radii"));
	registerStandardProperty(StructureTypeProperty, tr("Structure Type"), PropertyObject::Int, emptyList, &ElementType::OOClass(), tr("Structure types"));
	registerStandardProperty(IdentifierProperty, tr("Particle Identifier"), PropertyObject::Int64, emptyList, nullptr, tr("Particle identifiers"));
	registerStandardProperty(StressTensorProperty, tr("Stress Tensor"), PropertyObject::Float, symmetricTensorList);
	registerStandardProperty(StrainTensorProperty, tr("Strain Tensor"), PropertyObject::Float, symmetricTensorList);
	registerStandardProperty(DeformationGradientProperty, tr("Deformation Gradient"), PropertyObject::Float, tensorList);
	registerStandardProperty(OrientationProperty, tr("Orientation"), PropertyObject::Float, quaternionList);
	registerStandardProperty(ForceProperty, tr("Force"), PropertyObject::Float, xyzList);
	registerStandardProperty(MassProperty, tr("Mass"), PropertyObject::Float, emptyList);
	registerStandardProperty(ChargeProperty, tr("Charge"), PropertyObject::Float, emptyList);
	registerStandardProperty(PeriodicImageProperty, tr("Periodic Image"), PropertyObject::Int, xyzList);
	registerStandardProperty(TransparencyProperty, tr("Transparency"), PropertyObject::Float, emptyList);
	registerStandardProperty(DipoleOrientationProperty, tr("Dipole Orientation"), PropertyObject::Float, xyzList);
	registerStandardProperty(DipoleMagnitudeProperty, tr("Dipole Magnitude"), PropertyObject::Float, emptyList);
	registerStandardProperty(AngularVelocityProperty, tr("Angular Velocity"), PropertyObject::Float, xyzList);
	registerStandardProperty(AngularMomentumProperty, tr("Angular Momentum"), PropertyObject::Float, xyzList);
	registerStandardProperty(TorqueProperty, tr("Torque"), PropertyObject::Float, xyzList);
	registerStandardProperty(SpinProperty, tr("Spin"), PropertyObject::Float, emptyList);
	registerStandardProperty(CentroSymmetryProperty, tr("Centrosymmetry"), PropertyObject::Float, emptyList);
	registerStandardProperty(VelocityMagnitudeProperty, tr("Velocity Magnitude"), PropertyObject::Float, emptyList);
	registerStandardProperty(MoleculeProperty, tr("Molecule Identifier"), PropertyObject::Int64, emptyList);
	registerStandardProperty(AsphericalShapeProperty, tr("Aspherical Shape"), PropertyObject::Float, xyzList);
	registerStandardProperty(VectorColorProperty, tr("Vector Color"), PropertyObject::Float, rgbList, nullptr, tr("Vector colors"));
	registerStandardProperty(ElasticStrainTensorProperty, tr("Elastic Strain"), PropertyObject::Float, symmetricTensorList);
	registerStandardProperty(ElasticDeformationGradientProperty, tr("Elastic Deformation Gradient"), PropertyObject::Float, tensorList);
	registerStandardProperty(RotationProperty, tr("Rotation"), PropertyObject::Float, quaternionList);
	registerStandardProperty(StretchTensorProperty, tr("Stretch Tensor"), PropertyObject::Float, symmetricTensorList);
	registerStandardProperty(MoleculeTypeProperty, tr("Molecule Type"), PropertyObject::Float, emptyList, &ElementType::OOClass(), tr("Molecule types"));
	registerStandardProperty(NucleobaseTypeProperty, tr("Nucleobase"), PropertyObject::Int, emptyList, &ElementType::OOClass(), tr("Nucleobases"));
	registerStandardProperty(DNAStrandProperty, tr("DNA Strand"), PropertyObject::Int, emptyList, &ElementType::OOClass(), tr("DNA Strands"));
	registerStandardProperty(NucleotideAxisProperty, tr("Nucleotide Axis"), PropertyObject::Float, xyzList);
	registerStandardProperty(NucleotideNormalProperty, tr("Nucleotide Normal"), PropertyObject::Float, xyzList);
	registerStandardProperty(SuperquadricRoundnessProperty, tr("Superquadric Roundness"), PropertyObject::Float, QStringList() << "Phi" << "Theta");
}

/******************************************************************************
* Returns the default color for a numeric type ID.
******************************************************************************/
Color ParticlesObject::OOMetaClass::getElementTypeDefaultColor(const PropertyReference& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const
{
	if(property.type() == ParticlesObject::TypeProperty) {
		for(int predefType = 0; predefType < ParticleType::NUMBER_OF_PREDEFINED_PARTICLE_TYPES; predefType++) {
			if(ParticleType::getPredefinedParticleTypeName(static_cast<ParticleType::PredefinedParticleType>(predefType)) == typeName)
				return ParticleType::getPredefinedParticleTypeColor(static_cast<ParticleType::PredefinedParticleType>(predefType));
		}

		// Sometimes atom type names have additional letters/numbers appended.
		if(typeName.length() > 1 && typeName.length() <= 5) {
			return ElementType::getDefaultColor(property, typeName.left(typeName.length() - 1), numericTypeId, loadUserDefaults);
		}
	}
	else if(property.type() == ParticlesObject::StructureTypeProperty) {
		for(int predefType = 0; predefType < ParticleType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; predefType++) {
			if(ParticleType::getPredefinedStructureTypeName(static_cast<ParticleType::PredefinedStructureType>(predefType)) == typeName)
				return ParticleType::getPredefinedStructureTypeColor(static_cast<ParticleType::PredefinedStructureType>(predefType));
		}
		return Color(1,1,1);
	}

	return PropertyContainerClass::getElementTypeDefaultColor(property, typeName, numericTypeId, loadUserDefaults);
}

/******************************************************************************
* Returns the index of the element that was picked in a viewport.
******************************************************************************/
std::pair<size_t, ConstDataObjectPath> ParticlesObject::OOMetaClass::elementFromPickResult(const ViewportPickResult& pickResult) const
{
	// Check if a particle was picked.
	if(const ParticlePickInfo* pickInfo = dynamic_object_cast<ParticlePickInfo>(pickResult.pickInfo())) {
		const ParticlesObject* particles = pickInfo->particles();
		size_t particleIndex = pickInfo->particleIndexFromSubObjectID(pickResult.subobjectId());
		if(particleIndex < particles->elementCount())
			return std::make_pair(particleIndex, ConstDataObjectPath({particles}));
	}

	return std::pair<size_t, ConstDataObjectPath>(std::numeric_limits<size_t>::max(), {});
}

/******************************************************************************
* Tries to remap an index from one property container to another, considering the
* possibility that elements may have been added or removed.
******************************************************************************/
size_t ParticlesObject::OOMetaClass::remapElementIndex(const ConstDataObjectPath& source, size_t elementIndex, const ConstDataObjectPath& dest) const
{
	const ParticlesObject* sourceParticles = static_object_cast<ParticlesObject>(source.back());
	const ParticlesObject* destParticles = static_object_cast<ParticlesObject>(dest.back());

	// If unique IDs are available try to use them to look up the particle in the other data collection.
	if(ConstPropertyAccess<qlonglong> sourceIdentifiers = sourceParticles->getProperty(ParticlesObject::IdentifierProperty)) {
		if(ConstPropertyAccess<qlonglong> destIdentifiers = destParticles->getProperty(ParticlesObject::IdentifierProperty)) {
			qlonglong id = sourceIdentifiers[elementIndex];
			size_t mappedId = boost::find(destIdentifiers, id) - destIdentifiers.cbegin();
			if(mappedId != destIdentifiers.size())
				return mappedId;
		}
	}

	// Next, try to use the position to find the right particle in the other data collection.
	if(ConstPropertyAccess<Point3> sourcePositions = sourceParticles->getProperty(ParticlesObject::PositionProperty)) {
		if(ConstPropertyAccess<Point3> destPositions = destParticles->getProperty(ParticlesObject::PositionProperty)) {
			const Point3& pos = sourcePositions[elementIndex];
			size_t mappedId = boost::find(destPositions, pos) - destPositions.cbegin();
			if(mappedId != destPositions.size())
				return mappedId;
		}
	}

	// Give up.
	return PropertyContainerClass::remapElementIndex(source, elementIndex, dest);
}

/******************************************************************************
* Determines which elements are located within the given
* viewport fence region (=2D polygon).
******************************************************************************/
boost::dynamic_bitset<> ParticlesObject::OOMetaClass::viewportFenceSelection(const QVector<Point2>& fence, const ConstDataObjectPath& objectPath, PipelineSceneNode* node, const Matrix4& projectionTM) const
{
	const ParticlesObject* particles = static_object_cast<ParticlesObject>(objectPath.back());
	if(ConstPropertyAccess<Point3> posProperty = particles->getProperty(ParticlesObject::PositionProperty)) {

		if(!particles->visElement() || particles->visElement()->isEnabled() == false)
			node->throwException(tr("Cannot select particles while the corresponding visual element is disabled. Please enable the display of particles first."));

		boost::dynamic_bitset<> fullSelection(posProperty.size());
		QMutex mutex;
		parallelForChunks(posProperty.size(), [posProperty, &projectionTM, &fence, &mutex, &fullSelection](size_t startIndex, size_t chunkSize) {
			boost::dynamic_bitset<> selection(fullSelection.size());
			for(size_t index = startIndex; chunkSize != 0; chunkSize--, index++) {

				// Project particle center to screen coordinates.
				Point3 projPos = projectionTM * posProperty[index];

				// Perform z-clipping.
				if(std::abs(projPos.z()) >= FloatType(1))
					continue;

				// Perform point-in-polygon test.
				int intersectionsLeft = 0;
				int intersectionsRight = 0;
				for(auto p2 = fence.constBegin(), p1 = p2 + (fence.size()-1); p2 != fence.constEnd(); p1 = p2++) {
					if(p1->y() == p2->y()) continue;
					if(projPos.y() >= p1->y() && projPos.y() >= p2->y()) continue;
					if(projPos.y() < p1->y() && projPos.y() < p2->y()) continue;
					FloatType xint = (projPos.y() - p2->y()) / (p1->y() - p2->y()) * (p1->x() - p2->x()) + p2->x();
					if(xint >= projPos.x())
						intersectionsRight++;
					else
						intersectionsLeft++;
				}
				if(intersectionsRight & 1)
					selection.set(index);
			}
			// Transfer thread-local results to output bit array.
			QMutexLocker locker(&mutex);
			fullSelection |= selection;
		});

		return fullSelection;
	}

	// Give up.
	return PropertyContainerClass::viewportFenceSelection(fence, objectPath, node, projectionTM);
}

/******************************************************************************
 * This method is called by InputColumnMapping::validate() to let the container 
 * class perform custom checks on the mapping of the file data columns to 
 * internal properties.
 *****************************************************************************/
void ParticlesObject::OOMetaClass::validateInputColumnMapping(const InputColumnMapping& mapping) const
{
	// Make sure that at least the particle positions are read from the input file.
	if(std::none_of(mapping.begin(), mapping.end(), [](const InputColumnInfo& column) { return column.property.type() == ParticlesObject::PositionProperty; }))
		throw Exception(ParticlesObject::tr("Invalid file column mapping: At least one file column must be mapped to the '%1' particle property.").arg(standardPropertyName(ParticlesObject::PositionProperty)));
}

}	// End of namespace
