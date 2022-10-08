////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/mesh/surface/SurfaceMeshAccess.h>
#include "ParaViewVTPMeshImporter.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(ParaViewVTPMeshImporter);
IMPLEMENT_OVITO_CLASS(MeshParaViewVTMFileFilter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTPMeshImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
	// Initialize XML reader and open input file.
	std::unique_ptr<QIODevice> device = file.createIODevice();
	if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
		return false;
	QXmlStreamReader xml(device.get());

	// Parse XML. First element must be <VTKFile type="PolyData">.
	if(xml.readNext() != QXmlStreamReader::StartDocument)
		return false;
	if(xml.readNext() != QXmlStreamReader::StartElement)
		return false;
	if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
		return false;
	if(xml.attributes().value("type").compare(QStringLiteral("PolyData")) != 0)
		return false;

	// Continue until we reach the <Piece> element. 
	while(xml.readNextStartElement()) {
		if(xml.name().compare(QStringLiteral("Piece")) == 0) {
			// Number of triangle strips or polygons must be non-zero.
			if(xml.attributes().value("NumberOfStrips").toULongLong() != 0 || xml.attributes().value("NumberOfPolys").toULongLong() != 0)
				return !xml.hasError();
			break;
		}
	}

	return false;
}

/******************************************************************************
* Parses the input file.
******************************************************************************/
void ParaViewVTPMeshImporter::FrameLoader::loadFile()
{
	setProgressText(tr("Reading ParaView VTP PolyData file %1").arg(fileHandle().toString()));

	// Create the destination mesh object.
	QString meshIdentifier = loadRequest().dataBlockPrefix;
	if(meshIdentifier.isEmpty()) meshIdentifier = "mesh";
	SurfaceMesh* meshObj = state().getMutableLeafObject<SurfaceMesh>(SurfaceMesh::OOClass(), meshIdentifier);
	if(!meshObj) {
		meshObj = state().createObject<SurfaceMesh>(dataSource());
		meshObj->setIdentifier(meshIdentifier);
		SurfaceMeshVis* vis = meshObj->visElement<SurfaceMeshVis>();
		if(vis) {
			vis->setShowCap(false);
			vis->setSmoothShading(true);
			vis->setSurfaceIsClosed(false);
			vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(SurfaceMeshVis::showCap), SHADOW_PROPERTY_FIELD(SurfaceMeshVis::smoothShading)});
		}
		if(!loadRequest().dataBlockPrefix.isEmpty()) {
			meshObj->setTitle(tr("Mesh: %1").arg(loadRequest().dataBlockPrefix));
			if(vis) vis->setTitle(tr("Mesh: %1").arg(loadRequest().dataBlockPrefix));
		}
		else {
			meshObj->setTitle(tr("Mesh"));
			if(vis) vis->setTitle(tr("Mesh"));
		}
	}
	SurfaceMeshAccess mesh(meshObj);

	// Reset mesh or append data to existing mesh.
	if(!loadRequest().appendData)
		mesh.clearMesh();

	// Initialize XML reader and open input file.
	std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
	if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
		throw Exception(tr("Failed to open VTP file: %1").arg(device->errorString()));
	QXmlStreamReader xml(device.get());

	size_t numberOfPoints = 0;
	size_t numberOfVerts = 0;
	size_t numberOfLines = 0;
	size_t numberOfStrips = 0;
	size_t numberOfPolys = 0;
	size_t numberOfCells = 0;
	SurfaceMeshAccess::vertex_index vertexBaseIndex = SurfaceMeshAccess::InvalidIndex;
	SurfaceMeshAccess::face_index faceBaseIndex = SurfaceMeshAccess::InvalidIndex;
	std::vector<PropertyPtr> cellDataArrays;
	std::vector<PropertyPtr> pointDataArrays;

	// Parse the elements of the XML file.
	while(xml.readNextStartElement()) {
		if(isCanceled())
			return;

		if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
			if(xml.attributes().value("type").compare(QStringLiteral("PolyData")) != 0)
				xml.raiseError(tr("VTK file is not of type PolyData."));
			else if(xml.attributes().value("byte_order").compare(QStringLiteral("LittleEndian")) != 0)
				xml.raiseError(tr("Byte order must be 'LittleEndian'. Please ask the OVITO developers to extend the capabilities of the file parser."));
			else if(!xml.attributes().value("compressor").isEmpty())
				xml.raiseError(tr("Current implementation does not support compressed data arrays. Please ask the OVITO developers to extend the capabilities of the file parser."));
		}
		else if(xml.name().compare(QStringLiteral("PolyData")) == 0) {
			// Do nothing. Parse child elements.
		}
		else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
			// Parse geometric entity counts of the current piece.
			numberOfPoints = xml.attributes().value("NumberOfPoints").toULongLong();
			numberOfVerts = xml.attributes().value("NumberOfVerts").toULongLong();
			numberOfLines = xml.attributes().value("NumberOfLines").toULongLong();
			numberOfStrips = xml.attributes().value("NumberOfStrips").toULongLong();
			numberOfPolys = xml.attributes().value("NumberOfPolys").toULongLong();
			numberOfCells = numberOfVerts + numberOfLines + numberOfStrips + numberOfPolys;
			// Create geometry elements.
			vertexBaseIndex = mesh.createVertices(numberOfPoints);
			// Continue by parsing child elements.
		}
		else if(xml.name().compare(QStringLiteral("Points")) == 0) {
			// Parse child <DataArray> element containing the point coordinates.
			if(!xml.readNextStartElement())
				break;
			PropertyPtr property = parseDataArray(xml, PropertyObject::Float);
			if(!property)
				break;

			// Make sure the data array has the expected data layout.
			if(property->componentCount() != 3 || property->name() != "Points" || property->size() != numberOfPoints) {
				xml.raiseError(tr("Points data array has wrong data layout, size or name."));
				break;
			}
			// Copy point coordinates from temporary array to surface mesh data structure.
			OVITO_ASSERT(property->size() + vertexBaseIndex == mesh.vertexCount());
			boost::copy(ConstPropertyAccess<Point3>(property), std::next(std::begin(mesh.mutableVertexPositions()), vertexBaseIndex));
			xml.skipCurrentElement();
		}
		else if(xml.name().compare(QStringLiteral("Polys")) == 0) {
			// Parse child <DataArray> element containing the connectivity information.
			if(!xml.readNextStartElement())
				break;
			PropertyPtr connectivityArray = parseDataArray(xml, qMetaTypeId<SurfaceMeshAccess::vertex_index>());
			if(!connectivityArray)
				break;
			// Make sure the data array has the expected data layout.
			if(connectivityArray->componentCount() != 1 || connectivityArray->name() != "connectivity") {
				xml.raiseError(tr("Connectivity data array has wrong data layout, size or name."));
				break;
			}
			faceBaseIndex = mesh.faceCount();

			// Parse child <DataArray> element containing the offset information.
			if(!xml.readNextStartElement())
				break;
			PropertyPtr offsetsArray = parseDataArray(xml, PropertyObject::Int);
			if(!offsetsArray)
				break;
			// Make sure the data array has the expected data layout.
			if(offsetsArray->componentCount() != 1 || offsetsArray->size() != numberOfPolys || offsetsArray->name() != "offsets") {
				xml.raiseError(tr("Offsets data array has wrong data layout, size or name."));
				break;
			}

			// Shift vertex indices in the array by base vertex offset.
			PropertyAccess<SurfaceMeshAccess::vertex_index> vertexIndices(connectivityArray);
			if(vertexBaseIndex != 0)
				for(SurfaceMeshAccess::vertex_index& idx : vertexIndices) idx += vertexBaseIndex;

			// Go through the connectivity and the offsets arrays and create corresponding faces in the output mesh.
			int previousOffset = 0;
			for(int offset : ConstPropertyAccess<int>(offsetsArray)) {
				if(offset < previousOffset + 3 || offset > vertexIndices.size()) {
					xml.raiseError(tr("Invalid or inconsistent connectivity information in <Polys> element."));
					break;
				}
				mesh.createFace(vertexIndices.begin() + previousOffset, vertexIndices.begin() + offset);
				previousOffset = offset;
			}
			if(xml.hasError())
				break;

			xml.skipCurrentElement();
		}
		else if(xml.name().compare(QStringLiteral("CellData")) == 0) {
			// Parse <DataArray> child elements.
			while(xml.readNextStartElement() && !isCanceled()) {
				if(xml.name().compare(QStringLiteral("DataArray")) == 0) {
					if(PropertyPtr property = parseDataArray(xml))
						cellDataArrays.push_back(std::move(property));
					else
						break;
				}
				else {
					xml.skipCurrentElement();
				}
			}
		}
		else if(xml.name().compare(QStringLiteral("PointData")) == 0) {
			// Parse child elements.
			while(xml.readNextStartElement() && !isCanceled()) {
				if(xml.name().compare(QStringLiteral("DataArray")) == 0) {
					if(PropertyPtr property = parseDataArray(xml))
						pointDataArrays.push_back(std::move(property));
					else
						break;
				}
				else {
					xml.skipCurrentElement();
				}
			}
		}
		else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QStringLiteral("Verts")) == 0 || xml.name().compare(QStringLiteral("Lines")) == 0 || xml.name().compare(QStringLiteral("Strips")) == 0) {
			// Do nothing. Ignore element contents.
			xml.skipCurrentElement();
		}
		else {
			xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
		}
	}

	// Handle XML parsing errors.
	if(xml.hasError()) {
		throw Exception(tr("VTP file parsing error on line %1, column %2: %3")
			.arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
	}
	if(isCanceled())
		return;

	// Add cell data arrays to the mesh.
	if(numberOfPolys == numberOfCells) {
		for(auto& property : cellDataArrays) {
			OVITO_ASSERT(property->size() == numberOfCells);
			// If it is the first partial dataset we are loading, or if we are loading the mesh in once piece, then 
			// the loaded property arrays can simply be added to the mesh faces.
			// Otherwise, if we are loading subsequent parts of the distributed mesh, 
			// then the loaded property values must be copied into the correct subrange of the existing
			// face properties.
			if(!loadRequest().appendData) {
				OVITO_ASSERT(property->size() == mesh.faceCount());
				OVITO_ASSERT(faceBaseIndex == 0);
				mesh.addFaceProperty(std::move(property));
			}
			else {
				PropertyObject* existingProperty = property->type() != SurfaceMeshFaces::UserProperty 
					? mesh.mutableFaceProperty(static_cast<SurfaceMeshFaces::Type>(property->type())) 
					: mesh.mutableFaceProperty(property->name());
				if(existingProperty && existingProperty->dataType() == property->dataType() && existingProperty->componentCount() == property->componentCount()) {
					existingProperty->copyRangeFrom(*property, 0, faceBaseIndex, property->size());
				}
			}
		}
	}

	// Add point data arrays to the mesh vertices.
	for(auto& property : pointDataArrays) {
		OVITO_ASSERT(property->size() == numberOfPoints);
		// If it is the first partial dataset we are loading, or if we are loading the mesh in once piece, then 
		// the loaded property arrays can simply be added to the mesh vertices.
		// Otherwise, if we are loading subsequent parts of the distributed mesh, 
		// then the loaded property values must be copied into the correct subrange of the existing
		// vertex properties.
		if(!loadRequest().appendData) {
			OVITO_ASSERT(property->size() == mesh.vertexCount());
			OVITO_ASSERT(vertexBaseIndex == 0);
			mesh.addVertexProperty(std::move(property));
		}
		else {
			PropertyObject* existingProperty = property->type() != SurfaceMeshVertices::UserProperty 
				? mesh.mutableVertexProperty(static_cast<SurfaceMeshVertices::Type>(property->type())) 
				: mesh.mutableVertexProperty(property->name());
			if(existingProperty && existingProperty->dataType() == property->dataType() && existingProperty->componentCount() == property->componentCount()) {
				existingProperty->copyRangeFrom(*property, 0, vertexBaseIndex, property->size());
			}
		}
	}

	// Report number of vertices and faces to the user.
	if(meshIdentifier.isEmpty()) {
		state().setStatus(
			tr("Number of mesh vertices: %1\nNumber of mesh faces: %2")
			.arg(mesh.vertexCount())
			.arg(mesh.faceCount()));
	}
	else {
		state().setStatus(
			tr("Mesh %1: %2 vertices / %3 faces")
			.arg(meshIdentifier)
			.arg(mesh.vertexCount())
			.arg(mesh.faceCount()));
	}

	// Call base implementation.
	StandardFrameLoader::loadFile();
}

