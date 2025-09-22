////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

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

        /// Create a storage object for standard voxel properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(VoxelGrid, VoxelGridClass);

public:

    /// The types of uniform grids supported by OVITO.
    enum GridType {
        CellData,   ///< Data values are associated with the voxel cell centers.
        PointData,  ///< Data values are associated with the grid points (cell corners).
    };
    Q_ENUM(GridType);

public:

    /// Data type used to store the number of cells of the voxel grid in each dimension.
    using GridDimensions = std::array<size_t, 3>;

    /// The list of predefined voxel grid properties.
    enum Type {
        UserProperty = Property::GenericUserProperty, //< This is reserved for user-defined properties.
        ColorProperty = Property::GenericColorProperty
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, const QString& title = QString());

    /// Returns the spatial domain this voxel grid is embedded in after making sure it
    /// can safely be modified.
    SimulationCell* mutableDomain() {
        return makeMutable(domain());
    }

    /// Makes sure that all property arrays in this container have a consistent length.
    /// If this is not the case, the method throws an exception.
    void verifyIntegrity() const;

    /// Converts logical grid coordinates to a linear array index.
    inline static size_t voxelIndex(const std::array<size_t, 3>& coords, const GridDimensions& gridShape) {
        OVITO_ASSERT(coords[0] < gridShape[0]);
        OVITO_ASSERT(coords[1] < gridShape[1]);
        OVITO_ASSERT(coords[2] < gridShape[2]);
        return coords[2] * (gridShape[0] * gridShape[1]) + coords[1] * gridShape[0] + coords[0];
    }

    /// Converts logical grid coordinates to a linear array index.
    inline static size_t voxelIndex(size_t x, size_t y, size_t z, const GridDimensions& gridShape) {
        OVITO_ASSERT(x < gridShape[0]);
        OVITO_ASSERT(y < gridShape[1]);
        OVITO_ASSERT(z < gridShape[2]);
        return z * (gridShape[0] * gridShape[1]) + y * gridShape[0] + x;
    }

    /// Converts logical grid coordinates to a linear array index.
    size_t voxelIndex(size_t x, size_t y, size_t z) const {
        return voxelIndex(x, y, z, shape());
    }

    /// Converts a linear array index into logical grid coordinates.
    std::array<size_t, 3> voxelCoords(size_t index) const {
        OVITO_ASSERT(index < elementCount());
        size_t yz = shape()[0] * shape()[1];
        OVITO_ASSERT(voxelIndex(index % shape()[0], (index / shape()[0]) % shape()[1], index / yz) == index);
        return { index % shape()[0], (index / shape()[0]) % shape()[1], index / yz };
    }

    /// Returns the data for visualizing a vector property from this container using a VectorVis element.
    virtual VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                   const RendererResourceCache::ResourceFrame& visCache) const override;

    /// Generates the info string to be displayed in the OVITO status bar for an element from this container.
    virtual QString elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path = {}) const override;

    /// Returns whether this container can be appended to or not - throws an exception if appending is impossible.
    virtual bool isAppendable() const override
    {
        throw Exception(tr("Voxel grid property containers cannot be appended to."));
        return false;
    }

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// The shape of the grid (i.e. number of voxels in each dimension).
    DECLARE_RUNTIME_PROPERTY_FIELD((GridDimensions{{0,0,0}}), shape, setShape);

    /// Determines whether the stored field values are volume- or vertex-based, i.e.,
    /// whether values are associated with the centers or with the corners of the grid cells.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(GridType{CellData}, gridType, setGridType);
    DECLARE_SHADOW_PROPERTY_FIELD(gridType);

    /// The domain the object is embedded in.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const SimulationCell>, domain, setDomain, PROPERTY_FIELD_NO_SUB_ANIM);
};

/**
 * Encapsulates a mapping of input file columns to voxel grid properties.
 */
using VoxelInputColumnMapping = TypedInputColumnMapping<VoxelGrid>;

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::VoxelInputColumnMapping);
