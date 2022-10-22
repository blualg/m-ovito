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

#include <ovito/mesh/Mesh.h>
#include "SurfaceMeshVertices.h"
#include "SurfaceMeshVis.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(SurfaceMeshVertices);

/******************************************************************************
* Creates a storage object for standard vertex properties.
******************************************************************************/
PropertyPtr SurfaceMeshVertices::OOMetaClass::createStandardPropertyInternal(DataSet* dataset, size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
	int dataType;
	size_t componentCount;

	switch(type) {
	case PositionProperty:
		dataType = PropertyObject::Float;
		componentCount = 3;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Point3));
		break;
	case SelectionProperty:
		dataType = PropertyObject::Int;
		componentCount = 1;
		break;
	case ColorProperty:
		dataType = PropertyObject::Float;
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

	PropertyPtr property = PropertyPtr::create(dataset, elementCount, dataType, componentCount, propertyName, flags & ~DataBuffer::InitializeMemory, type, componentNames);

	// Initialize memory if requested.
	if(flags.testFlag(DataBuffer::InitializeMemory) && containerPath.size() >= 2) {
		// Certain standard properties need to be initialized with default values determined by the attached visual elements.
		if(type == ColorProperty) {
			if(const SurfaceMesh* surfaceMesh = dynamic_object_cast<SurfaceMesh>(containerPath[containerPath.size()-2])) {
				if(SurfaceMeshVis* vis = surfaceMesh->visElement<SurfaceMeshVis>()) {
					property->fill(vis->surfaceColor());
					flags.setFlag(DataBuffer::InitializeMemory, false);
				}
			}
		}
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
void SurfaceMeshVertices::OOMetaClass::initialize()
{
	PropertyContainerClass::initialize();

	setPropertyClassDisplayName(tr("Mesh Vertices"));
	setElementDescriptionName(QStringLiteral("vertices"));
	setPythonName(QStringLiteral("vertices"));

	const QStringList emptyList;
	const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
	const QStringList rgbList = QStringList() << "R" << "G" << "B";

	registerStandardProperty(SelectionProperty, tr("Selection"), PropertyObject::Int, emptyList);
	registerStandardProperty(ColorProperty, tr("Color"), PropertyObject::Float, rgbList, nullptr, tr("Vertex colors"));
	registerStandardProperty(PositionProperty, tr("Position"), PropertyObject::Float, xyzList, nullptr, tr("Vertex positions"));
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
SurfaceMeshVertices::SurfaceMeshVertices(ObjectCreationParams params) : PropertyContainer(params)
{
	// Assign the default data object identifier.
	setIdentifier(OOClass().pythonName());

	if(params.createSubObjects()) {
		// Create the standard 'Position' property.
		createProperty(SurfaceMeshVertices::PositionProperty);
	}
}

/******************************************************************************
* Returns the base point and vector information for visualizing a vector 
* property from this container using a VectorVis element.
******************************************************************************/
std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> SurfaceMeshVertices::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state) const
{
	OVITO_ASSERT(path.lastAs<SurfaceMeshVertices>(1) == this);
	if(const SurfaceMesh* mesh = path.lastAs<SurfaceMesh>(2)) {
		mesh->verifyMeshIntegrity();

		ConstDataBufferPtr vectorProperty = path.lastAs<DataBuffer>();
		if(vectorProperty && vectorProperty->dataType() == PropertyObject::Float && vectorProperty->componentCount() == 3) {
			// Does the mesh have cutting planes and do we need to perform point culling?
			if(!mesh->cuttingPlanes().empty()) {
				// Create a copy of the vector property in which the values of culled points
				// will be nulled out to hide the arrow glyphs for these points.
				DataBufferAccessAndRef<Vector3> filteredVectors = vectorProperty.makeCopy();
				if(ConstDataBufferAccess<Point3> positions = getProperty(PositionProperty)) {
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

		return { getProperty(PositionProperty), std::move(vectorProperty) };
	}
	return {};
}

}	// End of namespace
