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

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "AttributeFileExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AttributeFileExporter);
DEFINE_PROPERTY_FIELD(AttributeFileExporter, attributesToExport);

/******************************************************************************
* Constructor
*****************************************************************************/
void AttributeFileExporter::initializeObject(ObjectInitializationFlags flags)
{
    FileExporter::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject) && this_task::isInteractive()) {
        // This exporter is typically used to export attributes as functions of time.
        if(AnimationSettings* anim = this_task::ui()->datasetContainer().activeAnimationSettings()) {
            if(!anim->isSingleFrame())
                setExportTrajectory(true);
        }

#ifndef OVITO_DISABLE_QSETTINGS
        // Restore last output column mapping.
        QSettings settings;
        settings.beginGroup("exporter/attributes/");
        setAttributesToExport(settings.value("attrlist", QVariant::fromValue(QStringList())).toStringList());
        settings.endGroup();
#endif
    }
}

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> AttributeFileExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Constructor.
        void initializeObject(const AttributeFileExporter* exporter, const QString& filePath) {
            FileExportJob::initializeObject(exporter, filePath, true);

            // Write file header.
            textStream() << "#";
            for(const QString& attrName : exporter->attributesToExport()) {
                textStream() << " \"" << attrName << "\"";
            }
            textStream() << "\n";
        }

        /// Produces the data to be exported for a trajectory frame.
        virtual SCFuture<any_moveonly> getExportableFrameData(int frameNumber, TaskProgress& progress) override {
            co_return co_await static_cast<const AttributeFileExporter*>(exporter())->getAttributesMap(frameNumber);
        }

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(any_moveonly&& frameData, int frameNumber, const QString& filePath, TaskProgress& progress) override {
            // The exportable frame data.
            QVariantMap attrMap = any_cast<QVariantMap>(std::move(frameData));

            // Write the values of all attributes marked for export to the output file.
            for(const QString& attrName : static_cast<const AttributeFileExporter*>(exporter())->attributesToExport()) {
                if(!attrMap.contains(attrName))
                    throw Exception(tr("The global attribute '%1' to be exported is not available at trajectory frame %2.").arg(attrName).arg(frameNumber));
                QString str = attrMap.value(attrName).toString();

                // Put string in quotes if it contains whitespace.
                if(!str.contains(QChar(' ')))
                    textStream() << str << " ";
                else
                    textStream() << "\"" << str << "\" ";
            }
            textStream() << "\n";

            return Future<void>::createImmediateEmplace();
        }
    };

    return OORef<Job>::create(this, filePath);
}

/******************************************************************************
* Evaluates the pipeline of the PipelineSceneNode to be exported and returns
* the attributes list.
******************************************************************************/
Future<QVariantMap> AttributeFileExporter::getAttributesMap(int frameNumber) const
{
    const PipelineFlowState state = co_await getPipelineDataToBeExported(frameNumber);

    // Build list of attributes.
    QVariantMap attributes = state.data()->buildAttributesMap();

    // Add the implicit animation frame attribute.
    attributes.insert(QStringLiteral("Frame"), frameNumber);

    co_return attributes;
}

}   // End of namespace
