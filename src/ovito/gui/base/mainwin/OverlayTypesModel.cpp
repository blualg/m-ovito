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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "OverlayTypesModel.h"
#include "OverlayListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs an action for a built-in viewport layer class.
******************************************************************************/
OverlayAction* OverlayAction::createForClass(OvitoClassPtr clazz)
{
	OverlayAction* action = new OverlayAction();
	action->_layerClass = clazz;

	// Generate a unique identifier for the action:
	action->setObjectName(QStringLiteral("InsertViewportLayer.%1.%2").arg(clazz->pluginId(), clazz->name()));

	// Set the action's UI display name.
	action->setText(clazz->displayName());

	// Give the modifier a status bar text.
	QString description = clazz->descriptionString();
	action->setStatusTip(!description.isEmpty() ? std::move(description) : tr("Insert this viewport layer."));

	// Give the action an icon.
	static QIcon icon = QIcon::fromTheme("overlay_action_icon");
	action->setIcon(icon);

	return action;
}

/******************************************************************************
* Constructs an action for a Python viewport layer script.
******************************************************************************/
OverlayAction* OverlayAction::createForScript(const QString& fileName, const QDir& directory)
{
	OverlayAction* action = new OverlayAction();
	action->_scriptPath = directory.filePath(fileName);

	// Generate a unique identifier for the action:
	action->setObjectName(QStringLiteral("InsertViewportLayerScript.%1").arg(action->_scriptPath));

	// Set the action's UI display name. Chop off ".py" extension of filename.
	action->setText(fileName.chopped(3));

	// Give the layer type a status bar text.
	action->setStatusTip(tr("Insert this Python-based viewport layer."));

	// Give the action an icon.
	static QIcon icon = QIcon::fromTheme("overlay_action_icon");
	action->setIcon(icon);
	
	return action;
}

/******************************************************************************
* Constructor.
******************************************************************************/
OverlayTypesModel::OverlayTypesModel(QObject* parent, UserInterface& userInterface, OverlayListModel* overlayListModel) : QAbstractListModel(parent), _userInterface(userInterface), _overlayListModel(overlayListModel)
{
	OVITO_ASSERT(userInterface.actionManager());

	// Enumerate all built-in viewport layer classes.
	for(OvitoClassPtr clazz : PluginManager::instance().listClasses(ViewportOverlay::OOClass())) {

		// Create action for the viewport layer class.
		OverlayAction* action = OverlayAction::createForClass(clazz);
		_actions.push_back(action);

		// Register it with the global ActionManager.
		userInterface.actionManager()->addAction(action);
		OVITO_ASSERT(action->parent() == userInterface.actionManager());

		// Handle the insertion action.
		connect(action, &QAction::triggered, this, &OverlayTypesModel::insertViewportLayer);
	}

	// Add the built-in extension script directory.
	_layerScriptDirectories.push_back(PluginManager::instance().pythonDir() + QStringLiteral("/ovito/_extensions/scripts/layers"));

	// Add the user extension script directory.
	_layerScriptDirectories.push_back(QDir::homePath() + QStringLiteral("/.config/Ovito/scripts/layers"));
	for(QDir& dir : _layerScriptDirectories)
		dir.makeAbsolute();

	// Register Python script viewport layers.
	for(const QDir& scriptsDirectory : _layerScriptDirectories) {
		QStringList scriptFiles = scriptsDirectory.entryList(QStringList() << QStringLiteral("*.py"), QDir::Files, QDir::Name);
		for(const QString& fileName : scriptFiles) {
			// Filter out __init__.py.
			if(fileName == QStringLiteral("__init__.py"))
				continue;

			// Create action for the layer script.
			OverlayAction* action = OverlayAction::createForScript(fileName, scriptsDirectory);
			_actions.push_back(action);

			// Register it with the global ActionManager.
			userInterface.actionManager()->addAction(action);
			OVITO_ASSERT(action->parent() == userInterface.actionManager());

			// Handle the action.
			connect(action, &QAction::triggered, this, &OverlayTypesModel::insertViewportLayer);
		}
	}

	// Sort actions by name.
	std::sort(_actions.begin(), _actions.end(), [](OverlayAction* a, OverlayAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
}

/******************************************************************************
* Returns the action that belongs to the given model index.
******************************************************************************/
OverlayAction* OverlayTypesModel::actionFromIndex(int index) const
{
	if(index == 0) return nullptr;
	index--;

	if(index < _actions.size())
		return _actions[index];

	return nullptr;
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int OverlayTypesModel::rowCount(const QModelIndex& parent) const 
{
	return _actions.size() + 1; // First entry is the "Add layer..." item.
}

/******************************************************************************
* Returns the data associated with a list item.
******************************************************************************/
QVariant OverlayTypesModel::data(const QModelIndex& index, int role) const
{
	if(role == Qt::DisplayRole) {
		if(OverlayAction* action = actionFromIndex(index)) {
			return action->text();
		}
		else {
			return tr("Add layer...");
		}
	}
	return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags OverlayTypesModel::flags(const QModelIndex& index) const
{
	if(OverlayAction* action = actionFromIndex(index))
		return action->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;

	return QAbstractListModel::flags(index);
}

/******************************************************************************
* Signal handler that inserts the selected viewport layer into the active viewport.
******************************************************************************/
void OverlayTypesModel::insertViewportLayer()
{
	// Get the action that emitted the signal.
	OverlayAction* action = qobject_cast<OverlayAction*>(sender());
	OVITO_ASSERT(action);

	// Get the current dataset and viewport.
	Viewport* vp = _overlayListModel->selectedViewport();
	if(!vp) return;

	// Instantiate the new layer and add it to the active viewport.
	_userInterface.performTransaction(tr("Insert viewport layer"), [&]() {

		if(action->layerClass()) {

			int overlayIndex = -1;
			int underlayIndex = -1;
			if(OverlayListItem* item = _overlayListModel->selectedItem()) {
				overlayIndex = vp->overlays().indexOf(item->overlay());
				underlayIndex = vp->underlays().indexOf(item->overlay());
			}
			// Create an instance of the overlay class.
			OORef<ViewportOverlay> layer = static_object_cast<ViewportOverlay>(action->layerClass()->createInstance());
			// Make sure the new overlay gets selected in the UI.
			_overlayListModel->setNextToSelectObject(layer);
			// Insert it into either the overlays or the underlays list.
			if(underlayIndex >= 0)
				vp->insertUnderlay(underlayIndex+1, layer);
			else if(overlayIndex >= 0)
				vp->insertOverlay(overlayIndex+1, layer);
			else
				vp->insertOverlay(vp->overlays().size(), layer);
			// Automatically activate preview mode to make the overlay visible.
			vp->setRenderPreviewMode(true);
		}
	});
}

}	// End of namespace
