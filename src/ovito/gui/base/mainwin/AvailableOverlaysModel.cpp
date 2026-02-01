////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/gui/base/mainwin/templates/OverlayTemplates.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableOverlaysModel.h"
#include "OverlayListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs an action for a built-in viewport layer class.
******************************************************************************/
OverlayAction* OverlayAction::createForClass(const ViewportOverlay::OOMetaClass* clazz)
{
    OverlayAction* action = new OverlayAction();
    action->_layerClass = clazz;
    action->_category = clazz->viewportOverlayCategory();

    // Generate a unique identifier for the action:
    action->setObjectName(QStringLiteral("InsertViewportLayer.%1.%2").arg(clazz->pluginId(), clazz->name()));

    // Set the action's UI display name.
    action->setText(clazz->displayName());

    // Give the overlay a status bar text.
    QString description = clazz->descriptionString();
    action->setStatusTip(!description.isEmpty() ? std::move(description) : tr("Insert this viewport layer."));

    // Give the action an icon.
    static QIcon icon = QIcon::fromTheme("overlay_action_icon");
    action->setIcon(icon);

    return action;
}

/******************************************************************************
* Constructs an action for a viewport layer template.
******************************************************************************/
OverlayAction* OverlayAction::createForTemplate(const QString& templateName)
{
    OverlayAction* action = new OverlayAction();
    action->_templateName = templateName;

    // Generate a unique identifier for the action:
    action->setObjectName(QStringLiteral("InsertViewportLayerTemplate.%1").arg(templateName));

    // Set the action's UI display name.
    action->setText(templateName);

    // Give the modifier a status bar text.
    action->setStatusTip(tr("Insert this viewport layer template."));

    // Give the action an icon.
    static QIcon icon = QIcon::fromTheme("overlay_action_icon");
    action->setIcon(icon);

    return action;
}

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableOverlaysModel::AvailableOverlaysModel(QObject* parent, UserInterface& ui, OverlayListModel* overlayListModel) : QAbstractItemModel(parent), UserInterfaceComponent<UserInterface>(ui), _overlayListModel(overlayListModel)
{
    OVITO_ASSERT(actionManager());

    // Enumerate all built-in viewport layer classes.
    for(auto clazz : PluginManager::instance().metaclassMembers<ViewportOverlay>()) {

        // Skip overlays that want to be hidden from the user.
        // Do not add it to the list of available overlays.
        if(clazz->viewportOverlayCategory() == QStringLiteral("-"))
            continue;

        // Create action for the viewport layer class.
        OverlayAction* action = OverlayAction::createForClass(clazz);

        // Register it with the global ActionManager.
        actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the insertion action.
        connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

        // Sort action into categories.
        const QString& category = !action->category().isEmpty() ? action->category() : tr("Standard layers");
        auto categoryIter = std::ranges::find(_categoryNames, category);
        int categoryIndex = std::distance(_categoryNames.begin(), categoryIter);
        if(categoryIter == _categoryNames.end()) {
            // New category.
            _categoryNames.push_back(category);
            _actionsPerCategory.emplace_back();
        }
        _actionsPerCategory[categoryIndex].push_back(action);
    }

    // Sort actions by name within each category.
    for(std::vector<QAction*>& categoryActions : _actionsPerCategory) {
        std::sort(categoryActions.begin(), categoryActions.end(), [](QAction* a, QAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Create category for viewport layer templates.
    _categoryNames.push_back(tr("Layer templates"));
    _actionsPerCategory.emplace_back();
    for(const QString& templateName : OverlayTemplates::get()->templateList()) {
        // Create action for the template.
        OverlayAction* action = OverlayAction::createForTemplate(templateName);
        _actionsPerCategory.back().push_back(action);

        // Register it with the global ActionManager.
        actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the action.
        connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);
    }

    // Create "Manage templates..." action for later templates, which wraps the global one.
    // This is done to override the global action's text.
    QAction* manageTemplatesAction = actionManager()->getAction(ACTION_VIEWPORT_MANAGE_OVERLAY_TEMPLATES);
    _manageTemplatesAction = new QAction(this);
    _manageTemplatesAction->setText(tr("Manage templates..."));
    _manageTemplatesAction->setStatusTip(manageTemplatesAction->statusTip());
    _manageTemplatesAction->setIcon(manageTemplatesAction->icon());
    // Give this action an italic font to visually distinguish it.
    QFont font = manageTemplatesAction->font();
    font.setItalic(true);
    _manageTemplatesAction->setFont(font);
    connect(_manageTemplatesAction, &QAction::triggered, manageTemplatesAction, &QAction::trigger);
    _actionsPerCategory.back().push_back(_manageTemplatesAction);

    // Listen for changes to the underlying modifier template list.
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsInserted, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::modelReset, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::dataChanged, this, &AvailableOverlaysModel::refreshTemplates);

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &AvailableOverlaysModel::extensionClassAdded);
}

