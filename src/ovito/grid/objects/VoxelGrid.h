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


#include <ovito/grid/Grid.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>

namespace Ovito::Grid {

/**
 * \brief This object stores a data grid made of voxels.
 */
class OVITO_GRID_EXPORT VoxelGrid : public PropertyContainer
{
	/// Define a new property metaclass for voxel property containers.
	class VoxelGridClass : public PropertyContainerClass
	{
	public:

		/// Inherit constructor from base class.
		using PropertyContainerClass::PropertyContainerClass;

		/// \brief Create a storage object for standard voxel properties.
		virtual PropertyPtr createStandardPropertyInternal(DataSet* dataset, size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const override;

	protected:

		/// Is called by the system after construction of the meta-class instance.
		virtual void initialize() override;
	};

	OVITO_CLASS_META(VoxelGrid, VoxelGridClass);
	Q_CLASSINFO("DisplayName", "Voxel grid");

public:

	/// Data type used to store the number of cells of the voxel grid in each dimension.
	using GridDimensions = std::array<size_t,3>;

	/// \brief The list of standard voxel properties.
	enum Type {
		UserProperty = PropertyObject::GenericUserProperty,	//< This is reserved for user-defined properties.
		ColorProperty = PropertyObject::GenericColorProperty
	};

	/// \brief Constructor.
	Q_INVOKABLE VoxelGrid(ObjectCreationParams params, const QString& title = QString());

	/// Returns the spatial domain this voxel grid is embedded in after making sure it
	/// can safely be modified.
	SimulationCellObject* mutableDomain() {
		return makeMutable(domain());
	}

	/// Makes sure that all property arrays in this container have a consistent length.
	/// If this is not the case, the method throws an exception.
	void verifyIntegrity() const;

	/// Converts a grid coordinate to a linear voxel array index.
	size_t voxelIndex(size_t x, size_t y, size_t z) const {
		OVITO_ASSERT(x >= 0 && x < shape()[0]);
		OVITO_ASSERT(y >= 0 && y < shape()[1]);
		OVITO_ASSERT(z >= 0 && z < shape()[2]);
		return z * (shape()[0] * shape()[1]) + y * shape()[0] + x;
	}

	/// Converts a linear voxel array index into grid coordinates.
	std::array<size_t, 3> voxelCoords(size_t index) const {
		OVITO_ASSERT(index < elementCount());
		size_t yz = shape()[0] * shape()[1];
		OVITO_ASSERT(voxelIndex(index % shape()[0], (index / shape()[0]) % shape()[1], index / yz) == index);
		return { index % shape()[0], (index / shape()[0]) % shape()[1], index / yz };
	}

	/// Returns the base coordinates for visualizing a vector property from this container using a VectorVis element.
	virtual ConstDataBufferPtr getVectorVisBasePositions(const ConstDataObjectPath& path, const PipelineFlowState& state) const override;

	/// Generates the info string to be displayed in the OVITO status bar for an element from this container.
	virtual QString elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path = {}) const override;

protected:

	/// Saves the class' contents to the given stream.
	virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

	/// Loads the class' contents from the given stream.
	virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

	/// The shape of the grid (i.e. number of voxels in each dimension).
	DECLARE_RUNTIME_PROPERTY_FIELD(GridDimensions, shape, setShape);

	/// The domain the object is embedded in.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const SimulationCellObject>, domain, setDomain, PROPERTY_FIELD_NO_SUB_ANIM);
};

/**
 * Encapsulates a reference to a voxel grid property.
 */
using VoxelPropertyReference = TypedPropertyReference<VoxelGrid>;


}	// End of namespace

Q_DECLARE_METATYPE(Ovito::Grid::VoxelPropertyReference);