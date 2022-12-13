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
#include <ovito/mesh/surface/RenderableSurfaceMesh.h>
#include <ovito/core/app/Application.h>
#include "VTKTriangleMeshExporter.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(VTKTriangleMeshExporter);
DEFINE_PROPERTY_FIELD(VTKTriangleMeshExporter, exportCapPolygons);

/******************************************************************************
 * This is called once for every output file to be written and before
 * exportData() is called.
 *****************************************************************************/
void VTKTriangleMeshExporter::openOutputFile(const QString& filePath, int numberOfFrames)
{
	OVITO_ASSERT(!_outputFile.isOpen());
	OVITO_ASSERT(!_outputStream);

	_outputFile.setFileName(filePath);
	_outputStream = std::make_unique<CompressedTextWriter>(_outputFile);
}

/******************************************************************************
 * This is called once for every output file written after exportData()
 * has been called.
 *****************************************************************************/
void VTKTriangleMeshExporter::closeOutputFile(bool exportCompleted)
{
	_outputStream.reset();
	if(_outputFile.isOpen())
		_outputFile.close();

	if(!exportCompleted)
		_outputFile.remove();
}

/******************************************************************************
 * Exports a single animation frame to the current output file.
 *****************************************************************************/
bool VTKTriangleMeshExporter::exportFrame(int frameNumber, const QString& filePath, MainThreadOperation& operation)
{
	// Evaluate pipeline.
	// Note: We are requesting the rendering state from the pipeline,
	// because we are interested in renderable triangle meshes.
	const PipelineFlowState& state = getPipelineDataToBeExported(frameNumber, true);
	if(operation.isCanceled())
		return false;

	// Look up the RenderableSurfaceMesh to be exported in the pipeline state.
	DataObjectReference objectRef(&RenderableSurfaceMesh::OOClass(), dataObjectToExport().dataPath());
	const RenderableSurfaceMesh* meshObj = static_object_cast<RenderableSurfaceMesh>(state.getLeafObject(objectRef));
	if(!meshObj) {
		throw Exception(tr("The pipeline output does not contain the surface mesh to be exported (animation frame: %1; object key: %2). Available surface mesh keys: (%3)")
			.arg(frameNumber).arg(objectRef.dataPath()).arg(getAvailableDataObjectList(state, RenderableSurfaceMesh::OOClass())));
	}

	operation.setProgressText(tr("Writing file %1").arg(filePath));

	const TriMeshObject* surfaceMesh = meshObj->surfaceMesh();
	const TriMeshObject* capPolygonsMesh = exportCapPolygons() ? meshObj->capPolygonsMesh() : nullptr;
	auto totalVertexCount = (surfaceMesh ? surfaceMesh->vertexCount() : 0) + (capPolygonsMesh ? capPolygonsMesh->vertexCount() : 0);
	auto totalFaceCount = (surfaceMesh ? surfaceMesh->faceCount() : 0) + (capPolygonsMesh ? capPolygonsMesh->faceCount() : 0);
	textStream() << "# vtk DataFile Version 3.0\n";
	textStream() << "# Triangle surface mesh written by " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";
	textStream() << "ASCII\n";
	textStream() << "DATASET UNSTRUCTURED_GRID\n";
	textStream() << "POINTS " << totalVertexCount << " double\n";
	if(surfaceMesh) {
		for(const Point3& p : surfaceMesh->vertices())
			textStream() << p.x() << " " << p.y() << " " << p.z() << "\n";
	}
	if(capPolygonsMesh) {
		for(const Point3& p : capPolygonsMesh->vertices())
			textStream() << p.x() << " " << p.y() << " " << p.z() << "\n";
	}
	textStream() << "\nCELLS " << totalFaceCount << " " << (totalFaceCount * 4) << "\n";
	if(surfaceMesh) {
		for(const TriMeshFace& f : surfaceMesh->faces()) {
			textStream() << "3";
			for(size_t i = 0; i < 3; i++)
				textStream() << " " << f.vertex(i);
			textStream() << "\n";
		}
	}
	if(capPolygonsMesh) {
		for(const TriMeshFace& f : capPolygonsMesh->faces()) {
			textStream() << "3";
			for(size_t i = 0; i < 3; i++)
				textStream() << " " << (f.vertex(i) + (surfaceMesh ? surfaceMesh->vertexCount() : 0));
			textStream() << "\n";
		}
	}
	textStream() << "\nCELL_TYPES " << totalFaceCount << "\n";
	for(size_t i = 0; i < totalFaceCount; i++)
		textStream() << "5\n";	// Triangle

	textStream() << "\nCELL_DATA " << totalFaceCount << "\n";
	textStream() << "SCALARS cap unsigned_char\n";
	textStream() << "LOOKUP_TABLE default\n";
	if(surfaceMesh) {
		for(size_t i = 0; i < surfaceMesh->faceCount(); i++)
			textStream() << "0\n";
	}
	if(capPolygonsMesh) {
		for(size_t i = 0; i < capPolygonsMesh->faceCount(); i++)
			textStream() << "1\n";
	}

	if(!meshObj->materialColors().empty()) {
		textStream() << "\nSCALARS material_index int\n";
		textStream() << "LOOKUP_TABLE default\n";
		if(surfaceMesh) {
			for(size_t i = 0; i < surfaceMesh->faceCount(); i++)
				textStream() << surfaceMesh->face(i).materialIndex() << "\n";
		}
		if(capPolygonsMesh) {
			for(size_t i = 0; i < capPolygonsMesh->faceCount(); i++)
				textStream() << "0\n";
		}

		textStream() << "\nCOLOR_SCALARS color 3\n";
		if(surfaceMesh) {
			for(size_t i = 0; i < surfaceMesh->faceCount(); i++) {
				const ColorA& c = meshObj->materialColors()[surfaceMesh->face(i).materialIndex() % meshObj->materialColors().size()];
				textStream() << c.r() << " " << c.g() << " " << c.b() << "\n";
			}
		}
		if(capPolygonsMesh) {
			for(size_t i = 0; i < capPolygonsMesh->faceCount(); i++)
				textStream() << "1 1 1\n";
		}
	}

	if(surfaceMesh && capPolygonsMesh) {
		textStream() << "\nPOINT_DATA " << totalVertexCount << "\n";
		textStream() << "SCALARS cap unsigned_char\n";
		textStream() << "LOOKUP_TABLE default\n";
		for(size_t i = 0; i < surfaceMesh->vertexCount(); i++)
			textStream() << "0\n";
		for(size_t i = 0; i < capPolygonsMesh->vertexCount(); i++)
			textStream() << "1\n";
	}
	if(surfaceMesh && surfaceMesh->hasVertexColors()) {
		textStream() << "COLOR_SCALARS color 4\n";
		for(const ColorA& c : surfaceMesh->vertexColors())
			textStream() << c.r() << " " << c.g() << " " << c.b() << " " << c.a() << "\n";
		if(capPolygonsMesh) {
			for(size_t i = 0; i < capPolygonsMesh->vertexCount(); i++)
				textStream() << "1 1 1\n";
		}
	}

	return !operation.isCanceled();
}

}	// End of namespace
