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
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/dataset/data/mesh/TriangleMeshVis.h>
#include "STLImporter.h"

#include <QtEndian>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(STLImporter);
OVITO_CLASSINFO(STLImporter, "DisplayName", "STL");

/******************************************************************************
* Returns whether this importer class supports importing data of the given type.
******************************************************************************/
bool STLImporter::OOMetaClass::importsDataType(const DataObject::OOMetaClass& dataObjectType) const
{
    return TriangleMesh::OOClass().isDerivedFrom(dataObjectType);
}

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool STLImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Require the STL filename ending.
    if(!file.sourceUrl().fileName().endsWith(QStringLiteral(".stl"), Qt::CaseInsensitive))
        return false;

    {
        // Open input file and check if it is an ascii STL file.
        CompressedTextReader stream(file);
        // Read first line. It should start with the word "solid".
        stream.readLine(256);
        if(stream.lineStartsWithToken("solid", true)) {
            // Read a couple of more lines until we find the first "facet normal" line, just to make sure.
            while(!stream.eof()) {
                const char* line = stream.readLineTrimLeft();
                if(stream.lineStartsWithToken("facet normal", true))
                    return true;
                if(line[0] != '\0')
                    return false;
            }
            return false;
        }
    }

    // Open input file again and check if it is a binary STL file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly))
        return false;

    // Skip STL header (80 bytes).
    device->skip(80);

    // Read number of triangle faces.
    quint32 nfaces = 0;
    device->read(reinterpret_cast<char*>(&nfaces), sizeof(nfaces));

    // Each STL face is 50 bytes. Verify that the file size fits to the number of faces specified in the file header.
    return (qint64)qFromLittleEndian(nfaces) * 50 == device->size() - device->pos();
}

/******************************************************************************
* Parses the given input file and stores the data in the given container object.
******************************************************************************/
void STLImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Reading STL file %1").arg(fileHandle().toString()));

    // Add mesh to the data collection.
    TriangleMesh* mesh = state().getMutableObject<TriangleMesh>();
    if(!mesh)
        mesh = state().createObject<TriangleMesh>(pipelineNode());
    else
        mesh->clear();
    mesh->setIdentifier(QStringLiteral("mesh"));

    // Open file for reading, assuming it is an ASCII STL file for now.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Read first line and check if it begins with the mandatory "solid" keyword.
    stream.readLine(1024);
    if(stream.lineStartsWithToken("solid", true)) {
        progress.setProgressMaximum(stream.underlyingSize());

        // Parse file line by line.
        int nVertices = -1;
        int vindices[3];
        while(!stream.eof()) {
            const char* line = stream.readLineTrimLeft();

            if(line[0] == '\0')
                continue;   // Skip empty lines.

            if(stream.lineStartsWithToken("facet normal", true) || stream.lineStartsWithToken("endfacet", true)) {
                // Ignore these lines.
            }
            else if(stream.lineStartsWithToken("outer loop", true)) {
                // Begin a new face.
                nVertices = 0;
            }
            else if(stream.lineStartsWithToken("vertex", true)) {
                if(nVertices == -1)
                    throw Exception(tr("Unexpected vertex specification in line %1 of STL file").arg(stream.lineNumber()));
                // Parse face vertex.
                Point3 xyz;
                if(sscanf(line, "vertex " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &xyz.x(), &xyz.y(), &xyz.z()) != 3)
                    throw Exception(tr("Invalid vertex specification in line %1 of STL file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                vindices[std::min(nVertices,2)] = mesh->addVertex(xyz);
                nVertices++;
                // Emit a new face to triangulate the polygon.
                if(nVertices >= 3) {
                    TriMeshFace& f = mesh->addFace();
                    f.setVertices(vindices[0], vindices[1], vindices[2]);
                    if(nVertices == 3)
                        f.setEdgeVisibility(true, true, false);
                    else
                        f.setEdgeVisibility(false, true, false);
                    vindices[1] = vindices[2];
                }
            }
            else if(stream.lineStartsWithToken("endloop", true)) {
                // Close the current face.
                if(nVertices >= 3)
                    mesh->faces().back().setEdgeVisible(2);
                nVertices = -1;
            }
            else if(stream.lineStartsWithToken("endsolid", true)) {
                break;  // End of file.
            }
            else {
                throw Exception(tr("Unknown keyword encountered in line %1 of STL file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            }

            // Update progress bar and check for user cancellation.
            progress.setProgressValueIntermittent(stream.underlyingByteOffset());
        }
    }
    else {
        // Since the file did not start with the keyword "solid", let's assume it's a binary STL file.

        // Open input file again and check if it is a binary STL file.
        std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
        if(!device->open(QIODevice::ReadOnly))
            throw Exception(tr("Failed to open binary STL file: %1.").arg(device->errorString()));

        // Skip STL header (80 bytes).
        if(device->skip(80) != 80)
            throw Exception(tr("Failed to read binary STL file header. Unexpected end of file."));

        // Read number of triangle faces.
        quint32 nfaces = qToLittleEndian(std::numeric_limits<quint32>::max());
        device->read(reinterpret_cast<char*>(&nfaces), sizeof(nfaces));
        nfaces = qFromLittleEndian(nfaces);
        if(nfaces >= 10000000)
            throw Exception(tr("Binary STL file header indicates invalid number of faces: %1").arg(nfaces));

        progress.setProgressMaximum(nfaces);
        for(quint32 i = 0; i < nfaces; i++) {

            // Update progress bar and check for user cancellation.
            progress.setProgressValueIntermittent(i);

            Vector_3<float> normal;
            Point_3<float> coordinates[3];
            quint16 attr;
            qint64 nread1 = device->read(reinterpret_cast<char*>(&normal), sizeof(normal));
            qint64 nread2 = device->read(reinterpret_cast<char*>(coordinates), sizeof(coordinates));
            qint64 nread3 = device->read(reinterpret_cast<char*>(&attr), sizeof(attr));

            if(nread1 != sizeof(normal) || nread2 != sizeof(coordinates) || nread3 != sizeof(attr))
                throw Exception(tr("Failed to read binary STL file. Unexpected end of file or I/O error."));

            int vindices[3];
            vindices[0] = mesh->addVertex(coordinates[0].toDataType<FloatType>());
            vindices[1] = mesh->addVertex(coordinates[1].toDataType<FloatType>());
            vindices[2] = mesh->addVertex(coordinates[2].toDataType<FloatType>());
            TriMeshFace& f = mesh->addFace();
            f.setVertices(vindices[0], vindices[1], vindices[2]);
        }
    }

    // STL files do not use shared vertices.
    // Try to unite identical vertices now.
    mesh->removeDuplicateVertices(1e-8 * mesh->boundingBox().size().length());
    mesh->determineEdgeVisibility();

    // Show some stats to the user.
    state().setStatus(tr("%1 vertices, %2 triangles").arg(mesh->vertexCount()).arg(mesh->faceCount()));
}

}   // End of namespace
