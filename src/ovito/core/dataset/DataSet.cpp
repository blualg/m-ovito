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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/StandaloneApplication.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataSet);
DEFINE_REFERENCE_FIELD(DataSet, viewportConfig);
DEFINE_REFERENCE_FIELD(DataSet, renderSettings);
DEFINE_VECTOR_REFERENCE_FIELD(DataSet, globalObjects);
DEFINE_RUNTIME_PROPERTY_FIELD(DataSet, filePath);
SET_PROPERTY_FIELD_LABEL(DataSet, viewportConfig, "Viewport Configuration");
SET_PROPERTY_FIELD_LABEL(DataSet, renderSettings, "Render Settings");
SET_PROPERTY_FIELD_LABEL(DataSet, globalObjects, "Global objects");
SET_PROPERTY_FIELD_LABEL(DataSet, filePath, "File path");

/******************************************************************************
* Constructor.
******************************************************************************/
DataSet::DataSet(ObjectInitializationFlags flags) : RefTarget(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setViewportConfig(createDefaultViewportConfiguration());
        setRenderSettings(OORef<RenderSettings>::create(flags));
    }
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
DataSet::~DataSet()
{
}
#endif

/******************************************************************************
* Returns a viewport configuration that is used as template for new scenes.
******************************************************************************/
OORef<ViewportConfiguration> DataSet::createDefaultViewportConfiguration()
{
    OORef<ViewportConfiguration> viewConfig = OORef<ViewportConfiguration>::create();

    if(!StandaloneApplication::instance() || !StandaloneApplication::instance()->cmdLineParser().isSet("noviewports")) {

        // Create a scene with animation settings.
        OORef<Scene> scene = OORef<Scene>::create();
        OVITO_ASSERT(scene->animationSettings());

        // Create the 4 standard viewports.
        OORef<Viewport> topView = OORef<Viewport>::create();
        topView->setScene(scene);
        topView->setViewType(Viewport::VIEW_TOP);

        OORef<Viewport> frontView = OORef<Viewport>::create();
        frontView->setScene(scene);
        frontView->setViewType(Viewport::VIEW_FRONT);

        OORef<Viewport> leftView = OORef<Viewport>::create();
        leftView->setScene(scene);
        leftView->setViewType(Viewport::VIEW_LEFT);

        OORef<Viewport> perspectiveView = OORef<Viewport>::create();
        perspectiveView->setScene(scene);
        perspectiveView->setViewType(Viewport::VIEW_PERSPECTIVE);
        perspectiveView->setCameraTransformation(ViewportSettings::getSettings().coordinateSystemOrientation() * AffineTransformation::lookAlong({90, -120, 100}, {-90, 120, -100}, {0,0,1}).inverse());

        // Set up the 4-pane layout of the viewports.
        OORef<ViewportLayoutCell> rootLayoutCell = OORef<ViewportLayoutCell>::create();
        rootLayoutCell->setSplitDirection(ViewportLayoutCell::Horizontal);
        rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->children()[0]->setSplitDirection(ViewportLayoutCell::Vertical);
        rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->children()[0]->children()[0]->setViewport(topView);
        rootLayoutCell->children()[0]->children()[1]->setViewport(leftView);
        rootLayoutCell->children()[1]->setSplitDirection(ViewportLayoutCell::Vertical);
        rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create());
        rootLayoutCell->children()[1]->children()[0]->setViewport(frontView);
        rootLayoutCell->children()[1]->children()[1]->setViewport(perspectiveView);
        viewConfig->setLayoutRootCell(std::move(rootLayoutCell));

        viewConfig->setActiveViewport(perspectiveView);

#ifndef Q_OS_WASM
        Viewport::ViewType maximizedViewportType = static_cast<Viewport::ViewType>(ViewportSettings::getSettings().defaultMaximizedViewportType());
        if(maximizedViewportType != Viewport::VIEW_NONE) {
            for(Viewport* vp : viewConfig->viewports()) {
                if(vp->viewType() == maximizedViewportType) {
                    viewConfig->setActiveViewport(vp);
                    viewConfig->setMaximizedViewport(vp);
                    break;
                }
            }
            if(!viewConfig->maximizedViewport()) {
                viewConfig->setMaximizedViewport(viewConfig->activeViewport());
                if(maximizedViewportType > Viewport::VIEW_NONE && maximizedViewportType <= Viewport::VIEW_PERSPECTIVE)
                    viewConfig->maximizedViewport()->setViewType(maximizedViewportType);
            }
        }
        else viewConfig->setMaximizedViewport(nullptr);
#else
        viewConfig->setMaximizedViewport(viewConfig->activeViewport());
#endif
    }

    return viewConfig;
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool DataSet::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "DataSet::referenceEvent", "Reference events may only be processed in the main thread.");

    if(event.type() == ReferenceEvent::TargetChanged) {
        // Propagate change events only coming from certain sources to the DataSetContainer.
        return (source == renderSettings());
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Rescales the animation keys of all controllers in the scene.
******************************************************************************/
void DataSet::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
    // Iterate over all objects in the scene.
    for(RefTarget* reftarget : getAllDependencies()) {
        reftarget->rescaleTime(oldAnimationInterval, newAnimationInterval);
    }
}

