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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/StandaloneApplication.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataSet);
DEFINE_REFERENCE_FIELD(DataSet, viewportConfig);
DEFINE_REFERENCE_FIELD(DataSet, renderSettings);
SET_PROPERTY_FIELD_LABEL(DataSet, viewportConfig, "Viewport Configuration");
SET_PROPERTY_FIELD_LABEL(DataSet, renderSettings, "Render Settings");

/******************************************************************************
* Constructor.
******************************************************************************/
DataSet::DataSet(ObjectCreationParams params) : RefTarget(params)
{
	if(params.createSubObjects()) {
		setViewportConfig(createDefaultViewportConfiguration(params));
		setRenderSettings(OORef<RenderSettings>::create(params));
	}
}

/******************************************************************************
* Destructor.
******************************************************************************/
DataSet::~DataSet()
{
}

/******************************************************************************
* Returns a viewport configuration that is used as template for new scenes.
******************************************************************************/
OORef<ViewportConfiguration> DataSet::createDefaultViewportConfiguration(ObjectCreationParams params)
{
	OORef<ViewportConfiguration> viewConfig = OORef<ViewportConfiguration>::create(params);

	if(!StandaloneApplication::instance() || !StandaloneApplication::instance()->cmdLineParser().isSet("noviewports")) {

		// Create a scene with animation settings.
		OORef<Scene> scene = OORef<Scene>::create(params);
		OVITO_ASSERT(scene->animationSettings());

		// Create the 4 standard viewports.
		OORef<Viewport> topView = OORef<Viewport>::create(params);
		topView->setScene(scene);
		topView->setViewType(Viewport::VIEW_TOP);

		OORef<Viewport> frontView = OORef<Viewport>::create(params);
		frontView->setScene(scene);
		frontView->setViewType(Viewport::VIEW_FRONT);

		OORef<Viewport> leftView = OORef<Viewport>::create(params);
		leftView->setScene(scene);
		leftView->setViewType(Viewport::VIEW_LEFT);

		OORef<Viewport> perspectiveView = OORef<Viewport>::create(params);
		perspectiveView->setScene(scene);
		perspectiveView->setViewType(Viewport::VIEW_PERSPECTIVE);
		perspectiveView->setCameraTransformation(ViewportSettings::getSettings().coordinateSystemOrientation() * AffineTransformation::lookAlong({90, -120, 100}, {-90, 120, -100}, {0,0,1}).inverse());

		// Set up the 4-pane layout of the viewports.
		OORef<ViewportLayoutCell> rootLayoutCell = OORef<ViewportLayoutCell>::create(params);
		rootLayoutCell->setSplitDirection(ViewportLayoutCell::Horizontal);
		rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->setSplitDirection(ViewportLayoutCell::Vertical);
		rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[0]->children()[0]->setViewport(topView);
		rootLayoutCell->children()[0]->children()[1]->setViewport(leftView);
		rootLayoutCell->children()[1]->setSplitDirection(ViewportLayoutCell::Vertical);
		rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create(params));
		rootLayoutCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create(params));
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
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool DataSet::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "DataSet::referenceEvent", "Reference events may only be processed in the main thread.");

	if(event.type() == ReferenceEvent::TargetChanged) {
		// Propagate change events only from certain sources to the DataSetContainer.
		return (source == renderSettings());
	}
	return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DataSet::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(viewportConfig)) {
		Q_EMIT viewportConfigReplaced(viewportConfig());
	}
	else if(field == PROPERTY_FIELD(renderSettings)) {
		Q_EMIT renderSettingsReplaced(renderSettings());
	}

#if 0 // TODO: Remove ununsed code
	// Install a signal/slot connection that updates the viewports every time the animation time has changed.
	if(field == PROPERTY_FIELD(viewportConfig) || field == PROPERTY_FIELD(animationSettings)) {
		disconnect(_updateViewportOnTimeChangeConnection);
		if(animationSettings() && viewportConfig()) {
			_updateViewportOnTimeChangeConnection = connect(animationSettings(), &AnimationSettings::timeChangeComplete, viewportConfig(), &ViewportConfiguration::updateViewports);
			viewportConfig()->updateViewports();
		}
	}
#endif

	RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Returns the container to which this dataset belongs.
******************************************************************************/
DataSetContainer* DataSet::container() const
{
	OVITO_ASSERT_MSG(!_container.isNull(), "DataSet::container()", "DataSet is not in a DataSetContainer.");
	return _container.data();
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
void DataSet::saveToFile(const QString& filePath, MainThreadOperation operation) const
{
	// Make path absolute.
	QString absolutePath = QFileInfo(filePath).absoluteFilePath();

	QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::WriteOnly))
    	throw Exception(tr("Failed to open output file '%1' for writing: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectSaveStream stream(dataStream, operation);
	stream.saveObject(this);
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throw Exception(tr("Failed to write session state file '%1': %2").arg(absolutePath).arg(fileStream.errorString()));
	fileStream.close();
}

/******************************************************************************
* Loads the dataset's contents from a session state file.
******************************************************************************/
void DataSet::loadFromFile(const QString& filePath, MainThreadOperation operation)
{
	// Make path absolute.
	QString absolutePath = QFileInfo(filePath).absoluteFilePath();

	QFile fileStream(absolutePath);
    if(!fileStream.open(QIODevice::ReadOnly))
    	throw Exception(tr("Failed to open file '%1' for reading: %2").arg(absolutePath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectLoadStream stream(dataStream, operation);
	stream.setDataset(this);
	OORef<DataSet> dataSet = stream.loadObject<DataSet>();
	stream.close();

	if(fileStream.error() != QFile::NoError)
		throw Exception(tr("Failed to load state file '%1'.").arg(absolutePath));		
	fileStream.close();
}

}	// End of namespace