/******************************************************************************
* Returns the model index for the item at the given row and column.
******************************************************************************/
QModelIndex AvailableOverlaysModel::index(int row, int column, const QModelIndex& parent) const
{
    if(column != 0)
        return {};

    if(!parent.isValid()) {
        // Root level: categories
        if(row >= 0 && row < (int)_categoryNames.size())
            return createIndex(row, 0, quintptr(-1));
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Child level: overlays within a category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_actionsPerCategory.size()) {
            if(row >= 0 && row < (int)_actionsPerCategory[categoryIndex].size())
                return createIndex(row, 0, quintptr(categoryIndex));
        }
    }
    return {};
}

/******************************************************************************
* Returns the parent of the model item with the given index.
******************************************************************************/
QModelIndex AvailableOverlaysModel::parent(const QModelIndex& index) const
{
    if(!index.isValid())
        return {};

    quintptr id = index.internalId();
    if(id == quintptr(-1)) {
        // This is a category item at the root level.
        return {};
    }
    else {
        // This is an overlay item; its parent is the category.
        int categoryIndex = (int)id;
        return createIndex(categoryIndex, 0, quintptr(-1));
    }
}

/******************************************************************************
* Returns the number of rows under the given parent.
******************************************************************************/
int AvailableOverlaysModel::rowCount(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        // Root level: number of categories
        return (int)_categoryNames.size();
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Category level: number of overlays in this category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_actionsPerCategory.size())
            return (int)_actionsPerCategory[categoryIndex].size();
    }
    // Overlays don't have children.
    return 0;
}

/******************************************************************************
* Returns the number of columns for the children of the given parent.
******************************************************************************/
int AvailableOverlaysModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

/******************************************************************************
* Returns the data associated with an item.
******************************************************************************/
QVariant AvailableOverlaysModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid())
        return {};

    if(index.internalId() == quintptr(-1)) {
        // Category item.
        int categoryIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_categoryNames.size())
            return {};

        if(role == Qt::DisplayRole)
            return _categoryNames[categoryIndex];
    }
    else {
        // Overlay item.
        int categoryIndex = (int)index.internalId();
        int overlayIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_actionsPerCategory.size())
            return {};
        if(overlayIndex < 0 || overlayIndex >= (int)_actionsPerCategory[categoryIndex].size())
            return {};

        QAction* action = _actionsPerCategory[categoryIndex][overlayIndex];
        if(role == Qt::DisplayRole)
            return action->text();
        else if(role == Qt::UserRole)
            return QVariant::fromValue(static_cast<QObject*>(action));
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags AvailableOverlaysModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if(index.internalId() == quintptr(-1)) {
        // Category item: enabled but not selectable.
        return Qt::ItemIsEnabled;
    }
    else {
        // Overlay item.
        QAction* action = actionFromIndex(index);
        if(action)
            return action->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
    }
    return Qt::NoItemFlags;
}

/******************************************************************************
* Returns the action for an overlay at a given category and row.
******************************************************************************/
QAction* AvailableOverlaysModel::actionAt(int categoryIndex, int overlayIndex) const
{
    if(categoryIndex >= 0 && categoryIndex < (int)_actionsPerCategory.size()) {
        if(overlayIndex >= 0 && overlayIndex < (int)_actionsPerCategory[categoryIndex].size())
            return _actionsPerCategory[categoryIndex][overlayIndex];
    }
    return nullptr;
}

/******************************************************************************
* Returns the action for an overlay from a model index.
******************************************************************************/
QAction* AvailableOverlaysModel::actionFromIndex(const QModelIndex& index) const
{
    if(!index.isValid() || index.internalId() == quintptr(-1))
        return nullptr;

    int categoryIndex = (int)index.internalId();
    int overlayIndex = index.row();
    return actionAt(categoryIndex, overlayIndex);
}