/******************************************************************************
* Reads a <DataArray> element and returns it as an OVITO property.
******************************************************************************/
PropertyPtr ParaViewVTPMeshImporter::FrameLoader::parseDataArray(QXmlStreamReader& xml, int convertToDataType)
{
	// Make sure it is really an <DataArray>.
	if(xml.name().compare(QStringLiteral("DataArray")) != 0) {
		xml.raiseError(tr("Expected <DataArray> element but found <%1> element.").arg(xml.name().toString()));
		return {};
	}

	// Check value of the 'format' attribute.
	QString format = xml.attributes().value("format").toString();
	bool isBinary;
	if(format.isEmpty()) {
		xml.raiseError(tr("Expected 'format' attribute in <%1> element.").arg(xml.name().toString()));
		return {};
	}
	else if(format == "binary") {
		isBinary = true;
	}
	else if(format == "ascii") {
		isBinary = false;
	}
	else {
		xml.raiseError(tr("Invalid value of 'format' attribute in <%1> element: %2").arg(xml.name().toString()).arg(format));
		return {};
	}

	// Parse number of array components.
	int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());

	// Parse array name.
	QString name = xml.attributes().value("Name").toString();

	// Determine data type of the target property to create.
	if(convertToDataType == 0) {
		// Use the 'type' attribute to decide which data type to use for the OVITO property array.
		QString dataType = xml.attributes().value("type").toString();
		if(dataType == "Float32" || dataType == "Float64") convertToDataType = PropertyObject::Float;
		else if(dataType == "Int32" || dataType == "UInt32" || dataType == "Int16" || dataType == "UInt16" || dataType == "Int8" || dataType == "UInt8") convertToDataType = PropertyObject::Int;
		else if(dataType == "Int64" || dataType == "UInt64") convertToDataType = PropertyObject::Int64;
		else convertToDataType = PropertyObject::Float;
	}

	// Create destination property. Initially with zero elements, will be resized later when the size of the VTK data array is known.
	PropertyPtr property = DataOORef<PropertyObject>::create(dataset(), 0, convertToDataType, numComponents, name);

	// Delegate parsing of payload to sub-routine.
	if(!parseVTKDataArray(property.get(), xml))
		return {};

	return property;
}

