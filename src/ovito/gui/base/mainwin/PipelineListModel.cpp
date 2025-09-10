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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/undo/RefTargetOperations.h>
#include "PipelineListModel.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineListModel::PipelineListModel(UserInterface& ui, QObject* parent) :
    QAbstractListModel(parent),
    UserInterfaceComponent<UserInterface>(ui),
    _statusInfoIcon(":/guibase/mainwin/status/status_info.png"),
    _statusWarningIcon(":/guibase/mainwin/status/status_warning.png"),
    _statusErrorIcon(":/guibase/mainwin/status/status_error.png"),
    _statusNoneIcon(":/guibase/mainwin/status/status_none.png"),
    _statusPendingIcon(":/guibase/mainwin/status/status_pending.gif"),
    _sectionHeaderFont(QGuiApplication::font()),
    _modifierGroupCollapsed(QIcon::fromTheme("modify_modifier_group_collapsed")),
    _modifierGroupExpanded(QIcon::fromTheme("modify_modifier_group_expanded"))
{
    OVITO_ASSERT(actionManager());

    // Create a selection model.
    _selectionModel = new QItemSelectionModel(this);

    // Connect signals and slots.
    _selectedPipeline.connect(this, &PipelineListModel::onPipelineEvent);
    connect(&datasetContainer(), &DataSetContainer::selectionChangeComplete, this, &PipelineListModel::onSceneSelectionChangeComplete);
    connect(_selectionModel, &QItemSelectionModel::selectionChanged, this, &PipelineListModel::onSelectionModelChanged);
    connect(this, &PipelineListModel::selectedItemChanged, this, &PipelineListModel::updateActions);

    // Set up list item fonts, icons and colors.
    _statusPendingIcon.setCacheMode(QMovie::CacheAll);
    connect(&_statusPendingIcon, &QMovie::frameChanged, this, &PipelineListModel::iconAnimationFrameChanged);
    if(_sectionHeaderFont.pixelSize() < 0)
        _sectionHeaderFont.setPointSize(_sectionHeaderFont.pointSize() * 4 / 5);
    else
        _sectionHeaderFont.setPixelSize(_sectionHeaderFont.pixelSize() * 4 / 5);
    _sharedObjectFont.setItalic(true);
    updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &PipelineListModel::updateColorPalette);
QT_WARNING_POP

    // Create list item actions.
    _deleteItemAction = actionManager()->createCommandAction(ACTION_MODIFIER_DELETE, tr("Delete Modifier"), "modify_delete_modifier", tr("Delete the selected modifier from the pipeline."));
    connect(_deleteItemAction, &QAction::triggered, this, &PipelineListModel::deleteSelectedItems);
    _moveItemUpAction = actionManager()->createCommandAction(ACTION_MODIFIER_MOVE_UP, tr("Move Modifier Up"), "modify_modifier_move_up", tr("Move the selected modifier up in the pipeline."));
    connect(_moveItemUpAction, &QAction::triggered, this, &PipelineListModel::moveModifierUp);
    _moveItemDownAction = actionManager()->createCommandAction(ACTION_MODIFIER_MOVE_DOWN, tr("Move Modifier Down"), "modify_modifier_move_down", tr("Move the selected modifier down in the pipeline."));
    connect(_moveItemDownAction, &QAction::triggered, this, &PipelineListModel::moveModifierDown);
    _toggleModifierGroupAction = actionManager()->createCommandAction(ACTION_PIPELINE_TOGGLE_MODIFIER_GROUP, tr("Group Modifiers"), "modify_modifier_group_create", tr("Creates or dissolves a group of modifiers in the pipeline editor."));
    _toggleModifierGroupAction->setCheckable(true);
    connect(_toggleModifierGroupAction, &QAction::triggered, this, &PipelineListModel::toggleModifierGroup);
    _makeElementIndependentAction = actionManager()->createCommandAction(ACTION_PIPELINE_MAKE_INDEPENDENT, tr("Make Independent"), "modify_make_element_independent", tr("Duplicate an item shared by multiple pipelines to make it independent from the other pipeline(s)."));
    connect(_makeElementIndependentAction, &QAction::triggered, this, &PipelineListModel::makeElementIndependent);
    _copyItemToPipelineAction = actionManager()->createCommandAction(ACTION_PIPELINE_COPY_ITEM, tr("Copy To..."), "modify_pipeline_copy_item_to", tr("Copy an item to another pipeline or within the current pipeline."));
    _renamePipelineItemAction = actionManager()->createCommandAction(ACTION_PIPELINE_RENAME_ITEM, tr("Rename..."), "edit_rename_pipeline_item", tr("Rename the selected pipeline entry."));
    _shareOrSplitVisualElementsAction = actionManager()->createCommandAction(ACTION_PIPELINE_GROUP_VIS_ELEMENTS, tr("Replace With Shared Element"), nullptr, tr("Combine several visual elements into one."));
    connect(_shareOrSplitVisualElementsAction, &QAction::triggered, this, &PipelineListModel::shareOrSplitVisualElements);

    updateActions();
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void PipelineListModel::updateColorPalette(const QPalette& palette)
{
    bool darkTheme = palette.color(QPalette::Active, QPalette::Window).lightness() < 100;
#ifndef Q_OS_LINUX
    _sectionHeaderBackgroundBrush = QBrush(palette.color(QPalette::Midlight));
#else
    _sectionHeaderBackgroundBrush = darkTheme ? palette.window() : QBrush(palette.color(QPalette::Midlight));
#endif
    _sectionHeaderForegroundBrush = QBrush(darkTheme ? QColor(Qt::blue).lighter() : QColor(Qt::blue));
    _disabledForegroundBrush = palette.brush(QPalette::Disabled, QPalette::Text);
}

/******************************************************************************
* Returns the currently selected item in the modification list.
******************************************************************************/
PipelineListItem* PipelineListModel::selectedItem() const
{
    return (_selectedItems.size() == 1) ? _selectedItems.front() : nullptr;
}

/******************************************************************************
* Is called when a different pipeline scene node is selected.
******************************************************************************/
void PipelineListModel::onSceneSelectionChangeComplete(SelectionSet* selection)
{
    _selectedSceneNode = selection ? selection->firstNode() : nullptr;
    Pipeline* pipeline = _selectedSceneNode ? _selectedSceneNode->pipeline() : nullptr;
    if(pipeline != selectedPipeline()) {
        _selectedPipeline.setTarget(pipeline);
        if(pipeline)
            refreshListLater();
        else
            refreshListNow();
    }
}

/******************************************************************************
* Is called when the QItemSelectionModel changes.
******************************************************************************/
void PipelineListModel::onSelectionModelChanged()
{
    _selectedItems.clear();
    for(int listIndex = 0; listIndex < items().size(); listIndex++) {
        if(_selectionModel->isSelected(index(listIndex)))
            _selectedItems.push_back(items()[listIndex]);
    }
    Q_EMIT selectedItemChanged();
}

/******************************************************************************
* Returns the RefTarget object from the pipeline that is currently selected in the pipeline editor.
******************************************************************************/
RefTarget* PipelineListModel::selectedObject() const
{
    if(PipelineListItem* item = selectedItem())
        return item->object();
    return nullptr;
}

/******************************************************************************
* Returns the currently selected pipeline objects in the data pipeline editor.
******************************************************************************/
QVector<RefTarget*> PipelineListModel::selectedObjects() const
{
    QVector<RefTarget*> objects;
    for(PipelineListItem* item : selectedItems()) {
        if(RefTarget* obj = item->object())
            objects.push_back(obj);
    }
    return objects;
}

/******************************************************************************
* Repaints a single item in the list as soon as control returns to the GUI event loop.
******************************************************************************/
void PipelineListModel::refreshItemLater(PipelineListItem* item)
{
    auto iter = boost::find(_items, item);
    if(iter == _items.end())
        return;
    int index = std::distance(_items.begin(), iter);
    if(boost::find(_itemsRefreshPending, index) != _itemsRefreshPending.end())
        return;
    _itemsRefreshPending.push_back(index);
    // Invoke actual refresh function at a later time when control returns to the GUI event loop.
    if(_itemsRefreshPending.size() == 1)
        QTimer::singleShot(200, this, &PipelineListModel::refreshList);
}

