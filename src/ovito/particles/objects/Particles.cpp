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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/BondType.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "Particles.h"
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

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Particles);
DEFINE_REFERENCE_FIELD(Particles, bonds);
DEFINE_REFERENCE_FIELD(Particles, angles);
DEFINE_REFERENCE_FIELD(Particles, dihedrals);
DEFINE_REFERENCE_FIELD(Particles, impropers);
SET_PROPERTY_FIELD_LABEL(Particles, bonds, "Bonds");
SET_PROPERTY_FIELD_LABEL(Particles, angles, "Angles");
SET_PROPERTY_FIELD_LABEL(Particles, dihedrals, "Dihedrals");
SET_PROPERTY_FIELD_LABEL(Particles, impropers, "Impropers");

/******************************************************************************
* Constructor.
******************************************************************************/
Particles::Particles(ObjectInitializationFlags flags) : PropertyContainer(flags)
{
    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());

    if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement))) {
        // Create and attach a default visualization element for rendering the particles.
        setVisElement(OORef<ParticlesVis>::create(flags));
    }
}

/******************************************************************************
* Duplicates the Bonds if it is shared with other particle objects.
* After this method returns, the Bonds is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
Bonds* Particles::makeBondsMutable()
{
    OVITO_ASSERT(bonds());
    return makeMutable(bonds());
}

/******************************************************************************
* Duplicates the Angles if it is shared with other particle objects.
* After this method returns, the Angles is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
Angles* Particles::makeAnglesMutable()
{
    OVITO_ASSERT(angles());
    return makeMutable(angles());
}

/******************************************************************************
* Duplicates the Dihedrals if it is shared with other particle objects.
* After this method returns, the Dihedrals is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
Dihedrals* Particles::makeDihedralsMutable()
{
    OVITO_ASSERT(dihedrals());
    return makeMutable(dihedrals());
}

/******************************************************************************
* Duplicates the Impropers if it is shared with other particle objects.
* After this method returns, the Impropers is exclusively owned by the
* container and can be safely modified without unwanted side effects.
******************************************************************************/
Impropers* Particles::makeImpropersMutable()
{
    OVITO_ASSERT(impropers());
    return makeMutable(impropers());
}

/******************************************************************************
* Convinience method that makes sure that there is a Bonds.
* Throws an exception if there isn't.
******************************************************************************/
const Bonds* Particles::expectBonds() const
{
    if(!bonds())
        throw Exception(tr("This operation requires bonds as input, but the particle system has no bond topology defined."));
    return bonds();
}

/******************************************************************************
* Convinience method that makes sure that there is a Bonds and the
* bond topology property. Throws an exception if there isn't.
******************************************************************************/
const Property* Particles::expectBondsTopology() const
{
    return expectBonds()->expectProperty(Bonds::TopologyProperty);
}