template<typename F>
static inline void tokenizeString(const QString& str, F&& f) 
{
	// Split string at whitespace characters.
	QStringView textView(str);
	auto start = textView.cbegin();
	auto eos = textView.cend();
	size_t valueCount = 0;
	while(start != eos) {
		// Skip whitespace characters.
		while(start != eos && start->isSpace())
			++start;
		// Find end of current token.
		auto end = start;			
		while(end != eos && !end->isSpace())
			++end;
		if(end != start) {
			// Process token.
			f(QStringView(&*start, std::distance(start, end)));
		}
		start = end;
	}
}

/******************************************************************************
* Reads a <DataArray> element and stores it in the given OVITO data buffer.
******************************************************************************/
bool ParaViewVTPMeshImporter::parseVTKDataArray(DataBuffer* buffer, QXmlStreamReader& xml, int vectorComponent, size_t destBaseIndex)
{
	// Make sure it is really an <DataArray>.
	if(xml.name().compare(QStringLiteral("DataArray")) != 0) {
		xml.raiseError(tr("Expected <DataArray> element but found <%1> element.").arg(xml.name().toString()));
		return false;
	}

	// Check value of the 'format' attribute.
	QString format = xml.attributes().value("format").toString();
	bool isBinary;
	if(format.isEmpty()) {
		xml.raiseError(tr("Expected 'format' attribute in <%1> element.").arg(xml.name().toString()));
		return false;
	}
	else if(format == "binary") {
		isBinary = true;
	}
	else if(format == "ascii") {
		isBinary = false;
	}
	else if(format == "appended") {
		xml.raiseError(tr("OVITO does not support <%1> elements using the 'appended' formats yet. Please contact the developers to request an extension of the file reader.").arg(xml.name().toString()));
		return false;
	}
	else {
		xml.raiseError(tr("Invalid value of 'format' attribute in <%1> element: %2").arg(xml.name().toString()).arg(format));
		return false;
	}

	// Check value of the 'type' attribute.
	QString dataType = xml.attributes().value("type").toString();
	size_t dataTypeSize;
	if(dataType == "Float32") {
		OVITO_STATIC_ASSERT(sizeof(float) == 4);
		dataTypeSize = sizeof(float);
	}
	else if(dataType == "Float64") {
		OVITO_STATIC_ASSERT(sizeof(double) == 8);
		dataTypeSize = sizeof(double);
	}
	else if(dataType == "Int32" || dataType == "UInt32") {
		OVITO_STATIC_ASSERT(sizeof(qint32) == 4);
		dataTypeSize = sizeof(qint32);
	}
	else if(dataType == "Int64" || dataType == "UInt64") {
		OVITO_STATIC_ASSERT(sizeof(qint64) == 8);
		dataTypeSize = sizeof(qint64);
	}
	else {
		xml.raiseError(tr("Parser supports only data arrays of type 'Int32', 'Int64', 'Float32' and 'Float64'. Please contact the OVITO developers to request an extension of the file parser."));
		return false;
	}

	// Number of VTK array components (tuple size).
	int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());

	// Parse the contents of the XML element and convert binary data from base64 encoding.
	QString text = xml.readElementText(QXmlStreamReader::SkipChildElements);

	size_t elementCount;
	QByteArray byteArray;
	std::vector<qint8> int8Array;
	std::vector<qint16> int16Array;
	std::vector<qint32> int32Array;
	std::vector<qint64> int64Array;
	std::vector<float> float32Array;
	std::vector<double> float64Array;
	const void* rawDataPtr;

	if(isBinary) {
		byteArray = QByteArray::fromBase64(text.toLatin1());

		// Note: Decoded binary data is prepended with array size information.
		qint64 dataArraySize = (byteArray.size() >= sizeof(qint64)) ? *reinterpret_cast<const qint64*>(byteArray.constData()) : -1;
		if(dataArraySize + sizeof(qint64) != byteArray.size()) {
			xml.raiseError(tr("Data array size mismatch: Expected %1 bytes of base64 encoded data, but XML element contains %2 bytes.")
				.arg(dataArraySize + sizeof(qint64))
				.arg(byteArray.size()));
			return false;
		}

		// Calculate the number of array elements from the size in bytes.
		elementCount = dataArraySize / (dataTypeSize * numComponents);
		if(elementCount * dataTypeSize * numComponents != dataArraySize) {
			xml.raiseError(tr("Data array size is invalid: Not an integer number of tuples."));
			return false;
		}

		rawDataPtr = byteArray.constData() + sizeof(qint64);
	}
	else {
		// Tokenize the XML element contents.
		size_t nvalues = 0;
		if(dataType == "Float32") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { float32Array.push_back(sv.toFloat()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { float32Array.push_back(c.toFloat(sv)); });
#endif
			rawDataPtr = float32Array.data();
			nvalues = float32Array.size();
		}
		else if(dataType == "Float64") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { float64Array.push_back(sv.toDouble()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { float64Array.push_back(c.toDouble(sv)); });
#endif
			rawDataPtr = float64Array.data();
			nvalues = float64Array.size();
		}
		else if(dataType == "Int32") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int32Array.push_back(sv.toInt()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int32Array.push_back(c.toInt(sv)); });
