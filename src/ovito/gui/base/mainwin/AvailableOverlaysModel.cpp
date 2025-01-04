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
AvailableOverlaysModel::AvailableOverlaysModel(QObject* parent, UserInterface& userInterface, OverlayListModel* overlayListModel) : QAbstractListModel(parent), _userInterface(userInterface), _overlayListModel(overlayListModel)
{
    OVITO_ASSERT(userInterface.actionManager());

    // Initialize UI colors.
    updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &AvailableOverlaysModel::updateColorPalette);
QT_WARNING_POP

    // Enumerate all built-in viewport layer classes.
    for(auto clazz : PluginManager::instance().metaclassMembers<ViewportOverlay>()) {

        // Skip overlays that want to be hidden from the user.
        // Do not add it to the list of available overlays.
        if(clazz->viewportOverlayCategory() == QStringLiteral("-"))
            continue;

        // Create action for the viewport layer class.
        OverlayAction* action = OverlayAction::createForClass(clazz);
        _actions.push_back(action);

        // Register it with the global ActionManager.
        userInterface.actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == userInterface.actionManager());

        // Handle the insertion action.
        connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);
    }

    // Order actions list by category name.
    std::sort(_actions.begin(), _actions.end(), [](OverlayAction* a, OverlayAction* b) { return QString::localeAwareCompare(a->category(), b->category()) < 0; });

    // Sort actions into categories.
    for(OverlayAction* action : _actions) {
        const QString& category = !action->category().isEmpty() ? action->category() : tr("Standard layers");
        if(_categoryNames.empty() || _categoryNames.back() != category) {
            _categoryNames.push_back(category);
            _actionsPerCategory.emplace_back();
        }
        _actionsPerCategory.back().push_back(action);
    }

    // Sort actions by name within each category.
    for(std::vector<OverlayAction*>& categoryActions : _actionsPerCategory) {
        std::sort(categoryActions.begin(), categoryActions.end(), [](OverlayAction* a, OverlayAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Create category for viewport layer templates.
    _categoryNames.push_back(tr("Layer templates"));
    _actionsPerCategory.emplace_back();
    for(const QString& templateName : OverlayTemplates::get()->templateList()) {
        // Create action for the template.
        OverlayAction* action = OverlayAction::createForTemplate(templateName);
        _actionsPerCategory.back().push_back(action);

        // Register it with the global ActionManager.
        userInterface.actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == userInterface.actionManager());

        // Handle the action.
        connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

        // Insert action into complete list.
        _actions.push_back(action);
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const OverlayAction* a, const OverlayAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Listen for changes to the underlying modifier template list.
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsInserted, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::modelReset, this, &AvailableOverlaysModel::refreshTemplates);
    connect(OverlayTemplates::get(), &QAbstractItemModel::dataChanged, this, &AvailableOverlaysModel::refreshTemplates);

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &AvailableOverlaysModel::extensionClassAdded);

    // Define fonts, colors, etc.
    _categoryFont = QGuiApplication::font();
    _categoryFont.setBold(true);
#ifndef Q_OS_WIN
    if(_categoryFont.pixelSize() < 0)
        _categoryFont.setPointSize(_categoryFont.pointSize() * 4 / 5);
    else
        _categoryFont.setPixelSize(_categoryFont.pixelSize() * 4 / 5);
#endif
    _getMoreExtensionsFont = QGuiApplication::font();

    // Generate list items.
    updateModelLists();
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void AvailableOverlaysModel::updateColorPalette(const QPalette& palette)
{
    bool darkTheme = palette.color(QPalette::Active, QPalette::Window).lightness() < 100;
#ifndef Q_OS_LINUX
    _categoryBackgroundBrush = darkTheme ? palette.mid() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
#else
    _categoryBackgroundBrush = darkTheme ? palette.window() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
#endif
    _categoryForegroundBrush = QBrush(darkTheme ? QColor(Qt::blue).lighter() : QColor(Qt::blue));
    _getMoreExtensionsForegroundBrush = QBrush(darkTheme ? Qt::green : Qt::darkGreen);
}

/******************************************************************************
* Rebuilds the internal list of model items.
******************************************************************************/
void AvailableOverlaysModel::updateModelLists()
{
    beginResetModel();
    _modelStrings.clear();
    _modelStrings.push_back(tr("Add layer..."));
    _modelActions.clear();
    _modelActions.push_back(nullptr);
    _getMoreExtensionsItemIndex = -1;
    int categoryIndex = 0;
    for(const auto& categoryActions : _actionsPerCategory) {
        if(!categoryActions.empty()) {
            _modelActions.push_back(nullptr);
            _modelStrings.push_back(_categoryNames[categoryIndex]);
            for(OverlayAction* action : categoryActions) {
                _modelActions.push_back(action);
                _modelStrings.push_back(action->text());
            }
#ifndef OVITO_BUILD_BASIC
            if(_categoryNames[categoryIndex] == tr("Python layers")) {
#else
            if(_categoryNames[categoryIndex] == tr("Python layers (Pro)")) {
#endif
                _getMoreExtensionsItemIndex = _modelStrings.size();
                _modelActions.push_back(nullptr);
                _modelStrings.push_back(tr("Get more layers..."));
            }
        }
        categoryIndex++;
    }
    if(_getMoreExtensionsItemIndex == -1) {
        _getMoreExtensionsItemIndex = _modelStrings.size();
        _modelActions.push_back(nullptr);
        _modelStrings.push_back(tr("Get more layers..."));
    }

    endResetModel();
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int AvailableOverlaysModel::rowCount(const QModelIndex& parent) const
{
    return _modelStrings.size();
}

/******************************************************************************
* Returns the data associated with a list item.
******************************************************************************/
QVariant AvailableOverlaysModel::data(const QModelIndex& index, int role) const
{
    if(role == Qt::DisplayRole) {
        if(index.row() >= 0 && index.row() < _modelStrings.size())
            return _modelStrings[index.row()];
    }
    else if(role == Qt::FontRole) {
        if(index.row() == getMoreExtensionsItemIndex())
            return _getMoreExtensionsFont;
        // Is it a category header?
        else if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryFont;
    }
    else if(role == Qt::ForegroundRole) {
        if(index.row() == getMoreExtensionsItemIndex())
            return _getMoreExtensionsForegroundBrush;
        // Is it a category header?
        else if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryForegroundBrush;
    }
    else if(role == Qt::BackgroundRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr && index.row() != getMoreExtensionsItemIndex())
            return _categoryBackgroundBrush;
    }
    else if(role == Qt::TextAlignmentRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr && index.row() != getMoreExtensionsItemIndex())
            return Qt::AlignCenter;
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags AvailableOverlaysModel::flags(const QModelIndex& index) const
{
    if(index.row() > 0 && index.row() < _modelActions.size()) {
        if(_modelActions[index.row()])
            return _modelActions[index.row()]->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
        else if(index.row() == _getMoreExtensionsItemIndex)
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        else
            return Qt::ItemIsEnabled;
    }
    return QAbstractListModel::flags(index);
}

/******************************************************************************
* Rebuilds the list of actions for the viewport layer templates.
******************************************************************************/
void AvailableOverlaysModel::refreshTemplates()
{
    std::vector<OverlayAction*>& templateActions = _actionsPerCategory[templatesCategory()];

    // Discard old list of actions.
    if(!templateActions.empty()) {
        for(OverlayAction* action : templateActions) {
            auto iter = std::find(_actions.begin(), _actions.end(), action);
            OVITO_ASSERT(iter != _actions.end());
            _actions.erase(iter);
            _userInterface.actionManager()->deleteAction(action);
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
            _userInterface.actionManager()->addAction(action);
            OVITO_ASSERT(action->parent() == _userInterface.actionManager());

            // Handle the action.
            connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

            // Append action to flat list.
            _actions.push_back(action);
        }
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const OverlayAction* a, const OverlayAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Regenerate list items.
    updateModelLists();
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
    _userInterface.performTransaction(tr("Insert viewport layer"), [&]() {
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
        _userInterface.actionManager()->getAction(ACTION_COMMAND_PANEL_OVERLAYS)->trigger();
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
    _userInterface.actionManager()->addAction(action);

    // Handle the insertion action.
    connect(action, &QAction::triggered, this, &AvailableOverlaysModel::insertViewportLayer);

    // Insert action into the list, which is sorted by name.
    auto iter = std::lower_bound(_actions.begin(), _actions.end(), action,
        [](const OverlayAction* a, const OverlayAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
    _actions.insert(iter, action);

    // Insert action into the right category. Or create a new category if necessary.
    auto categoryIter = boost::find(_categoryNames, action->category());
    int categoryIndex = std::distance(_categoryNames.begin(), categoryIter);
    if(categoryIter == _categoryNames.end()) {
        _categoryNames.push_back(action->category());
        _actionsPerCategory.emplace_back();
    }
    _actionsPerCategory[categoryIndex].push_back(action);

    // Regenerate list model items.
    updateModelLists();
}

}   // End of namespace