/******************************************************************************
* Rebuilds the list of actions for the viewport layer templates.
******************************************************************************/
void AvailableOverlaysModel::refreshTemplates()
{
    std::vector<QAction*>& templateActions = _actionsPerCategory[templatesCategory()];

    // Discard old list of actions.
    if(!templateActions.empty()) {
        for(QAction* action : templateActions) {
            if(OverlayAction* overlayAction = qobject_cast<OverlayAction*>(action)) {
                actionManager()->deleteAction(overlayAction);
            }
        }
        templateActions.clear();
    }

    // Create new actions for the templates.
    int count = OverlayTemplates::get()->templateList().size();
    if(count != 0) {
        for(const QString& templateName : OverlayTemplates::get()->templateList()) {
            // Create action for the template.
            OverlayAction* action = OverlayAction::createForTemplate(templateName);
            templateActions.push_back(action);

            // Register it with the ActionManager.
            actionManager()->addAction(action);
            OVITO_ASSERT(action->parent() == actionManager());

            // Handle the action.
            connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);
        }
    }
    templateActions.push_back(_manageTemplatesAction);

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

/******************************************************************************
* Signal handler that inserts the selected viewport layer into the active viewport.
******************************************************************************/
void AvailableOverlaysModel::insertViewportLayer()
{
    // Get the action that emitted the signal.
    OverlayAction* action = qobject_cast<OverlayAction*>(sender());
    OVITO_ASSERT(action);

    // Get the current dataset and viewport.
    Viewport* vp = _overlayListModel->selectedViewport();
    if(!vp) return;

    // Instantiate the new layer and add it to the active viewport.
    performTransaction(tr("Insert viewport layer"), [&]() {
        int overlayIndex = -1;
        int underlayIndex = -1;
        if(OverlayListItem* item = _overlayListModel->selectedItem()) {
            overlayIndex = vp->overlays().indexOf(item->overlay());
            underlayIndex = vp->underlays().indexOf(item->overlay());
        }
        if(overlayIndex == -1 && underlayIndex == -1)
            overlayIndex = vp->overlays().size() - 1;
        if(action->layerClass()) {
            // Create an instance of the overlay class.
            OORef<ViewportOverlay> layer = static_object_cast<ViewportOverlay>(action->layerClass()->createInstance());
            // Make sure the new overlay gets selected in the UI.
            _overlayListModel->setNextToSelectObject(layer);
            // Insert it into either the overlays or the underlays list.
            if(underlayIndex >= 0)
                vp->insertUnderlay(underlayIndex+1, layer);
            else
                vp->insertOverlay(overlayIndex+1, layer);
        }
        else if(!action->templateName().isEmpty()) {
            // Load template from the store.
            QVector<OORef<ViewportOverlay>> layerSet = OverlayTemplates::get()->instantiateTemplate(action->templateName());
            // Insert the layer(s) into either the overlays or the underlays list.
            for(OORef<ViewportOverlay>& layer : layerSet) {
                // Make sure the new overlay gets selected in the UI.
                _overlayListModel->setNextToSelectObject(layer);
                if(underlayIndex >= 0)
                    vp->insertUnderlay(underlayIndex+1, std::move(layer));
                else
                    vp->insertOverlay(overlayIndex+1, std::move(layer));
            }
        }
        else return;

        // Automatically activate preview mode to make the overlay visible.
        vp->setRenderPreviewMode(true);

        // Show the overlays tab of the command panel.
        actionManager()->getAction(ACTION_COMMAND_PANEL_OVERLAYS)->trigger();
    });
}

/******************************************************************************
* This handler is called whenever a new extension class has been registered at runtime.
******************************************************************************/
void AvailableOverlaysModel::extensionClassAdded(OvitoClassPtr cls)
{
    // Skip classes that are not viewport layers.
    if(!cls->isDerivedFrom(ViewportOverlay::OOClass()))
        return;
    const ViewportOverlay::OOMetaClass* clazz = static_cast<const ViewportOverlay::OOMetaClass*>(cls);

    // Skip modifiers that want to be hidden from the user.
    // Do not add it to the list of available modifiers.
    if(clazz->viewportOverlayCategory() == QStringLiteral("-"))
        return;

    // Create action for the viewport layer class.
    OverlayAction* action = OverlayAction::createForClass(clazz);

    // Register it with the global ActionManager.
    actionManager()->addAction(action);

    // Handle the insertion action.
    connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

    // Insert action into the right category. Or create a new category if necessary.
    auto categoryIter = std::ranges::find(_categoryNames, action->category());
    int categoryIndex = std::distance(_categoryNames.begin(), categoryIter);
    if(categoryIter == _categoryNames.end()) {
        _categoryNames.push_back(action->category());
        _actionsPerCategory.emplace_back();
    }
    _actionsPerCategory[categoryIndex].push_back(action);

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

}   // End of namespace