/******************************************************************************
* Deletes the particles for which bits are set in the given bit-mask.
* Returns the number of deleted particles.
******************************************************************************/
size_t Particles::deleteElements(ConstDataBufferPtr selection, size_t selectionCount)
{
    OVITO_ASSERT(selection && selection->size() == elementCount());
    size_t oldParticleCount = elementCount();

    // Delete the particles from this container.
    size_t deleteParticleCount = PropertyContainer::deleteElements(selection, selectionCount);
    if(deleteParticleCount == 0)
        return 0;   // Nothing to do.
    size_t newParticleCount = elementCount();

    // Remapping of old particle indices to new ones.
    std::vector<size_t> particleIndexMap;
    auto computeParticleIndexRemapping = [&]() {
        if(particleIndexMap.empty()) {
            particleIndexMap.resize(oldParticleCount);
            auto iter = particleIndexMap.begin();
            size_t newIndex = 0;
            for(auto isDeleted : BufferReadAccess<SelectionIntType>(selection))
                *iter++ = isDeleted ? std::numeric_limits<size_t>::max() : newIndex++;
        }
    };

    // Generic method for deleting dangling bonds/angles/dihedrals/impropers.
    auto filterTopologyObject = [&](auto* oldContainer, int topologyPropertyType, auto particleIndexTuple) {
        using ParticleIndexTuple = decltype(particleIndexTuple);

        if(!oldContainer)
            return; // Nothing to do.

        // Make sure we can safely modify the child object.
        auto* newContainer = makeMutable(oldContainer);
        BufferReadAccessAndRef<ParticleIndexTuple> oldTopology = newContainer->takeProperty(topologyPropertyType);
        if(!oldTopology)
            return; // Nothing to do if there is no topology property for some reason.

        size_t oldCount = oldTopology.size();
        BufferFactory<SelectionIntType> deletionMask(oldCount);

        // Build map from old particle indices to new indices.
        computeParticleIndexRemapping();

        // Remap particle indices and remove dangling bonds etc.
        PropertyFactory<ParticleIndexTuple> newTopology(newContainer->getOOMetaClass(), oldCount, topologyPropertyType);
        size_t numRemaining = 0;
        for(size_t index = 0; index < oldCount; index++) {
            ParticleIndexTuple particleIndices = oldTopology[index];

            bool deleteElement = false;
            for(auto& particleIndex : particleIndices) {
                // Mark invalid entries for deletion, i.e., if particle indices are out of range for some reason.
                if(particleIndex < 0 || (size_t)particleIndex >= oldParticleCount) {
                    deleteElement = true;
                    break;
                }
                // Mark dangling entries for deletion and otherwise remap the particle indices.
                particleIndex = particleIndexMap[particleIndex];
                if(particleIndex == std::numeric_limits<size_t>::max()) {
                    deleteElement = true;
                    break;
                }
            }
            deletionMask[index] = deleteElement;
            if(!deleteElement) {
                newTopology[numRemaining] = particleIndices;
                numRemaining++;
            }
        }

        // Delete the marked elements.
        newContainer->deleteElements(deletionMask.take(), oldCount - numRemaining);

        // Insert the rewritten topology back into the container after truncating the array to the new length.
        PropertyPtr newTopologyProperty = newTopology.take();
        newTopologyProperty->resize(numRemaining, true);
        newContainer->addProperty(std::move(newTopologyProperty));
    };

    // Delete dangling bonds, i.e. those that are incident on deleted particles.
    filterTopologyObject(bonds(), Bonds::TopologyProperty, ParticleIndexPair{});

    // Delete dangling angles, i.e. those that are incident on deleted particles.
    filterTopologyObject(angles(), Angles::TopologyProperty, ParticleIndexTriplet{});

    // Delete dangling dihedrals, i.e. those that are incident on deleted particles.
    filterTopologyObject(dihedrals(), Dihedrals::TopologyProperty, ParticleIndexQuadruplet{});

    // Delete dangling impropers, i.e. those that are incident on deleted particles.
    filterTopologyObject(impropers(), Impropers::TopologyProperty, ParticleIndexQuadruplet{});

    return deleteParticleCount;
}

