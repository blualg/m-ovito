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
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#include "ParaViewPVDImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewPVDImporter);
OVITO_CLASSINFO(ParaViewPVDImporter, "DisplayName", "PVD");
DEFINE_REFERENCE_FIELD(ParaViewPVDImporter, childImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewPVDImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="Collection">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("Collection")) != 0)
        return false;

    return !xml.hasError();
}

/******************************************************************************
* Scans the input file for simulation timesteps.
******************************************************************************/
void ParaViewPVDImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    setProgressText(tr("Scanning file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open PVD file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // Parse the elements of the XML file.
    std::vector<std::pair<QUrl, QString>> blocks;
    while(xml.readNextStartElement()) {
        if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QStringLiteral("Collection")) != 0)
                xml.raiseError(tr("PVD file is not of type 'Collection'."));
        }
        else if(xml.name().compare(QStringLiteral("Collection")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QStringLiteral("DataSet")) == 0) {

            // Get value of 'file' attribute.
            QString file = xml.attributes().value("file").toString();
            if(!file.isEmpty()) {
                // Resolve file path.
                QUrl url = fileHandle().sourceUrl().resolved(QUrl(file));
                // Parse 'timestep' attribute.
                double timestep = xml.attributes().value("timestep").toDouble();

                Frame frame(std::move(url));
                frame.parserData = QVariant::fromValue(timestep);
                frame.label = tr("Timestep %1").arg(xml.attributes().value("timestep"));
                frames.push_back(std::move(frame));
            }

            xml.skipCurrentElement();
        }
        else {
            xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("PVD file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }
}

/******************************************************************************
* Loads the data for the given frame from the external file.
******************************************************************************/
Future<PipelineFlowState> ParaViewPVDImporter::loadFrame(const LoadOperationRequest& request)
{
    // Note: FileSourceImporter::loadFrame() may only be called from the main thread.
    OVITO_ASSERT(this_task::isMainThread());

    // Detect format of the referenced file and create an importer for it.
    OORef<FileImporter> importer = FileImporter::autodetectFileFormat(request.fileHandle, childImporter());

    // This works only for FileSourceImporters.
    // File formats handled by other kinds of importers will be skipped.
    OORef<FileSourceImporter> fsImporter = dynamic_object_cast<FileSourceImporter>(std::move(importer));
    if(!fsImporter)
        return request.state;

    // Fetch 'timestep' attribute from PVD file.
    double timestep = request.frame.parserData.value<double>();

    // Keep a reference to the child importer.
    _childImporter.set(this, PROPERTY_FIELD(childImporter), fsImporter);

    // Delegate file parsing to sub-importer.
    return fsImporter->loadFrame(request).then([timestep, pipelineNode = request.pipelineNode](PipelineFlowState state) {
        // Inject 'timestep' attribute from PVD file into the pipeline state.
        state.setAttribute(QStringLiteral("Timestep"), timestep, pipelineNode);
        return state;
    });
}

}   // End of namespace
