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
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include "PipelineListModel.h"

#include <boost/range/algorithm_ext/is_sorted.hpp>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineListModel::PipelineListModel(UserInterface& userInterface, QObject* parent) : QAbstractListModel(parent),
    _userInterface(userInterface),
    _statusInfoIcon(":/guibase/mainwin/status/status_info.png"),
    _statusWarningIcon(":/guibase/mainwin/status/status_warning.png"),
    _statusErrorIcon(":/guibase/mainwin/status/status_error.png"),
    _statusNoneIcon(":/guibase/mainwin/status/status_none.png"),
    _statusPendingIcon(":/guibase/mainwin/status/status_pending.gif"),
    _sectionHeaderFont(QGuiApplication::font()),
    _modifierGroupCollapsed(QIcon::fromTheme("modify_modifier_group_collapsed")),
    _modifierGroupExpanded(QIcon::fromTheme("modify_modifier_group_expanded"))
{
    OVITO_ASSERT(userInterface.actionManager());

    // Create a selection model.
    _selectionModel = new QItemSelectionModel(this);

    // Connect signals and slots.
    connect(&_selectedPipeline, &RefTargetListener<PipelineSceneNode>::notificationEvent, this, &PipelineListModel::onPipelineEvent);
    connect(&userInterface.datasetContainer(), &DataSetContainer::selectionChangeComplete, this, &PipelineListModel::onSceneSelectionChangeComplete);
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
    _deleteItemAction = userInterface.actionManager()->createCommandAction(ACTION_MODIFIER_DELETE, tr("Delete Modifier"), "modify_delete_modifier", tr("Delete the selected modifier from the pipeline."));
    connect(_deleteItemAction, &QAction::triggered, this, &PipelineListModel::deleteSelectedItems);
    _moveItemUpAction = userInterface.actionManager()->createCommandAction(ACTION_MODIFIER_MOVE_UP, tr("Move Modifier Up"), "modify_modifier_move_up", tr("Move the selected modifier up in the pipeline."));
    connect(_moveItemUpAction, &QAction::triggered, this, &PipelineListModel::moveModifierUp);
    _moveItemDownAction = userInterface.actionManager()->createCommandAction(ACTION_MODIFIER_MOVE_DOWN, tr("Move Modifier Down"), "modify_modifier_move_down", tr("Move the selected modifier down in the pipeline."));
    connect(_moveItemDownAction, &QAction::triggered, this, &PipelineListModel::moveModifierDown);
    _toggleModifierGroupAction = userInterface.actionManager()->createCommandAction(ACTION_PIPELINE_TOGGLE_MODIFIER_GROUP, tr("Group Modifiers"), "modify_modifier_group_create", tr("Creates or dissolves a group of modifiers in the pipeline editor."));
    _toggleModifierGroupAction->setCheckable(true);
    connect(_toggleModifierGroupAction, &QAction::triggered, this, &PipelineListModel::toggleModifierGroup);
    _makeElementIndependentAction = userInterface.actionManager()->createCommandAction(ACTION_PIPELINE_MAKE_INDEPENDENT, tr("Make Independent"), "modify_make_element_independent", tr("Duplicate an item shared by multiple pipelines to make it independent from the other pipeline(s)."));
    connect(_makeElementIndependentAction, &QAction::triggered, this, &PipelineListModel::makeElementIndependent);
    _copyItemToPipelineAction = userInterface.actionManager()->createCommandAction(ACTION_PIPELINE_COPY_ITEM, tr("Copy To..."), "modify_pipeline_copy_item_to", tr("Copy an item to another pipeline or within the current pipeline."));
    _renamePipelineItemAction = userInterface.actionManager()->createCommandAction(ACTION_PIPELINE_RENAME_ITEM, tr("Rename..."), "edit_rename_pipeline_item", tr("Rename the selected pipeline entry."));

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
    PipelineSceneNode* pipeline = selection ? dynamic_object_cast<PipelineSceneNode>(selection->firstNode()) : nullptr;
    if(pipeline != selectedPipeline()) {
        _selectedPipeline.setTarget(pipeline);
        refreshListLater();
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
        return;
    }

    // Determine the currently selected objects and select them again after the list has been rebuilt.
    // _nextObjectToSelect may have been set to replace the selection.
    if(!_nextObjectToSelect && _previouslySelectedPipeline.data() == selectedPipeline()) {
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
        PipelineObject* pipelineObject = selectedPipeline()->dataProvider();
        PipelineObject* firstPipelineObj = pipelineObject;
        ModifierGroup* currentGroup = nullptr;
        while(pipelineObject) {

            // Create entries for the modifier applications.
            if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(pipelineObject)) {

                if(pipelineObject == firstPipelineObj)
                    appendListItem(nullptr, PipelineListItem::ModificationsHeader);

                if(pipelineObject->isPipelineBranch(true))
                    appendListItem(nullptr, PipelineListItem::PipelineBranch);

                if(modApp->modifierGroup() != currentGroup) {
                    if(modApp->modifierGroup())
                        appendListItem(modApp->modifierGroup(), PipelineListItem::ModifierGroup);
                    currentGroup = modApp->modifierGroup();
                }

                if(!currentGroup || !currentGroup->isCollapsed())
                    appendListItem(modApp, PipelineListItem::Modifier);

                pipelineObject = modApp->input();
            }
            else if(pipelineObject) {

                if(pipelineObject->isPipelineBranch(true))
                    appendListItem(nullptr, PipelineListItem::PipelineBranch);

                appendListItem(nullptr, PipelineListItem::DataSourceHeader);

                // Create a list item for the data source.
                PipelineListItem* item = appendListItem(pipelineObject, PipelineListItem::DataSource);

                // Create list items for the source's editable data objects.
                if(const DataCollection* collection = pipelineObject->getSourceDataCollection()) {
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
* object (and their subobjects).
******************************************************************************/
void PipelineListModel::createListItemsForSubobjects(const DataObject* dataObj, PipelineListItem* parentItem)
{
    if(dataObj->showInPipelineEditor() && dataObj->editableProxy()) {
        parentItem = appendListItem(dataObj->editableProxy(), PipelineListItem::DataObject, parentItem);
    }

    // Recursively visit the sub-objects of the data object.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        createListItemsForSubobjects(subObject, parentItem);
        return false;
    });
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
                else if(itemType == PipelineListItem::DataObject && oldItem->itemType() == PipelineListItem::DataObject && oldItem->title() == item->title()) {
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
* Handles notification events generated by the selected pipeline node.
******************************************************************************/
void PipelineListModel::onPipelineEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Update the entire modification list if the PipelineSceneNode has been assigned a new
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
    AnimationTime time = datasetContainer().currentAnimationTime();

    // Get the selected pipeline item. The new modifier is inserted right behind it in the pipeline.
    PipelineListItem* currentItem = selectedItem();

    if(currentItem) {
        while(currentItem->parent()) {
            currentItem = currentItem->parent();
        }
        
        RefTarget* selectedObject = currentItem->object();
        if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(selectedObject)) {
            selectedObject = group->modifierApplications().first();
        }

        if(OORef<PipelineObject> pobj = dynamic_object_cast<PipelineObject>(selectedObject)) {
            
            ModifierGroup* modifierGroup = nullptr;
            if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(selectedObject)) {
                if(selectedObject == currentItem->object())
                    modifierGroup = modApp->modifierGroup();
            }
            if(!modifierGroup)
                modifierGroup = group;

            for(int i = modifiers.size() - 1; i >= 0; i--) {
                Modifier* modifier = modifiers[i];
                std::vector<OORef<RefMaker>> dependentsList;
                pobj->visitDependents([&](RefMaker* dependent) {
                    if(dynamic_object_cast<ModifierApplication>(dependent) || dynamic_object_cast<PipelineSceneNode>(dependent)) {
                        dependentsList.push_back(dependent);
                    }
                });
                OORef<ModifierApplication> modApp = modifier->createModifierApplication();
                modApp->setModifier(modifier);
                modApp->setInput(pobj);
                modApp->setModifierGroup(modifierGroup);
                modifier->initializeModifier(ModifierInitializationRequest(time, modApp));
                setNextObjectToSelect(modApp);
                for(RefMaker* dependent : dependentsList) {
                    if(ModifierApplication* predecessorModApp = dynamic_object_cast<ModifierApplication>(dependent)) {
                        predecessorModApp->setInput(modApp);
                    }
                    else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                        if(pipeline->dataProvider() == pobj)
                            pipeline->setDataProvider(modApp);
                    }
                }
                pobj = modApp;
            }
            if(group)
                setNextObjectToSelect(group);
            return;
        }
    }

    // Insert modifiers at the end of the selected pipelines.
    for(int index = modifiers.size() - 1; index >= 0; --index) {
        ModifierApplication* modApp = selectedPipeline()->applyModifier(time, modifiers[index]);
        if(group)
            modApp->setModifierGroup(group);
        else
            setNextObjectToSelect(modApp);
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

    // Build list of modapps to delete from the pipeline.
    std::set<ModifierApplication*> modApps;
    for(PipelineListItem* item : items) {
        if(OORef<ModifierApplication> modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
            modApps.insert(modApp);
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            for(ModifierApplication* modApp : group->modifierApplications())
                modApps.insert(modApp);
        }
    }

    // Perform the deletion one by one.
    _userInterface.performTransaction(tr("Delete modifier"), [&]() {
        for(ModifierApplication* modApp : modApps) {
            deleteModifierApplication(modApp);
        }
    });

    refreshList();
}

/******************************************************************************
* Deletes a modifier application from the pipeline.
******************************************************************************/
void PipelineListModel::deleteModifierApplication(ModifierApplication* modApp)
{
    _userInterface.performTransaction(tr("Delete modifier"), [modApp = OORef<ModifierApplication>(modApp), this]() {
        modApp->visitDependents([&](RefMaker* dependent) {
            if(ModifierApplication* precedingModApp = dynamic_object_cast<ModifierApplication>(dependent)) {
                if(precedingModApp->input() == modApp) {
                    setNextObjectToSelect(modApp->input());
                    precedingModApp->setInput(modApp->input());
                }
            }
            else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                if(pipeline->dataProvider() == modApp) {
                    setNextObjectToSelect(modApp->input());
                    pipeline->setDataProvider(modApp->input());
                }
            }
        });
        modApp->deleteReferenceObject();
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
    OVITO_ASSERT(index.row() >= 0 && index.row() < _items.size());

    PipelineListItem* item = this->item(index.row());

    if(role == Qt::DisplayRole || role == TitleRole) {
        // Indent modifiers that are part of a group.
        if(item->itemType() == PipelineListItem::Modifier) {
            if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
                if(modApp->modifierGroup())
#ifndef Q_OS_WIN
                    return QStringLiteral(" ") + item->title();
#else
                    return QStringLiteral("   ") + item->title();
#endif
            }
        }
        return item->title();
    }
    else if(role == Qt::EditRole) {
        return item->title();
    }
    else if(role == ItemTypeRole) {
        return item->itemType();
    }
    else if(role == IsCollapsedRole) {
        if(item->itemType() == PipelineListItem::ModifierGroup)
            return static_object_cast<ModifierGroup>(item->object())->isCollapsed();
    }
    else if(role == StatusInfoRole) {
        if(PipelineSceneNode* pipeline = selectedPipeline()) {
            QVariant v;
            if(_userInterface.handleExceptions([&] {
                v = item->shortInfo(pipeline);
            })) return v;
        }
    }
    else if(role == Qt::DecorationRole) {
        // This role is only used by the QWidgets GUI.
        if(item->itemType() == PipelineListItem::ModifierGroup) {
            if(!static_object_cast<ModifierGroup>(item->object())->isCollapsed())
                return _modifierGroupExpanded;
        }
        if(item->isObjectActive()) {
            const_cast<QMovie&>(_statusPendingIcon).start();
            return QVariant::fromValue(_statusPendingIcon.currentPixmap());
        }
        if(item->itemType() == PipelineListItem::ModifierGroup) {
            if(item->status().type() == PipelineStatus::Success)
                return _modifierGroupCollapsed;
        }
        if(item->isObjectItem()) {
            switch(item->status().type()) {
            case PipelineStatus::Warning: return QVariant::fromValue(_statusWarningIcon);
            case PipelineStatus::Error: return QVariant::fromValue(_statusErrorIcon);
            default: return QVariant::fromValue(_statusNoneIcon);
            }
        }
    }
    else if(role == PipelineListModel::DecorationRole) {
        // This role is only used by the QML GUI.
        if(item->itemType() == PipelineListItem::ModifierGroup) {
            if(!static_object_cast<ModifierGroup>(item->object())->isCollapsed())
                return QStringLiteral("modify_modifier_group_expanded");
        }
        if(item->itemType() == PipelineListItem::ModifierGroup) {
            if(item->status().type() == PipelineStatus::Success)
                return QStringLiteral("modify_modifier_group_collapsed");
        }
        if(item->isObjectItem()) {
            switch(item->status().type()) {
            case PipelineStatus::Warning: return QStringLiteral("qrc:/guibase/mainwin/status/status_warning.png");
            case PipelineStatus::Error: return QStringLiteral("qrc:/guibase/mainwin/status/status_error.png");
            default: return QStringLiteral("qrc:/guibase/mainwin/status/status_none.png");
            }
        }
        return QString();
    }
    else if(role == Qt::ToolTipRole || role == PipelineListModel::ToolTipRole) {
        return QVariant::fromValue(item->status().text());
    }
    else if(role == Qt::CheckStateRole) {
        if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(item->object()))
            return (modApp->modifier() && modApp->modifier()->isEnabled()) ? Qt::Checked : Qt::Unchecked;
        else if(ActiveObject* object = dynamic_object_cast<ActiveObject>(item->object())) {
            if(item->itemType() != PipelineListItem::DataSource)
                return object->isEnabled() ? Qt::Checked : Qt::Unchecked;
        }
    }
    else if(role == CheckedRole) {
        if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(item->object()))
            return modApp->modifier() && modApp->modifier()->isEnabled();
        else if(ActiveObject* object = dynamic_object_cast<ActiveObject>(item->object())) {
            if(item->itemType() != PipelineListItem::DataSource)
                return object->isEnabled();
        }
        return false;
    }
    else if(role == Qt::TextAlignmentRole) {
        if(!item->isObjectItem()) {
            return Qt::AlignCenter;
        }
    }
    else if(role == Qt::BackgroundRole) {
        if(!item->isObjectItem()) {
            if(item->itemType() != PipelineListItem::PipelineBranch)
                return _sectionHeaderBackgroundBrush;
            else
                return QBrush(Qt::lightGray, Qt::Dense6Pattern);
        }
    }
    else if(role == Qt::ForegroundRole) {
        if(!item->isObjectItem())
            return _sectionHeaderForegroundBrush;
        else if(item->itemType() == PipelineListItem::Modifier && static_object_cast<ModifierApplication>(item->object())->modifierAndGroupEnabled() == false)
            return _disabledForegroundBrush;
        else if(item->itemType() == PipelineListItem::ModifierGroup && static_object_cast<ModifierGroup>(item->object())->isEnabled() == false)
            return _disabledForegroundBrush;
    }
    else if(role == Qt::FontRole) {
        if(!item->isObjectItem())
            return _sectionHeaderFont;
        else if(isSharedObject(item->object()))
            return _sharedObjectFont;
    }

    return {};
}

