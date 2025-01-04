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

#include <ovito/mesh/Mesh.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/utilities/concurrent/Reduce.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ParaViewVTMImporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParaViewVTMFileFilter);
IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTMImporter);
OVITO_CLASSINFO(ParaViewVTMImporter, "DisplayName", "VTM");
DEFINE_PROPERTY_FIELD(ParaViewVTMImporter, uniteMeshes);
SET_PROPERTY_FIELD_LABEL(ParaViewVTMImporter, uniteMeshes, "Unite all meshes");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTMImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="vtkMultiBlockDataSet">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("vtkMultiBlockDataSet")) != 0)
        return false;

    return !xml.hasError();
}

/******************************************************************************
* Parses the given VTM file and returns the list of referenced data files.
******************************************************************************/
std::vector<ParaViewVTMBlockInfo> ParaViewVTMImporter::loadVTMFile(const FileHandle& fileHandle)
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTM file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // The list of <DataSet> elements found in the file.
    std::vector<ParaViewVTMBlockInfo> datasetList;
    // The current branch in the block hierarchy.
    QStringList blockBranch;
    // Indicates that we are currently inside a <piece> element.
    bool isPiece = false;

    // Parse the elements of the XML file.
    while(!xml.atEnd()) {
        while(xml.readNextStartElement()) {
            if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
                if(xml.attributes().value("type").compare(QStringLiteral("vtkMultiBlockDataSet")) != 0)
                    xml.raiseError(tr("VTM file is not of type vtkMultiBlockDataSet."));
            }
            else if(xml.name().compare(QStringLiteral("vtkMultiBlockDataSet")) == 0) {
                // Do nothing. Parse child elements.
            }
            else if(xml.name().compare(QStringLiteral("Block")) == 0) {
                // Get value of 'name' attribute.
                blockBranch.push_back(xml.attributes().value("name").toString());

                // Continue by parsing child elements.
            }
            else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
                // This is the start of a new piece.
                OVITO_ASSERT(!isPiece);
                isPiece = true;

                // Get value of 'name' attribute.
                blockBranch.push_back(xml.attributes().value("name").toString());

                // Continue by parsing child elements.
            }
            else if(xml.name().compare(QStringLiteral("DataSet")) == 0) {

                // The current path in the block hierarchy:
                ParaViewVTMBlockInfo blockInfo;
                blockInfo.blockPath = blockBranch;

                if(!isPiece) {
                    // Add dataset leaf name to block path.
                    auto name = xml.attributes().value("name");
                    if(!name.isEmpty())
                        blockInfo.blockPath.append(name.toString());
                }
                else {
                    // Parse piece index.
                    blockInfo.pieceIndex = xml.attributes().value("index").toInt();
                }

                // Parse value of 'file' attribute if present.
                QString file = xml.attributes().value("file").toString();
                // Aspherix fix: The Windows version may include backslashes in the file path.
                file.replace('\\', '/');
                // Resolve file path and record the URL, which will be loaded later.
                if(!file.isEmpty())
                    blockInfo.location = fileHandle.sourceUrl().resolved(QUrl(file));

                datasetList.push_back(std::move(blockInfo));

                xml.skipCurrentElement();
            }
            else {
                xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
            }
        }
        if(xml.tokenType() == QXmlStreamReader::EndElement) {
            if(xml.name().compare(QStringLiteral("Block")) == 0) {
                blockBranch.pop_back();
            }
            else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
                OVITO_ASSERT(isPiece);
                // Determine the range of blocks that are part of the current piece-wise dataset.
                // Also count the number of block pieces that are not empty.
                int pieceCount = 0;
                auto iter = std::find_if(datasetList.rbegin(), datasetList.rend(), [&](const ParaViewVTMBlockInfo& block) {
                    if(block.blockPath != blockBranch)
                        return true;
                    if(!block.location.isEmpty())
                        pieceCount++;
                    return false;
                });
                OVITO_ASSERT(pieceCount <= datasetList.size());
                // Update the pieceCount field of all partial blocks belonging to the current piece-wise dataset.
                int pieceIndex = pieceCount;
                std::for_each(datasetList.rbegin(), iter, [&](ParaViewVTMBlockInfo& block) {
                    block.pieceCount = pieceCount;
                    if(!block.location.isEmpty())
                        block.pieceIndex = --pieceIndex;
                    else
                        block.pieceIndex = -1;
                });
                OVITO_ASSERT(pieceIndex == 0);
                blockBranch.pop_back();
                isPiece = false;
            }
            else if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
                break;
            }
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("VTM file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }

    return datasetList;
}

