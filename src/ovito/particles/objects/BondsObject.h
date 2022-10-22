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
#include <ovito/particles/objects/BondType.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>

namespace Ovito::Particles {

/**
 * The data type used for the 'Topology' bond property: two indices into the particles list.
 */
using ParticleIndexPair = std::array<qlonglong, 2>;

/**
 * A helper data structure describing a single bond between two particles.
 */
struct Bond
{
	/// The index of the first particle.
	size_t index1;

	/// The index of the second particle.
	size_t index2;

	/// If the bond crosses a periodic boundary, this indicates the direction.
	Vector3I pbcShift;

	/// Returns the flipped version of this bond, where the two particles are swapped
	/// and the PBC shift vector is reversed.
	Bond flipped() const { return Bond{ index2, index1, -pbcShift }; }

	/// For a pair of bonds, A<->B and B<->A, determines whether this bond
	/// counts as the 'odd' or the 'even' bond of the pair.
	bool isOdd() const {
		// Is this bond connecting two different particles?
		// If yes, it's easy to determine whether it's an even or an odd bond.
		if(index1 > index2) return true;
		else if(index1 < index2) return false;
		// Whether the bond is 'odd' is determined by the PBC shift vector.
		if(pbcShift[0] != 0) return pbcShift[0] < 0;
		if(pbcShift[1] != 0) return pbcShift[1] < 0;
		// A particle shouldn't be bonded to itself unless the bond crosses a periodic cell boundary:
		OVITO_ASSERT(pbcShift != Vector3I::Zero());
		return pbcShift[2] < 0;
	}
};

/**
 * \brief This data object type is a container for bond properties.
 */
class OVITO_PARTICLES_EXPORT BondsObject : public PropertyContainer
{
	/// Define a new property metaclass for bond property containers.
	class OVITO_PARTICLES_EXPORT OOMetaClass : public PropertyContainerClass
	{
	public:

		/// Inherit constructor from base class.
		using PropertyContainerClass::PropertyContainerClass;

		/// \brief Create a storage object for standard bond properties.
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

		/// Returns a default color for an ElementType given its numeric type ID.
		virtual Color getElementTypeDefaultColor(const PropertyReference& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const override;

	protected:

		/// Is called by the system after construction of the meta-class instance.
		virtual void initialize() override;
	};

	OVITO_CLASS_META(BondsObject, OOMetaClass);
	Q_CLASSINFO("DisplayName", "Bonds");

public:

	/// \brief The list of standard bond properties.
	enum Type {
		UserProperty = PropertyObject::GenericUserProperty,	//< This is reserved for user-defined properties.
		SelectionProperty = PropertyObject::GenericSelectionProperty,
		ColorProperty = PropertyObject::GenericColorProperty,
		TypeProperty = PropertyObject::GenericTypeProperty,
		LengthProperty = PropertyObject::FirstSpecificProperty,
		TopologyProperty,
		PeriodicImageProperty,
		TransparencyProperty,
		ParticleIdentifiersProperty,
		WidthProperty,
	};

	/// \brief Constructor.
	Q_INVOKABLE BondsObject(ObjectCreationParams params);
	
	/// Convinience method that returns the bond topology property.
	const PropertyObject* getTopology() const { return getProperty(TopologyProperty); }

	/// Determines the PBC shift vectors for bonds using the minimum image convention.
	void generatePeriodicImageProperty(const ParticlesObject* particles, const SimulationCellObject* simulationCellObject);

	/// Creates new bonds making sure bonds are not created twice.
	size_t addBonds(const std::vector<Bond>& newBonds, BondsVis* bondsVis, const ParticlesObject* particles, const std::vector<PropertyPtr>& bondProperties = {}, DataOORef<const BondType> bondType = {});

	/// Returns a property array with the input bond widths.
	ConstPropertyPtr inputBondWidths() const;

	/// Returns the base point and vector information for visualizing a vector property from this container using a VectorVis element.
	virtual std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state) const override;
};

/**
 * Encapsulates a reference to a bond property.
 */
using BondPropertyReference = TypedPropertyReference<BondsObject>;

/**
 * Encapsulates a mapping of input file columns to bond properties.
 */
using BondInputColumnMapping = TypedInputColumnMapping<BondsObject>;

}	// End of namespace

Q_DECLARE_METATYPE(Ovito::Particles::BondPropertyReference);
Q_DECLARE_METATYPE(Ovito::Particles::BondInputColumnMapping);
