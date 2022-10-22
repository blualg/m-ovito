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
#include "SurfaceMeshFaces.h"
#include "SurfaceMeshVis.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(SurfaceMeshFaces);

/******************************************************************************
* Creates a storage object for standard face properties.
******************************************************************************/
PropertyPtr SurfaceMeshFaces::OOMetaClass::createStandardPropertyInternal(DataSet* dataset, size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
	int dataType;
	size_t componentCount;

	switch(type) {
	case SelectionProperty:
	case RegionProperty:
	case FaceTypeProperty:
		dataType = PropertyObject::Int;
		componentCount = 1;
		break;
	case ColorProperty:
		dataType = PropertyObject::Float;
		componentCount = 3;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Color));
		break;
	case BurgersVectorProperty:
	case CrystallographicNormalProperty:
		dataType = PropertyObject::Float;
		componentCount = 3;
		OVITO_ASSERT(componentCount * sizeof(FloatType) == sizeof(Vector3));
		break;
	default:
		OVITO_ASSERT_MSG(false, "SurfaceMeshFaces::createStandardPropertyInternal", "Invalid standard property type");
		throw Exception(tr("This is not a valid standard face property type: %1").arg(type));
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
				ConstPropertyAccess<Color> regionColorProperty = surfaceMesh->regions()->getProperty(SurfaceMeshRegions::ColorProperty);
				ConstPropertyAccess<int> faceRegionProperty = surfaceMesh->faces()->getProperty(SurfaceMeshFaces::RegionProperty);
				if(regionColorProperty && faceRegionProperty && faceRegionProperty.size() == elementCount) {
					// Inherit face colors from regions.
					boost::transform(faceRegionProperty, PropertyAccess<Color>(property).begin(), 
						[&](int region) { return (region >= 0 && region < regionColorProperty.size()) ? regionColorProperty[region] : Color(1,1,1); });
					flags.setFlag(DataBuffer::InitializeMemory, false);
				}
				else if(SurfaceMeshVis* vis = surfaceMesh->visElement<SurfaceMeshVis>()) {
					// Initialize face colors from uniform color set in SurfaceMeshVis.
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
void SurfaceMeshFaces::OOMetaClass::initialize()
{
	PropertyContainerClass::initialize();

	setPropertyClassDisplayName(tr("Mesh Faces"));
	setElementDescriptionName(QStringLiteral("faces"));
	setPythonName(QStringLiteral("faces"));

	const QStringList emptyList;
	const QStringList xyzList = QStringList() << "X" << "Y" << "Z";
	const QStringList rgbList = QStringList() << "R" << "G" << "B";

	registerStandardProperty(SelectionProperty, tr("Selection"), PropertyObject::Int, emptyList);
	registerStandardProperty(ColorProperty, tr("Color"), PropertyObject::Float, rgbList, nullptr, tr("Face colors"));
	registerStandardProperty(FaceTypeProperty, tr("Type"), PropertyObject::Int, emptyList);
	registerStandardProperty(RegionProperty, tr("Region"), PropertyObject::Int, emptyList);
	registerStandardProperty(BurgersVectorProperty, tr("Burgers Vector"), PropertyObject::Float, xyzList, nullptr, tr("Burgers vectors"));
	registerStandardProperty(CrystallographicNormalProperty, tr("Crystallographic Normal"), PropertyObject::Float, xyzList);
}

/******************************************************************************
* Generates a human-readable string representation of the data object reference.
******************************************************************************/
QString SurfaceMeshFaces::OOMetaClass::formatDataObjectPath(const ConstDataObjectPath& path) const
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
* Returns the base point and vector information for visualizing a vector 
* property from this container using a VectorVis element.
******************************************************************************/
std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> SurfaceMeshFaces::getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state) const
{
	OVITO_ASSERT(path.lastAs<SurfaceMeshFaces>(1) == this);
	if(const SurfaceMesh* mesh = path.lastAs<SurfaceMesh>(2)) {
		mesh->verifyMeshIntegrity();
		// Look up the face centroids in the cache.
		using CacheKey = RendererResourceKey<struct SurfaceMeshFacesCentroidsCache, ConstDataObjectRef, ConstDataObjectRef>;
		auto& [basePositions, vectorProperty] = dataset()->visCache().get<std::tuple<ConstDataBufferPtr,ConstDataBufferPtr>>(CacheKey(mesh, path.lastAs<DataBuffer>()));
		if(!basePositions) {
			DataBufferAccessAndRef<Vector3> filteredVectors;
			vectorProperty = path.lastAs<DataBuffer>();
			if(vectorProperty && vectorProperty->dataType() == PropertyObject::Float && vectorProperty->componentCount() == 3) {
				// Does the mesh have cutting planes and do we need to perform point culling?
				if(!mesh->cuttingPlanes().empty()) {
					// Create a copy of the vector property in which the values of culled points
					// will be nulled out to hide the arrow glyphs for these points.
					filteredVectors = vectorProperty.makeCopy();
				}
			}

			// Compute face centroids.
			DataBufferAccessAndRef<Point3> centroids = DataBufferPtr::create(dataset(), mesh->faces()->elementCount(), DataBuffer::Float, 3);
			const SurfaceMeshAccess meshAccess(mesh);
			for(SurfaceMeshAccess::face_index face = 0; face < meshAccess.faceCount(); face++) {
				Vector3 c = Vector3::Zero();
				Vector3 com = Vector3::Zero();
				int n = 0;
				SurfaceMeshAccess::edge_index firstFaceEdge = meshAccess.firstFaceEdge(face);
				if(firstFaceEdge != SurfaceMeshAccess::InvalidIndex) {
					SurfaceMeshAccess::edge_index edge = firstFaceEdge;
					do {
						c += meshAccess.edgeVector(edge);
						com += c;
						n++;
						edge = meshAccess.nextFaceEdge(edge);
					}
					while(edge != firstFaceEdge);
					centroids[face] = meshAccess.wrapPoint(meshAccess.vertexPosition(meshAccess.vertex1(firstFaceEdge)) + (com / n));
					if(filteredVectors && mesh->isPointCulled(centroids[face]))
						filteredVectors[face].setZero();
				}
				else {
					centroids[face] = Point3::Origin();
					if(filteredVectors) 
						filteredVectors[face].setZero();
				}
			}
			basePositions = centroids.take();
			if(filteredVectors) 
				vectorProperty = filteredVectors.take();
		}
		return { basePositions, vectorProperty };
	}
	return {};
}

}	// End of namespace
