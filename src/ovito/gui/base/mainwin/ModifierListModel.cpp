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
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModifierTemplates.h>
#include "ModifierListModel.h"
#include "PipelineListModel.h"

namespace Ovito {

QVector<ModifierListModel*> ModifierListModel::_allModels;

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
ModifierListModel::ModifierListModel(QObject* parent, UserInterface& userInterface, PipelineListModel* pipelineListModel) : QAbstractListModel(parent), _userInterface(userInterface), _pipelineListModel(pipelineListModel)
{
    OVITO_ASSERT(userInterface.actionManager());

    // Register this instance.
    _allModels.push_back(this);

    // Update the state of this model's actions whenever the ActionManager requests it.
    connect(userInterface.actionManager(), &ActionManager::actionUpdateRequested, this, &ModifierListModel::updateActionState);

    // Initialize UI colors.
    updateColorPalette(QGuiApplication::palette());
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &ModifierListModel::updateColorPalette);
QT_WARNING_POP

    // Enumerate all built-in modifier classes.
    for(ModifierClassPtr clazz : PluginManager::instance().metaclassMembers<Modifier>()) {

        // Skip modifiers that want to be hidden from the user.
        // Do not add it to the list of available modifiers.
        if(clazz->modifierCategory() == QStringLiteral("-"))
            continue;

        // Create action for the modifier class.
        ModifierAction* action = ModifierAction::createForClass(clazz);
        _actions.push_back(action);

        // Register it with the global ActionManager.
        userInterface.actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == userInterface.actionManager());

        // Handle the insertion action.
        connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);
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
        userInterface.actionManager()->addAction(action);
        OVITO_ASSERT(action->parent() == userInterface.actionManager());

        // Handle the action.
        connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);

        // Insert action into complete list.
        _actions.push_back(action);
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Listen for changes to the underlying modifier template list.
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsInserted, this, &ModifierListModel::refreshModifierTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &ModifierListModel::refreshModifierTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::modelReset, this, &ModifierListModel::refreshModifierTemplates);
    connect(ModifierTemplates::get(), &QAbstractItemModel::dataChanged, this, &ModifierListModel::refreshModifierTemplates);

    // Define font, colors, etc.
    _categoryFont = QGuiApplication::font();
    _categoryFont.setBold(true);
#ifndef Q_OS_WIN
    if(_categoryFont.pixelSize() < 0)
        _categoryFont.setPointSize(_categoryFont.pointSize() * 4 / 5);
    else
        _categoryFont.setPixelSize(_categoryFont.pixelSize() * 4 / 5);