#endif
			rawDataPtr = int32Array.data();
			nvalues = int32Array.size();
		}
		else if(dataType == "UInt32") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int32Array.push_back(sv.toUInt()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int32Array.push_back(c.toUInt(sv)); });
#endif
			rawDataPtr = int32Array.data();
			nvalues = int32Array.size();
		}
		else if(dataType == "Int64") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int64Array.push_back(sv.toLongLong()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int64Array.push_back(c.toLongLong(sv)); });
#endif
			rawDataPtr = int64Array.data();
			nvalues = int64Array.size();
		}
		else if(dataType == "UInt64") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int64Array.push_back(sv.toULongLong()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int64Array.push_back(c.toULongLong(sv)); });
#endif
			rawDataPtr = int64Array.data();
			nvalues = int64Array.size();
		}
		else if(dataType == "Int16" || dataType == "UInt16") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int16Array.push_back(sv.toShort()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int16Array.push_back(c.toShort(sv)); });
#endif
			rawDataPtr = int16Array.data();
			nvalues = int16Array.size();
		}
		else if(dataType == "Int8" || dataType == "UInt8") {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
			tokenizeString(text, [&](QStringView sv) { int8Array.push_back(sv.toShort()); });
#else
			tokenizeString(text, [&,c=QLocale::c()](QStringView sv) { int8Array.push_back(c.toShort(sv)); });
#endif
			rawDataPtr = int8Array.data();
			nvalues = int8Array.size();
		}

		// Calculate the number of array elements from the size in bytes.
		elementCount = nvalues / numComponents;
		if(elementCount * numComponents != nvalues) {
			xml.raiseError(tr("Data array size is invalid: Not an integer number of tuples."));
			return false;
		}
	}

	// Check if VTK data array size fits to the size of the target buffer provided by the caller.
	if(buffer->size() != 0 && buffer->size() != elementCount + destBaseIndex) {
		xml.raiseError(tr("Data array size mismatch: Expected %1 data tuples, but <DataArray> element contains %2 tuples.").arg(buffer->size() - destBaseIndex).arg(elementCount));
		return false;
	}
	if(vectorComponent == -1) {
		if(buffer->componentCount() != numComponents) {
			xml.raiseError(tr("Data array size mismatch: Expected %1 components, but <DataArray> element contains %2 components.").arg(buffer->componentCount()).arg(numComponents));
			return false;
		}
	}
	else {
		if(numComponents != 1) {
			xml.raiseError(tr("Data array size mismatch: Expected 1 component, but <DataArray> element contains %1 components.").arg(numComponents));
			return false;
		}
	}

	// Allocate destination buffer (if not already done).
	if(buffer->size() == 0) {
		OVITO_ASSERT(destBaseIndex == 0);
		buffer->resize(elementCount, false);
	}

	// Verify parameters.
	if(destBaseIndex + elementCount > buffer->size()) {
		xml.raiseError(tr("Data array size mismatch: Number of elements in the <DataArray> exceeds expected range."));
		return false;
	}

	auto copyValuesToBuffer = [&](auto srcData) {
		const auto begin = srcData;
		const auto end = begin + elementCount * numComponents;
		if(buffer->dataType() == PropertyObject::Float) {
			if(vectorComponent == -1)
				std::copy(begin, end, std::next(DataBufferAccess<FloatType, true>(buffer).begin(), destBaseIndex * buffer->componentCount()));
			else
				std::copy(begin, end, std::next(std::begin(DataBufferAccess<FloatType, true>(buffer).componentRange(vectorComponent)), destBaseIndex));
		}
		else if(buffer->dataType() == PropertyObject::Int) {
			if(vectorComponent == -1)
				std::copy(begin, end, std::next(DataBufferAccess<int, true>(buffer).begin(), destBaseIndex * buffer->componentCount()));
			else
				std::copy(begin, end, std::next(std::begin(DataBufferAccess<int, true>(buffer).componentRange(vectorComponent)), destBaseIndex));
		}
		else if(buffer->dataType() == PropertyObject::Int64) {
			if(vectorComponent == -1)
				std::copy(begin, end, std::next(DataBufferAccess<qlonglong, true>(buffer).begin(), destBaseIndex * buffer->componentCount()));
			else
				std::copy(begin, end, std::next(std::begin(DataBufferAccess<qlonglong, true>(buffer).componentRange(vectorComponent)), destBaseIndex));
		}
		else OVITO_ASSERT(false);
	};

	if(dataType == "Float32") {
		copyValuesToBuffer(reinterpret_cast<const float*>(rawDataPtr));
	}
	else if(dataType == "Float64") {
		copyValuesToBuffer(reinterpret_cast<const double*>(rawDataPtr));
	}
	else if(dataType == "Int32") {
		copyValuesToBuffer(reinterpret_cast<const qint32*>(rawDataPtr));
	}
	else if(dataType == "UInt32") {
		copyValuesToBuffer(reinterpret_cast<const quint32*>(rawDataPtr));
	}
	else if(dataType == "Int64" || dataType == "UInt64") {
		copyValuesToBuffer(reinterpret_cast<const qint64*>(rawDataPtr));
	}
	else if(dataType == "Int16") {
		copyValuesToBuffer(reinterpret_cast<const qint16*>(rawDataPtr));
	}
	else if(dataType == "UInt16") {
		copyValuesToBuffer(reinterpret_cast<const quint16*>(rawDataPtr));
	}
	else if(dataType == "Int8") {
		copyValuesToBuffer(reinterpret_cast<const qint8*>(rawDataPtr));
	}
	else if(dataType == "UInt8") {
		copyValuesToBuffer(reinterpret_cast<const quint8*>(rawDataPtr));
	}
	else {
		OVITO_ASSERT(false);
		buffer->fillZero();
	}

	return true;
}

