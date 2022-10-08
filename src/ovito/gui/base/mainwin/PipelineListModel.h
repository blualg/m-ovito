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

#pragma once


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/oo/RefTargetListener.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include "PipelineListItem.h"

namespace Ovito {

/**
 * This Qt model class is used to populate the QListView widget.
 */
class OVITO_GUIBASE_EXPORT PipelineListModel : public QAbstractListModel
{
	Q_OBJECT

#ifdef OVITO_QML_GUI
	Q_PROPERTY(Ovito::RefTarget* selectedObject READ selectedObject NOTIFY selectedItemChanged)
	Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedItemChanged)
#endif

public:

	enum ItemRoles {
		TitleRole = Qt::UserRole + 1,
		ItemTypeRole,
		CheckedRole,
		IsCollapsedRole,
		DecorationRole,
		ToolTipRole,
		StatusInfoRole,
	};

	/// Constructor.
	PipelineListModel(DataSetContainer& datasetContainer, ActionManager* actionManager, QObject* parent);

	/// Returns the number of list items.
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { return (int)_items.size(); }

	/// Returns the data associated with a list entry.
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	/// Changes the data associated with a list entry.
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

	/// Returns the flags for an item.
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

	/// Returns the model's role names.
	virtual QHash<int, QByteArray> roleNames() const override;

	/// Returns the icon size to be used by the list widget.
	QSize iconSize() const { return _statusInfoIcon.size(); }

	/// Discards all list items.
	void clear();

	/// Returns the associated selection model.
	QItemSelectionModel* selectionModel() const { return _selectionModel; }

	/// Returns the currently selected item in the data pipeline editor.
	PipelineListItem* selectedItem() const;

	/// Returns the currently selected list items in the data pipeline editor.
	const QVector<PipelineListItem*>& selectedItems() const { return _selectedItems; }

	/// Returns the RefTarget object from the pipeline that is currently selected in the pipeline editor.
	RefTarget* selectedObject() const;

	/// Returns the currently selected pipeline objects in the data pipeline editor.
	QVector<RefTarget*> selectedObjects() const;

	/// Returns an item from the list model.
	PipelineListItem* item(int index) const {
		OVITO_ASSERT(index >= 0 && index < _items.size());
		return _items[index];
	}

	/// Returns the list of items.
	const std::vector<OORef<PipelineListItem>>& items() const { return _items; }

	/// Returns the type of drag and drop operations supported by the model.
	Qt::DropActions supportedDropActions() const override;

	/// Returns the list of allowed MIME types.
	QStringList mimeTypes() const override;

	/// Returns an object that contains serialized items of data corresponding to the list of indexes specified.
	QMimeData* mimeData(const QModelIndexList& indexes) const override;

	/// Returns true if the model can accept a drop of the data.
	bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;

	/// Handles the data supplied by a drag and drop operation that ended with the given action.
	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

	/// The list of currently selected PipelineSceneNode.
	PipelineSceneNode* selectedPipeline() const { return _selectedPipeline.target(); }

	/// Returns the container of the dataset being edited.
	DataSetContainer& datasetContainer() { return _datasetContainer; }

	/// Inserts the given modifier(s) into the currently selected pipeline.
	void applyModifiers(const QVector<OORef<Modifier>>& modifiers, ModifierGroup* group = nullptr);

	/// Sets the item in the modification list that should be selected on the next list update.
	void setNextObjectToSelect(RefTarget* obj) { 
		if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj)) {
			if(modApp->modifierGroup() && modApp->modifierGroup()->isCollapsed())
				obj = modApp->modifierGroup();
		}
		_nextObjectToSelect = obj;
	}

	/// Moves a list item up one position in the stack.
	void moveItemUp(PipelineListItem* item);

	/// Moves a list item down one position in the stack.
	void moveItemDown(PipelineListItem* item);

	/// Deletes the given model items from the data pipeline.
	void deleteItems(const QVector<PipelineListItem*>& items);

	/// Deletes a modifier application from the pipeline.
	void deleteModifierApplication(ModifierApplication* modApp);

	/// Helper method that determines if the given object is part of more than one pipeline.
	static bool isSharedObject(RefTarget* obj);

	/// Executes a drag-and-drop operation within the pipeline editor.
	Q_INVOKABLE bool performDragAndDropOperation(const QVector<int>& indexList, int row, bool dryRun);

Q_SIGNALS:

	/// This signal is emitted if a new list item has been selected, or if the currently
	/// selected item has changed.
	void selectedItemChanged();