/******************************************************************************
* Changes the data associated with a list entry.
******************************************************************************/
bool PipelineListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role == Qt::CheckStateRole || role == CheckedRole) {
        PipelineListItem* item = this->item(index.row());
        if(DataVis* vis = dynamic_object_cast<DataVis>(item->object())) {
            _userInterface.performTransaction((value.toBool()) ? tr("Enable visual element") : tr("Disable visual element"), [vis, &value]() {
                vis->setEnabled(value.toBool());
            });
            return true;
        }
        else if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
            _userInterface.performTransaction((value.toInt() != Qt::Unchecked) ? tr("Enable modifier") : tr("Disable modifier"), [modApp, &value, index, role, this]() {
                if(modApp->modifier()) 
                    modApp->modifier()->setEnabled(value.toInt() != Qt::Unchecked);
            });
            return true;
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            _userInterface.performTransaction((value.toBool()) ? tr("Enable modifier group") : tr("Disable modifier group"), [group, &value]() {
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
                _userInterface.performTransaction(tr("Rename visual element"), [vis, &newName]() {
                    vis->setObjectTitle(newName);
                });
            }
            return true;
        }
        else if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
            QString newName = value.toString();
            if(modApp->modifier() && modApp->modifier()->objectTitle() != newName) {
                _userInterface.performTransaction(tr("Rename modifier"), [modApp, &newName]() {
                    modApp->modifier()->setObjectTitle(newName);
                });
            }
            return true;
        }
        else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
            QString newName = value.toString();
            if(group->objectTitle() != newName) {
                _userInterface.performTransaction(tr("Rename modifier group"), [group, &newName]() {
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
    RefTarget* currentObject = (_itemsRefreshPending.empty() && objects.size() == 1) ? objects.front() : nullptr;

    // Check if all selected objects are deletable.
    _deleteItemAction->setEnabled(!objects.empty() && boost::algorithm::all_of(objects, [](RefTarget* obj) {
        return dynamic_object_cast<ModifierApplication>(obj) || dynamic_object_cast<ModifierGroup>(obj);
    }));
    if(objects.size() == 1 && dynamic_object_cast<ModifierApplication>(objects[0]))
        _deleteItemAction->setText(tr("Delete Modifier"));
    else if(objects.size() == 1 && dynamic_object_cast<ModifierGroup>(objects[0]))
        _deleteItemAction->setText(tr("Delete Modifier Group"));
    else
        _deleteItemAction->setText(tr("Delete"));

    // Check if the selected object is a shared object which can be made independent.
    _makeElementIndependentAction->setEnabled(
        isSharedObject(currentObject)
        && (dynamic_object_cast<ModifierApplication>(currentObject) == nullptr || static_object_cast<ModifierApplication>(currentObject)->modifierGroup() == nullptr || static_object_cast<ModifierApplication>(currentObject)->pipelines(true).size() == 1));

    _copyItemToPipelineAction->setEnabled(boost::algorithm::any_of(objects, [](RefTarget* obj) {
        return dynamic_object_cast<PipelineObject>(obj) || dynamic_object_cast<ModifierGroup>(obj);
    }));

    _renamePipelineItemAction->setEnabled(ModifierApplication::OOClass().isMember(currentObject) || ModifierGroup::OOClass().isMember(currentObject) || DataVis::OOClass().isMember(currentObject));

    // Update the state of the move up/down actions.
    if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(currentObject)) {
        _moveItemDownAction->setText(tr("Move Modifier Down"));
        _moveItemDownAction->setEnabled(
            modApp->input()
            && (dynamic_object_cast<ModifierApplication>(modApp->input()) != nullptr || modApp->modifierGroup() != nullptr)
            && (modApp->input()->isPipelineBranch(true) == false || modApp->modifierGroup() != nullptr)
            && modApp->pipelines(true).empty() == false
            && (modApp->modifierGroup() == nullptr || modApp->modifierGroup()->modifierApplications().size() > 1));

        _moveItemUpAction->setText(tr("Move Modifier Up"));
        _moveItemUpAction->setEnabled(
            (modApp->getPredecessorModApp() != nullptr || modApp->modifierGroup() != nullptr)
            && (modApp->isPipelineBranch(true) == false || modApp->modifierGroup() != nullptr)
            && modApp->pipelines(true).empty() == false
            && (modApp->modifierGroup() == nullptr || modApp->modifierGroup()->modifierApplications().size() > 1));
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(currentObject)) {
        _moveItemUpAction->setEnabled(false);
        _moveItemDownAction->setEnabled(false);
        _moveItemUpAction->setText(tr("Move Modifier Group Up"));
        _moveItemDownAction->setText(tr("Move Modifier Group Down"));

        // Determine whether it would be possible to move the entire modifier group up and/or down.
        if(group->pipelines(true).empty() == false) {
            QVector<ModifierApplication*> groupModApps = group->modifierApplications();
            if(ModifierApplication* inputModApp = dynamic_object_cast<ModifierApplication>(groupModApps.back()->input())) {
                OVITO_ASSERT(inputModApp->modifierGroup() != group);
                _moveItemDownAction->setEnabled(!inputModApp->isPipelineBranch(true));
            }
            _moveItemUpAction->setEnabled(groupModApps.front()->getPredecessorModApp() != nullptr);
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
    // Are all selected objects modifier applications and are they not in a group?
    if(!objects.empty() && boost::algorithm::all_of(objects, [](RefTarget* obj) { 
            ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj);
            return modApp && modApp->modifierGroup() == nullptr; })) 
    {
        // Do all selected modifier applications form a contiguous sequence.
        bool isContinguousSequence = true;
        for(auto obj = std::next(objects.cbegin()); obj != objects.cend(); ++obj) {
            if(static_object_cast<ModifierApplication>(*obj) != static_object_cast<ModifierApplication>(*std::prev(obj))->input()) {
                isContinguousSequence = false;
                break;
            }
        }
        if(isContinguousSequence) {
            _toggleModifierGroupAction->setEnabled(true);
        }
    }
    else if(dynamic_object_cast<ModifierGroup>(currentObject) != nullptr) {
        _toggleModifierGroupAction->setEnabled(true);
        _toggleModifierGroupAction->setChecked(true);
        _toggleModifierGroupAction->setText(tr("Ungroup Modifiers"));
    }
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
    PipelineObject* insertBefore = nullptr;
    ModifierApplication* insertAfter = nullptr;
    if(insertAfterItem->itemType() == PipelineListItem::ModificationsHeader) {
        insertBefore = nullptr;
    }
    else if(insertAfterItem->itemType() == PipelineListItem::Modifier) {
        insertAfter = static_object_cast<ModifierApplication>(insertAfterItem->object());
        destinationGroup = insertAfter->modifierGroup();
        if(destinationGroup && destinationGroup->modifierApplications().back() == insertAfter)
            isOptionalDestinationGroup = true;
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::Modifier) {
        insertBefore = static_object_cast<ModifierApplication>(insertBeforeItem->object());
        destinationGroup = static_object_cast<ModifierApplication>(insertBeforeItem->object())->modifierGroup();
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::DataSourceHeader) {
        insertBefore = selectedPipeline()->pipelineSource();
    }
    else if(insertAfterItem->itemType() == PipelineListItem::ModifierGroup && insertBeforeItem->itemType() == PipelineListItem::Modifier) {
        insertBefore = static_object_cast<ModifierApplication>(insertBeforeItem->object());
        destinationGroup = static_object_cast<ModifierGroup>(insertAfterItem->object());
    }
    else if(insertAfterItem->itemType() == PipelineListItem::ModifierGroup && static_object_cast<ModifierGroup>(insertAfterItem->object())->isCollapsed()) {
        insertAfter = static_object_cast<ModifierGroup>(insertAfterItem->object())->modifierApplications().back();
    }
    else if(insertBeforeItem->itemType() == PipelineListItem::ModifierGroup) {
        insertBefore = static_object_cast<ModifierGroup>(insertBeforeItem->object())->modifierApplications().first();
    }
    else {
        return false;
    }

    // Determine the contiguous sequence of modifiers to be moved. 
    ModifierApplication* head = nullptr;
    ModifierApplication* tail = nullptr;
    std::vector<ModifierApplication*> regroupModApps;
    for(int row : indexList) {
        if(row <= 0 || row >= items().size())
            return false;
        PipelineListItem* movedItem = item(row);
        if(movedItem->itemType() == PipelineListItem::Modifier) {
            ModifierApplication* modApp = static_object_cast<ModifierApplication>(movedItem->object());
            if(head == nullptr) head = modApp;
            if(tail == nullptr || (modApp->isReferencedBy(tail) && modApp != tail)) {
                tail = modApp;
                regroupModApps.push_back(modApp);
            }
        }
        else if(movedItem->itemType() == PipelineListItem::ModifierGroup) {
            ModifierGroup* group = static_object_cast<ModifierGroup>(movedItem->object());
            const auto& modApps = group->modifierApplications();
            if(head == nullptr) head = modApps.front();
            if(tail == nullptr || modApps.back()->isReferencedBy(tail)) tail = modApps.back();
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

        _userInterface.performTransaction(tr("Move modifier"), [&]() {
            // Make the pipeline rearrangement.
            moveModifierRange(head, tail, insertBefore, insertAfter);

            // Update group memberships.
            for(ModifierApplication* modApp : regroupModApps)
                modApp->setModifierGroup(destinationGroup);
        });
    }

    return true;
}

/******************************************************************************
* Moves a sequence of modifiers to a new position in the pipeline.
******************************************************************************/
bool PipelineListModel::moveModifierRange(OORef<ModifierApplication> head, OORef<ModifierApplication> tail, PipelineObject* insertBefore, ModifierApplication* insertAfter)
{
    if(insertAfter == head)
        return false;
    if(insertAfter == tail)
        return false;
    if(insertBefore == tail)
        return false;

    // Remove modapps from pipeline.
    head->visitDependents([&](RefMaker* dependent) {
        if(ModifierApplication* precedingModApp = dynamic_object_cast<ModifierApplication>(dependent)) {
            if(precedingModApp->input() == head) {
                precedingModApp->setInput(tail->input());
            }
        }
        else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent)) {
            if(pipeline->dataProvider() == head) {
                pipeline->setDataProvider(tail->input());
            }
        }
    });
    tail->setInput(nullptr);

    // Re-insert modapps into pipeline.
    if(insertBefore) {
        insertBefore->visitDependents([&](RefMaker* dependent) {
            if(ModifierApplication* precedingModApp = dynamic_object_cast<ModifierApplication>(dependent)) {
                if(precedingModApp->input() == insertBefore) {
                    precedingModApp->setInput(head);
                }
            }
            else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                if(pipeline->dataProvider() == insertBefore) {
                    pipeline->setDataProvider(head);
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
        tail->setInput(selectedPipeline()->dataProvider());
        selectedPipeline()->setDataProvider(head);
    }
    refreshList();

    return true;
}

/******************************************************************************
* Helper method that determines if the given object is part of more than one pipeline.
******************************************************************************/
bool PipelineListModel::isSharedObject(RefTarget* obj)
{
    if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj)) {
        if(modApp->modifier()) {
            const auto& modApps = modApp->modifier()->modifierApplications();
            if(modApps.size() > 1)
                return true;
            QSet<PipelineSceneNode*> pipelines;
            for(ModifierApplication* ma : modApps)
                pipelines.unite(ma->pipelines(true));
            return pipelines.size() > 1;
        }
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(obj)) {
        return group->pipelines(true).size() > 1;
    }
    else if(PipelineObject* pipelineObject = dynamic_object_cast<PipelineObject>(obj)) {
        return pipelineObject->pipelines(true).size() > 1;
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

    if(OORef<ModifierApplication> modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
        _userInterface.performTransaction(tr("Move modifier up"), [modApp]() {
            if(OORef<ModifierApplication> predecessor = modApp->getPredecessorModApp()) {
                OVITO_ASSERT(!predecessor->pipelines(true).empty());
                if(modApp->modifierGroup() != nullptr && predecessor->modifierGroup() != modApp->modifierGroup()) {
                    // If the modifier application is the first entry in a modifier group, move it out of the group.
                    modApp->setModifierGroup(nullptr);
                }
                else if(modApp->modifierGroup() == nullptr && predecessor->modifierGroup() != nullptr && predecessor->modifierGroup()->isCollapsed() == false) {
                    // If the modifier application is preceded by a modifier group that is currently expanded, move the modifier application into the group.
                    modApp->setModifierGroup(predecessor->modifierGroup());
                }
                else if(modApp->modifierGroup() == nullptr && predecessor->modifierGroup() != nullptr && predecessor->modifierGroup()->isCollapsed() == true) {
                    // If the modifier application is preceded by a modifier group that is currently collapsed, move the modifier application above the entire group.
                    OORef<ModifierApplication> current = predecessor;
                    for(;;) {
                        ModifierApplication* next = nullptr;
                        current->visitDependents([&](RefMaker* dependent2) {
                            if(ModifierApplication* predecessor2 = dynamic_object_cast<ModifierApplication>(dependent2)) {
                                if(predecessor2->modifierGroup() != predecessor->modifierGroup())
                                    predecessor2->setInput(modApp);
                                else 
                                    next = predecessor2;
                            }
                            else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent2)) {
                                if(pipeline->dataProvider() == current)
                                    pipeline->setDataProvider(modApp);
                            }
                        });
                        if(!next) break;
                        current = next;
                    }
                    predecessor->setInput(modApp->input());
                    modApp->setInput(current);
                }
                else {
                    // Standard case: If the modifier application is preceeded by another modifier application, swap the two.
                    predecessor->visitDependents([&](RefMaker* dependent2) {
                        if(ModifierApplication* predecessor2 = dynamic_object_cast<ModifierApplication>(dependent2)) {
                            OVITO_ASSERT(predecessor2->input() == predecessor);
                            predecessor2->setInput(modApp);
                        }
                        else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent2)) {
                            if(pipeline->dataProvider() == predecessor)
                                pipeline->setDataProvider(modApp);
                        }
                    });
                    predecessor->setInput(modApp->input());
                    modApp->setInput(predecessor);
                }
            }
            else if(modApp->modifierGroup() != nullptr) {
                modApp->setModifierGroup(nullptr);
            }
        });
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
        // Determine the modapps that form the head and the tail for the group.
        QVector<ModifierApplication*> groupModApps = group->modifierApplications();
        OORef<ModifierApplication> headModApp = groupModApps.front();
        OORef<ModifierApplication> tailModApp = groupModApps.back();
        ModifierApplication* predecessor = headModApp->getPredecessorModApp();
        OVITO_ASSERT(tailModApp->isReferencedBy(headModApp));
        OVITO_ASSERT(!predecessor || !headModApp->isPipelineBranch(true));

        // Don't move the group it is preceded by a pipeline branch or no modifier application at all.
        if(!predecessor)
            return;

        // Determine where to reinsert the group of modifiers into the pipeline.
        OORef<ModifierApplication> insertBefore = predecessor;
        if(predecessor->modifierGroup() != nullptr) {
            for(;;) {
                ModifierApplication* prev = nullptr;
                insertBefore->visitDependents([&](RefMaker* dependent) {
                    if(ModifierApplication* predecessor2 = dynamic_object_cast<ModifierApplication>(dependent)) {
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
        _userInterface.performTransaction(tr("Move modifier group up"), [&]() {
            insertBefore->visitDependents([&](RefMaker* dependent) {
                if(ModifierApplication* predecessor = dynamic_object_cast<ModifierApplication>(dependent)) {
                    OVITO_ASSERT(predecessor->input() == insertBefore);
                    predecessor->setInput(headModApp);
                }
                else if(PipelineSceneNode* predecessor = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                    if(predecessor->dataProvider() == insertBefore)
                        predecessor->setDataProvider(headModApp);
                }
            });
            predecessor->setInput(tailModApp->input());
            tailModApp->setInput(insertBefore);
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

    if(OORef<ModifierApplication> modApp = dynamic_object_cast<ModifierApplication>(item->object())) {
        _userInterface.performTransaction(tr("Move modifier down"), [modApp]() {
            OORef<ModifierApplication> successor = dynamic_object_cast<ModifierApplication>(modApp->input());
            if(successor && successor->isPipelineBranch(true) == false) {
                if(modApp->modifierGroup() != nullptr && successor->modifierGroup() != modApp->modifierGroup()) {
                    // If the modifier application is the last entry in the modifier group, move it out of the group. 
                    modApp->setModifierGroup(nullptr);
                }
                else if(modApp->modifierGroup() == nullptr && successor->modifierGroup() != nullptr && successor->modifierGroup()->isCollapsed() == false) {
                    // If the modifier application is above a group that is currently expanded, move it into the group.
                    modApp->setModifierGroup(successor->modifierGroup());
                }
                else {
                    // Standard case: If the modifier application is followed by another modifier application, swap the two.
                    OORef<ModifierApplication> insertAfter = successor;

                    // If the modifier application is above a group that is currently collapsed, move it all the way below that group.
                    if(modApp->modifierGroup() == nullptr && successor->modifierGroup() != nullptr && successor->modifierGroup()->isCollapsed() == true) {
                        while(ModifierApplication* next = dynamic_object_cast<ModifierApplication>(insertAfter->input())) {
                            if(next->modifierGroup() != successor->modifierGroup())
                                break;
                            insertAfter = next;
                        }
                    }

                    // Make the pipeline rearrangement.
                    modApp->visitDependents([&](RefMaker* dependent) {
                        if(ModifierApplication* predecessor = dynamic_object_cast<ModifierApplication>(dependent)) {
                            predecessor->setInput(successor);
                        }
                        else if(PipelineSceneNode* predecessor = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                            if(predecessor->dataProvider() == modApp)
                                predecessor->setDataProvider(successor);
                        }
                    });
                    modApp->setInput(insertAfter->input());
                    insertAfter->setInput(modApp);
                }
            }
            else if(modApp->modifierGroup() != nullptr) {
                modApp->setModifierGroup(nullptr);
            }
        });
    }
    else if(ModifierGroup* group = dynamic_object_cast<ModifierGroup>(item->object())) {
        QVector<ModifierApplication*> groupModApps = group->modifierApplications();
        OORef<ModifierApplication> headModApp = groupModApps.front();
        OORef<ModifierApplication> tailModApp = groupModApps.back();
        ModifierApplication* successor = dynamic_object_cast<ModifierApplication>(tailModApp->input());

        // Don't move the group over a pipeline branch.
        if(!successor || successor->isPipelineBranch(true))
            return;

        // Determine where to reinsert the group of modifiers into the pipeline.
        OORef<ModifierApplication> insertAfter = successor;
        if(successor->modifierGroup() != nullptr) {
            while(ModifierApplication* next = dynamic_object_cast<ModifierApplication>(insertAfter->input())) {
                if(next->modifierGroup() != successor->modifierGroup())
                    break;
                insertAfter = next;
            }
        }

        // Make the pipeline rearrangement.
        _userInterface.performTransaction(tr("Move modifier group down"), [&]() {
            headModApp->visitDependents([&](RefMaker* dependent) {
                if(ModifierApplication* predecessor = dynamic_object_cast<ModifierApplication>(dependent)) {
                    predecessor->setInput(successor);
                }
                else if(PipelineSceneNode* predecessor = dynamic_object_cast<PipelineSceneNode>(dependent)) {
                    if(predecessor->dataProvider() == headModApp)
                        predecessor->setDataProvider(successor);
                }
            });
            tailModApp->setInput(insertAfter->input());
            insertAfter->setInput(headModApp);
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
        _userInterface.performTransaction(tr("Make visual element independent"), [&]() {
            PipelineSceneNode* pipeline = selectedPipeline();
            DataVis* replacementVisElement = pipeline->makeVisElementIndependent(visElement);
            setNextObjectToSelect(replacementVisElement);
        });
    }
    else if(PipelineObject* selectedPipelineObj = dynamic_object_cast<PipelineObject>(item->object())) {
        _userInterface.performTransaction(tr("Make pipeline element independent"), [&]() {
            CloneHelper cloneHelper;
            PipelineObject* clonedObject = makeElementIndependentImpl(selectedPipelineObj, cloneHelper);
            if(clonedObject)
                setNextObjectToSelect(clonedObject);
        });
    }
    else if(ModifierGroup* selectedGroup = dynamic_object_cast<ModifierGroup>(item->object())) {
        _userInterface.performTransaction(tr("Make modifier group independent"), [&]() {
            CloneHelper cloneHelper;
            for(ModifierApplication* modApp : selectedGroup->modifierApplications()) {
                ModifierApplication* clonedModApp = static_object_cast<ModifierApplication>(makeElementIndependentImpl(modApp, cloneHelper));
                OVITO_ASSERT(clonedModApp);
                if(clonedModApp && clonedModApp->modifierGroup()) {
                    setNextObjectToSelect(clonedModApp->modifierGroup());
                }
            }
        });
    }
    refreshList();
}

/******************************************************************************
* Replaces the a pipeline item with an independent copy.
******************************************************************************/
PipelineObject* PipelineListModel::makeElementIndependentImpl(PipelineObject* pipelineObj, CloneHelper& cloneHelper)
{
    OORef<PipelineObject> currentObj = selectedPipeline()->dataProvider();
    ModifierApplication* predecessorModApp = nullptr;
    // Walk up the pipeline, starting at the node, until we reach the selected pipeline object.
    // Duplicate all shared ModifierApplications to remove pipeline branches.
    // When arriving at the selected modifier application, duplicate the modifier too
    // in case it is being shared by multiple pipelines.
    while(currentObj) {
        PipelineObject* nextObj = nullptr;
        if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(currentObj)) {

            // Clone all modifier applications along the way if they are shared by multiple pipeline branches.
            if(modApp->pipelines(true).size() > 1) {
                OORef<ModifierApplication> clonedModApp = cloneHelper.cloneObject(modApp, false);
                if(predecessorModApp)
                    predecessorModApp->setInput(clonedModApp);
                else
                    selectedPipeline()->setDataProvider(clonedModApp);
                predecessorModApp = clonedModApp;
            }
            else {
                predecessorModApp = modApp;
            }

            // Terminate pipeline walk at the target object to be made independent.
            if(currentObj == pipelineObj) {
                // Clone the selected modifier if it is referenced by multiple modapps.
                if(predecessorModApp->modifier()) {
                    if(predecessorModApp->modifier()->modifierApplications().size() > 1)
                        predecessorModApp->setModifier(cloneHelper.cloneObject(predecessorModApp->modifier(), true));
                }
                return predecessorModApp;
            }
            currentObj = predecessorModApp->input();
        }
        else if(currentObj == pipelineObj) {
            // If the object to be made independent is not a modifier application, simply clone it.
            if(currentObj->pipelines(true).size() > 1) {
                OORef<PipelineObject> clonedObject = cloneHelper.cloneObject(currentObj, false);
                if(predecessorModApp)
                    predecessorModApp->setInput(clonedObject);
                else
                    selectedPipeline()->setDataProvider(clonedObject);
                return clonedObject;
            }
            return currentObj;
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

    if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(objects.front())) {
        // If modifier applications are currently selected, put them into a new group.
        // But first make sure the modifier application aren't already part of an existing group.
        existingGroup = modApp->modifierGroup();
        if(!existingGroup) {
            // Create a new group.
            OORef<ModifierGroup> group = OORef<ModifierGroup>::create();
            _userInterface.performTransaction(tr("Create modifier group"), [&]() {
                for(RefTarget* obj : objects) {
                    if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj)) {
                        modApp->setModifierGroup(group);
                    }
                }
            });
            setNextObjectToSelect(group);
            refreshList();
            return;
        }
    }
    
    // If an existing modifier group is currently selected, dissolve the group.
    if(!existingGroup)
        existingGroup = dynamic_object_cast<ModifierGroup>(objects.front());
    if(existingGroup) {
        _userInterface.performTransaction(tr("Dissolve modifier group"), [&]() {
            QVector<ModifierApplication*> groupModApps = existingGroup->modifierApplications();
            setNextObjectToSelect(groupModApps.front());
            for(ModifierApplication* modApp : groupModApps) {
                if(modApp->modifierGroup() == existingGroup)
                    modApp->setModifierGroup(nullptr);
            }
            existingGroup->deleteReferenceObject();
        });
    }
    refreshList();
}

}   // End of namespace
