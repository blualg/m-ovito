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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include "BondsObject.h"
#include "AnglesObject.h"
#include "DihedralsObject.h"
#include "ImpropersObject.h"
#include "BondType.h"

namespace Ovito::Particles {

/**
 * \brief This data object type is a container for particle properties.
 */
class OVITO_PARTICLES_EXPORT ParticlesObject : public PropertyContainer
{
	/// Define a new property metaclass for particle containers.
	class OVITO_PARTICLES_EXPORT OOMetaClass : public PropertyContainerClass
	{
	public:
	
		/// Inherit constructor from base class.
		using PropertyContainerClass::PropertyContainerClass;

		/// \brief Create a storage object for standard particle properties.
		virtual PropertyPtr createStandardPropertyInternal(DataSet* dataset, size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const override;

		/// Indicates whether this kind of property container supports picking of individual elements in the viewports.
		virtual bool supportsViewportPicking() const override { return true; }

		/// Returns the index of the element that was picked in a viewport.
		virtual std::pair<size_t, ConstDataObjectPath> elementFromPickResult(const ViewportPickResult& pickResult) const override;

		/// Tries to remap an index from one property container to another, considering the possibility that
		/// elements may have been added or removed.
		virtual size_t remapElementIndex(const ConstDataObjectPath& source, size_t elementIndex, const ConstDataObjectPath& dest) const override;

		/// Determines which elements are located within the given viewport fence region (=2D polygon).
		virtual boost::dynamic_bitset<> viewportFenceSelection(const QVector<Point2>& fence, const ConstDataObjectPath& objectPath, PipelineSceneNode* node, const Matrix4& projectionTM) const override;

		/// Generates a human-readable string representation of the data object reference.
		virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override { return this->displayName(); }

		/// This method is called by InputColumnMapping::validate() to let the container class perform custom checks
		/// on the mapping of the file data columns to internal properties.
		virtual void validateInputColumnMapping(const InputColumnMapping& mapping) const override;

		/// Returns a default color for an ElementType given its numeric type ID.
		virtual Color getElementTypeDefaultColor(const PropertyReference& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const override;

	protected:

		/// Is called by the system after construction of the meta-class instance.
		virtual void initialize() override;
	};

	OVITO_CLASS_META(ParticlesObject, OOMetaClass);
	Q_CLASSINFO("DisplayName", "Particles");

public:

	/// \brief The list of standard particle properties.
	enum Type {
		UserProperty = PropertyObject::GenericUserProperty,	//< This is reserved for user-defined properties.
		SelectionProperty = PropertyObject::GenericSelectionProperty,
		ColorProperty = PropertyObject::GenericColorProperty,
		TypeProperty = PropertyObject::GenericTypeProperty,
		IdentifierProperty = PropertyObject::GenericIdentifierProperty,
		PositionProperty = PropertyObject::FirstSpecificProperty,
		DisplacementProperty,
		DisplacementMagnitudeProperty,
		PotentialEnergyProperty,
		KineticEnergyProperty,
		TotalEnergyProperty,
		VelocityProperty,
		RadiusProperty,
		ClusterProperty,
		CoordinationProperty,
		StructureTypeProperty,
		StressTensorProperty,
		StrainTensorProperty,
		DeformationGradientProperty,
		OrientationProperty,
		ForceProperty,
		MassProperty,
		ChargeProperty,
		PeriodicImageProperty,
		TransparencyProperty,
		DipoleOrientationProperty,
		DipoleMagnitudeProperty,
		AngularVelocityProperty,
		AngularMomentumProperty,
		TorqueProperty,
		SpinProperty,
		CentroSymmetryProperty,
		VelocityMagnitudeProperty,
		MoleculeProperty,
		AsphericalShapeProperty,
		VectorColorProperty,
		ElasticStrainTensorProperty,
		ElasticDeformationGradientProperty,
		RotationProperty,
		StretchTensorProperty,
		MoleculeTypeProperty,
		NucleobaseTypeProperty,
		DNAStrandProperty,
		NucleotideAxisProperty,
		NucleotideNormalProperty,
		SuperquadricRoundnessProperty
	};

	/// \brief Constructor.
	Q_INVOKABLE ParticlesObject(ObjectCreationParams params);
	
	/// Deletes the particles for which bits are set in the given bit-mask.
	/// Returns the number of deleted particles.
	virtual size_t deleteElements(const boost::dynamic_bitset<>& mask) override;

	/// Duplicates the BondsObject if it is shared with other particle objects.
	/// After this method returns, the BondsObject is exclusively owned by the ParticlesObject and
	/// can be safely modified without expected side effects.
	BondsObject* makeBondsMutable();

	/// Duplicates the AnglesObject if it is shared with other particle objects.
	/// After this method returns, the AnglesObject is exclusively owned by the ParticlesObject and
	/// can be safely modified without expected side effects.
	AnglesObject* makeAnglesMutable();

	/// Duplicates the DihedralsObject if it is shared with other particle objects.
	/// After this method returns, the DihedralsObject is exclusively owned by the ParticlesObject and
	/// can be safely modified without expected side effects.
	DihedralsObject* makeDihedralsMutable();

	/// Duplicates the ImpropersObject if it is shared with other particle objects.
	/// After this method returns, the ImpropersObject is exclusively owned by the ParticlesObject and
	/// can be safely modified without expected side effects.
	ImpropersObject* makeImpropersMutable();

	/// Sorts the particles list with respect to particle IDs.
	/// Does nothing if particles do not have IDs.
	virtual std::vector<size_t> sortById() override;

	/// Convinience method that makes sure that there is a BondsObject.
	const BondsObject* expectBonds() const;

	/// Convinience method that makes sure that there is a BondsObject and the bond topology property.
	const PropertyObject* expectBondsTopology() const;

	/// Adds a set of new bonds to the particle system.
	void addBonds(const std::vector<Bond>& newBonds, BondsVis* bondsVis, const std::vector<PropertyPtr>& bondProperties = {}, DataOORef<const BondType> bondType = {});

	/// Returns a property array with the input particle colors.
	ConstPropertyPtr inputParticleColors() const;

	/// Returns a property array with the input particle radii. The global radius scaling factor of the ParticlesVis is not included.
	ConstPropertyPtr inputParticleRadii() const;

	/// Returns a property array with the input particle masses.
	ConstPropertyPtr inputParticleMasses() const;

	/// Returns a vector with the input bond colors.
	ConstPropertyPtr inputBondColors(bool ignoreExistingColorProperty = false) const;

	/// Returns the base coordinates for visualizing a vector property from this container using a VectorVis element.
	virtual ConstDataBufferPtr getVectorVisBasePositions(const ConstDataObjectPath& path, const PipelineFlowState& state) const override { return getProperty(PositionProperty); }

private:

	/// The bonds list sub-object.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const BondsObject>, bonds, setBonds);

	/// The angles list sub-object.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const AnglesObject>, angles, setAngles);

	/// The dihedrals list sub-object.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const DihedralsObject>, dihedrals, setDihedrals);

	/// The impropers list sub-object.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const ImpropersObject>, impropers, setImpropers);
};

/**
 * Encapsulates a reference to a particle property.
 */
using ParticlePropertyReference = TypedPropertyReference<ParticlesObject>;

/**
 * Encapsulates a mapping of input file columns to particle properties.
 */
using ParticleInputColumnMapping = TypedInputColumnMapping<ParticlesObject>;

}	// End of namespace

Q_DECLARE_METATYPE(Ovito::Particles::ParticlePropertyReference);
Q_DECLARE_METATYPE(Ovito::Particles::ParticleInputColumnMapping);