/******************************************************************************
* Loads the data for the given frame from the external file.
******************************************************************************/
Future<PipelineFlowState> ParaViewVTMImporter::loadFrame(const LoadOperationRequest& request)
{
    OVITO_ASSERT(!isUndoRecording());

    struct ExtendedLoadRequest : public LoadOperationRequest {
        /// Constructor.
        ExtendedLoadRequest(const LoadOperationRequest& other) : LoadOperationRequest(other) {}
        /// The current dataset being loaded from the multi-block structure.
        ParaViewVTMBlockInfo blockInfo;
        /// Plugin filters processing the datasets referenced by the VTM file.
        std::vector<OORef<ParaViewVTMFileFilter>> filters;
    };

    // Copy the pipeline request into an extended data structure, which can be modified.
    ExtendedLoadRequest modifiedRequest(request);

    // Load the VTM file, which contains the list of referenced data files.
    std::vector<ParaViewVTMBlockInfo> blockDatasets = loadVTMFile(request.fileHandle);

    // Create filter objects.
    static const std::vector<OvitoClassPtr> filterClassList = PluginManager::instance().listClasses(ParaViewVTMFileFilter::OOClass());
    for(OvitoClassPtr clazz : filterClassList) {
        modifiedRequest.filters.push_back(static_object_cast<ParaViewVTMFileFilter>(clazz->createInstance()));

        // Let the plugin filter objects preprocess the multi-block structure before the referenced data files get loaded.
        modifiedRequest.filters.back()->preprocessDatasets(blockDatasets, modifiedRequest, *this);
    }

    // Load each dataset referenced by the VTM file.
    Future<ExtendedLoadRequest> future = reduce_sequential(
        std::move(modifiedRequest), std::move(blockDatasets), DeferredObjectExecutor(request.pipelineNode),
        [](const ParaViewVTMBlockInfo& blockInfo, ExtendedLoadRequest& request) {
            // We can skip empty datasets which are not associated with a VTK file.
            if(blockInfo.location.isEmpty()) return Future<void>::createImmediateEmpty();

            // Set up the load request submitted to the FileSourceImporter.
            request.dataBlockPrefix = blockInfo.blockPath.back();
            request.blockInfo = blockInfo;
            request.appendData =
                (blockInfo.pieceIndex >
                 0);  // Append data (instead of replacing it) when loading subsequent partial blocks of a piece-wise (parallel) dataset.

            OORef<const PipelineNode> fileSource = request.pipelineNode.lock();
            if(!fileSource) throw Exception(QStringLiteral("Object requesting the data import has been deleted."));

            // Retrieve the data file, then detect its format.
            // Note: FileImporter::autodetectFileFormat() may only be called from the main thread.
            return Application::instance()->fileManager()
                .fetchUrl(blockInfo.location)
                .then(ObjectExecutor(fileSource), [&request](const SharedFuture<FileHandle>& fileFuture) mutable -> Future<void> {
                    OVITO_ASSERT(this_task::get());

                    try {
                        // Obtain a handle to the referenced data file.
                        const FileHandle& file = fileFuture.result();

                        // Give plugin filter objects the possibility to override the loading of the data file.
                        for(const auto& filter : request.filters) {
                            if(Future<void> future = filter->loadDataset(request.blockInfo, file, request))
                                return future;
                        }
                        // If none of the filter objects decided to handle the loading process, fall back to our standard procedure,
                        // which consists of detecting the file's format and delegating the file parsing to the corresponding
                        // FileSourceImporter class.

                        // Detect file format and create an importer for it.
                        // This currently works only for FileSourceImporters. Files handled by other kinds of importers will be skipped.
                        // VTK dataset blocks using a file format not supported by OVITO are silently ignored.
                        OORef<FileSourceImporter> importer = dynamic_object_cast<FileSourceImporter>(FileImporter::autodetectFileFormat(file));
                        if(!importer)
                            return Future<void>::createImmediateEmpty();

                        // Remember the current status returned by the loading operations completed so far.
                        // We will prepend this existing status text to the one generated by the current file importer.
                        PipelineStatus lastStatus = request.state.status();

                        // Set up the load request submitted to the FileSourceImporter.
                        request.frame = Frame(file);
                        request.fileHandle = file;
                        request.state.setStatus(PipelineStatus::Success);

                        // Give plugin filter objects the possibility to pass additional information to the specific FileSourceImporter.
                        for(const auto& filter : request.filters) filter->configureImporter(request.blockInfo, request, importer);

                        // Parse the referenced file.
                        // Note: We need to keep the FileSourceImporter object while the asynchronous parsing process is
                        // in progress. That's why we store an otherwise unused pointer to it in the lambda function.
                        return importer->loadFrame(request).then([importer, filename = file.sourceUrl().fileName(), &request,
                                                                  lastStatus](Future<PipelineFlowState> blockDataFuture) mutable {
                            try {
                                request.state = blockDataFuture.result();

                                // Concatenate status strings for the data blocks loaded so far.
                                QString statusString = lastStatus.text();
                                if(!request.state.status().text().isEmpty()) {
                                    // Append only the status text of the last partial dataset when loading a partial dataset.
                                    if(request.blockInfo.pieceIndex < 0 ||
                                       request.blockInfo.pieceIndex == request.blockInfo.pieceCount - 1) {
                                        if(!statusString.isEmpty() && !statusString.endsWith(QChar('\n'))) statusString += QChar('\n');
                                        statusString += request.state.status().text();
                                    }
                                }

                                // Also calculate a combined status code.
                                PipelineStatus::StatusType statusType = lastStatus.type();
                                if(statusType == PipelineStatus::Success ||
                                   (statusType == PipelineStatus::Warning && request.state.status().type() == PipelineStatus::Error))
                                    statusType = request.state.status().type();
                                request.state.setStatus(PipelineStatus(statusType, std::move(statusString)));
                            }
                            catch(Exception& ex) {
                                throw ex.prependGeneralMessage(
                                    tr("Failed to load VTK multi-block dataset '%1': %2").arg(request.dataBlockPrefix).arg(filename));
                            }
                        });
                    }
                    catch(Exception& ex) {
                        // Handle file errors, e.g. if the data block file referenced in the VTM file does not exist.
                        request.state.setStatus(PipelineStatus(ex, QStringLiteral(" ")));
                        ex.prependGeneralMessage(tr("Failed to access data file referenced by block '%1' in VTK multi-block file.")
                                                     .arg(request.dataBlockPrefix));
                        this_task::ui()->reportError(ex);
                        // We treat such an error as recoverable and continue with loading the remaining data blocks.
                        return Future<void>::createImmediateEmpty();
                    }
                });
        });

    return future.then([](ExtendedLoadRequest&& request) -> PipelineFlowState {

        // Let the plugin filter objects post-process the loaded data.
        for(const auto& filter : request.filters)
            filter->postprocessDatasets(request);

        // Return just the PipelineFlowState to the caller.
        return std::move(request.state);
    });
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ParaViewVTMImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
    FileSourceImporter::propertyChanged(field);

    if(field == PROPERTY_FIELD(uniteMeshes) && !isBeingLoaded()) {
        // Reload input file(s) when this option is changed by the user.
        // There is no need to refetch the data file(s) from the remote location though. Reparsing the cached files is sufficient.
        requestReload();
    }
}

}   // End of namespace