/******************************************************************************
* Is called once before the datasets referenced in a multi-block VTM file will be loaded.
******************************************************************************/
void MeshParaViewVTMFileFilter::preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets, FileSourceImporter::LoadOperationRequest& request, const ParaViewVTMImporter& vtmImporter)
{
	// Special handling of meshes that are grouped in the "Meshes" block of an Aspherix VTM file.
	// This is specific bheavior for VTM files written by the Aspherix code.
	if(vtmImporter.uniteMeshes()) {
		// Count the total number of mesh data files referenced in the "Meshes" sections of the VTM file.
		int numMeshFiles = boost::count_if(blockDatasets, [](const ParaViewVTMBlockInfo& block) {
			return block.blockPath.size() == 2 && block.blockPath[0] == QStringLiteral("Meshes");
		});

		// Special handling of legacy Aspherix files, which didn't have the "Meshes" group block.
		bool isLegacyAspherixFormat = false;
		if(numMeshFiles == 0) {
			for(const ParaViewVTMBlockInfo& block : blockDatasets) {
				// Verify that this VTM file was indeed written by Aspherix by looking for the mandatory "Particle" block.
				if(block.blockPath.size() == 1 && block.blockPath[0] == QStringLiteral("Particles"))
					isLegacyAspherixFormat = true;
				else if(block.blockPath.size() == 1 && !block.location.isEmpty() && block.location.fileName().endsWith(".vtp"))
					numMeshFiles++;
			}
		}

		// Make all mesh data files a part of the same block. This will tell the VTP mesh file reader 
		// to combine all mesh parts into a single SurfaceMesh object.
		int index = 0;
		for(ParaViewVTMBlockInfo& block : blockDatasets) {
			if((!isLegacyAspherixFormat && block.blockPath.size() == 2 && block.blockPath[0] == QStringLiteral("Meshes") && !block.location.isEmpty()) 
				|| (isLegacyAspherixFormat && block.blockPath.size() == 1 && block.blockPath[0] != QStringLiteral("Particles") && !block.location.isEmpty() && block.location.fileName().endsWith(".vtp"))) {
				block.pieceIndex = index++;
				block.pieceCount = numMeshFiles;
				// Discard original block identifier and give the united mesh a standard identifier.
				block.blockPath[isLegacyAspherixFormat ? 0 : 1] = QStringLiteral("combined");
			}
		}
		// Remove all other surface meshes from the data collection which might have been left over from a previous load operation.
		std::vector<const DataObject*> meshesToDiscard;
		for(const DataObject* obj : request.state.data()->objects()) {
			if(const SurfaceMesh* mesh = dynamic_object_cast<SurfaceMesh>(obj)) {
				if(mesh->identifier() != QStringLiteral("combined"))
					meshesToDiscard.push_back(mesh);
			}
		}
		for(const DataObject* obj : meshesToDiscard)
			request.state.mutableData()->removeObject(obj);
	}
	else {
		// When loading separate meshes, remove the combined mesh from the data collection, which might have been left over from a previous load operation.
		ConstDataObjectPath path = request.state.getObject<SurfaceMesh>(QStringLiteral("combined"));
		if(path.size() == 1)
			request.state.mutableData()->removeObject(path.back());
	}
}

}	// End of namespace