/******************************************************************************
* Rebuilds the model's list of items immediately.
******************************************************************************/
void PipelineListModel::refreshListNow()
{
    if(_itemsRefreshPending.empty() || _itemsRefreshPending.front() != -1)
        _itemsRefreshPending.insert(_itemsRefreshPending.begin(), -1);
    refreshList();
}

/******************************************************************************
* Will rebuild the model's list of items after a short delay.
******************************************************************************/
void PipelineListModel::refreshListLater()
{
    bool wasEmpty = _itemsRefreshPending.empty();
    if(!wasEmpty && _itemsRefreshPending.front() == -1)
        return;
    _itemsRefreshPending.insert(_itemsRefreshPending.begin(), -1);
    if(wasEmpty)
        // Invoke actual refresh function at a later time when control returns to the GUI event loop.
        QTimer::singleShot(200, this, &PipelineListModel::refreshList);
}

/******************************************************************************
* Completely rebuilds the pipeline list.
******************************************************************************/
void PipelineListModel::refreshList()
{
    if(_itemsRefreshPending.empty())
        return;

    // Unless a full list refresh has been requested, just refresh individual list items
    // which have been marked for a pending update.
    if(!_itemsRefreshPending.empty() && _itemsRefreshPending.front() != -1) {
        for(int listIndex : _itemsRefreshPending) {
            QModelIndex idx = index(listIndex);
            Q_EMIT dataChanged(idx, idx);
            if(_selectionModel->isSelected(idx))
                Q_EMIT selectedItemChanged();
        }
        _itemsRefreshPending.clear();
        if(_actionUpdateRequired)
            updateActions();
        return;
    }

    // Determine the currently selected objects and select them again after the list has been rebuilt.
    // _nextObjectToSelect may have been set to replace the selection.
    if(!_nextObjectToSelect && _previouslySelectedPipeline.lock().get() == selectedPipeline()) {
        for(const QModelIndex& idx : _selectionModel->selectedRows()) {
            OVITO_ASSERT(idx.isValid() && idx.row() < items().size());
            _previouslySelectedItems.push_back(items()[idx.row()]);
        }
    }

    _nextInsertionItem = _items.begin();
    if(selectedPipeline()) {

        // Create list items for visualization elements.
        for(DataVis* vis : selectedPipeline()->visElements()) {
            if(_nextInsertionItem == _items.begin())
                appendListItem(nullptr, PipelineListItem::VisualElementsHeader);
            appendListItem(selectedPipeline()->getReplacementVisElement(vis), PipelineListItem::VisualElement);
        }

        // Traverse the modifiers in the pipeline.
        PipelineNode* pipelineNode = selectedPipeline()->head();
        PipelineNode* firstPipelineNode = pipelineNode;
        ModifierGroup* currentGroup = nullptr;
        while(pipelineNode) {

            // Create entries for the modifier node.
            if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(pipelineNode)) {

                if(pipelineNode == firstPipelineNode)
                    appendListItem(nullptr, PipelineListItem::ModificationsHeader);

                if(pipelineNode->isPipelineBranch(true))
                    appendListItem(nullptr, PipelineListItem::PipelineBranch);

                if(modNode->modifierGroup() != currentGroup) {
                    if(modNode->modifierGroup())
                        appendListItem(modNode->modifierGroup(), PipelineListItem::ModifierGroup);
                    currentGroup = modNode->modifierGroup();
                }

                if(!currentGroup || !currentGroup->isCollapsed())
                    appendListItem(modNode, PipelineListItem::Modifier);

                pipelineNode = modNode->input();
            }
            else if(pipelineNode) {

                if(pipelineNode->isPipelineBranch(true))
                    appendListItem(nullptr, PipelineListItem::PipelineBranch);

                appendListItem(nullptr, PipelineListItem::DataSourceHeader);

                // Create a list item for the data source.
                PipelineListItem* item = appendListItem(pipelineNode, PipelineListItem::DataSource);

                // Create list items for the source's editable data objects.
                if(const DataCollection* collection = pipelineNode->getSourceDataCollection()) {
                    createListItemsForSubobjects(collection, item);
                }

                // Done.
                break;
            }
        }
    }

    // Remove excess list items.
    if(_nextInsertionItem != _items.end()) {
        beginRemoveRows(QModelIndex(), std::distance(_items.begin(), _nextInsertionItem), _items.size() - 1);
        _items.erase(_nextInsertionItem, _items.end());
        endRemoveRows();
    }

    // Reset internal fields.
    _nextObjectToSelect = nullptr;
    _itemsRefreshPending.clear();
    _previouslySelectedItems.clear();
    _previouslySelectedPipeline = selectedPipeline();

    // Update the selection.
    _selectedItems.clear();
    for(int listIndex = 0; listIndex < items().size(); listIndex++) {
        if(_itemsToSelect.contains(index(listIndex)))
            _selectedItems.push_back(items()[listIndex]);
    }
    _selectionModel->select(std::move(_itemsToSelect), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Clear);
    _itemsToSelect.clear();
    Q_EMIT selectedItemChanged();
}

/******************************************************************************
* Create the pipeline editor entries for the subjects of the given
* object (and their sub-objects).
******************************************************************************/
void PipelineListModel::createListItemsForSubobjects(const DataObject* dataObj, PipelineListItem* parentItem)
{
    DataObject::PipelineEditorObjectListMode mode = dataObj->pipelineEditorObjectListMode();

    if(dataObj->editableProxy() && (mode == DataObject::PipelineEditorObjectListMode::Show || mode == DataObject::PipelineEditorObjectListMode::ShowIncludingSubObjects)) {
        parentItem = appendListItem(dataObj->editableProxy(), PipelineListItem::DataObject, parentItem);
    }

    if(mode == DataObject::PipelineEditorObjectListMode::ShowIncludingSubObjects || mode == DataObject::PipelineEditorObjectListMode::Hide) {
        // Recursively visit the sub-objects of the data object.
        dataObj->visitSubObjects([&](const DataObject* subObject) {
            createListItemsForSubobjects(subObject, parentItem);
        });
    }
}

/******************************************************************************
* Is called during population of the list model.
******************************************************************************/
PipelineListItem* PipelineListModel::appendListItem(RefTarget* object, PipelineListItem::PipelineItemType itemType, PipelineListItem* parent)
{
    int listIndex = std::distance(_items.begin(), _nextInsertionItem);
    QModelIndex modelIndex;

    PipelineListItem* item;
    if(_nextInsertionItem != _items.end()) {
        modelIndex = index(listIndex);
        if((*_nextInsertionItem)->object() != object || (*_nextInsertionItem)->itemType() != itemType || (*_nextInsertionItem)->parent() != parent) {
            *_nextInsertionItem = OORef<PipelineListItem>::create(object, itemType, parent);
            connect(*_nextInsertionItem, &PipelineListItem::itemChanged, this, &PipelineListModel::refreshItemLater);
            connect(*_nextInsertionItem, &PipelineListItem::subitemsChanged, this, &PipelineListModel::refreshListLater);
            Q_EMIT dataChanged(modelIndex, modelIndex);
        }
        else {
            if(boost::find(_itemsRefreshPending, listIndex) != _itemsRefreshPending.end()) {
                Q_EMIT dataChanged(modelIndex, modelIndex);
            }
        }
        item = *_nextInsertionItem++;
    }
    else {
        beginInsertRows(QModelIndex(), _items.size(), _items.size());
        _items.push_back(OORef<PipelineListItem>::create(object, itemType, parent));
        _nextInsertionItem = _items.end();
        endInsertRows();
        item = _items.back();
        modelIndex = index(listIndex);
        connect(item, &PipelineListItem::itemChanged, this, &PipelineListModel::refreshItemLater);
        connect(item, &PipelineListItem::subitemsChanged, this, &PipelineListModel::refreshListLater);
    }

    // Determine whether this list item is going to be selected.
    bool selectItem = false;
    if(_nextObjectToSelect) {
        // Select the pipeline object that has been explicitly requested.
        if(_nextObjectToSelect == object)
            selectItem = true;
    }
    else {
        if(!_previouslySelectedItems.empty() && object != nullptr) {
            // Check if the same list entry was selected before the list refresh.
            for(const auto& oldItem : _previouslySelectedItems) {
                if(oldItem->object() == object) {
                    selectItem = true;
                    break;
                }
                else if(itemType == PipelineListItem::DataObject && (oldItem->itemType() == PipelineListItem::DataObject || oldItem->itemType() == PipelineListItem::DeletedDataObject) && oldItem->title() == item->title()) {
                    selectItem = true;
                    break;
                }
            }
        }
        else {
            // The data source is the object to be selected initially.
            if(itemType == PipelineListItem::DataSource)
                selectItem = true;
        }
    }

    if(selectItem)
        _itemsToSelect.select(modelIndex, modelIndex);

    return item;
}

