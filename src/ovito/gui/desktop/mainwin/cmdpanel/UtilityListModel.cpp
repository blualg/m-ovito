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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "UtilityListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs an action for a utility object class.
******************************************************************************/
UtilityAction* UtilityAction::createForClass(const UtilityObject::OOMetaClass* clazz)
{
    UtilityAction* action = new UtilityAction();
    action->_utilityClass = clazz;
    action->_category = clazz->utilityCategory();

    // Generate a unique identifier for the action:
    action->setObjectName(QStringLiteral("Utility.%1.%2").arg(clazz->pluginId(), clazz->name()));

    // Set the action's UI display name.
    action->setText(clazz->displayName());

    // Give the utility a status bar text.
    QString description = clazz->descriptionString();
    action->setStatusTip(!description.isEmpty() ? std::move(description) : tr("Open utility."));

    // Give the action an icon.
    static QIcon icon = QIcon::fromTheme("utility_action_icon");
    action->setIcon(icon);

    return action;
}

/******************************************************************************
* Constructor.
******************************************************************************/
UtilityListModel::UtilityListModel(QObject* parent, MainWindow& mainWindow) : QAbstractListModel(parent), _mainWindow(mainWindow)
{
    OVITO_ASSERT(mainWindow.actionManager());

    // Initialize UI colors.
    updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &UtilityListModel::updateColorPalette);
