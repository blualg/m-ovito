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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/RootSceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include "SceneNodesListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs the model.
******************************************************************************/
SceneNodesListModel::SceneNodesListModel(DataSetContainer& datasetContainer, ActionManager* actionManager, QWidget* parent) : QAbstractListModel(parent),
	_datasetContainer(datasetContainer),
	_pipelineSceneNodeIcon(QIcon::fromTheme("edit_pipeline_icon"))
{
	// React to the dataset being replaced.
	connect(&datasetContainer, &DataSetContainer::dataSetChanged, this, &SceneNodesListModel::onDataSetChanged);

	// Listen for scene node selection changes.
	connect(&datasetContainer, &DataSetContainer::selectionChangeComplete, this, &SceneNodesListModel::onSceneSelectionChanged);
	connect(this, &SceneNodesListModel::modelReset, this, &SceneNodesListModel::onSceneSelectionChanged);

	// Listen for signals from the root scene node.
	connect(&_rootNodeListener, &RefTargetListener<RootSceneNode>::notificationEvent, this, &SceneNodesListModel::onRootNodeNotificationEvent);

	// Listen for events of the other scene nodes.
	connect(&_nodeListener, &VectorRefTargetListener<SceneNode>::notificationEvent, this, &SceneNodesListModel::onNodeNotificationEvent);

	// Font for rendering currently selected scene nodes.
	_selectedNodeFont.setBold(true);

	updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
	connect(qGuiApp, &QGuiApplication::paletteChanged, this, &SceneNodesListModel::updateColorPalette);
QT_WARNING_POP

	for(QAction* action : actionManager->actions()) {
		if(action->objectName().startsWith("NewPipeline."))
			_pipelineActions.push_back(action);
	}
	_pipelineActions.push_back(nullptr); // Separator
	_pipelineActions.push_back(actionManager->getAction(ACTION_EDIT_CLONE_PIPELINE));
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void SceneNodesListModel::updateColorPalette(const QPalette& palette)
{
	bool darkTheme = palette.color(QPalette::Active, QPalette::Window).lightness() < 100;
	_sectionHeaderBackgroundBrush = darkTheme ? palette.mid() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
	_sectionHeaderForegroundBrush = QBrush(darkTheme ? QColor(Qt::blue).lighter() : QColor(Qt::blue));
}

/******************************************************************************
* Returns the number of rows of the model.
******************************************************************************/
int SceneNodesListModel::rowCount(const QModelIndex& parent) const
{
	return firstActionIndex() + _pipelineActions.size();
}

/******************************************************************************
* Returns the model's data stored under the given role for the item referred to by the index.
******************************************************************************/
QVariant SceneNodesListModel::data(const QModelIndex& index, int role) const
{
	if(role == Qt::DisplayRole) {
		if(index.row() == 0)
			return tr("Existing pipelines:");
		
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		if(pipelineIndex >= 0 && pipelineIndex < sceneNodes().size())
			return _nodeListener.targets()[pipelineIndex]->objectTitle();
		else if(pipelineIndex == 0)
			return tr("‹None›");

		int actionIndex = index.row() - firstActionIndex();
		if(actionIndex == -1)
			return tr("Create pipeline with data source:");
		if(actionIndex >= 0 && actionIndex < _pipelineActions.size()) {
			if(_pipelineActions[actionIndex] == nullptr)
				return {}; // Separator
			else if(actionIndex == _pipelineActions.size() - 1)
#ifdef OVITO_BUILD_PROFESSIONAL
				return tr("Clone current pipeline...");
#else
				return tr("Clone current pipeline... (Pro)");
#endif
			else if(_pipelineActions[actionIndex] != nullptr)
				return _pipelineActions[actionIndex]->text();
		}
	}
	else if(role == Qt::UserRole) {
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		if(pipelineIndex >= 0 && pipelineIndex < sceneNodes().size())
			return QVariant::fromValue(static_cast<QObject*>(sceneNodes()[pipelineIndex]));

		int actionIndex = index.row() - firstActionIndex();
		if(actionIndex >= 0 && actionIndex < _pipelineActions.size())
			return QVariant::fromValue(_pipelineActions[actionIndex]);
	}
	else if(role == Qt::FontRole) {
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		if(pipelineIndex >= 0 && pipelineIndex < sceneNodes().size()) {
			if(_nodeListener.targets()[pipelineIndex]->isSelected())
				return _selectedNodeFont;
		}
	}
	else if(role == Qt::DecorationRole) {
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		if(pipelineIndex >= 0 && pipelineIndex < (sceneNodes().empty() ? 1 : sceneNodes().size()))
			return _pipelineSceneNodeIcon;

		int actionIndex = index.row() - firstActionIndex();
		if(actionIndex >= 0 && actionIndex < _pipelineActions.size() && _pipelineActions[actionIndex] != nullptr)
			return _pipelineActions[actionIndex]->icon();
	}
	else if(role == Qt::SizeHintRole) {
		int actionIndex = index.row() - firstActionIndex();
		if(actionIndex >= 0 && actionIndex < _pipelineActions.size() && !_pipelineActions[actionIndex])
			return QSize(0, 2);	// Action separator line
	}
	else {
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		int actionIndex = index.row() - firstActionIndex();
		if(pipelineIndex == -1 || actionIndex == -1 || (actionIndex >= 0 && !_pipelineActions[actionIndex])) {
			if(role == Qt::TextAlignmentRole)
				return Qt::AlignCenter;
			else if(role == Qt::BackgroundRole)
				return _sectionHeaderBackgroundBrush;
			else if(role == Qt::ForegroundRole)
				return _sectionHeaderForegroundBrush;
		}
	}

	return {};
}

/******************************************************************************
* Returns the item flags for the given index.
******************************************************************************/
Qt::ItemFlags SceneNodesListModel::flags(const QModelIndex& index) const
{
	if(index.isValid()) {
		int pipelineIndex = index.row() - firstSceneNodeIndex();
		int actionIndex = index.row() - firstActionIndex();
		if(pipelineIndex == -1)
			return Qt::NoItemFlags; // Item: Existing pipelines
		if(pipelineIndex >= 0 && pipelineIndex < (sceneNodes().empty() ? 1 : sceneNodes().size()))
			return QAbstractListModel::flags(index);
		if(actionIndex >= 0 && actionIndex < _pipelineActions.size() && _pipelineActions[actionIndex])
			return (_pipelineActions[actionIndex]->isEnabled() ? (Qt::ItemIsSelectable | Qt::ItemIsEnabled) : Qt::NoItemFlags);
		return Qt::NoItemFlags; // Separator item
	}
	return QAbstractListModel::flags(index);
}

/******************************************************************************
* This is called when a new dataset has been loaded.
******************************************************************************/
void SceneNodesListModel::onDataSetChanged(DataSet* newDataSet)
{
	beginResetModel();
	_nodeListener.clear();
	_rootNodeListener.setTarget(nullptr);
	if(newDataSet) {
		_rootNodeListener.setTarget(newDataSet->sceneRoot());
		newDataSet->sceneRoot()->visitChildren([this](SceneNode* node) -> bool {
			_nodeListener.push_back(node);
			return true;
		});
	}
	endResetModel();
	onSceneSelectionChanged();
}

/******************************************************************************
* This is called whenever the node selection has changed.
******************************************************************************/
void SceneNodesListModel::onSceneSelectionChanged()
{
	SelectionSet* selection = _datasetContainer.currentSet() ? _datasetContainer.currentSet()->selection() : nullptr;
	if(!selection || selection->nodes().empty()) {
		Q_EMIT selectionChangeRequested(1);
	}
	else {
		int index = sceneNodes().indexOf(selection->nodes().front().get());
		Q_EMIT selectionChangeRequested(index + 1);
	}
}

/******************************************************************************
* This handles reference events generated by the root node.
******************************************************************************/
void SceneNodesListModel::onRootNodeNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
	onNodeNotificationEvent(_rootNodeListener.target(), event);
}