/******************************************************************************
* Handles notification events generated by the selected pipeline.
******************************************************************************/
void PipelineListModel::onPipelineEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Update the entire modification list if the Pipeline has been assigned a new
    // data object, or if the list of visual elements has changed.
    if(event.type() == ReferenceEvent::ReferenceChanged
        || event.type() == ReferenceEvent::ReferenceAdded
        || event.type() == ReferenceEvent::ReferenceRemoved
        || event.type() == ReferenceEvent::PipelineChanged)
    {
        refreshListLater();
    }
}

/******************************************************************************
* Inserts the given modifier(s) into the currently selected pipeline.
******************************************************************************/
void PipelineListModel::applyModifiers(const QVector<OORef<Modifier>>& modifiers, ModifierGroup* group)
{
    if(modifiers.empty() || !selectedPipeline())
        return;

    // Get current animation time.
    AnimationTime time = currentAnimationTime();

    // Get the selected pipeline item. The new modifier is inserted right behind it in the pipeline.
    PipelineListItem* currentItem = selectedItem();

    if(currentItem) {
        while(currentItem->parent()) {
            currentItem = currentItem->parent();
        }

        RefTarget* selectedObject = currentItem->object();
        if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(selectedObject)) {
            selectedObject = group->nodes().first();
        }

        if(OORef<PipelineNode> pnode = dynamic_object_cast<PipelineNode>(selectedObject)) {

            ModifierGroup* modifierGroup = nullptr;
            if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(selectedObject)) {
                if(selectedObject == currentItem->object())
                    modifierGroup = modNode->modifierGroup();
            }
            if(!modifierGroup)
                modifierGroup = group;

            for(int i = modifiers.size() - 1; i >= 0; i--) {
                Modifier* modifier = modifiers[i];
                std::vector<OORef<RefMaker>> dependentsList;
                pnode->visitDependents([&](RefMaker* dependent) {
                    if(dynamic_object_cast<ModificationNode>(dependent) || dynamic_object_cast<Pipeline>(dependent)) {
                        dependentsList.push_back(dependent);
                    }
                });
                OORef<ModificationNode> modNode = modifier->createModificationNode();
                modNode->setModifier(modifier);
                modNode->setInput(pnode);
                modNode->setModifierGroup(modifierGroup);
                modifier->initializeModifier(ModifierInitializationRequest(time, false, true, modNode));
                setNextObjectToSelect(modNode);
                for(RefMaker* dependent : dependentsList) {
                    if(ModificationNode* predecessorModNode = dynamic_object_cast<ModificationNode>(dependent)) {
                        predecessorModNode->setInput(modNode);
                    }
                    else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
                        if(pipeline->head() == pnode)
                            pipeline->setHead(modNode);
                    }
                }
                pnode = modNode;
            }
            if(group)
                setNextObjectToSelect(group);
            return;
        }
    }

    // Insert modifiers at the end of the selected pipelines.
    for(int index = modifiers.size() - 1; index >= 0; --index) {
        ModificationNode* modNode = selectedPipeline()->applyModifier(time, true, modifiers[index]);
        if(group)
            modNode->setModifierGroup(group);
        else
            setNextObjectToSelect(modNode);
    }
    if(group)
        setNextObjectToSelect(group);

    refreshList();
}

/******************************************************************************
* Deletes the given model items from the data pipeline.
******************************************************************************/
void PipelineListModel::deleteItems(const QVector<PipelineListItem*>& items)
{
    if(items.empty())
        return;

    // Build list of modifier nodes to delete from the pipeline.
    std::set<ModificationNode*> nodes;
    for(PipelineListItem* item : items) {
        if(OORef<ModificationNode> node = dynamic_object_cast<ModificationNode>(item->object())) {
            nodes.insert(node);
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            for(ModificationNode* node : group->nodes())
                nodes.insert(node);
        }
    }

    // Perform the deletion one by one.
    performTransaction(tr("Delete modifier"), [&]() {
        for(ModificationNode* node : nodes) {
            deleteModificationNode(node);
        }
    });

    refreshList();
}

/******************************************************************************
* Deletes a modifier node from the pipeline.
******************************************************************************/
void PipelineListModel::deleteModificationNode(ModificationNode* node)
{
    performTransaction(tr("Delete modifier"), [node = OORef<ModificationNode>(node), this]() {
        node->visitDependents([&](RefMaker* dependent) {
            if(ModificationNode* precedingModNode = dynamic_object_cast<ModificationNode>(dependent)) {
                if(precedingModNode->input() == node) {
                    setNextObjectToSelect(node->input());
                    precedingModNode->setInput(node->input());
                }
            }
            else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
                if(pipeline->head() == node) {
                    setNextObjectToSelect(node->input());
                    pipeline->setHead(node->input());
                }
            }
        });
        node->requestObjectDeletion();
    });

    // Invalidate the items list of the model.
    refreshListLater();
}

/******************************************************************************
* Is called by the system when the animated status icon changed.
******************************************************************************/
void PipelineListModel::iconAnimationFrameChanged()
{
    bool stopMovie = true;
    for(int i = 0; i < items().size(); i++) {
        if(item(i)->isObjectActive()) {
            dataChanged(index(i), index(i), { Qt::DecorationRole });
            stopMovie = false;
        }
    }
    if(stopMovie)
        _statusPendingIcon.stop();
}

/******************************************************************************
* Returns the data for the QListView widget.
******************************************************************************/
QVariant PipelineListModel::data(const QModelIndex& index, int role) const
{
    QModelRoleData roleData(role);
    multiData(index, roleData);
    return roleData.data();
}