/******************************************************************************
* Saves the dataset to a session state file.
******************************************************************************/
void DataSet::saveToFile(const QString& filePath) const
{
    // Make path absolute.
    QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::WriteOnly))
        throw Exception(tr("Failed to open output file '%1' for writing: %2").arg(absolutePath).arg(fileStream.errorString()));

    QDataStream dataStream(&fileStream);
    ObjectSaveStream stream(dataStream);
    stream.saveObject(this);
    stream.close();

    if(fileStream.error() != QFile::NoError)
        throw Exception(tr("Failed to write session state file '%1': %2").arg(absolutePath).arg(fileStream.errorString()));
    fileStream.close();
}

/******************************************************************************
* Loads the dataset's contents from a session state file.
******************************************************************************/
void DataSet::loadFromFile(const QString& filePath)
{
    // Make path absolute.
    QString absolutePath = QFileInfo(filePath).absoluteFilePath();

    QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open file '%1' for reading: %2").arg(absolutePath).arg(fileStream.errorString()));

    QDataStream dataStream(&fileStream);
    ObjectLoadStream stream(dataStream);

    // Note: DataSet::loadFromFile() is only called by Python scripts.
    OVITO_ASSERT(ExecutionContext::isScripting());
    if(stream.applicationName() != QStringLiteral("OVITO Pro"))
        throw Exception(tr("This function can only load session states written by OVITO Pro or the OVITO Python package. Files created with OVITO Basic are no longer supported."));

    stream.setDatasetToBePopulated(this);
    OORef<DataSet> dataSet = stream.loadObject<DataSet>();
    stream.close();

    if(fileStream.error() != QFile::NoError)
        throw Exception(tr("Failed to load state file '%1'.").arg(absolutePath));
    fileStream.close();
}

/******************************************************************************
* Loads a DataSet from a session state file.
******************************************************************************/
OORef<DataSet> DataSet::createFromFile(const QString& filename)
{
    // Make path absolute.
    QString absoluteFilepath = QFileInfo(filename).absoluteFilePath();

    // Open file for reading.
    QFile fileStream(absoluteFilepath);
    if(!fileStream.open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open session state file '%1' for reading: %2").arg(absoluteFilepath).arg(fileStream.errorString()));

    QDataStream dataStream(&fileStream);
    ObjectLoadStream stream(dataStream);

    OORef<DataSet> dataSet = stream.loadObject<DataSet>();
    stream.close();

    if(!dataSet)
        throw Exception(tr("Session state file '%1' does not contain a dataset.").arg(absoluteFilepath));

    dataSet->setFilePath(absoluteFilepath);
    return dataSet;
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.7.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr DataSet::OOMetaClass::overrideFieldDeserialization(const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // The DataSet class used to store an AnimationSettings object and the scene root node in OVITO 3.7 and earlier.
    if(field.definingClass == &DataSet::OOClass()) {
        // Load the legacy objects from the stream and temporarily store them in the DataSet::globalObjects list.
        // Once the entire DataSet has been loaded, loadFromStreamComplete() will move them rto the right places.
        if(field.identifier == "animationSettings") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                static_object_cast<DataSet>(&owner)->addGlobalObject(stream.loadObject<AnimationSettings>());
                stream.closeChunk();
            };
        }
        else if(field.identifier == "sceneRoot") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                static_object_cast<DataSet>(&owner)->addGlobalObject(stream.loadObject<Scene>());
                stream.closeChunk();
            };
        }
        else if(field.identifier == "selection") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                static_object_cast<DataSet>(&owner)->addGlobalObject(stream.loadObject<SelectionSet>());
                stream.closeChunk();
            };
        }
    }
    return nullptr;
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void DataSet::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.7:
    if(stream.formatVersion() <= 30008) {
        // Retrieve legacy AnimationSettings, Scene, and SelectionSet loaded by the overrideFieldDeserialization() method above.
        OORef<AnimationSettings> animSettings = findGlobalObject<AnimationSettings>();
        OORef<Scene> scene = findGlobalObject<Scene>();
        OORef<SelectionSet> selection = findGlobalObject<SelectionSet>();
        OVITO_ASSERT(animSettings && scene && selection);
        scene->setAnimationSettings(animSettings);
        scene->setSelection(selection);
        for(Viewport* vp : viewportConfig()->viewports())
            vp->setScene(scene);
        removeGlobalObject(animSettings);
        removeGlobalObject(scene);
        removeGlobalObject(selection);
    }
}

}   // End of namespace