/******************************************************************************
* This handles reference events generated by the scene nodes.
******************************************************************************/
void SceneNodesListModel::onNodeNotificationEvent(RefTarget* source, const ReferenceEvent& event)
{
	// Whenever a new node is being inserted into the scene, add it to our internal list.
	if(event.type() == ReferenceEvent::ReferenceAdded) {
		const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
		if(refEvent.field() == PROPERTY_FIELD(SceneNode::children)) {
			if(SceneNode* node = dynamic_object_cast<SceneNode>(refEvent.newTarget())) {				
				// Extend the list model by one entry.
				if(sceneNodes().empty()) {
					_nodeListener.push_back(node);
					Q_EMIT dataChanged(createIndex(1, 0, node), createIndex(1, 0, node));
				}
				else {
					beginInsertRows(QModelIndex(), sceneNodes().size() + 1, sceneNodes().size() + 1);
					_nodeListener.push_back(node);
					endInsertRows();
				}

				// Add the children of the node too.
				node->visitChildren([this](SceneNode* node) -> bool {
					// Extend the list model by one entry.
					OVITO_ASSERT(!sceneNodes().empty());
					beginInsertRows(QModelIndex(), sceneNodes().size() + 1, sceneNodes().size() + 1);
					_nodeListener.push_back(node);
					endInsertRows();
					return true;
				});
			}
		}
	}

	// If a node is being removed from the scene, remove it from our internal list.
	if(event.type() == ReferenceEvent::ReferenceRemoved || event.type() == ReferenceEvent::ReferenceChanged) {
		// Don't know how else to do this in a safe manner. Rebuild the entire model from scratch.
		onDataSetChanged(_datasetContainer.currentSet());
	}

	// If a node is being renamed, let the model emit an update signal.
	if(event.type() == ReferenceEvent::TitleChanged) {
		int index = sceneNodes().indexOf(static_object_cast<SceneNode>(source));
		if(index >= 0) {
			QModelIndex modelIndex = createIndex(index + 1, 0, source);
			Q_EMIT dataChanged(modelIndex, modelIndex);
		}
	}
}