/******************************************************************************
* Fills the roleDataSpan with the requested data for the given index.
******************************************************************************/
void PipelineListModel::multiData(const QModelIndex& index, QModelRoleDataSpan roleDataSpan) const
{
    OVITO_ASSERT(index.row() >= 0 && index.row() < _items.size());
    PipelineListItem* item = this->item(index.row());

    for(QModelRoleData& roleData : roleDataSpan) {
        int role = roleData.role();
        if(role == Qt::DisplayRole || role == TitleRole) {
            // Indent modifiers that are part of a group.
            if(item->itemType() == PipelineListItem::Modifier) {
                if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(item->object())) {
                    if(modNode->modifierGroup()) {
#ifndef Q_OS_WIN
                        roleData.setData(QStringLiteral(" ") + item->title());
#else
                        roleData.setData(QStringLiteral("   ") + item->title());
#endif
                        continue;
                    }
                }
            }
            roleData.setData(item->title());
            continue;
        }
        else if(role == Qt::EditRole) {
            roleData.setData(item->title());
            continue;
        }
        else if(role == ItemTypeRole) {
            roleData.setData(item->itemType());
            continue;
        }
        else if(role == IsCollapsedRole) {
            if(item->itemType() == PipelineListItem::ModifierGroup) {
                roleData.setData(static_object_cast<ModifierGroup>(item->object())->isCollapsed());
                continue;
            }
        }
        else if(role == StatusInfoRole) {
            if(dynamic_object_cast<ActiveObject>(item->object())) {
                if(SceneNode* sceneNode = selectedSceneNode()) {
                    handleExceptions<true>([&] {
                        roleData.setData(item->shortInfo(sceneNode));
                    });
                    if(!roleData.data().isValid()) {
                        // For shared visual elements, display the list of data objects they are attached to (only if it is more than one).
                        if(item->object() && item->itemType() == PipelineListItem::VisualElement && selectedPipeline()) {
                            if(const QStringList* titles = selectedPipeline()->getDataObjectTitlesForVisElement(static_object_cast<DataVis>(item->object()))) {
                                if(titles->size() > 1) {
                                    QStringList newTitles;
                                    // Check if all titles start with the same prefix, e.g. "Mesh: ...".
                                    if(auto idx = titles->front().indexOf(": "); idx > 0) {
                                        QString prefix = titles->front().left(idx + 2);
                                        if(std::ranges::all_of(*titles, [&](const QString& s) { return s.startsWith(prefix); })) {
                                            // All titles start with the same prefix, so we can remove it from all titles.
                                            std::ranges::transform(*titles, std::back_inserter(newTitles), [&](const QString& title) {
                                                return title.mid(prefix.length());
                                            });
                                            titles = &newTitles;
                                        }
                                    }
                                    // Join all titles into one info string.
                                    roleData.setData(titles->join(QStringLiteral(", ")));
                                }
                            }
                        }
                    }
                    if(roleData.data().isValid())
                        continue;
                }
            }
        }
        else if(role == Qt::DecorationRole) {
            // This role is only used by the QWidgets GUI.
            if(item->itemType() == PipelineListItem::ModifierGroup) {
                if(!static_object_cast<ModifierGroup>(item->object())->isCollapsed()) {
                    roleData.setData(_modifierGroupExpanded);
                    continue;
                }
            }
            if(item->isObjectActive()) {
                _statusPendingIcon.start();
                roleData.setData(_statusPendingIcon.currentPixmap());
                continue;
            }
            if(item->itemType() == PipelineListItem::ModifierGroup) {
                if(item->status().type() == PipelineStatus::Success) {
                    roleData.setData(_modifierGroupCollapsed);
                    continue;
                }
            }
            if(item->isObjectItem()) {
                switch(item->status().type()) {
                case PipelineStatus::Warning: roleData.setData(_statusWarningIcon); break;
                case PipelineStatus::Error: roleData.setData(_statusErrorIcon); break;
                default: roleData.setData(_statusNoneIcon); break;
                }
                continue;
            }
        }
        else if(role == PipelineListModel::DecorationRole) {
            // This role is only used by the QML GUI.
            if(item->itemType() == PipelineListItem::ModifierGroup) {
                if(!static_object_cast<ModifierGroup>(item->object())->isCollapsed()) {
                    roleData.setData(QStringLiteral("modify_modifier_group_expanded"));
                    continue;
                }
            }
            if(item->itemType() == PipelineListItem::ModifierGroup) {
                if(item->status().type() == PipelineStatus::Success) {
                    roleData.setData(QStringLiteral("modify_modifier_group_collapsed"));
                    continue;
                }
            }
            if(item->isObjectItem()) {
                switch(item->status().type()) {
                case PipelineStatus::Warning: roleData.setData(QStringLiteral("qrc:/guibase/mainwin/status/status_warning.png")); break;
                case PipelineStatus::Error: roleData.setData(QStringLiteral("qrc:/guibase/mainwin/status/status_error.png")); break;
                default: roleData.setData(QStringLiteral("qrc:/guibase/mainwin/status/status_none.png"));
                }
                continue;
            }
        }
        else if(role == Qt::ToolTipRole || role == PipelineListModel::ToolTipRole) {
            roleData.setData(item->status().text());
            continue;
        }
        else if(role == Qt::CheckStateRole) {
            if(ModificationNode* node = dynamic_object_cast<ModificationNode>(item->object())) {
                roleData.setData((node->modifier() && node->modifier()->isEnabled()) ? Qt::Checked : Qt::Unchecked);
                continue;
            }
            else if(ActiveObject* object = dynamic_object_cast<ActiveObject>(item->object())) {
                if(item->itemType() != PipelineListItem::DataSource) {
                    roleData.setData(object->isEnabled() ? Qt::Checked : Qt::Unchecked);
                    continue;
                }
            }
            else if(item->itemType() == PipelineListItem::DeletedVisualElement) {
                roleData.setData(Qt::Unchecked); // This is to prevent the checkbox element from temporarily disappearing while the list of visual elements is being regenerated.
                continue;
            }
        }
        else if(role == CheckedRole) {
            if(ModificationNode* node = dynamic_object_cast<ModificationNode>(item->object())) {
                roleData.setData(node->modifier() && node->modifier()->isEnabled());
                continue;
            }
            else if(ActiveObject* object = dynamic_object_cast<ActiveObject>(item->object())) {
                if(item->itemType() != PipelineListItem::DataSource) {
                    roleData.setData(object->isEnabled());
                    continue;
                }
            }
            roleData.setData(false);
            continue;
        }
        else if(role == Qt::TextAlignmentRole) {
            if(!item->isObjectItem()) {
                roleData.setData(Qt::AlignCenter);
                continue;
            }
        }
        else if(role == Qt::BackgroundRole) {
            if(!item->isObjectItem()) {
                if(item->itemType() != PipelineListItem::PipelineBranch)
                    roleData.setData(_sectionHeaderBackgroundBrush);
                else
                    roleData.setData(QBrush(Qt::lightGray, Qt::Dense6Pattern));
                continue;
            }
        }
        else if(role == Qt::ForegroundRole) {
            if(!item->isObjectItem()) {
                roleData.setData(_sectionHeaderForegroundBrush);
                continue;
            }
            else if(item->itemType() == PipelineListItem::Modifier && static_object_cast<ModificationNode>(item->object())->modifierAndGroupEnabled() == false) {
                roleData.setData(_disabledForegroundBrush);
                continue;
            }
            else if(item->itemType() == PipelineListItem::ModifierGroup && static_object_cast<ModifierGroup>(item->object())->isEnabled() == false) {
                roleData.setData(_disabledForegroundBrush);
                continue;
            }
        }
        else if(role == Qt::FontRole) {
            if(!item->isObjectItem()) {
                roleData.setData(_sectionHeaderFont);
                continue;
            }
            else if(isSharedObject(item->object())) {
                roleData.setData(_sharedObjectFont);
                continue;
            }
        }
        roleData.clearData();
    }
}

/******************************************************************************
* Changes the data associated with a list entry.
******************************************************************************/
bool PipelineListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role == Qt::CheckStateRole || role == CheckedRole) {
        PipelineListItem* item = this->item(index.row());
        if(DataVis* vis = dynamic_object_cast<DataVis>(item->object())) {
            performTransaction((value.toBool()) ? tr("Enable visual element") : tr("Disable visual element"), [vis, &value]() {
                vis->setEnabled(value.toBool());
            });
            return true;
        }
        else if(ModificationNode* node = dynamic_object_cast<ModificationNode>(item->object())) {
            performTransaction((value.toInt() != Qt::Unchecked) ? tr("Enable modifier") : tr("Disable modifier"), [node, &value]() {
                if(node->modifier())
                    node->modifier()->setEnabled(value.toInt() != Qt::Unchecked);
            });
            return true;
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            performTransaction((value.toBool()) ? tr("Enable modifier group") : tr("Disable modifier group"), [group, &value]() {
                group->setEnabled(value.toBool());
            });
            return true;
        }
    }
    else if(role == Qt::EditRole) {
        PipelineListItem* item = this->item(index.row());
        if(DataVis* vis = dynamic_object_cast<DataVis>(item->object())) {
            QString newName = value.toString();
            if(vis->objectTitle() != newName) {
                performTransaction(tr("Rename visual element"), [vis, &newName]() {
                    vis->setObjectTitle(newName);
                });
            }
            return true;
        }
        else if(ModificationNode* node = dynamic_object_cast<ModificationNode>(item->object())) {
            QString newName = value.toString();
            if(node->modifier() && node->modifier()->objectTitle() != newName) {
                performTransaction(tr("Rename modifier"), [node, &newName]() {
                    node->modifier()->setObjectTitle(newName);
                });
            }
            return true;
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            QString newName = value.toString();
            if(group->objectTitle() != newName) {
                performTransaction(tr("Rename modifier group"), [group, &newName]() {
                    group->setObjectTitle(newName);
                });
            }
            return true;
        }
    }
    else if(role == IsCollapsedRole) {
        if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(this->item(index.row())->object())) {
            group->setCollapsed(value.toBool());
            return true;
        }
    }
    return QAbstractListModel::setData(index, value, role);
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags PipelineListModel::flags(const QModelIndex& index) const
{
    if(index.row() >= 0 && index.row() < _items.size()) {
        switch(this->item(index.row())->itemType()) {
            case PipelineListItem::VisualElement:
                return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
            case PipelineListItem::Modifier:
            case PipelineListItem::ModifierGroup:
                return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
            case PipelineListItem::DataSource:
            case PipelineListItem::DataObject:
                return QAbstractListModel::flags(index);
            case PipelineListItem::PipelineBranch:
                return Qt::ItemIsDropEnabled;
            case PipelineListItem::DeletedObject:
            case PipelineListItem::DeletedDataObject:
            case PipelineListItem::DeletedVisualElement:
                return QAbstractListModel::flags(index); // Keep entries with deleted objects selectable to not loose the current selection before the model is updated.
            default:
                return Qt::NoItemFlags;
        }
    }
    return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
}

