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

#include <ovito/mesh/Mesh.h>
#include "SurfaceMeshVertices.h"
#include "SurfaceMeshVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshVertices);
OVITO_CLASSINFO(SurfaceMeshVertices, "DisplayName", "Mesh Vertices");

/******************************************************************************
* Creates a storage object for standard vertex properties.
******************************************************************************/
PropertyPtr SurfaceMeshVertices::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    int dataType;
    size_t componentCount;

    switch(type) {
    case PositionProperty:
        dataType = Property::FloatDefault;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Point3));
        break;
    case SelectionProperty:
        dataType = Property::IntSelection;
        componentCount = 1;
        break;
    case ColorProperty:
        dataType = Property::FloatGraphics;
        componentCount = 3;
        OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Color));
        break;
    default:
        OVITO_ASSERT_MSG(false, "SurfaceMeshVertices::createStandardPropertyInternal", "Invalid standard property type");
        throw Exception(tr("This is not a valid standard vertex property type: %1").arg(type));
    }
    const QStringList& componentNames = standardPropertyComponentNames(type);
    const QString& propertyName = standardPropertyName(type);

    OVITO_ASSERT(componentCount == standardPropertyComponentCount(type));

    PropertyPtr property = PropertyPtr::create(DataBuffer::Uninitialized, elementCount, dataType, componentCount, propertyName, type, componentNames);

    // Initialize memory if requested.
    if(init == DataBuffer::Initialized && containerPath.size() >= 2) {
        // Certain standard properties need to be initialized with default values determined by the attached visual elements.
        if(type == ColorProperty) {
            if(const SurfaceMesh* surfaceMesh = dynamic_object_cast<SurfaceMesh>(containerPath[containerPath.size()-2])) {
                if(SurfaceMeshVis* vis = surfaceMesh->visElement<SurfaceMeshVis>()) {
                    property->fill<ColorG>(vis->surfaceColor().toDataType<GraphicsFloatType>());
                    init = DataBuffer::Uninitialized;
                }
            }
        }
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
void SurfaceMeshVertices::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Mesh Vertices"));
    setElementDescriptionName(QStringLiteral("vertices"));
    setPythonName(QStringLiteral("vertices"));

    const QStringList emptyList;
    const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
    const QStringList rgbList = QStringList() << "R" << "G" << "B";

    registerStandardProperty(SelectionProperty, tr("Selection"), Property::IntSelection, emptyList);
    registerStandardProperty(ColorProperty, tr("Color"), Property::FloatGraphics, rgbList, nullptr, tr("Vertex colors"));
    registerStandardProperty(PositionProperty, tr("Position"), Property::FloatDefault, xyzList, nullptr, tr("Vertex positions"));
}

/******************************************************************************
* Generates a human-readable string representation of the data object reference.
******************************************************************************/
QString SurfaceMeshVertices::OOMetaClass::formatDataObjectPath(const ConstDataObjectPath& path) const
{
    QString str;
    for(const DataObject* obj : path) {
        if(!str.isEmpty())
            str += QStringLiteral(u" \u2192 ");  // Unicode arrow
        str += obj->objectTitle();
    }
    return str;
}

/******************************************************************************
* Constructor.
******************************************************************************/
void SurfaceMeshVertices::initializeObject(ObjectInitializationFlags flags)
{
    PropertyContainer::initializeObject(flags);

    // Assign the default data object identifier.
    setIdentifier(OOClass().pythonName());

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the standard 'Position' property.
        createProperty(SurfaceMeshVertices::PositionProperty);
    }
}

/******************************************************************************
 * Returns the data for visualizing a vector property from this container using a VectorVis element.
 ******************************************************************************/
VectorVis::VectorData SurfaceMeshVertices::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                            const RendererResourceCache::ResourceFrame& visCache) const
{
    OVITO_ASSERT(path.lastAs<SurfaceMeshVertices>(1) == this);
    if(const SurfaceMesh* mesh = path.lastAs<SurfaceMesh>(2)) {
        mesh->verifyMeshIntegrity();

        ConstDataBufferPtr vectorProperty = path.lastAs<DataBuffer>();
        if(vectorProperty && vectorProperty->componentCount() == 3) {
            OVITO_ASSERT(vectorProperty->dataType() == Property::FloatDefault);
            if(vectorProperty->dataType() == Property::FloatDefault) {
                // Does the mesh have cutting planes and do we need to perform point culling?
                if(!mesh->cuttingPlanes().empty()) {
                    // Create a copy of the vector property in which the values of culled points
                    // will be nulled out to hide the arrow glyphs at these points.
                    BufferWriteAccessAndRef<Vector3, access_mode::write> filteredVectors = vectorProperty.makeCopy();
                    if(BufferReadAccess<Point3> positions = getProperty(PositionProperty)) {
                        Vector3* v = filteredVectors.begin();
                        for(const Point3& p : positions) {
                            if(mesh->isPointCulled(p))
                                v->setZero();
                            ++v;
                        }
                        vectorProperty = filteredVectors.take();
                    }
                }
            }
        }

        return { getProperty(PositionProperty), std::move(vectorProperty), nullptr, nullptr };
    }
    return {};
}

}   // End of namespace