/******************************************************************************
* This slot executes the action associated with the given list item.
******************************************************************************/
void SceneNodesListModel::activateItem(int index)
{
	// Change scene node selection when a scene node has been selected in the combobox.
	int pipelineIndex = index - firstSceneNodeIndex();
	if(pipelineIndex >= 0 && pipelineIndex < sceneNodes().size()) {
		SceneNode* node = sceneNodes()[pipelineIndex];
		if(_datasetContainer.currentSet() && node) {
			UndoableTransaction::handleExceptions(_datasetContainer.currentSet()->undoStack(), tr("Select pipeline"), [&]() {
				_datasetContainer.currentSet()->selection()->setNode(node);
			});
		}
		return;
	}

	// This resets the current item of the combobox back to the selected scene node.
	onSceneSelectionChanged();

	int actionIndex = index - firstActionIndex();
	if(actionIndex >= 0 && actionIndex < _pipelineActions.size()) {
		if(QAction* action = _pipelineActions[actionIndex])
			action->trigger();
	}
}

/******************************************************************************
* Performs a deletion action on an item.
******************************************************************************/
void SceneNodesListModel::deleteItem(int index)
{
	// Change scene node selection when a scene node has been selected in the combobox.
	int pipelineIndex = index - firstSceneNodeIndex();
	if(pipelineIndex >= 0 && pipelineIndex < sceneNodes().size()) {
		SceneNode* node = sceneNodes()[pipelineIndex];
		if(_datasetContainer.currentSet() && node) {
			UndoableTransaction::handleExceptions(_datasetContainer.currentSet()->undoStack(), tr("Delete pipeline"), [&]() {
				bool wasSelected = node->isSelected();
				node->deleteNode();

				// Automatically select one of the remaining nodes.
				DataSet* dataset = _datasetContainer.currentSet();
				if(wasSelected && dataset->sceneRoot()->children().isEmpty() == false)
					dataset->selection()->setNode(dataset->sceneRoot()->children().front());

			});
		}
	}
}

}	// End of namespace