/******************************************************************************
* Returns the model's role names.
******************************************************************************/
QHash<int, QByteArray> PipelineListModel::roleNames() const
{
    return {
        { TitleRole, "title" },
        { ItemTypeRole, "type" },
        { CheckedRole, "ischecked" },
        { DecorationRole, "decoration" },
        { ToolTipRole, "tooltip" },
        { StatusInfoRole, "statusinfo" }
    };
}

/******************************************************************************
* Updates the state of the actions that can be invoked on the currently
* selected list item.
******************************************************************************/
void PipelineListModel::updateActions()
{
    // Get all currently selected pipeline objects.
    const QVector<RefTarget*>& objects = selectedObjects();

    // Get the single currently selected object.
    // While the items of the model are out of date, do not enable any actions and wait until the items list is rebuilt.
    _actionUpdateRequired = !_itemsRefreshPending.empty();
    RefTarget* currentObject = (_itemsRefreshPending.empty() && objects.size() == 1) ? objects.front() : nullptr;

    // Check if all selected objects are deletable.
    _deleteItemAction->setEnabled(!objects.empty() && boost::algorithm::all_of(objects, [](RefTarget* obj) {
        return dynamic_object_cast<ModificationNode>(obj) || dynamic_object_cast<ModifierGroup>(obj);
    }));
    if(objects.size() == 1 && dynamic_object_cast<ModificationNode>(objects[0]))
        _deleteItemAction->setText(tr("Delete Modifier"));
    else if(objects.size() == 1 && dynamic_object_cast<ModifierGroup>(objects[0]))
        _deleteItemAction->setText(tr("Delete Modifier Group"));
    else
        _deleteItemAction->setText(tr("Delete"));

    // Check if the selected object is a shared object which can be made independent.
    _makeElementIndependentAction->setEnabled(
        isSharedObject(currentObject)
        && (dynamic_object_cast<ModificationNode>(currentObject) == nullptr || static_object_cast<ModificationNode>(currentObject)->modifierGroup() == nullptr || static_object_cast<ModificationNode>(currentObject)->pipelines(true).size() == 1));

    if(DataVis* visElement = dynamic_object_cast<DataVis>(currentObject); visElement && isSharedVisualElement(visElement)) {
        _shareOrSplitVisualElementsAction->setText(tr("Split Into Multiple Elements"));
        _shareOrSplitVisualElementsAction->setEnabled(true);
        _shareOrSplitVisualElementsAction->setVisible(true);
    }
    else if(objects.size() >= 1 && std::ranges::all_of(objects, [](RefTarget* obj) { return dynamic_object_cast<DataVis>(obj) != nullptr; })) {
        _shareOrSplitVisualElementsAction->setText(tr("Collapse Into One Element"));
        _shareOrSplitVisualElementsAction->setEnabled(canShareVisualElements(objects));
        _shareOrSplitVisualElementsAction->setVisible(true);
    }
    else {
        _shareOrSplitVisualElementsAction->setEnabled(false);
        _shareOrSplitVisualElementsAction->setVisible(false);
    }

    _copyItemToPipelineAction->setEnabled(boost::algorithm::any_of(objects, [](RefTarget* obj) {
        return dynamic_object_cast<PipelineNode>(obj) || dynamic_object_cast<ModifierGroup>(obj);
    }));

    _renamePipelineItemAction->setEnabled(ModificationNode::OOClass().isMember(currentObject) || ModifierGroup::OOClass().isMember(currentObject) || DataVis::OOClass().isMember(currentObject));

    // Update the state of the move up/down actions.
    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(currentObject)) {
        _moveItemDownAction->setText(tr("Move Modifier Down"));
        _moveItemDownAction->setEnabled(
            modNode->input()
            && (dynamic_object_cast<ModificationNode>(modNode->input()) != nullptr || modNode->modifierGroup() != nullptr)
            && (modNode->input()->isPipelineBranch(true) == false || modNode->modifierGroup() != nullptr)
            && modNode->pipelines(true).empty() == false
            && (modNode->modifierGroup() == nullptr || modNode->modifierGroup()->nodes().size() > 1));

        _moveItemUpAction->setText(tr("Move Modifier Up"));
        _moveItemUpAction->setEnabled(
            (modNode->getPredecessorModNode() != nullptr || modNode->modifierGroup() != nullptr)
            && (modNode->isPipelineBranch(true) == false || modNode->modifierGroup() != nullptr)
            && modNode->pipelines(true).empty() == false
            && (modNode->modifierGroup() == nullptr || modNode->modifierGroup()->nodes().size() > 1));
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(currentObject)) {
        _moveItemUpAction->setEnabled(false);
        _moveItemDownAction->setEnabled(false);
        _moveItemUpAction->setText(tr("Move Modifier Group Up"));
        _moveItemDownAction->setText(tr("Move Modifier Group Down"));

        // Determine whether it would be possible to move the entire modifier group up and/or down.
        if(group->pipelines(true).empty() == false) {
            QVector<ModificationNode*> groupModNodes = group->nodes();
            if(ModificationNode* inputModNode = dynamic_object_cast<ModificationNode>(groupModNodes.back()->input())) {
                OVITO_ASSERT(inputModNode->modifierGroup() != group);
                _moveItemDownAction->setEnabled(!inputModNode->isPipelineBranch(true));
            }
            _moveItemUpAction->setEnabled(groupModNodes.front()->getPredecessorModNode() != nullptr);
        }
    }
    else {
        _moveItemUpAction->setEnabled(false);
        _moveItemDownAction->setEnabled(false);
        _moveItemUpAction->setText(tr("Move Up"));
        _moveItemDownAction->setText(tr("Move Down"));
    }

    // Update the modifier grouping action.
    _toggleModifierGroupAction->setChecked(false);
    _toggleModifierGroupAction->setEnabled(false);
    _toggleModifierGroupAction->setText(tr("Create Modifier Group"));
    // Are all selected objects modifier nodes and are they not in a group?
    if(!objects.empty() && boost::algorithm::all_of(objects, [](RefTarget* obj) {
            ModificationNode* modNode = dynamic_object_cast<ModificationNode>(obj);
            return modNode && modNode->modifierGroup() == nullptr; }))
    {
        // Do all selected modifier nodes form a contiguous sequence?
        bool isContiguousSequence = true;
        for(auto obj = std::next(objects.cbegin()); obj != objects.cend(); ++obj) {
            if(static_object_cast<ModificationNode>(*obj) != static_object_cast<ModificationNode>(*std::prev(obj))->input()) {
                isContiguousSequence = false;
                break;
            }
        }
        if(isContiguousSequence) {
            _toggleModifierGroupAction->setEnabled(true);
        }
    }
    else if(dynamic_object_cast<ModifierGroup>(currentObject) != nullptr) {
        _toggleModifierGroupAction->setEnabled(true);
        _toggleModifierGroupAction->setChecked(true);
        _toggleModifierGroupAction->setText(tr("Ungroup Modifiers"));
    }
    _toggleModifierGroupAction->setVisible(!objects.empty() && std::ranges::any_of(objects, [](RefTarget* obj) {
            return dynamic_object_cast<ModificationNode>(obj) || dynamic_object_cast<ModifierGroup>(obj);
        }));
}

/******************************************************************************
* Returns the list of allowed MIME types.
******************************************************************************/
QStringList PipelineListModel::mimeTypes() const
{
    return QStringList() << QStringLiteral("application/ovito.pipeline.item.list");
}

/******************************************************************************
* Returns an object that contains serialized items of data corresponding to the
* list of indexes specified.
******************************************************************************/
QMimeData* PipelineListModel::mimeData(const QModelIndexList& indexes) const
{
    // Collect the list of list model indices to be dragged.
    QVector<int> rows;
    for(const QModelIndex& index : indexes) {
        if(index.isValid())
            rows.push_back(index.row());
    }
    if(rows.empty())
        return nullptr;
    boost::sort(rows);

    // Only allow dragging a contiguous sequence of pipeline items.
    for(auto i1 = rows.cbegin(), i2 = std::next(i1); i2 != rows.cend(); i1 = i2++)
        if(*i1 + 1 != *i2)
            return nullptr;

    // Encode the item list as a MIME data record.
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << rows.size();
    for(const auto& row : rows)
        stream << row;
    std::unique_ptr<QMimeData> mimeData = std::make_unique<QMimeData>();
    mimeData->setData(mimeTypes().front(), encodedData);
    return mimeData.release();
}