public Q_SLOTS:

	/// Rebuilds the complete list immediately.
	void refreshList();

	/// Will rebuild the model's list of items after a short delay.
	void refreshListLater();

	/// Repaints a single item in the list as soon as control returns to the GUI event loop.
	void refreshItemLater(PipelineListItem* item);

	/// Deletes the pipeline objects that are currently selected in the list.
	void deleteSelectedItems() { deleteItems(selectedItems()); }

	/// Deletes the pipeline objects that are currently selected in the list.
	void deleteItemIndex(int index) { deleteItems({item(index)}); }

	/// Moves the a modifier up one position in the stack.
	void moveItemIndexUp(int index) { moveItemUp(item(index)); }

	/// Moves the a modifier down one position in the stack.
	void moveItemIndexDown(int index) { moveItemDown(item(index)); }

	/// Moves the selected modifier up one position in the stack.
	void moveModifierUp() { moveItemUp(selectedItem()); }

	/// Moves the selected modifier down one position in the stack.
	void moveModifierDown() { moveItemDown(selectedItem()); }

	/// Replaces the selected pipeline item with an independent copy.
	void makeElementIndependent();

	/// Creates or dissolves a group of modifiers.
	void toggleModifierGroup();

	/// Enables/disables a list model item.
	void setChecked(int index, bool checked) {
		setData(this->index(index, 0), QVariant::fromValue(checked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole);
	}

private Q_SLOTS:

	/// Is called when the QItemSelectionModel changes.
	void onSelectionModelChanged();

	/// Is called by the system when the animated status icon changed.
	void iconAnimationFrameChanged();

	/// Handles notification events generated by the selected pipeline node.
	void onPipelineEvent(RefTarget* source, const ReferenceEvent& event);

	/// Updates the state of the actions that can be invoked on the currently selected list item.
	void updateActions();

	/// Updates the color brushes of the model.
	void updateColorPalette(const QPalette& palette);

private:

	/// Is called during population of the list model.
	PipelineListItem* appendListItem(RefTarget* object, PipelineListItem::PipelineItemType itemType, PipelineListItem* parent = nullptr);

	/// Create the pipeline editor entries for the subjects of the given object (and their subobjects).
	void createListItemsForSubobjects(const DataObject* dataObj, PipelineListItem* parentItem);

	/// Replaces the a pipeline item with an independent copy.
	PipelineObject* makeElementIndependentImpl(PipelineObject* pipelineObj, CloneHelper& cloneHelper);

	/// Extracts the list of model indices from a drag and drop data record.
	QVector<int> indexListFromMimeData(const QMimeData* data) const;

	/// Moves a sequence of modifiers to a new position in the pipeline.
	bool moveModifierRange(OORef<ModifierApplication> head, OORef<ModifierApplication> tail, PipelineObject* insertBefore, ModifierApplication* insertAfter);

	/// List of visible items in the model.
	std::vector<OORef<PipelineListItem>> _items;

	/// Points to the existing item which will overwritten by the next new item during list population.
	std::vector<OORef<PipelineListItem>>::iterator _nextInsertionItem;

	/// List of selected items that were selected prior to the list refresh.
	std::vector<OORef<PipelineListItem>> _previouslySelectedItems;

	/// Holds reference to the currently selected PipelineSceneNode.
	RefTargetListener<PipelineSceneNode> _selectedPipeline;

	/// The item in the list that should be selected on the next list update.
	OORef<RefTarget> _nextObjectToSelect;

	/// The list items which will become the selected ones after a list refresh.
	QItemSelection _itemsToSelect;

	/// The selection model of the list view widget.
	QItemSelectionModel* _selectionModel;

	/// The currently selected list items.
	QVector<PipelineListItem*> _selectedItems;

	/// List item indices that need to be repainted. A negative entry indicates a refresh of the entire list.
	std::vector<int> _itemsRefreshPending;

	// Status icons:
	QPixmap _statusInfoIcon;
	QPixmap _statusWarningIcon;
	QPixmap _statusErrorIcon;
	QPixmap _statusNoneIcon;
	QMovie _statusPendingIcon;
	QIcon _modifierGroupCollapsed;
	QIcon _modifierGroupExpanded;

	/// Font used for section headers.
	QFont _sectionHeaderFont;

	/// Font used to highlight shared pipeline objects.
	QFont _sharedObjectFont;

	/// The background brush used for list section headers.
	QBrush _sectionHeaderBackgroundBrush;

	/// The foreground brush used for list section headers.
	QBrush _sectionHeaderForegroundBrush;

	/// The foreground brush used for list items that are disabled.
	QBrush _disabledForegroundBrush;

	/// Container of the dataset being edited.
	DataSetContainer& _datasetContainer;

	/// The action that deletes the selected list item.
	QAction* _deleteItemAction;

	/// Action that moves the selected item up one entry in the list.
	QAction* _moveItemUpAction;

	/// Action that moves the selected item down one entry in the list.
	QAction* _moveItemDownAction;

	/// Action that creates or dissolves a modifier group.
	QAction* _toggleModifierGroupAction;

	/// Action that creates an independent copy of a cloned pipeline object.
	QAction* _makeElementIndependentAction;

	/// Action that copies the selected pipeline item(s) to another pipeline in the scene.
	QAction* _copyItemToPipelineAction;

	/// Action that renames selected pipeline item(s).
	QAction* _renamePipelineItemAction;
};

}	// End of namespace