/******************************************************************************
* Sorts the particles list with respect to particle IDs.
* Does nothing if particles do not have IDs.
******************************************************************************/
std::vector<size_t> Particles::sortById()
{
    std::vector<size_t> invertedPermutation = PropertyContainer::sortById();

    // If the storage order of particles has changed, we need to update other topological
    // structures that refer to the particle indices.
    if(!invertedPermutation.empty()) {

        // Update bond topology data to match new particle ordering.
        if(bonds()) {
            if(BufferWriteAccess<ParticleIndexPair, access_mode::read_write> bondTopology = makeBondsMutable()->getMutableProperty(Bonds::TopologyProperty)) {
                for(ParticleIndexPair& bond : bondTopology) {
                    for(int64_t& idx : bond) {
                        if(idx >= 0 && idx < (int64_t)invertedPermutation.size())
                            idx = invertedPermutation[idx];
                    }
                }
            }
        }

        // Update angle topology data to match new particle ordering.
        if(angles()) {
            if(BufferWriteAccess<ParticleIndexTriplet, access_mode::read_write> angleTopology = makeAnglesMutable()->getMutableProperty(Angles::TopologyProperty)) {
                for(ParticleIndexTriplet& angle : angleTopology) {
                    for(int64_t& idx : angle) {
                        if(idx >= 0 && idx < (int64_t)invertedPermutation.size())
                            idx = invertedPermutation[idx];
                    }
                }
            }
        }

        // Update dihedral topology data to match new particle ordering.
        if(dihedrals()) {
            if(BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> dihedralTopology = makeDihedralsMutable()->getMutableProperty(Dihedrals::TopologyProperty)) {
                for(ParticleIndexQuadruplet& dihedral : dihedralTopology) {
                    for(int64_t& idx : dihedral) {
                        if(idx >= 0 && idx < (int64_t)invertedPermutation.size())
                            idx = invertedPermutation[idx];
                    }
                }
            }
        }

        // Update improper topology data to match new particle ordering.
        if(impropers()) {
            if(BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> improperTopology = makeImpropersMutable()->getMutableProperty(Impropers::TopologyProperty)) {
                for(ParticleIndexQuadruplet& improper : improperTopology) {
                    for(int64_t& idx : improper) {
                        if(idx >= 0 && idx < (int64_t)invertedPermutation.size())
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
void Particles::addBonds(const std::vector<Bond>& newBonds, BondsVis* bondsVis, const std::vector<PropertyPtr>& bondProperties, DataOORef<const BondType> bondType)
{
    OVITO_ASSERT(isSafeToModify());

    // Check if there is an existing bonds object.
    Bonds* mutableBonds;
    if(!bonds()) {
        // Create the bonds object.
        DataOORef<Bonds> bonds = DataOORef<Bonds>::create();
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
ConstPropertyPtr Particles::inputParticleColors() const
{
    // Access the particles vis element.
    if(ParticlesVis* particleVis = visElement<ParticlesVis>()) {
        // Query particle colors from vis element.
        return particleVis->particleColors(this, false);
    }

    // Return an array with uniform colors if there is no vis element attached to the particles object.
    PropertyPtr colors = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, elementCount(), Particles::ColorProperty);
    colors->fill<ColorG>(ColorG(1,1,1));
    return colors;
}

/******************************************************************************
* Returns a property array with the input bond colors.
******************************************************************************/
ConstPropertyPtr Particles::inputBondColors(bool ignoreExistingColorProperty) const
{
    // Access the bonds vis element.
    if(bonds()) {
        if(BondsVis* bondsVis = bonds()->visElement<BondsVis>()) {

            // Request half-bond colors from vis element.
            std::vector<ColorG> halfBondColors = bondsVis->halfBondColors(this, false, bondsVis->coloringMode(), ignoreExistingColorProperty);
            OVITO_ASSERT(bonds()->elementCount() * 2 == halfBondColors.size());

            // Map half-bond colors to full bond colors.
            PropertyPtr colors = Bonds::OOClass().createStandardProperty(DataBuffer::Uninitialized, bonds()->elementCount(), Bonds::ColorProperty);
            auto ci = halfBondColors.cbegin();
            for(ColorG& co : BufferWriteAccess<ColorG, access_mode::discard_write>(colors)) {
                co = *ci;
                ci += 2;
            }
            return colors;
        }

        // If no vis element is available, create an array filled with the default bond color.
        PropertyPtr colors = Bonds::OOClass().createStandardProperty(DataBuffer::Uninitialized, bonds()->elementCount(), Bonds::ColorProperty);
        colors->fill<ColorG>(ColorG(1,1,1));
        return colors;
    }
    return {};
}

/******************************************************************************
* Returns a property array with the input particle radii.
******************************************************************************/
ConstPropertyPtr Particles::inputParticleRadii() const
{
    // Access the particles vis element.
    if(ParticlesVis* particleVis = visElement<ParticlesVis>()) {
        // Query particle radii from vis element.
        return particleVis->particleRadii(this, false);
    }

    // Return uniform default radius for all particles.
    PropertyPtr buffer = OOClass().createStandardProperty(DataBuffer::Uninitialized, elementCount(), Particles::RadiusProperty);
    buffer->fill<GraphicsFloatType>(1);
    return buffer;
}

/******************************************************************************
* Returns a property array with the input particle masses.
******************************************************************************/
ConstPropertyPtr Particles::inputParticleMasses() const
{
    // Take masses directly from the 'Mass' property if available.
    if(const Property* massProperty = getProperty(Particles::MassProperty))
        return massProperty;

    if(const Property* typeProperty = getProperty(Particles::TypeProperty)) {
        // Assign masses based on particle types.
        // Build a lookup map for particle type masses.
        std::map<int,FloatType> massMap = ParticleType::typeMassMap(typeProperty);

        // Skip the following loop if all per-type masses are zero. In this case, simply use the default mass for all particles.
        if(boost::algorithm::any_of(massMap, [](const std::pair<int,FloatType>& it) { return it.second != 0; })) {

            // Allocate output array.
            PropertyPtr massProperty = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, elementCount(), Particles::MassProperty);

            // Fill output array using lookup table.
            BufferReadAccess<int32_t> typeData(typeProperty);
            boost::transform(typeData, BufferWriteAccess<FloatType, access_mode::discard_write>(massProperty).begin(), [&](int t) {
                auto it = massMap.find(t);
                if(it != massMap.end())
                    return it->second;
                else
                    return 0.0;
            });

            return massProperty;
        }
    }

    // Return uniform default mass 0 for all particles.
    return OOClass().createStandardProperty(DataBuffer::Initialized, elementCount(), Particles::MassProperty);
}

/******************************************************************************
* Creates a storage object for standard particle properties.
******************************************************************************/
PropertyPtr Particles::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    // Certain standard properties need to be initialized with default values determined by the visual element attached to the property container.
    if(init == DataBuffer::Initialized && !containerPath.empty()) {
        if(type == ColorProperty) {
            if(const Particles* particles = dynamic_object_cast<Particles>(containerPath.back())) {
                OVITO_ASSERT(particles->elementCount() == elementCount);
                ConstPropertyPtr property = particles->inputParticleColors();
                OVITO_ASSERT(property);
                OVITO_ASSERT(property->size() == elementCount);
                OVITO_ASSERT(property->type() == ColorProperty);
                return std::move(property).makeMutable();
            }
        }
        else if(type == RadiusProperty) {
            if(const Particles* particles = dynamic_object_cast<Particles>(containerPath.back())) {
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
    case SelectionProperty:
        dataType = DataBuffer::IntSelection;
        componentCount = 1;
        break;
    case TypeProperty:
    case StructureTypeProperty:
    case CoordinationProperty:
    case MoleculeTypeProperty:
    case NucleobaseTypeProperty:
    case DNAStrandProperty:
        dataType = Property::Int32;
        componentCount = 1;
        break;
    case IdentifierProperty:
    case ClusterProperty:
    case MoleculeProperty:
        dataType = Property::Int64;
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
    case NucleotideAxisProperty:
    case NucleotideNormalProperty:
        dataType = Property::FloatDefault;
        componentCount = 3;
        break;
    case ColorProperty:
    case VectorColorProperty:
    case AsphericalShapeProperty:
        dataType = Property::FloatGraphics;
        componentCount = 3;
        break;
    case RadiusProperty:
    case TransparencyProperty:
        dataType = Property::FloatGraphics;
        componentCount = 1;
        break;
    case PotentialEnergyProperty:
    case KineticEnergyProperty:
    case TotalEnergyProperty:
    case MassProperty:
    case ChargeProperty:
    case SpinProperty:
    case DipoleMagnitudeProperty:
    case CentroSymmetryProperty:
    case DisplacementMagnitudeProperty:
    case VelocityMagnitudeProperty:
        dataType = Property::FloatDefault;
        componentCount = 1;
        break;
    case StressTensorProperty:
    case StrainTensorProperty:
    case ElasticStrainTensorProperty:
    case StretchTensorProperty:
        dataType = Property::FloatDefault;
        componentCount = 6;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(SymmetricTensor2));
        break;
    case DeformationGradientProperty:
    case ElasticDeformationGradientProperty:
        dataType = Property::FloatDefault;
        componentCount = 9;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Matrix3));
        break;
    case RotationProperty:
        dataType = Property::FloatDefault;
        componentCount = 4;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Quaternion));
        break;
    case OrientationProperty:
        dataType = Property::FloatGraphics;
        componentCount = 4;
        OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(QuaternionG));
        break;
    case PeriodicImageProperty:
        dataType = Property::Int32;
        componentCount = 3;
        break;
    case SuperquadricRoundnessProperty:
        dataType = Property::FloatGraphics;
        componentCount = 2;
        OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(Vector_2<GraphicsFloatType>));
        break;
    default:
        OVITO_ASSERT_MSG(false, "Particles::createStandardProperty()", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard property type: %1").arg(type));
    }

    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    // Allocate the storage array.
    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && !containerPath.empty()) {
        // Certain standard properties need to be initialized with default values determined by the attached visual elements.
        if(type == MassProperty) {
            if(const Particles* particles = dynamic_object_cast<Particles>(containerPath.back())) {
                if(const Property* typeProperty = particles->getProperty(Particles::TypeProperty)) {
                    // Use per-type mass information and initialize the per-particle mass array from it.
                    std::map<int,FloatType> massMap = ParticleType::typeMassMap(typeProperty);
                    if(!massMap.empty()) {
                        boost::transform(BufferReadAccess<int32_t>(typeProperty), BufferWriteAccess<FloatType, access_mode::discard_write>(property).begin(), [&](int t) {
                            auto iter = massMap.find(t);
                            return iter != massMap.end() ? iter->second : FloatType(0);
                        });
                        init = DataBuffer::Uninitialized;
                    }
                }
            }
        }
        else if(type == VectorColorProperty) {
            if(const Particles* particles = dynamic_object_cast<Particles>(containerPath.back())) {
                for(const Property* p : particles->properties()) {
                    if(VectorVis* vectorVis = dynamic_object_cast<VectorVis>(p->visElement())) {
                        property->fill<ColorG>(vectorVis->arrowColor().toDataType<GraphicsFloatType>());
                        init = DataBuffer::Uninitialized;
                        break;
                    }
                }
            }
        }
    }

    // Some properties get an attached visual element.
    if(type == Particles::DisplacementProperty) {
        OORef<VectorVis> vis = OORef<VectorVis>::create();
        vis->setObjectTitle(tr("Displacements"));
        vis->setEnabled(false);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled)});
        property->addVisElement(std::move(vis));
    }
    else if(type == Particles::ForceProperty) {
        OORef<VectorVis> vis = OORef<VectorVis>::create();
        vis->setObjectTitle(tr("Forces"));
        vis->setEnabled(false);
        vis->setReverseArrowDirection(false);
        vis->setArrowPosition(VectorVis::Base);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
        property->addVisElement(std::move(vis));
    }
    else if(type == Particles::VelocityProperty) {
        OORef<VectorVis> vis = OORef<VectorVis>::create();
        vis->setObjectTitle(tr("Velocities"));
        vis->setEnabled(false);
        vis->setReverseArrowDirection(false);
        vis->setArrowPosition(VectorVis::Base);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
        property->addVisElement(std::move(vis));
    }
    else if(type == Particles::DipoleOrientationProperty) {
        OORef<VectorVis> vis = OORef<VectorVis>::create();
        vis->setObjectTitle(tr("Dipoles"));
        vis->setEnabled(false);
        vis->setReverseArrowDirection(false);
        vis->setArrowPosition(VectorVis::Center);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
        property->addVisElement(std::move(vis));
    }

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void Particles::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    // Enable automatic conversion of a ParticlePropertyReference to a generic PropertyReference and vice versa.
    QMetaType::registerConverter<ParticlePropertyReference, PropertyReference>();
    QMetaType::registerConverter<PropertyReference, ParticlePropertyReference>();

    setPropertyClassDisplayName(tr("Particles"));
    setElementDescriptionName(QStringLiteral("particles"));
    setPythonName(QStringLiteral("particles"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";
    const QStringList symmetricTensorList = QStringList() << "XX" << "YY" << "ZZ" << "XY" << "XZ" << "YZ";
    const QStringList tensorList = QStringList() << "XX" << "YX" << "ZX" << "XY" << "YY" << "ZY" << "XZ" << "YZ" << "ZZ";
    const QStringList quaternionList = QStringList() << "X" << "Y" << "Z" << "W";

    registerStandardProperty(TypeProperty, tr("Particle Type"), Property::Int32, emptyList, &ParticleType::OOClass(), tr("Particle types"));
    registerStandardProperty(SelectionProperty, tr("Selection"), Property::IntSelection, emptyList);
    registerStandardProperty(ClusterProperty, tr("Cluster"), Property::Int64, emptyList);
    registerStandardProperty(CoordinationProperty, tr("Coordination"), Property::Int32, emptyList);
    registerStandardProperty(PositionProperty, tr("Position"), Property::FloatDefault, xyzList, nullptr, tr("Particle positions"));
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList, nullptr, tr("Particle colors"));
    registerStandardProperty(DisplacementProperty, tr("Displacement"), Property::FloatDefault, xyzList, nullptr, tr("Displacements"));
    registerStandardProperty(DisplacementMagnitudeProperty, tr("Displacement Magnitude"), Property::FloatDefault, emptyList);
    registerStandardProperty(VelocityProperty, tr("Velocity"), Property::FloatDefault, xyzList, nullptr, tr("Velocities"));
    registerStandardProperty(PotentialEnergyProperty, tr("Potential Energy"), Property::FloatDefault, emptyList);
    registerStandardProperty(KineticEnergyProperty, tr("Kinetic Energy"), Property::FloatDefault, emptyList);
    registerStandardProperty(TotalEnergyProperty, tr("Total Energy"), Property::FloatDefault, emptyList);
    registerStandardProperty(RadiusProperty, tr("Radius"), Property::FloatGraphics, emptyList, nullptr, tr("Radii"));
    registerStandardProperty(StructureTypeProperty, tr("Structure Type"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Structure types"));
    registerStandardProperty(IdentifierProperty, tr("Particle Identifier"), Property::IntIdentifier, emptyList, nullptr, tr("Particle identifiers"));
    registerStandardProperty(StressTensorProperty, tr("Stress Tensor"), Property::FloatDefault, symmetricTensorList);
    registerStandardProperty(StrainTensorProperty, tr("Strain Tensor"), Property::FloatDefault, symmetricTensorList);
    registerStandardProperty(DeformationGradientProperty, tr("Deformation Gradient"), Property::FloatDefault, tensorList);
    registerStandardProperty(OrientationProperty, tr("Orientation"), Property::FloatGraphics, quaternionList);
    registerStandardProperty(ForceProperty, tr("Force"), Property::FloatDefault, xyzList);
    registerStandardProperty(MassProperty, tr("Mass"), Property::FloatDefault, emptyList);
    registerStandardProperty(ChargeProperty, tr("Charge"), Property::FloatDefault, emptyList);
    registerStandardProperty(PeriodicImageProperty, tr("Periodic Image"), Property::Int32, xyzList);
    registerStandardProperty(TransparencyProperty, tr("Transparency"), Property::FloatGraphics, emptyList);
    registerStandardProperty(DipoleOrientationProperty, tr("Dipole Orientation"), Property::FloatDefault, xyzList);
    registerStandardProperty(DipoleMagnitudeProperty, tr("Dipole Magnitude"), Property::FloatDefault, emptyList);
    registerStandardProperty(AngularVelocityProperty, tr("Angular Velocity"), Property::FloatDefault, xyzList);
    registerStandardProperty(AngularMomentumProperty, tr("Angular Momentum"), Property::FloatDefault, xyzList);
    registerStandardProperty(TorqueProperty, tr("Torque"), Property::FloatDefault, xyzList);
    registerStandardProperty(SpinProperty, tr("Spin"), Property::FloatDefault, emptyList);
    registerStandardProperty(CentroSymmetryProperty, tr("Centrosymmetry"), Property::FloatDefault, emptyList);
    registerStandardProperty(VelocityMagnitudeProperty, tr("Velocity Magnitude"), Property::FloatDefault, emptyList);
    registerStandardProperty(MoleculeProperty, tr("Molecule Identifier"), Property::IntIdentifier, emptyList);
    registerStandardProperty(AsphericalShapeProperty, tr("Aspherical Shape"), Property::FloatGraphics, xyzList);
    registerStandardProperty(VectorColorProperty, tr("Vector Color"), Property::FloatGraphics, rgbList, nullptr, tr("Vector colors"));
    registerStandardProperty(ElasticStrainTensorProperty, tr("Elastic Strain"), Property::FloatDefault, symmetricTensorList);
    registerStandardProperty(ElasticDeformationGradientProperty, tr("Elastic Deformation Gradient"), Property::FloatDefault, tensorList);
    registerStandardProperty(RotationProperty, tr("Rotation"), Property::FloatDefault, quaternionList);
    registerStandardProperty(StretchTensorProperty, tr("Stretch Tensor"), Property::FloatDefault, symmetricTensorList);
    registerStandardProperty(MoleculeTypeProperty, tr("Molecule Type"), Property::FloatDefault, emptyList, &ElementType::OOClass(), tr("Molecule types"));
    registerStandardProperty(NucleobaseTypeProperty, tr("Nucleobase"), Property::Int32, emptyList, &ElementType::OOClass(), tr("Nucleobases"));
    registerStandardProperty(DNAStrandProperty, tr("DNA Strand"), Property::Int32, emptyList, &ElementType::OOClass(), tr("DNA Strands"));
    registerStandardProperty(NucleotideAxisProperty, tr("Nucleotide Axis"), Property::FloatDefault, xyzList);
    registerStandardProperty(NucleotideNormalProperty, tr("Nucleotide Normal"), Property::FloatDefault, xyzList);
    registerStandardProperty(SuperquadricRoundnessProperty, tr("Superquadric Roundness"), Property::FloatGraphics, QStringList() << "Phi" << "Theta");
}

/******************************************************************************
* Returns the default color for a numeric type ID.
******************************************************************************/
Color Particles::OOMetaClass::getElementTypeDefaultColor(const PropertyReference& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const
{
    if(property.type() == Particles::TypeProperty) {
        for(int predefType = 0; predefType < ParticleType::NUMBER_OF_PREDEFINED_PARTICLE_TYPES; predefType++) {
            if(ParticleType::getPredefinedParticleTypeName(static_cast<ParticleType::PredefinedParticleType>(predefType)) == typeName)
                return ParticleType::getPredefinedParticleTypeColor(static_cast<ParticleType::PredefinedParticleType>(predefType));
        }

        // Sometimes atom type names have additional letters/numbers appended.
        if(typeName.length() > 1 && typeName.length() <= 5) {
            return ElementType::getDefaultColor(property, typeName.left(typeName.length() - 1), numericTypeId, loadUserDefaults);
        }
    }
    else if(property.type() == Particles::StructureTypeProperty) {
        for(int predefType = 0; predefType < ParticleType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; predefType++) {
            if(ParticleType::getPredefinedStructureTypeName(static_cast<ParticleType::PredefinedStructureType>(predefType)) == typeName)
                return ParticleType::getPredefinedStructureTypeColor(static_cast<ParticleType::PredefinedStructureType>(predefType));
        }
        return Color(1,1,1);
    }
    else if(property.type() == Particles::NucleobaseTypeProperty) {
        // Color scheme adopted from oxdna-viewer:
        if(typeName == "A") return Color(0.3, 0.3, 1.0);
        else if(typeName == "C") return Color(0.3, 1.0, 0.3);
        else if(typeName == "G") return Color(1.0, 1.0, 0.3);
        else if(typeName == "T") return Color(1.0, 0.3, 0.3);
    }

    return PropertyContainerClass::getElementTypeDefaultColor(property, typeName, numericTypeId, loadUserDefaults);
}

/******************************************************************************
* Returns the index of the element that was picked in a viewport.
******************************************************************************/
std::pair<size_t, ConstDataObjectPath> Particles::OOMetaClass::elementFromPickResult(const ViewportPickResult& pickResult) const
{
    // Check if a particle was picked.
    if(const ParticlePickInfo* pickInfo = dynamic_object_cast<ParticlePickInfo>(pickResult.pickInfo())) {
        const Particles* particles = pickInfo->particles();
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
size_t Particles::OOMetaClass::remapElementIndex(const ConstDataObjectPath& source, size_t elementIndex, const ConstDataObjectPath& dest) const
{
    const Particles* sourceParticles = static_object_cast<Particles>(source.back());
    const Particles* destParticles = static_object_cast<Particles>(dest.back());

    // If unique IDs are available try to use them to look up the particle in the other data collection.
    if(BufferReadAccess<int64_t> sourceIdentifiers = sourceParticles->getProperty(Particles::IdentifierProperty)) {
        if(BufferReadAccess<int64_t> destIdentifiers = destParticles->getProperty(Particles::IdentifierProperty)) {
            int64_t id = sourceIdentifiers[elementIndex];
            size_t mappedId = boost::find(destIdentifiers, id) - destIdentifiers.cbegin();
            if(mappedId != destIdentifiers.size())
                return mappedId;
        }
    }

    // Next, try to use the position to find the right particle in the other data collection.
    if(BufferReadAccess<Point3> sourcePositions = sourceParticles->getProperty(Particles::PositionProperty)) {
        if(BufferReadAccess<Point3> destPositions = destParticles->getProperty(Particles::PositionProperty)) {
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
boost::dynamic_bitset<> Particles::OOMetaClass::viewportFenceSelection(const QVector<Point2>& fence, const ConstDataObjectPath& objectPath, Pipeline* pipeline, const Matrix4& projectionTM) const
{
    const Particles* particles = static_object_cast<Particles>(objectPath.back());
    if(BufferReadAccess<Point3> posProperty = particles->getProperty(Particles::PositionProperty)) {

        if(!particles->visElement() || particles->visElement()->isEnabled() == false)
            throw Exception(tr("Cannot select particles while the corresponding visual element is disabled. Please enable the display of particles first."));

        boost::dynamic_bitset<> fullSelection(posProperty.size());
        QMutex mutex;
        parallelForChunks(posProperty.size(), [&](size_t startIndex, size_t chunkSize) {
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
    return PropertyContainerClass::viewportFenceSelection(fence, objectPath, pipeline, projectionTM);
}

}   // End of namespace