/******************************************************************************
* Returns the type of drag and drop operations supported by the model.
******************************************************************************/
Qt::DropActions PipelineListModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

/******************************************************************************
* Returns true if the model can accept a drop of the data.
******************************************************************************/
bool PipelineListModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
    if(!data->hasFormat(mimeTypes().front()))
        return false;

    if(column > 0)
        return false;

    if(action != Qt::MoveAction)
        return false;

    return const_cast<PipelineListModel*>(this)->performDragAndDropOperation(indexListFromMimeData(data), row, true);
}

/******************************************************************************
* Handles the data supplied by a drag and drop operation that ended with the
* given action.
******************************************************************************/
bool PipelineListModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if(action != Qt::MoveAction)
        return false;

    return performDragAndDropOperation(indexListFromMimeData(data), row, false);
}

/******************************************************************************
* Extracts the list of model indices from a drag and drop data record.
******************************************************************************/
QVector<int> PipelineListModel::indexListFromMimeData(const QMimeData* data) const
{
    QVector<int> indexList;
    QByteArray encodedData = data->data(mimeTypes().front());
    if(!encodedData.isEmpty()) {
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QVector<int>::size_type count;
        stream >> count;
        if(count != 0) {
            indexList.resize(count);
            for(auto& row : indexList)
                stream >> row;
        }
    }
    return indexList;
}

/******************************************************************************
* Executes a drag-and-drop operation within the pipeline editor.
******************************************************************************/
bool PipelineListModel::performDragAndDropOperation(const QVector<int>& indexList, int row, bool dryRun)
{
    if(indexList.empty())
        return false;
    if(row <= 0 || row >= items().size())
        return false;

    // The modifier group the modapps will be placed into.
    ModifierGroup* destinationGroup = nullptr;
    bool isOptionalDestinationGroup = false;

    // Determine the insertion location in the pipeline.
    PipelineListItem* insertBeforeItem = item(row);
    PipelineListItem* insertAfterItem = item(row - 1);
    PipelineNode* insertBefore = nullptr;
    ModificationNode* insertAfter = nullptr;
    if(insertAfterItem->itemType() == PipelineListItem::ModificationsHeader) {
        insertBefore = nullptr;
    }
    else if(insertAfterItem->itemType() == PipelineListItem::Modifier) {
        insertAfter = static_object_cast<ModificationNode>(insertAfterItem->object());
        destinationGroup = insertAfter->modifierGroup();
        if(destinationGroup && destinationGroup->nodes().back() == insertAfter)
            isOptionalDestinationGroup = true;
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::Modifier) {
        insertBefore = static_object_cast<ModificationNode>(insertBeforeItem->object());
        destinationGroup = static_object_cast<ModificationNode>(insertBeforeItem->object())->modifierGroup();
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::DataSourceHeader) {
        insertBefore = selectedPipeline()->source();
    }
    else if(insertAfterItem->itemType() == PipelineListItem::ModifierGroup && insertBeforeItem->itemType() == PipelineListItem::Modifier) {
        insertBefore = static_object_cast<ModificationNode>(insertBeforeItem->object());
        destinationGroup = static_object_cast<ModifierGroup>(insertAfterItem->object());
    }
    else if(insertAfterItem->itemType() == PipelineListItem::ModifierGroup && static_object_cast<ModifierGroup>(insertAfterItem->object())->isCollapsed()) {
        insertAfter = static_object_cast<ModifierGroup>(insertAfterItem->object())->nodes().back();
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::ModifierGroup) {
        insertBefore = static_object_cast<ModifierGroup>(insertBeforeItem->object())->nodes().first();
    }
    else {
        return false;
    }

    // Determine the contiguous sequence of modifiers to be moved.
    ModificationNode* head = nullptr;
    ModificationNode* tail = nullptr;
    std::vector<ModificationNode*> regroupModNodes;
    for(int row : indexList) {
        if(row <= 0 || row >= items().size())
            return false;
        PipelineListItem* movedItem = item(row);
        if(movedItem->itemType() == PipelineListItem::Modifier) {
            ModificationNode* modNode = static_object_cast<ModificationNode>(movedItem->object());
            if(head == nullptr) head = modNode;
            if(tail == nullptr || (modNode->isReferencedBy(tail) && modNode != tail)) {
                tail = modNode;
                regroupModNodes.push_back(modNode);
            }
        }
        else if(movedItem->itemType() == PipelineListItem::ModifierGroup) {
            ModifierGroup* group = static_object_cast<ModifierGroup>(movedItem->object());
            const auto& modNodes = group->nodes();
            if(head == nullptr) head = modNodes.front();
            if(tail == nullptr || modNodes.back()->isReferencedBy(tail)) tail = modNodes.back();
            if(isOptionalDestinationGroup)
                destinationGroup = nullptr;
            if(dryRun && destinationGroup)
                return false;
        }
    }
    if(!head || !tail)
        return false;
    OVITO_ASSERT(tail->isReferencedBy(head));

    if(!dryRun) {
        if(destinationGroup && tail == insertAfter)
            destinationGroup = nullptr;

        performTransaction(tr("Move modifier"), [&]() {
            // Make the pipeline rearrangement.
            moveModifierRange(head, tail, insertBefore, insertAfter);

            // Update group memberships.
            for(ModificationNode* node : regroupModNodes)
                node->setModifierGroup(destinationGroup);
        });
    }

    return true;
}

/******************************************************************************
* Moves a sequence of modifiers to a new position in the pipeline.
******************************************************************************/
bool PipelineListModel::moveModifierRange(OORef<ModificationNode> head, OORef<ModificationNode> tail, PipelineNode* insertBefore, ModificationNode* insertAfter)
{
    if(insertAfter == head)
        return false;
    if(insertAfter == tail)
        return false;
    if(insertBefore == tail)
        return false;

    // Remove modapps from pipeline.
    head->visitDependents([&](RefMaker* dependent) {
        if(ModificationNode* precedingModNode = dynamic_object_cast<ModificationNode>(dependent)) {
            if(precedingModNode->input() == head) {
                precedingModNode->setInput(tail->input());
            }
        }
        else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->head() == head) {
                pipeline->setHead(tail->input());
            }
        }
    });
    tail->setInput(nullptr);

    // Re-insert modapps into pipeline.
    if(insertBefore) {
        insertBefore->visitDependents([&](RefMaker* dependent) {
            if(ModificationNode* precedingModNode = dynamic_object_cast<ModificationNode>(dependent)) {
                if(precedingModNode->input() == insertBefore) {
                    precedingModNode->setInput(head);
                }
            }
            else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
                if(pipeline->head() == insertBefore) {
                    pipeline->setHead(head);
                }
            }
        });
        tail->setInput(insertBefore);
    }
    else if(insertAfter) {
        tail->setInput(insertAfter->input());
        insertAfter->setInput(head);
    }
    else {
        tail->setInput(selectedPipeline()->head());
        selectedPipeline()->setHead(head);
    }
    refreshList();

    return true;
}

/******************************************************************************
* Determines whether an object (e.g. modifier, vis element, or source) is
* part of more than one pipeline in the scene.
******************************************************************************/
bool PipelineListModel::isSharedObject(RefTarget* obj) const
{
    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(obj)) {
        if(modNode->modifier()) {
            const auto& modNodes = modNode->modifier()->nodes();
            if(modNodes.size() > 1)
                return true;
            QSet<Pipeline*> pipelines;
            for(ModificationNode* mn : modNodes)
                pipelines.unite(mn->pipelines(true));
            return pipelines.size() > 1;
        }
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(obj)) {
        return group->pipelines(true).size() > 1;
    }
    else if(PipelineNode* node = dynamic_object_cast<PipelineNode>(obj)) {
        return node->pipelines(true).size() > 1;
    }
    else if(DataVis* visElement = dynamic_object_cast<DataVis>(obj)) {
        return visElement->pipelines(true).size() > 1;
    }
    return false;
}

