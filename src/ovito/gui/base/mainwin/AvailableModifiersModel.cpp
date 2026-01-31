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
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/templates/ModifierTemplates.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableModifiersModel.h"
#include "PipelineListModel.h"

namespace Ovito {

QVector<AvailableModifiersModel*> AvailableModifiersModel::_allModels;
QVector<AvailableModifiersListModel*> AvailableModifiersListModel::_allListModels;

/******************************************************************************
* Constructs an action for a built-in modifier class.
******************************************************************************/
ModifierAction* ModifierAction::createForClass(ModifierClassPtr clazz)
{
    ModifierAction* action = new ModifierAction();
    action->_modifierClass = clazz;
    action->_category = clazz->modifierCategory();

    // Generate a unique identifier for the action:
    action->setObjectName(QStringLiteral("InsertModifier.%1.%2").arg(clazz->pluginId(), clazz->name()));

    // Set the action's UI display name.
    action->setText(clazz->displayName());

    // Give the modifier a status bar text.
    QString description = clazz->descriptionString();
    action->setStatusTip(!description.isEmpty() ? std::move(description) : tr("Insert this modifier into the data pipeline."));

    // Give the action an icon.
    static QIcon icon = QIcon::fromTheme("modify_modifier_action_icon");
    action->setIcon(icon);

    // Modifiers without a category are moved into the "Other" category.
    if(action->_category.isEmpty())
        action->_category = tr("Other");

    return action;
}

/******************************************************************************
* Constructs an action for a modifier template.
******************************************************************************/
ModifierAction* ModifierAction::createForTemplate(const QString& templateName)
{
    ModifierAction* action = new ModifierAction();
    action->_templateName = templateName;

    // Generate a unique identifier for the action:
    action->setObjectName(QStringLiteral("InsertModifierTemplate.%1").arg(templateName));

    // Set the action's UI display name.
    action->setText(templateName);

    // Give the modifier a status bar text.
    action->setStatusTip(tr("Insert this modifier template into the data pipeline."));

    // Give the action an icon.
    static QIcon icon = QIcon::fromTheme("modify_modifier_action_icon");
    action->setIcon(icon);

    return action;
}

/******************************************************************************
* Updates the actions enabled/disabled state depending on the current data pipeline.
******************************************************************************/
bool ModifierAction::updateState(const PipelineFlowState& input)
{
    bool enable = input.data() && (!modifierClass() || modifierClass()->isApplicableTo(*input.data()));
    if(isEnabled() != enable) {
        setEnabled(enable);
        return true;
    }
    return false;
}

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableModifiersModel::AvailableModifiersModel(QObject* parent, UserInterface& ui, PipelineListModel* pipelineListModel) : QAbstractItemModel(parent), UserInterfaceComponent<UserInterface>(ui), _pipelineListModel(pipelineListModel)
{
    OVITO_ASSERT(actionManager());

    // Register this instance.
    _allModels.push_back(this);

    // Update the state of this model's actions whenever the ActionManager requests it.
    connect(actionManager(), &ActionManager::actionUpdateRequested, this, &AvailableModifiersModel::updateActionState);

    // Enumerate all registered modifier classes.
    for(ModifierClassPtr clazz : PluginManager::instance().metaclassMembers<Modifier>()) {

        // Skip modifiers that want to be hidden from the user.
        // Do not add it to the list of available modifiers.
        if(clazz->modifierCategory() == QStringLiteral("-"))
            continue;

        // Create action for the modifier class.
        ModifierAction* action = ModifierAction::createForClass(clazz);
        _actions.push_back(action);

        // Register it with the global ActionManager.
        actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the insertion action.
        connect(action, &QAction::triggered, this, &AvailableModifiersModel::insertModifier);
    }

    // Order actions list by category name.
    std::sort(_actions.begin(), _actions.end(), [](ModifierAction* a, ModifierAction* b) { return QString::localeAwareCompare(a->category(), b->category()) < 0; });

    // Sort actions into categories.
    for(ModifierAction* action : _actions) {
        if(_categoryNames.empty() || _categoryNames.back() != action->category()) {
            _categoryNames.push_back(action->category());
            _actionsPerCategory.emplace_back();
        }
        _actionsPerCategory.back().push_back(action);
    }

    // Sort actions by name within each category.
    for(std::vector<ModifierAction*>& categoryActions : _actionsPerCategory) {
        std::sort(categoryActions.begin(), categoryActions.end(), [](ModifierAction* a, ModifierAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });
    }

    // Create category for modifier templates.
    _categoryNames.push_back(tr("Modifier templates"));
    _actionsPerCategory.emplace_back();
    for(const QString& templateName : ModifierTemplates::get()->templateList()) {
        // Create action for the modifier template.
        ModifierAction* action = ModifierAction::createForTemplate(templateName);
        _actionsPerCategory.back().push_back(action);

        // Register it with the global ActionManager.
        actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == actionManager());

        // Handle the action.
        connect(action, &QAction::triggered, this, &AvailableModifiersModel::insertModifier);

        // Insert action into complete list.
        _actions.push_back(action);
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Listen for changes to the underlying modifier template list.
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsInserted, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::modelReset, this, &AvailableModifiersModel::refreshTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::dataChanged, this, &AvailableModifiersModel::refreshTemplates);