#endif

    // Generate list items.
    updateModelLists();
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void ModifierListModel::updateColorPalette(const QPalette& palette)
{
    bool darkTheme = palette.color(QPalette::Active, QPalette::Window).lightness() < 100;
#ifndef Q_OS_LINUX
    _categoryBackgroundBrush = darkTheme ? palette.mid() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
#else
    _categoryBackgroundBrush = darkTheme ? palette.window() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
#endif
    _categoryForegroundBrush = QBrush(darkTheme ? QColor(Qt::blue).lighter() : QColor(Qt::blue));
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int ModifierListModel::rowCount(const QModelIndex& parent) const
{
    return _modelStrings.size();
}

/******************************************************************************
* Returns the model's role names.
******************************************************************************/
QHash<int, QByteArray> ModifierListModel::roleNames() const
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
void ModifierListModel::updateModelLists()
{
    beginResetModel();
    _modelStrings.clear();
    _modelStrings.push_back(tr("Add modification..."));
    _modelActions.clear();
    _modelActions.push_back(nullptr);
    if(_useCategories) {
        int categoryIndex = 0;
        for(const auto& categoryActions : _actionsPerCategory) {
            if(!categoryActions.empty()) {
                _modelActions.push_back(nullptr);
                _modelStrings.push_back(_categoryNames[categoryIndex]);
                for(ModifierAction* action : categoryActions) {
                    _modelActions.push_back(action);
                    _modelStrings.push_back(action->text());
                }
            }
            categoryIndex++;
        }
    }
    else {
        _modelActions.insert(_modelActions.end(), _actions.begin(), _actions.end());
        _modelStrings.reserve(_modelActions.size());
        for(ModifierAction* action : _actions)
            _modelStrings.push_back(action->text());
    }
    endResetModel();
}

/******************************************************************************
* Returns the data associated with a list item.
******************************************************************************/
QVariant ModifierListModel::data(const QModelIndex& index, int role) const
{
    if(role == Qt::DisplayRole) {
        if(index.row() >= 0 && index.row() < _modelStrings.size())
            return _modelStrings[index.row()];
    }
    else if(role == Qt::UserRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return true;
        else
            return false;
    }
    else if(role == Qt::FontRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryFont;
    }
    else if(role == Qt::ForegroundRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryForegroundBrush;
    }
    else if(role == Qt::BackgroundRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return _categoryBackgroundBrush;
    }
    else if(role == Qt::TextAlignmentRole) {
        // Is it a category header?
        if(index.row() > 0 && index.row() < _modelActions.size() && _modelActions[index.row()] == nullptr)
            return Qt::AlignCenter;
    }
    return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags ModifierListModel::flags(const QModelIndex& index) const
{
    if(index.row() > 0 && index.row() < _modelActions.size()) {
        if(_modelActions[index.row()])
            return _modelActions[index.row()]->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
        else
            return Qt::ItemIsEnabled;
    }
    return QAbstractListModel::flags(index);
}

/******************************************************************************
* Signal handler that inserts the selected modifier into the current pipeline.
******************************************************************************/
void ModifierListModel::insertModifier()
{
    // Get the action that emitted the signal.
    ModifierAction* action = qobject_cast<ModifierAction*>(sender());
    OVITO_ASSERT(action);

    // Instantiate the new modifier(s) and insert them into the pipeline.
    _userInterface.performTransaction(tr("Insert modifier"), [&]() {

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
    });
}

/******************************************************************************
* Inserts the i-th modifier from this model into the current pipeline.
******************************************************************************/
void ModifierListModel::insertModifierByIndex(int index)
{
    if(QAction* action = actionFromIndex(index))
        action->trigger();
}

/******************************************************************************
* Rebuilds the list of actions for the modifier templates.
******************************************************************************/
void ModifierListModel::refreshModifierTemplates()
{
    std::vector<ModifierAction*>& templateActions = _actionsPerCategory[modifierTemplatesCategory()];

    // Discard old list of actions.
    if(!templateActions.empty()) {
        for(ModifierAction* action : templateActions) {
            auto iter = std::find(_actions.begin(), _actions.end(), action);
            OVITO_ASSERT(iter != _actions.end());
            _actions.erase(iter);
            _userInterface.actionManager()->deleteAction(action);
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
            _userInterface.actionManager()->addAction(action);
            OVITO_ASSERT(action->parent() == _userInterface.actionManager());

            // Handle the action.
            connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);

            // Append action to flat list.
            _actions.push_back(action);
        }
    }

    // Sort complete list of actions by name.
    std::sort(_actions.begin(), _actions.end(), [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

    // Regenerate list items.
    updateModelLists();
}

/******************************************************************************
* Updates the enabled/disabled state of all modifier actions based on the current pipeline.
******************************************************************************/
void ModifierListModel::updateActionState()
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
    if(currentItem) {
        if(PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(currentItem->object())) {
            inputState = pipelineNode->getCachedPipelineNodeOutput(_userInterface.datasetContainer().currentAnimationTime());
        }
        else if(Pipeline* pipeline = _pipelineListModel->selectedPipeline()) {
            inputState = pipeline->getCachedPipelineOutput(_userInterface.datasetContainer().currentAnimationTime());
        }
    }

    // Update the actions.
    for(int row = 1; row < _modelActions.size(); row++) {
        if(_modelActions[row] && _modelActions[row]->updateState(inputState))
            Q_EMIT dataChanged(index(row), index(row));
    }
}

/******************************************************************************
* Sets whether available modifiers are storted by category instead of name.
******************************************************************************/
void ModifierListModel::setUseCategories(bool on)
{
    if(on != _useCategories) {
        _useCategories = on;
        updateModelLists();
    }
}

/******************************************************************************
* Returns whether sorting of available modifiers into categories is enabled globally for the application.
******************************************************************************/
bool ModifierListModel::useCategoriesGlobal()
{
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    return settings.value("modifiers/sort_by_category", true).toBool();
#else
    return true;
#endif
}

/******************************************************************************
* Sets whether available modifiers are storted by category gloablly for the application.
******************************************************************************/
void ModifierListModel::setUseCategoriesGlobal(bool on)
{
#ifndef OVITO_DISABLE_QSETTINGS
    if(on != useCategoriesGlobal()) {
        QSettings settings;
        settings.setValue("modifiers/sort_by_category", on);
    }

    for(ModifierListModel* model : _allModels)
        model->setUseCategories(on);
#endif
}

}   // End of namespace
