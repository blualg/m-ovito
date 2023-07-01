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

#include <ovito/grid/Grid.h>
#include "VoxelGrid.h"
#include "VoxelGridVis.h"

namespace Ovito::Grid {

IMPLEMENT_OVITO_CLASS(VoxelGrid);
DEFINE_RUNTIME_PROPERTY_FIELD(VoxelGrid, shape);
DEFINE_PROPERTY_FIELD(VoxelGrid, gridType);
DEFINE_SHADOW_PROPERTY_FIELD(VoxelGrid, gridType);
DEFINE_REFERENCE_FIELD(VoxelGrid, domain);
SET_PROPERTY_FIELD_LABEL(VoxelGrid, shape, "Shape");
SET_PROPERTY_FIELD_LABEL(VoxelGrid, domain, "Domain");
SET_PROPERTY_FIELD_LABEL(VoxelGrid, gridType, "Grid type");

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void VoxelGrid::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    // Enable automatic conversion of a VoxelPropertyReference to a generic PropertyReference and vice versa.
    QMetaType::registerConverter<VoxelPropertyReference, PropertyReference>();
    QMetaType::registerConverter<PropertyReference, VoxelPropertyReference>();

    setPropertyClassDisplayName(tr("Voxel grid"));
    setElementDescriptionName(QStringLiteral("voxels"));
    setPythonName(QStringLiteral("voxels"));

    const QStringList emptyList;
    const QStringList rgbList = QStringList() << "R" << "G" << "B";

    registerStandardProperty(ColorProperty, tr("Color"), PropertyObject::FloatGraphics, rgbList, nullptr, tr("Voxel colors"));
}

/******************************************************************************
* Creates a storage object for standard voxel properties.
******************************************************************************/
PropertyPtr VoxelGrid::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case ColorProperty:
        dataType = PropertyObject::FloatGraphics;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(GraphicsFloatType) == sizeof(ColorG));
        break;
    default:
        OVITO_ASSERT_MSG(false, "VoxelGrid::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard voxel property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    if(init == DataBuffer::Initialized) {
        // Default-initialize property values with zeros.
        property->fillZero();
    }

    return property;
}

/******************************************************************************
* Constructor.
******************************************************************************/
VoxelGrid::VoxelGrid(ObjectInitializationFlags flags, const QString& title) : PropertyContainer(flags, title),
    _gridType(CellData)
{
    // Create and attach a default visualization element for rendering the grid.
    if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement))) {
        setVisElement(OORef<VoxelGridVis>::create(flags));
    }
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void VoxelGrid::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    PropertyContainer::saveToStream(stream, excludeRecomputableData);

    stream.beginChunk(0x01);
    stream.writeSizeT(shape().size());
    for(size_t d : shape())
        stream.writeSizeT(d);
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void VoxelGrid::loadFromStream(ObjectLoadStream& stream)
{
    PropertyContainer::loadFromStream(stream);

    stream.expectChunk(0x01);

    size_t ndim = stream.readSizeT();
    if(ndim != _shape.get().size()) throw Exception(tr("Invalid voxel grid dimensionality."));

    for(size_t& d : _shape.mutableValue())
        stream.readSizeT(d);

    stream.closeChunk();
}

/******************************************************************************
* Makes sure that all property arrays in this container have a consistent length.
* If this is not the case, the method throws an exception.
******************************************************************************/
void VoxelGrid::verifyIntegrity() const
{
    PropertyContainer::verifyIntegrity();

    size_t expectedElementCount = shape()[0] * shape()[1] * shape()[2];
    if(elementCount() != expectedElementCount)
        throw Exception(tr("VoxelGrid has inconsistent dimensions. PropertyContainer array length (%1) does not match the total number of voxel cells (%2) for a grid with shape %3 x %4 x %5.")
            .arg(elementCount()).arg(expectedElementCount).arg(shape()[0]).arg(shape()[1]).arg(shape()[2]));

    if(!domain())
        throw Exception(tr("Voxel grid has no simulation cell assigned."));
}

/******************************************************************************
* Generates the info string to be displayed in the OVITO status bar for an element from this container.
******************************************************************************/
QString VoxelGrid::elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path) const
{
    std::array<size_t, 3> coords = voxelCoords(elementIndex);
    QString str = (gridType() == GridType::CellData) ? tr("Cell ") : tr("Point ");
    if(domain() && domain()->is2D() && shape()[2] <= 1)
        str += QStringLiteral("(%1, %2)").arg(coords[0]).arg(coords[1]);
    else
        str += QStringLiteral("(%1, %2, %3)").arg(coords[0]).arg(coords[1]).arg(coords[2]);
    str += QStringLiteral("<sep>");
    str += PropertyContainer::elementInfoString(elementIndex, path);
    return str;
}

/******************************************************************************
* Returns the base point and vector information for visualizing a vector
* property from this container using a VectorVis element.
******************************************************************************/
std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> VoxelGrid::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state, MixedKeyCache& visCache) const
{
    OVITO_ASSERT(path.lastAs<VoxelGrid>(1) == this);

    // Make sure the voxel grid has a domain.
    verifyIntegrity();

    // Look up the cell center coordinates in the cache.
    using CacheKey = RendererResourceKey<struct VoxelGridCellCentersCache, ConstDataObjectRef>;
    auto& basePositions = visCache.get<ConstDataBufferPtr>(CacheKey(this));

    if(!basePositions) {
        BufferFactory<Point3> points(elementCount());
        if(points.size() != 0) {
            if(gridType() == GridType::CellData) {
                // Compute grid cell centers.
                OVITO_ASSERT(shape()[0] != 0 && shape()[1] != 0 && shape()[2] != 0);
                Point3 xyz;
                FloatType dx = FloatType(1) / shape()[0];
                FloatType dy = FloatType(1) / shape()[1];
                FloatType dz = FloatType(1) / shape()[2];
                auto p = points.begin();
                size_t x,y,z;
                for(z = 0, xyz.z() = dz/2; z < shape()[2]; z++, xyz.z() += dz) {
                    if(domain()->is2D()) xyz.z() = 0;
                    for(y = 0, xyz.y() = dy/2; y < shape()[1]; y++, xyz.y() += dy) {
                        for(x = 0, xyz.x() = dx/2; x < shape()[0]; x++, xyz.x() += dx) {
                            *p++ = domain()->reducedToAbsolute(xyz);
                        }
                    }
                }
            }
            else if(gridType() == GridType::PointData) {
                // Compute grid vertex positions.
                Point3 xyz;
                FloatType dx = FloatType(1) / ((domain()->pbcFlags()[0] || shape()[0] == 1) ? shape()[0] : (shape()[0] - 1));
                FloatType dy = FloatType(1) / ((domain()->pbcFlags()[1] || shape()[1] == 1) ? shape()[1] : (shape()[1] - 1));
                FloatType dz = FloatType(1) / ((domain()->pbcFlags()[2] || shape()[2] == 1) ? shape()[2] : (shape()[2] - 1));
                auto p = points.begin();
                size_t x,y,z;
                for(z = 0, xyz.z() = 0; z < shape()[2]; z++, xyz.z() += dz) {
                    for(y = 0, xyz.y() = 0; y < shape()[1]; y++, xyz.y() += dy) {
                        for(x = 0, xyz.x() = 0; x < shape()[0]; x++, xyz.x() += dx) {
                            *p++ = domain()->reducedToAbsolute(xyz);
                        }
                    }
                }
            }
            else OVITO_ASSERT(false);
        }
        basePositions = points.take();
    }
    return { basePositions, path.lastAs<DataBuffer>() };
}

}   // End of namespace