    // Extend the list when a new Python extension is being registered at runtime.
    connect(&PluginManager::instance(), &PluginManager::extensionClassAdded, this, &AvailableModifiersModel::extensionClassAdded);
}

/******************************************************************************
* Returns the model index for the item at the given row and column.
******************************************************************************/
QModelIndex AvailableModifiersModel::index(int row, int column, const QModelIndex& parent) const
{
    if(column != 0)
        return {};

    if(!parent.isValid()) {
        // Root level: categories
        if(row >= 0 && row < (int)_categoryNames.size())
            return createIndex(row, 0, quintptr(-1));
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Child level: modifiers within a category
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
QModelIndex AvailableModifiersModel::parent(const QModelIndex& index) const
{
    if(!index.isValid())
        return {};

    quintptr id = index.internalId();
    if(id == quintptr(-1)) {
        // This is a category item at the root level.
        return {};
    }
    else {
        // This is a modifier item; its parent is the category.
        int categoryIndex = (int)id;
        return createIndex(categoryIndex, 0, quintptr(-1));
    }
}

/******************************************************************************
* Returns the number of rows under the given parent.
******************************************************************************/
int AvailableModifiersModel::rowCount(const QModelIndex& parent) const
{
    if(!parent.isValid()) {
        // Root level: number of categories
        return (int)_categoryNames.size();
    }
    else if(parent.internalId() == quintptr(-1)) {
        // Category level: number of modifiers in this category
        int categoryIndex = parent.row();
        if(categoryIndex >= 0 && categoryIndex < (int)_actionsPerCategory.size())
            return (int)_actionsPerCategory[categoryIndex].size();
    }
    // Modifiers don't have children.
    return 0;
}

/******************************************************************************
* Returns the number of columns for the children of the given parent.
******************************************************************************/
int AvailableModifiersModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

/******************************************************************************
* Returns the model's role names.
******************************************************************************/
QHash<int, QByteArray> AvailableModifiersModel::roleNames() const
{
    return {
        { Qt::DisplayRole, "title" },
        { Qt::UserRole, "iscategory" }
    };
}

/******************************************************************************
* Returns the data associated with an item.
******************************************************************************/
QVariant AvailableModifiersModel::data(const QModelIndex& index, int role) const
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
        else if(role == Qt::UserRole)
            return true; // Is a category
    }
    else {
        // Modifier item.
        int categoryIndex = (int)index.internalId();
        int modifierIndex = index.row();
        if(categoryIndex < 0 || categoryIndex >= (int)_actionsPerCategory.size())
            return {};
        if(modifierIndex < 0 || modifierIndex >= (int)_actionsPerCategory[categoryIndex].size())
            return {};

        ModifierAction* action = _actionsPerCategory[categoryIndex][modifierIndex];
        if(role == Qt::DisplayRole)
            return action->text();
        else if(role == Qt::UserRole)
            return false; // Not a category
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags AvailableModifiersModel::flags(const QModelIndex& index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    if(index.internalId() == quintptr(-1)) {
        // Category item: enabled but not selectable.
        return Qt::ItemIsEnabled;
    }
    else {
        // Modifier item.
        ModifierAction* action = actionFromIndex(index);
        if(action)
            return action->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
    }
    return Qt::NoItemFlags;
}

/******************************************************************************
* Returns the action for a modifier at a given category and row.
******************************************************************************/
ModifierAction* AvailableModifiersModel::actionAt(int categoryIndex, int modifierIndex) const
{
    if(categoryIndex >= 0 && categoryIndex < (int)_actionsPerCategory.size()) {
        if(modifierIndex >= 0 && modifierIndex < (int)_actionsPerCategory[categoryIndex].size())
            return _actionsPerCategory[categoryIndex][modifierIndex];
    }
    return nullptr;
}

/******************************************************************************
* Returns the action for a modifier from a model index.
******************************************************************************/
ModifierAction* AvailableModifiersModel::actionFromIndex(const QModelIndex& index) const
{
    if(!index.isValid() || index.internalId() == quintptr(-1))
        return nullptr;

    int categoryIndex = (int)index.internalId();
    int modifierIndex = index.row();
    return actionAt(categoryIndex, modifierIndex);
}

/******************************************************************************
* Signal handler that inserts the selected modifier into the current pipeline.
******************************************************************************/
void AvailableModifiersModel::insertModifier()
{
    // Get the action that emitted the signal.
    ModifierAction* action = qobject_cast<ModifierAction*>(sender());
    OVITO_ASSERT(action);

    // Instantiate the new modifier(s) and insert them into the pipeline.
    performTransaction(tr("Insert modifier"), [&]() {

        if(action->modifierClass()) {
            // Create an instance of the modifier.
            OORef<Modifier> modifier = static_object_cast<Modifier>(action->modifierClass()->createInstance());
            // Insert modifier into the data pipeline.
            _pipelineListModel->applyModifiers({modifier});
        }
        else if(!action->templateName().isEmpty()) {
            // Load modifier template from the store.
            QVector<OORef<Modifier>> modifierSet = ModifierTemplates::get()->instantiateTemplate(action->templateName());
            // Put the modifiers into a group if the template consists of two or more modifiers.
            OORef<ModifierGroup> modifierGroup;
            if(modifierSet.size() >= 2) {
                modifierGroup = OORef<ModifierGroup>::create();
                modifierGroup->setCollapsed(true);
                modifierGroup->setTitle(action->templateName());
            }
            // Insert modifier(s) into the data pipeline.
            _pipelineListModel->applyModifiers(modifierSet, modifierGroup);
        }

        // Show the modify tab of the command panel.
        actionManager()->getAction(ACTION_COMMAND_PANEL_MODIFY)->trigger();
    });
}

/******************************************************************************
* Inserts the modifier from the given model index into the current pipeline.
******************************************************************************/
void AvailableModifiersModel::insertModifierByIndex(const QModelIndex& index)
{
    if(ModifierAction* action = actionFromIndex(index))
        action->trigger();
}

/******************************************************************************
* Rebuilds the list of actions for the modifier templates.
******************************************************************************/
void AvailableModifiersModel::refreshTemplates()
{
    std::vector<ModifierAction*>& templateActions = _actionsPerCategory[templatesCategory()];

    // Discard old list of actions.
    if(!templateActions.empty()) {
        for(ModifierAction* action : templateActions) {
            auto iter = std::find(_actions.begin(), _actions.end(), action);
            OVITO_ASSERT(iter != _actions.end());
            _actions.erase(iter);
            actionManager()->deleteAction(action);
        }
        templateActions.clear();
    }

    // Create new actions for the modifier templates.
    int count = ModifierTemplates::get()->templateList().size();
    if(count != 0) {
        for(const QString& templateName : ModifierTemplates::get()->templateList()) {
            // Create action for the modifier template.
            ModifierAction* action = ModifierAction::createForTemplate(templateName);
            templateActions.push_back(action);

            // Register it with the ActionManager.
            actionManager()->addAction(action);
            OVITO_ASSERT(action->parent() == actionManager());

            // Handle the action.
            connect(action, &QAction::triggered, this, &AvailableModifiersModel::insertModifier);

            // Append action to flat list.
            _actions.push_back(action);
        }
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Notify views that the model has been reset.
    beginResetModel();
    endResetModel();
}

/******************************************************************************
* Updates the enabled/disabled state of all modifier actions based on the current pipeline.
******************************************************************************/
void AvailableModifiersModel::updateActionState()
{
    // Retrieve the input pipeline state, which a newly inserted modifier would be applied to.
    // This is used to determine which modifiers are applicable.
    PipelineFlowState inputState;

    // Get the selected item in the pipeline editor.
    PipelineListItem* currentItem = _pipelineListModel->selectedItem();
    while(currentItem && currentItem->parent()) {
        currentItem = currentItem->parent();
    }

    // Obtain pipeline output at the selected stage.
    if(PipelineNode* pipelineNode = currentItem ? dynamic_object_cast<PipelineNode>(currentItem->object()) : nullptr) {
        inputState = pipelineNode->getCachedPipelineNodeOutput(currentAnimationTime());
    }
    else if(Pipeline* pipeline = _pipelineListModel->selectedPipeline()) {
        inputState = pipeline->getCachedPipelineOutput(currentAnimationTime());
    }

    // Update the actions.
    for(int categoryIndex = 0; categoryIndex < (int)_actionsPerCategory.size(); categoryIndex++) {
        for(int modifierIndex = 0; modifierIndex < (int)_actionsPerCategory[categoryIndex].size(); modifierIndex++) {
            ModifierAction* action = _actionsPerCategory[categoryIndex][modifierIndex];
            if(action->updateState(inputState)) {
                QModelIndex idx = index(modifierIndex, 0, index(categoryIndex, 0));
                Q_EMIT dataChanged(idx, idx);
            }
        }
    }
}

/******************************************************************************
* This handler is called whenever a new extension class has been registered at runtime.
******************************************************************************/
void AvailableModifiersModel::extensionClassAdded(OvitoClassPtr cls)
{
    // Skip classes that are not modifiers.
    if(!cls->isDerivedFrom(Modifier::OOClass()))
        return;
    ModifierClassPtr clazz = static_cast<ModifierClassPtr>(cls);

    // Skip modifiers that want to be hidden from the user.
    // Do not add it to the list of available modifiers.
    if(clazz->modifierCategory() == QStringLiteral("-"))
        return;

    // Create action for the modifier class.
    ModifierAction* action = ModifierAction::createForClass(clazz);

    // Register it with the global ActionManager.
    actionManager()->addAction(action);

    // Handle the insertion action.
    connect(action, &QAction::triggered, this, &AvailableModifiersModel::insertModifier);

    // Insert action into the list, which is sorted by name.
    auto iter = std::lower_bound(_actions.begin(), _actions.end(), action,
        [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
    _actions.insert(iter, action);

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

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableModifiersListModel::AvailableModifiersListModel(AvailableModifiersModel* sourceModel, QObject* parent)
    : QAbstractListModel(parent), _sourceModel(sourceModel)
{
    // Register this instance.
    _allListModels.push_back(this);

    // Initialize UI colors.
    updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &AvailableModifiersListModel::updateColorPalette);
QT_WARNING_POP

    // Connect to source model signals.
    connect(_sourceModel, &QAbstractItemModel::modelAboutToBeReset, this, &AvailableModifiersListModel::onSourceModelAboutToBeReset);
    connect(_sourceModel, &QAbstractItemModel::modelReset, this, &AvailableModifiersListModel::onSourceModelReset);
    connect(_sourceModel, &QAbstractItemModel::dataChanged, this, &AvailableModifiersListModel::onSourceDataChanged);

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
void AvailableModifiersListModel::updateColorPalette(const QPalette& palette)
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
* Called when the source model is about to be reset.
******************************************************************************/
void AvailableModifiersListModel::onSourceModelAboutToBeReset()
{
    beginResetModel();
}

/******************************************************************************
* Called when the source model has been reset.
******************************************************************************/
void AvailableModifiersListModel::onSourceModelReset()
{
    updateModelLists();
    endResetModel();
}

/******************************************************************************
* Called when data in the source model changes.
******************************************************************************/
void AvailableModifiersListModel::onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    // Find the corresponding rows in our flat list and emit dataChanged.
    // For simplicity, we'll emit dataChanged for the entire model.
    if(!_modelActions.empty())
        Q_EMIT dataChanged(index(0), index((int)_modelActions.size() - 1));
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int AvailableModifiersListModel::rowCount(const QModelIndex& parent) const
{
    return _modelStrings.size();
}

/******************************************************************************
* Returns the model's role names.
******************************************************************************/
QHash<int, QByteArray> AvailableModifiersListModel::roleNames() const
{
    return {
        { Qt::DisplayRole, "title" },
        { Qt::UserRole, "isheader" },
        { Qt::FontRole, "font" }
    };
}

/******************************************************************************
* Rebuilds the internal list of model items.
******************************************************************************/
void AvailableModifiersListModel::updateModelLists()
{
    _modelStrings.clear();
    _modelStrings.push_back(tr("Add modification..."));
    _modelActions.clear();
    _modelActions.push_back(nullptr);
    _getMoreExtensionsItemIndex = -1;

    if(_useCategories) {
        for(int categoryIndex = 0; categoryIndex < _sourceModel->categoryCount(); categoryIndex++) {
            const std::vector<ModifierAction*>& categoryActions = _sourceModel->categoryActions(categoryIndex);
            if(!categoryActions.empty()) {
                _modelActions.push_back(nullptr);
                _modelStrings.push_back(_sourceModel->categoryName(categoryIndex));
                for(ModifierAction* action : categoryActions) {
                    _modelActions.push_back(action);
                    _modelStrings.push_back(action->text());
                }
#ifndef OVITO_BUILD_BASIC
                if(_sourceModel->categoryName(categoryIndex) == tr("Python modifiers")) {
#else
                if(_sourceModel->categoryName(categoryIndex) == tr("Python modifiers (Pro)")) {
#endif
                    _getMoreExtensionsItemIndex = _modelStrings.size();
                    _modelActions.push_back(nullptr);
                    _modelStrings.push_back(tr("Get more modifiers..."));
                }
            }
        }
    }
    else {
        const std::vector<ModifierAction*>& allActions = _sourceModel->allActions();
        _modelActions.insert(_modelActions.end(), allActions.begin(), allActions.end());
        _modelStrings.reserve(_modelActions.size());
        for(ModifierAction* action : allActions)
            _modelStrings.push_back(action->text());
    }

    if(_getMoreExtensionsItemIndex == -1) {
        _getMoreExtensionsItemIndex = _modelStrings.size();
        _modelActions.push_back(nullptr);
        _modelStrings.push_back(tr("Get more modifiers..."));
    }
}

/******************************************************************************
* Returns the data associated with a list item.
******************************************************************************/
QVariant AvailableModifiersListModel::data(const QModelIndex& index, int role) const
{
    if(role == Qt::DisplayRole) {
        if(index.row() >= 0 && index.row() < (int)_modelStrings.size())
            return _modelStrings[index.row()];
    }
    else if(role == Qt::UserRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < (int)_modelActions.size() && _modelActions[index.row()] == nullptr && index.row() != getMoreExtensionsItemIndex())
            return true;
        else
            return false;
    }
    else if(role == Qt::FontRole) {
        if(index.row() == getMoreExtensionsItemIndex())
            return _getMoreExtensionsFont;
        // Is it a category header?
        else if(index.row() > 0 && index.row() < (int)_modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryFont;
    }
    else if(role == Qt::ForegroundRole) {
        if(index.row() == getMoreExtensionsItemIndex())
            return _getMoreExtensionsForegroundBrush;
        // Is it a category header?
        else if(index.row() > 0 && index.row() < (int)_modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryForegroundBrush;
    }
    else if(role == Qt::BackgroundRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < (int)_modelActions.size() && _modelActions[index.row()] == nullptr && index.row() != getMoreExtensionsItemIndex())
            return _categoryBackgroundBrush;
    }
    else if(role == Qt::TextAlignmentRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < (int)_modelActions.size() && _modelActions[index.row()] == nullptr && index.row() != getMoreExtensionsItemIndex())
            return Qt::AlignCenter;
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags AvailableModifiersListModel::flags(const QModelIndex& index) const
{
    if(index.row() > 0 && index.row() < (int)_modelActions.size()) {
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
* Returns the action that belongs to the given model index.
******************************************************************************/
ModifierAction* AvailableModifiersListModel::actionFromIndex(int index) const
{
    return (index >= 0 && index < (int)_modelActions.size()) ? _modelActions[index] : nullptr;
}

/******************************************************************************
* Sets whether available modifiers are sorted by category instead of name.
******************************************************************************/
void AvailableModifiersListModel::setUseCategories(bool on)
{
    if(on != _useCategories) {
        _useCategories = on;
        beginResetModel();
        updateModelLists();
        endResetModel();
    }
}

/******************************************************************************
* Inserts the i-th modifier from this model into the current pipeline.
******************************************************************************/
void AvailableModifiersListModel::insertModifierByIndex(int index)
{
    if(ModifierAction* action = actionFromIndex(index))
        action->trigger();
}

/******************************************************************************
* Returns whether sorting of available modifiers into categories is enabled globally for the application.
******************************************************************************/
bool AvailableModifiersListModel::useCategoriesGlobal()
{
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    return settings.value("modifiers/sort_by_category", true).toBool();
#else
    return true;
#endif
}

/******************************************************************************
* Sets whether available modifiers are sorted by category globally for the application.
******************************************************************************/
void AvailableModifiersListModel::setUseCategoriesGlobal(bool on)
{
#ifndef OVITO_DISABLE_QSETTINGS
    if(on != useCategoriesGlobal()) {
        QSettings settings;
        settings.setValue("modifiers/sort_by_category", on);
    }

    for(AvailableModifiersListModel* listModel : _allListModels)
        listModel->setUseCategories(on);
#endif
}

}   // End of namespace