QT_WARNING_POP

    // Enumerate all utility object classes.
    for(auto clazz : PluginManager::instance().metaclassMembers<UtilityObject>()) {

        // Skip utilities that want to be hidden from the user.
        // Do not add it to the list.
        if(clazz->utilityCategory() == QStringLiteral("-"))
            continue;

        // Create action for the utility class.
        UtilityAction* action = UtilityAction::createForClass(clazz);
        _actions.push_back(action);

        // Register it with the global ActionManager.
        mainWindow.actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == mainWindow.actionManager());

        // Handle the action.
        connect(action, &QAction::triggered, this, &UtilityListModel::activateUtility);
    }

    // Order actions list by category name.
    std::sort(_actions.begin(), _actions.end(), [](UtilityAction* a, UtilityAction* b) { return QString::localeAwareCompare(a->category(), b->category()) < 0; });

    // Sort actions into categories.
    for(UtilityAction* action : _actions) {
        const QString& category = !action->category().isEmpty() ? action->category() : tr("Standard utilities");
        if(_categoryNames.empty() || _categoryNames.back() != category) {
            _categoryNames.push_back(category);
            _actionsPerCategory.emplace_back();
        }
        _actionsPerCategory.back().push_back(action);
    }

    // Sort actions by name within each category.
    for(std::vector<UtilityAction*>& categoryActions : _actionsPerCategory) {
        std::sort(categoryActions.begin(), categoryActions.end(), [](UtilityAction* a, UtilityAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const UtilityAction* a, const UtilityAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &UtilityListModel::extensionClassAdded);

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

    // Adopt deserialized utility objects if a state file has been loaded.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::dataSetChanged, this, &UtilityListModel::datasetReplaced);

    // Shut down the model when the main window is closed.
    connect(&mainWindow, &MainWindow::closingWindow, this, [this]() {
        _utilityObjects.clear();
    });
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void UtilityListModel::updateColorPalette(const QPalette& palette)
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
void UtilityListModel::updateModelLists()
{
    beginResetModel();
    _modelStrings.clear();
    _modelStrings.push_back(tr("Utilities..."));
    _modelActions.clear();
    _modelActions.push_back(nullptr);
    _getMoreExtensionsItemIndex = -1;
#if 0
    int categoryIndex = 0;
    for(const auto& categoryActions : _actionsPerCategory) {
        if(!categoryActions.empty()) {
            _modelActions.push_back(nullptr);
            _modelStrings.push_back(_categoryNames[categoryIndex]);
            for(UtilityAction* action : categoryActions) {
                _modelActions.push_back(action);
                _modelStrings.push_back(action->text());
            }
#ifndef OVITO_BUILD_BASIC
            if(_categoryNames[categoryIndex] == tr("Python utilities")) {
#else
            if(_categoryNames[categoryIndex] == tr("Python utilities (Pro)")) {
#endif
                _getMoreExtensionsItemIndex = _modelStrings.size();
                _modelActions.push_back(nullptr);
                _modelStrings.push_back(tr("Get more utilities..."));
            }
        }
        categoryIndex++;
    }
    if(_getMoreExtensionsItemIndex == -1) {
        _getMoreExtensionsItemIndex = _modelStrings.size();
        _modelActions.push_back(nullptr);
        _modelStrings.push_back(tr("Get more utilities..."));
    }
#else
    for(UtilityAction* action : _actions) {
        _modelActions.push_back(action);
        _modelStrings.push_back(action->text());
    }
#endif

    endResetModel();
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int UtilityListModel::rowCount(const QModelIndex& parent) const
{
    return _modelStrings.size();
}

/******************************************************************************
* Returns the data associated with a list item.
******************************************************************************/
QVariant UtilityListModel::data(const QModelIndex& index, int role) const
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
Qt::ItemFlags UtilityListModel::flags(const QModelIndex& index) const
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
* Signal handler that activates the selected utility.
******************************************************************************/
void UtilityListModel::activateUtility()
{
    // Get the action that emitted the signal.
    UtilityAction* action = qobject_cast<UtilityAction*>(sender());
    OVITO_ASSERT(action);

    // Show the utilities tab of the command panel.
    _mainWindow.actionManager()->getAction(ACTION_COMMAND_PANEL_UTILITIES)->trigger();

    _mainWindow.handleExceptions([&]() {

        // Check if a utility object of the selected class has already been instantiated before.
        OvitoClassPtr utilityClass = action->utilityClass();
        OORef<UtilityObject> utility;
        for(const OORef<UtilityObject>& obj : _utilityObjects) {
            if(utilityClass->isMember(obj)) {
                utility = obj;
                break;
            }
        }

        // Otherwise, create an instance of the utility object class.
        if(!utility) {
            utility = static_object_cast<UtilityObject>(utilityClass->createInstance());
            _utilityObjects.push_back(utility);
            OVITO_ASSERT(utility);

            // Add utility objects to the current dataset so that its state gets serialized into a state file.
            if(_mainWindow.datasetContainer().currentSet())
                _mainWindow.datasetContainer().currentSet()->addGlobalObject(utility);
        }

        // Show the utility in the command panel tab.
        Q_EMIT utilitySelected(utility.get());
    });
}

/******************************************************************************
* Determines the corresponding model index for a utility.
******************************************************************************/
int UtilityListModel::indexFromUtilityObject(const UtilityObject* utility) const
{
    if(utility) {
        int index = 0;
        for(UtilityAction* action : _modelActions) {
            if(action && action->utilityClass()->isMember(utility))
                return index;
            index++;
        }
    }
    return -1;
}

/******************************************************************************
* This handler is called whenever a dataset has been loaded from a state file.
******************************************************************************/
void UtilityListModel::datasetReplaced(DataSet* dataset)
{
    if(!dataset)
        return;

    // Adopt deserialized utility objects from the state file.
    for(const auto& obj : dataset->globalObjects()) {
        if(UtilityObject* utility = dynamic_object_cast<UtilityObject>(obj)) {
            // Replace any existing utility of the same class.
            auto iter = boost::find_if(_utilityObjects, [&](const auto& utility2) { return utility->getOOClass().isMember(utility2); });
            if(iter != _utilityObjects.end())
                *iter = utility;
            else
                _utilityObjects.push_back(utility);
        }
    }

    // Add existing utility objects to the dataset.
    for(const auto& utility : _utilityObjects) {
        dataset->addGlobalObject(utility);
    }
}

/******************************************************************************
* This handler is called whenever a new extension class has been registered at runtime.
******************************************************************************/
void UtilityListModel::extensionClassAdded(OvitoClassPtr cls)
{
    // Skip classes that are not utilities.
    if(!cls->isDerivedFrom(UtilityObject::OOClass()))
        return;
    const UtilityObject::OOMetaClass* clazz = static_cast<const UtilityObject::OOMetaClass*>(cls);

    // Skip utilities that want to be hidden from the user.
    // Do not add it to the list.
    if(clazz->utilityCategory() == QStringLiteral("-"))
        return;

    // Create action for the utility class.
    UtilityAction* action = UtilityAction::createForClass(clazz);

    // Register it with the global ActionManager.
    _mainWindow.actionManager()->addAction(action);

    // Handle the action.
    connect(action, &QAction::triggered, this, &UtilityListModel::activateUtility);

    // Insert action into the list, which is sorted by name.
    auto iter = std::lower_bound(_actions.begin(), _actions.end(), action,
        [](const UtilityAction* a, const UtilityAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
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
