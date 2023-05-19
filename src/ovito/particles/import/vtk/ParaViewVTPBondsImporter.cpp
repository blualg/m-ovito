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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "ParaViewVTPBondsImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParaViewVTPBondsImporter);
IMPLEMENT_OVITO_CLASS(BondsParaViewVTMFileFilter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTPBondsImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
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
    if(xml.name().compare(QLatin1String("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QLatin1String("PolyData")) != 0)
        return false;

    // Continue until we reach the <Piece> element.
    while(xml.readNextStartElement()) {
        if(xml.name().compare(QLatin1String("Piece")) == 0) {
            // Number of vertices, triangle strips, and polygons must be zero.
            if(xml.attributes().value("NumberOfVerts").toULongLong() == 0 && xml.attributes().value("NumberOfStrips").toULongLong() == 0 && xml.attributes().value("NumberOfPolys").toULongLong() == 0) {
                // Number of lines must match to the number of points.
                // Either there are two points per line (particle center to particle center) or
                // three points per line (center - contact point - center).
                qulonglong numPoints = xml.attributes().value("NumberOfPoints").toULongLong();
                qulonglong numLines = xml.attributes().value("NumberOfLines").toULongLong();
                if(numPoints != 2 * numLines && numPoints != 3 * numLines)
                    return false;

                // Check if the cell attributes "id1" and "id2" are defined.
                bool foundId1 = false;
                bool foundId2 = false;
                while(xml.readNextStartElement()) {
                    if(xml.name().compare(QLatin1String("CellData")) == 0) {
                        while(xml.readNextStartElement()) {
                            if(xml.name().compare(QLatin1String("DataArray")) == 0) {
                                if(xml.attributes().value("Name").compare(QLatin1String("id1"), Qt::CaseInsensitive) == 0)
                                    foundId1 = true;
                                if(xml.attributes().value("Name").compare(QLatin1String("id2"), Qt::CaseInsensitive) == 0)
                                    foundId2 = true;
                            }
                            xml.skipCurrentElement();
                        }
                    }
                    xml.skipCurrentElement();
                }
                return !xml.hasError() && foundId1 && foundId2;
            }
            break;
        }
    }

    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void ParaViewVTPBondsImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading ParaView VTP contact network file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTP file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // Append bonds to existing bonds object when requested by the caller.
    // This may be the case when loading a multi-block dataset specified in a VTM file.
    size_t baseBondIndex = 0;
    bool preserveExistingData = false;
    if(loadRequest().appendData) {
        baseBondIndex = bonds()->elementCount();
        preserveExistingData = (baseBondIndex != 0);
    }
    setKeepExistingTopology(true);

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        if(isCanceled())
            return;

        if(xml.name().compare(QLatin1String("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QLatin1String("PolyData")) != 0)
                xml.raiseError(tr("VTK file is not of type PolyData."));
            else if(xml.attributes().value("byte_order").compare(QLatin1String("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please contact the OVITO developers to request an extension of the file parser."));
            else if(xml.attributes().value("compressor").compare(QLatin1String("")) != 0)
                xml.raiseError(tr("The parser does not support compressed data arrays. Please contact the OVITO developers to request an extension of the file parser."));
        }
        else if(xml.name().compare(QLatin1String("PolyData")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QLatin1String("Piece")) == 0) {

            // Parse number of vertices, triangle strips and polygons.
            if(xml.attributes().value("NumberOfVerts").toULongLong() != 0
                    || xml.attributes().value("NumberOfStrips").toULongLong() != 0
                    || xml.attributes().value("NumberOfPolys").toULongLong() != 0) {
                xml.raiseError(tr("Number of vertices, strips and polys are nonzero. This file doesn't seem to contain an Aspherix contact network."));
                break;
            }

            // Parse number of points.
            size_t numPoints = xml.attributes().value("NumberOfPoints").toULongLong();
            // Parse number of lines.
            size_t numLines = xml.attributes().value("NumberOfLines").toULongLong();
            if(numPoints != 2 * numLines && numPoints != 3 * numLines) {
                xml.raiseError(tr("Number of lines does not match to the number of points in the contact network."));
                break;
            }
            OVITO_ASSERT(baseBondIndex + numLines != 0); // Calling setBondCount(0) discards an existing BondsObject. We never want that to happen!
            setBondCount(baseBondIndex + numLines);
        }
        else if(xml.name().compare(QLatin1String("CellData")) == 0) {
            // Parse child elements.
            while(xml.readNextStartElement() && !isCanceled()) {
                if(xml.name().compare(QLatin1String("DataArray")) == 0) {
                    int vectorComponent = -1;
                    if(PropertyObject* property = createBondPropertyForDataArray(xml, vectorComponent, preserveExistingData)) {
                        if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, xml, vectorComponent, baseBondIndex))
                            break;
                        if(xml.hasError() || isCanceled())
                            break;
                    }
                    if(xml.tokenType() != QXmlStreamReader::EndElement)
                        xml.skipCurrentElement();
                }
                else {
                    xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
                }
            }
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QLatin1String("PointData")) == 0 || xml.name().compare(QLatin1String("Points")) == 0 || xml.name().compare(QLatin1String("Lines")) == 0 || xml.name().compare(QLatin1String("Verts")) == 0 || xml.name().compare(QLatin1String("Strips")) == 0 || xml.name().compare(QLatin1String("Polys")) == 0) {
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

    // Change title of the bonds visual element. But only do it the very first time the bonds object is created.
    if(areBondsNewlyCreated() && bonds()->visElement()) {
        bonds()->visElement()->setTitle(tr("Particle-particle contacts"));
        bonds()->visElement()->setEnabled(false);
        // Take a snapshot of the object's parameter values, which serves as reference to detect future changes made by the user.
        bonds()->visElement()->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(ActiveObject::title)});
    }

    // Report number of bonds to the user.
    QString statusString = tr("Particle-particle contacts: %1").arg(bonds()->elementCount());
    state().setStatus(std::move(statusString));

    // Call base implementation to finalize the loaded bond data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Creates the right kind of OVITO property object that will receive the data
* read from a <DataArray> element.
******************************************************************************/
PropertyObject* ParaViewVTPBondsImporter::FrameLoader::createBondPropertyForDataArray(QXmlStreamReader& xml, int& vectorComponent, bool preserveExistingData)
{
    int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());
    auto name = xml.attributes().value("Name");

    if(name.compare(QLatin1String("id1"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
        vectorComponent = 0;
        return bonds()->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, BondsObject::ParticleIdentifiersProperty);
    }
    else if(name.compare(QLatin1String("id2"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
        vectorComponent = 1;
        return bonds()->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, BondsObject::ParticleIdentifiersProperty);
    }
    else {
        return bonds()->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, name.toString(), PropertyObject::FloatDefault, numComponents);
    }
    return nullptr;
}

/******************************************************************************
* Is called after all datasets referenced in a multi-block VTM file have been loaded.
******************************************************************************/
void BondsParaViewVTMFileFilter::postprocessDatasets(FileSourceImporter::LoadOperationRequest& request)
{
    ParticlesObject* particles = request.state.getMutableObject<ParticlesObject>();
    if(!particles || !particles->bonds())
        return;

    if(const PropertyObject* bondParticleIdentifiers = particles->bonds()->getProperty(BondsObject::ParticleIdentifiersProperty)) {

        // Build map from particle identifiers to particle indices.
        std::map<int64_t, size_t> idToIndexMap;
        if(ConstBufferAccess<int64_t> particleIdentifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty)) {
            size_t index = 0;
            for(int64_t id : particleIdentifierProperty) {
                if(idToIndexMap.insert(std::make_pair(id, index++)).second == false)
                    throw Exception(tr("Duplicate particle identifier %1 detected. Please make sure particle identifiers are unique.").arg(id));
            }
        }
        else {
            // Generate implicit IDs if the "Particle Identifier" property is not defined.
            for(size_t i = 0; i < particles->elementCount(); i++)
                idToIndexMap[i+1] = i;
        }

        // Perform lookup of particle IDs.
        BufferAccess<ParticleIndexPair> bondTopologyArray = particles->makeBondsMutable()->createProperty(BondsObject::TopologyProperty);
        auto t = bondTopologyArray.begin();
        for(const ParticleIndexPair& bond : ConstBufferAccess<ParticleIndexPair>(bondParticleIdentifiers)) {
            auto iter1 = idToIndexMap.find(bond[0]);
            auto iter2 = idToIndexMap.find(bond[1]);
            if(iter1 == idToIndexMap.end())
                throw Exception(tr("Particle id %1 referenced by pair contact #%2 does not exist.").arg(bond[0]).arg(std::distance(bondTopologyArray.begin(), t)));
            if(iter2 == idToIndexMap.end())
                throw Exception(tr("Particle id %1 referenced by pair contact #%2 does not exist.").arg(bond[1]).arg(std::distance(bondTopologyArray.begin(), t)));
            (*t)[0] = iter1->second;
            (*t)[1] = iter2->second;
            ++t;
        }

        // Remove the "Particle Identifiers" property from bonds again, because it is no longer needed.
        particles->makeBondsMutable()->removeProperty(bondParticleIdentifiers);
    }
}

}   // End of namespace