/******************************************************************************
* Moves a list item up one position in the stack.
******************************************************************************/
void PipelineListModel::moveItemUp(PipelineListItem* item)
{
    if(!item) return;

    if(OORef<ModificationNode> modNode = dynamic_object_cast<ModificationNode>(item->object())) {
        performTransaction(tr("Move modifier up"), [modNode]() {
            if(OORef<ModificationNode> predecessor = modNode->getPredecessorModNode()) {
                OVITO_ASSERT(!predecessor->pipelines(true).empty());
                if(modNode->modifierGroup() != nullptr && predecessor->modifierGroup() != modNode->modifierGroup()) {
                    // If the modifier node is the first entry in a modifier group, move it out of the group.
                    modNode->setModifierGroup(nullptr);
                }
                else if(modNode->modifierGroup() == nullptr && predecessor->modifierGroup() != nullptr && predecessor->modifierGroup()->isCollapsed() == false) {
                    // If the modifier node is preceded by a modifier group that is currently expanded, move the modifier node into the group.
                    modNode->setModifierGroup(predecessor->modifierGroup());
                }
                else if(modNode->modifierGroup() == nullptr && predecessor->modifierGroup() != nullptr && predecessor->modifierGroup()->isCollapsed() == true) {
                    // If the modifier node is preceded by a modifier group that is currently collapsed, move the modifier node above the entire group.
                    OORef<ModificationNode> current = predecessor;
                    for(;;) {
                        ModificationNode* next = nullptr;
                        current->visitDependents([&](RefMaker* dependent2) {
                            if(ModificationNode* predecessor2 = dynamic_object_cast<ModificationNode>(dependent2)) {
                                if(predecessor2->modifierGroup() != predecessor->modifierGroup())
                                    predecessor2->setInput(modNode);
                                else
                                    next = predecessor2;
                            }
                            else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent2)) {
                                if(pipeline->head() == current)
                                    pipeline->setHead(modNode);
                            }
                        });
                        if(!next) break;
                        current = next;
                    }
                    predecessor->setInput(modNode->input());
                    modNode->setInput(current);
                }
                else {
                    // Standard case: If the modifier application is preceeded by another modifier application, swap the two.
                    predecessor->visitDependents([&](RefMaker* dependent2) {
                        if(ModificationNode* predecessor2 = dynamic_object_cast<ModificationNode>(dependent2)) {
                            OVITO_ASSERT(predecessor2->input() == predecessor);
                            predecessor2->setInput(modNode);
                        }
                        else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent2)) {
                            if(pipeline->head() == predecessor)
                                pipeline->setHead(modNode);
                        }
                    });
                    predecessor->setInput(modNode->input());
                    modNode->setInput(predecessor);
                }
            }
            else if(modNode->modifierGroup() != nullptr) {
                modNode->setModifierGroup(nullptr);
            }
        });
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
        // Determine the nodes that form the head and the tail for the group.
        QVector<ModificationNode*> groupModNodes = group->nodes();
        OORef<ModificationNode> headModNode = groupModNodes.front();
        OORef<ModificationNode> tailModNode = groupModNodes.back();
        ModificationNode* predecessor = headModNode->getPredecessorModNode();
        OVITO_ASSERT(tailModNode->isReferencedBy(headModNode));
        OVITO_ASSERT(!predecessor || !headModNode->isPipelineBranch(true));

        // Don't move the group it is preceded by a pipeline branch or no modification node at all.
        if(!predecessor)
            return;

        // Determine where to reinsert the group of modifiers into the pipeline.
        OORef<ModificationNode> insertBefore = predecessor;
        if(predecessor->modifierGroup() != nullptr) {
            for(;;) {
                ModificationNode* prev = nullptr;
                insertBefore->visitDependents([&](RefMaker* dependent) {
                    if(ModificationNode* predecessor2 = dynamic_object_cast<ModificationNode>(dependent)) {
                        OVITO_ASSERT(!predecessor2->isPipelineBranch(true));
                        if(predecessor2->modifierGroup() == predecessor->modifierGroup()) {
                            insertBefore = predecessor2;
                            prev = predecessor2;
                        }
                    }
                });
                if(!prev) break;
            }
        }

        // Make the pipeline rearrangement.
        performTransaction(tr("Move modifier group up"), [&]() {
            insertBefore->visitDependents([&](RefMaker* dependent) {
                if(ModificationNode* predecessor = dynamic_object_cast<ModificationNode>(dependent)) {
                    OVITO_ASSERT(predecessor->input() == insertBefore);
                    predecessor->setInput(headModNode);
                }
                else if(Pipeline* predecessor = dynamic_object_cast<Pipeline>(dependent)) {
                    if(predecessor->head() == insertBefore)
                        predecessor->setHead(headModNode);
                }
            });
            predecessor->setInput(tailModNode->input());
            tailModNode->setInput(insertBefore);
        });
    }
    refreshList();
}

/******************************************************************************
* Moves a list item down one position in the stack.
******************************************************************************/
void PipelineListModel::moveItemDown(PipelineListItem* item)
{
    if(!item) return;

    if(OORef<ModificationNode> modNode = dynamic_object_cast<ModificationNode>(item->object())) {
        performTransaction(tr("Move modifier down"), [modNode]() {
            OORef<ModificationNode> successor = dynamic_object_cast<ModificationNode>(modNode->input());
            if(successor && successor->isPipelineBranch(true) == false) {
                if(modNode->modifierGroup() != nullptr && successor->modifierGroup() != modNode->modifierGroup()) {
                    // If the modifier node is the last entry in the modifier group, move it out of the group.
                    modNode->setModifierGroup(nullptr);
                }
                else if(modNode->modifierGroup() == nullptr && successor->modifierGroup() != nullptr && successor->modifierGroup()->isCollapsed() == false) {
                    // If the modifier node is above a group that is currently expanded, move it into the group.
                    modNode->setModifierGroup(successor->modifierGroup());
                }
                else {
                    // Standard case: If the modifier node is followed by another modifier node, swap the two.
                    OORef<ModificationNode> insertAfter = successor;

                    // If the modifier node is above a group that is currently collapsed, move it all the way below that group.
                    if(modNode->modifierGroup() == nullptr && successor->modifierGroup() != nullptr && successor->modifierGroup()->isCollapsed() == true) {
                        while(ModificationNode* next = dynamic_object_cast<ModificationNode>(insertAfter->input())) {
                            if(next->modifierGroup() != successor->modifierGroup())
                                break;
                            insertAfter = next;
                        }
                    }

                    // Make the pipeline rearrangement.
                    modNode->visitDependents([&](RefMaker* dependent) {
                        if(ModificationNode* predecessor = dynamic_object_cast<ModificationNode>(dependent)) {
                            predecessor->setInput(successor);
                        }
                        else if(Pipeline* predecessor = dynamic_object_cast<Pipeline>(dependent)) {
                            if(predecessor->head() == modNode)
                                predecessor->setHead(successor);
                        }
                    });
                    modNode->setInput(insertAfter->input());
                    insertAfter->setInput(modNode);
                }
            }
            else if(modNode->modifierGroup() != nullptr) {
                modNode->setModifierGroup(nullptr);
            }
        });
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
        QVector<ModificationNode*> groupModNodes = group->nodes();
        OORef<ModificationNode> headModNode = groupModNodes.front();
        OORef<ModificationNode> tailModNode = groupModNodes.back();
        ModificationNode* successor = dynamic_object_cast<ModificationNode>(tailModNode->input());

        // Don't move the group over a pipeline branch.
        if(!successor || successor->isPipelineBranch(true))
            return;

        // Determine where to reinsert the group of modifiers into the pipeline.
        OORef<ModificationNode> insertAfter = successor;
        if(successor->modifierGroup() != nullptr) {
            while(ModificationNode* next = dynamic_object_cast<ModificationNode>(insertAfter->input())) {
                if(next->modifierGroup() != successor->modifierGroup())
                    break;
                insertAfter = next;
            }
        }

        // Make the pipeline rearrangement.
        performTransaction(tr("Move modifier group down"), [&]() {
            headModNode->visitDependents([&](RefMaker* dependent) {
                if(ModificationNode* predecessor = dynamic_object_cast<ModificationNode>(dependent)) {
                    predecessor->setInput(successor);
                }
                else if(Pipeline* predecessor = dynamic_object_cast<Pipeline>(dependent)) {
                    if(predecessor->head() == headModNode)
                        predecessor->setHead(successor);
                }
            });
            tailModNode->setInput(insertAfter->input());
            insertAfter->setInput(headModNode);
        });
    }
    refreshList();
}

