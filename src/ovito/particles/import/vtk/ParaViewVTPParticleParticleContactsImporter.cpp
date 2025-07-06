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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/lines/LinesVis.h>
#include "ParaViewVTPParticleParticleContactsImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTPParticleParticleContactsImporter);
OVITO_CLASSINFO(ParaViewVTPParticleParticleContactsImporter, "DisplayName", "VTP");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTPParticleParticleContactsImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
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
                // Number of lines must match with the number of points.
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
void ParaViewVTPParticleParticleContactsImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading ParaView VTP particle-particle contact network file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTP file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // Create the destination lines object.
    QString linesIdentifier = "particle-particle-contacts";
    Lines* lines = state().getMutableLeafObject<Lines>(Lines::OOClass(), linesIdentifier);
    if(!lines) {
        lines = state().createObject<Lines>(pipelineNode());
        lines->setIdentifier(linesIdentifier);
        lines->setTitle(tr("Particle-particle contacts"));
        lines->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(PropertyContainer::title)});
        LinesVis* vis = lines->visElement<LinesVis>();
        if(vis) {
            vis->setTitle(tr("Particle-particle contacts"));
            vis->setEnabled(false);
            // Take a snapshot of the object's parameter values, which serves as reference to detect future changes made by the user.
            vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(ActiveObject::title)});
        }
    }

    // Append lines to existing container object when requested by the caller.
    // This may be the case when loading a multi-block dataset specified in a VTM file.
    size_t baseLineIndex = 0;
    bool preserveExistingData = false;
    if(loadRequest().appendData) {
        baseLineIndex = lines->elementCount();
        preserveExistingData = (baseLineIndex != 0);
    }
    DataBuffer::BufferInitialization propertyAccessMode = preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized;
    size_t numLines = 0;
    size_t pointsPerLine = 0;
    int vtkHeaderType = 8; // Assume UInt64 by default

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        this_task::throwIfCanceled();

        if(xml.name().compare(QLatin1String("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QLatin1String("PolyData")) != 0)
                xml.raiseError(tr("VTK file is not of type PolyData."));
            else if(xml.attributes().value("byte_order").compare(QLatin1String("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please contact the OVITO developers to request an extension of the file parser."));
            else if(xml.attributes().value("compressor").compare(QLatin1String("")) != 0)
                xml.raiseError(tr("The parser does not support compressed data arrays. Please contact the OVITO developers to request an extension of the file parser."));
            if(xml.attributes().value("header_type").compare(QStringLiteral("UInt32")) == 0)
                vtkHeaderType = 4;
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
            numLines = xml.attributes().value("NumberOfLines").toULongLong();
            // Number of lines must match with the number of points.
            // Either there are two points per line (particle center to particle center) or
            // three points per line (center - contact point - center).
            if(numPoints != 2 * numLines && numPoints != 3 * numLines) {
                xml.raiseError(tr("Number of lines does not match with the number of points in the contact network."));
                break;
            }
            pointsPerLine = numPoints / numLines;
            lines->setElementCount(baseLineIndex + numPoints);
        }
        else if(xml.name().compare(QLatin1String("CellData")) == 0) {
            // Parse child elements.
            while(xml.readNextStartElement() && !this_task::isCanceled()) {
                if(xml.name().compare(QLatin1String("DataArray")) == 0) {
                    int vectorComponent = -1;
                    if(Property* property = createLinesPropertyForDataArray(xml, vectorComponent, lines, propertyAccessMode)) {
                        if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, vtkHeaderType, xml, vectorComponent, baseLineIndex, pointsPerLine))
                            break;
                        if(xml.hasError() || this_task::isCanceled())
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
        else if(xml.name().compare(QStringLiteral("Points")) == 0) {
            // Parse child <DataArray> element containing the point coordinates.
            if(!xml.readNextStartElement())
                break;

            int vectorComponent = -1;
            if(Property* property = createLinesPropertyForDataArray(xml, vectorComponent, lines, propertyAccessMode)) {
                if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, vtkHeaderType, xml, vectorComponent, baseLineIndex))
                    break;
                if(xml.hasError() || this_task::isCanceled())
                    break;
            }
            xml.skipCurrentElement();
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QLatin1String("PointData")) == 0 || xml.name().compare(QLatin1String("Lines")) == 0 || xml.name().compare(QLatin1String("Verts")) == 0 || xml.name().compare(QLatin1String("Strips")) == 0 || xml.name().compare(QLatin1String("Polys")) == 0) {
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
    if(this_task::isCanceled() || pointsPerLine == 0)
        return;

    // Create section property to mark connected pairs or triplets of points.
    Property* sectionProperty = lines->createProperty(propertyAccessMode, Lines::SectionProperty);
    BufferWriteAccess<int64_t, access_mode::write> sectionAccess(sectionProperty, propertyAccessMode);
    auto sectionIter = sectionAccess.begin() + baseLineIndex;
    for(size_t i = 0; i < numLines; ++i) {
        for(size_t j = 0; j < pointsPerLine; ++j)
            *sectionIter++ = i;
    }

    // Compute magnitudes for some vector properties.
    for(const QString& propertyName : { QStringLiteral("Force"), QStringLiteral("Force Normal"), QStringLiteral("Force Tangential"), QStringLiteral("Velocity 1"), QStringLiteral("Velocity 2") }) {
        if(const Property* vectorProperty = lines->getProperty(propertyName)) {
            this_task::throwIfCanceled();
            if(vectorProperty->dataType() == DataBuffer::FloatDefault && vectorProperty->componentCount() == 3) {
                Property* magnitudeProperty = lines->createProperty(propertyAccessMode, vectorProperty->name() + QStringLiteral(" Magnitude"), Property::FloatDefault, 1);
                BufferReadAccess<Vector3> vectorAccess{vectorProperty};
                BufferWriteAccess<FloatType, access_mode::write> magnitudeAccess(magnitudeProperty, propertyAccessMode);
                auto v = vectorAccess.cbegin() + baseLineIndex;
                for(FloatType& mag : boost::make_iterator_range(magnitudeAccess.begin() + baseLineIndex, magnitudeAccess.end())) {
                    mag = (v++)->length();
                }
                OVITO_ASSERT(v == vectorAccess.cend());
            }
        }
    }

    // Report number of contacts to the user.
    QString statusString = tr("Particle-particle contacts: %1").arg(lines->elementCount() / pointsPerLine);
    state().setStatus(std::move(statusString));
}

/******************************************************************************
* Creates the right kind of OVITO property object that will receive the data
* read from a <DataArray> element.
******************************************************************************/
Property* ParaViewVTPParticleParticleContactsImporter::FrameLoader::createLinesPropertyForDataArray(QXmlStreamReader& xml, int& vectorComponent, Lines* lines, DataBuffer::BufferInitialization propertyAccessMode)
{
    int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());
    auto name = xml.attributes().value("Name");

    // Parse optional list of vector component names.
    QStringList componentNames;
    for(int c = 0; c < numComponents; ++c) {
        QString componentName = xml.attributes().value(QStringLiteral("ComponentName%1").arg(c)).toString();
        if(componentName.isEmpty()) {
            componentNames.clear();
            break;
        }
        componentNames.push_back(Property::makeComponentNameValid(componentName));
    }

    if(name.compare(QLatin1String("Points"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, Lines::PositionProperty);
    }
    else if(name.compare(QLatin1String("force"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Force"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("force_normal"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Force Normal"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("force_tangential"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Force Tangential"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("vel1"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Velocity 1"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("vel2"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Velocity 2"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("torque"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Torque"), Property::FloatDefault, numComponents, QStringList() << "X" << "Y" << "Z");
    }
    else if(name.compare(QLatin1String("particle_history"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Particle History"), Property::FloatDefault, numComponents, std::move(componentNames));
    }
    else if(name.compare(QLatin1String("id1"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Particle Identifier 1"), Property::IntIdentifier, numComponents);
    }
    else if(name.compare(QLatin1String("id2"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
        return lines->createProperty(propertyAccessMode, QStringLiteral("Particle Identifier 2"), Property::IntIdentifier, numComponents);
    }
    else {
        return lines->createProperty(propertyAccessMode, Property::makePropertyNameValid(name.toString()), Property::FloatDefault, numComponents, std::move(componentNames));
    }
}

}   // End of namespace