/******************************************************************************
* Replaces the selected pipeline item with an independent copy.
******************************************************************************/
void PipelineListModel::makeElementIndependent()
{
    // Get the currently selected pipeline item.
    PipelineListItem* item = selectedItem();
    if(!item) return;

    if(DataVis* visElement = dynamic_object_cast<DataVis>(item->object())) {
        performTransaction(tr("Make visual element independent"), [&]() {
            Pipeline* pipeline = selectedPipeline();
            DataVis* replacementVisElement = pipeline->makeVisElementIndependent(visElement);
            setNextObjectToSelect(replacementVisElement);
        });
    }
    else if(PipelineNode* selectedPipelineNode = dynamic_object_cast<PipelineNode>(item->object())) {
        performTransaction(tr("Make pipeline element independent"), [&]() {
            CloneHelper cloneHelper;
            if(PipelineNode* clonedNode = makeElementIndependentImpl(selectedPipelineNode, cloneHelper))
                setNextObjectToSelect(clonedNode);
        });
    }
    else if(ModifierGroup* selectedGroup = dynamic_object_cast<ModifierGroup>(item->object())) {
        performTransaction(tr("Make modifier group independent"), [&]() {
            CloneHelper cloneHelper;
            for(ModificationNode* modNode : selectedGroup->nodes()) {
                ModificationNode* clonedModNode = static_object_cast<ModificationNode>(makeElementIndependentImpl(modNode, cloneHelper));
                OVITO_ASSERT(clonedModNode);
                if(clonedModNode && clonedModNode->modifierGroup()) {
                    setNextObjectToSelect(clonedModNode->modifierGroup());
                }
            }
        });
    }
    refreshList();
}

/******************************************************************************
* Replaces the a pipeline item with an independent copy.
******************************************************************************/
PipelineNode* PipelineListModel::makeElementIndependentImpl(PipelineNode* pipelineNode, CloneHelper& cloneHelper)
{
    OORef<PipelineNode> currentNode = selectedPipeline()->head();
    ModificationNode* predecessorModNode = nullptr;
    // Walk up the pipeline, starting at the head, until we reach the selected pipeline node.
    // Duplicate all shared ModificationNodes to remove pipeline branches.
    // When arriving at the selected ModificationNode, duplicate the modifier too
    // in case it is being shared by multiple pipelines.
    while(currentNode) {
        if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(currentNode.get())) {

            // Clone all modification nodes along the way if they are shared by multiple pipeline branches.
            if(modNode->pipelines(true).size() > 1) {
                OORef<ModificationNode> clonedModNode = cloneHelper.cloneObject(modNode, false);
                if(predecessorModNode)
                    predecessorModNode->setInput(clonedModNode);
                else
                    selectedPipeline()->setHead(clonedModNode);
                predecessorModNode = clonedModNode;
            }
            else {
                predecessorModNode = modNode;
            }

            // Terminate pipeline walk at the target object to be made independent.
            if(currentNode == pipelineNode) {
                // Clone the selected modifier if it is referenced by multiple nodes.
                if(predecessorModNode->modifier()) {
                    if(predecessorModNode->modifier()->nodes().size() > 1)
                        predecessorModNode->setModifier(cloneHelper.cloneObject(predecessorModNode->modifier(), true));
                }
                return predecessorModNode;
            }
            currentNode = predecessorModNode->input();
        }
        else if(currentNode == pipelineNode) {
            // If the node to be made independent is not a modifier node, simply clone it.
            if(currentNode->pipelines(true).size() > 1) {
                OORef<PipelineNode> clonedNode = cloneHelper.cloneObject(currentNode, false);
                if(predecessorModNode)
                    predecessorModNode->setInput(clonedNode);
                else
                    selectedPipeline()->setHead(clonedNode);
                return clonedNode.get();
            }
            return currentNode.get();
        }
        else {
            OVITO_ASSERT(false);
            break;
        }
    }
    return nullptr;
}

/******************************************************************************
* Creates or dissolves a group of modifiers.
******************************************************************************/
void PipelineListModel::toggleModifierGroup()
{
    QVector<RefTarget*> objects = selectedObjects();
    if(objects.empty()) return;

    OORef<ModifierGroup> existingGroup;

    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(objects.front())) {
        // If modifier nodes are currently selected, put them into a new group.
        // But first make sure the modifier nodes aren't already part of an existing group.
        existingGroup = modNode->modifierGroup();
        if(!existingGroup) {
            performTransaction(tr("Create modifier group"), [&]() {
                // Create a new group.
                OORef<ModifierGroup> group = OORef<ModifierGroup>::create();
                for(RefTarget* obj : objects) {
                    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(obj)) {
                        modNode->setModifierGroup(group);
                    }
                }
                setNextObjectToSelect(group);
                refreshList();
            });
            return;
        }
    }

    // If an existing modifier group is currently selected, dissolve the group.
    if(!existingGroup)
        existingGroup = dynamic_object_cast<ModifierGroup>(objects.front());
    if(existingGroup) {
        performTransaction(tr("Dissolve modifier group"), [&]() {
            QVector<ModificationNode*> groupModNodes = existingGroup->nodes();
            setNextObjectToSelect(groupModNodes.front());
            for(ModificationNode* modNode : groupModNodes) {
                if(modNode->modifierGroup() == existingGroup)
                    modNode->setModifierGroup(nullptr);
            }
            existingGroup->requestObjectDeletion();
        });
    }
    refreshList();
}

/******************************************************************************
* Determines whether an object is a visual element that is shared by more
* than one data object in the same pipeline.
******************************************************************************/
int PipelineListModel::isSharedVisualElement(DataVis* visElement) const
{
    OVITO_ASSERT(visElement);

    if(!selectedPipeline())
        return 0;

    if(const QStringList* titles = selectedPipeline()->getDataObjectTitlesForVisElement(visElement)) {
        if(titles->size() > 1)
            return titles->size();
    }
    return 0;
}

/******************************************************************************
* Determines whether the given objects are all visual elements of the same
* class that can be collapsed into a single visual element.
******************************************************************************/
bool PipelineListModel::canShareVisualElements(const QVector<RefTarget*>& objects) const
{
    if(objects.size() < 2)
        return false;

    DataVis* masterObject = dynamic_object_cast<DataVis>(objects.front());
    if(!masterObject)
        return false;

    // Check if all objects share the same uniform DataVis class.
    OvitoClassPtr visElementClass = &masterObject->getOOMetaClass();
    for(const RefTarget* obj : objects) {
        if(&obj->getOOMetaClass() != visElementClass)
            return false;
    }

    return true;
}

/******************************************************************************
* Combines several compatible visual elements by replacing them all with a single instance
* or performs the reverse operation, creating several independent instances of the
* selected visual element.
******************************************************************************/
void PipelineListModel::shareOrSplitVisualElements()
{
    const QVector<RefTarget*> objects = selectedObjects();
    if(objects.empty())
        return;

    if(canShareVisualElements(objects)) {
        performTransaction(tr("Replace with shared visual element"), [&]() {
            OVITO_ASSERT(objects.size() > 1);
            DataVis* masterObject = static_object_cast<DataVis>(objects.front());
            masterObject->setTitle({}); // Discard user-defined title
            for(OORef<RefTarget> obj : objects) {
                if(obj != masterObject) {
                    // Tell this visual element to replace itself with the master element.
                    // Some visual element types implement special logic to do this correctly,
                    // e.g., if they own a PropertyColorMapping sub-object, they update direct references
                    // to this sub-object too.
                    static_object_cast<DataVis>(obj)->replaceWithSharedElement(masterObject);
                }
            }
        });
    }
    else if(objects.size() == 1) {
        if(OORef<DataVis> visElement = dynamic_object_cast<DataVis>(objects.front())) {
            if(isSharedVisualElement(visElement)) {
                performTransaction(tr("Split into multiple visual elements"), [&]() {
                    selectedPipeline()->replaceVisualElement(visElement, [visElement, first=true](const QString& title) mutable -> OORef<DataVis> {
                        if(first) {
                            first = false;
                            visElement->setTitle(title);
                            return visElement;
                        }
                        else {
                            OORef<DataVis> clone = CloneHelper::cloneSingleObject(visElement, true);
                            clone->setTitle(title);
                            return clone;
                        }
                    });
                });
            }
        }
    }
}

}   // End of namespace
