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
* Constructs an action for a Python modifier script.
******************************************************************************/
ModifierAction* ModifierAction::createForScript(const QString& fileName, const QDir& directory)
{
	ModifierAction* action = new ModifierAction();
	action->_scriptPath = directory.filePath(fileName);

	// Generate a unique identifier for the action:
	action->setObjectName(QStringLiteral("InsertModifierScript.%1").arg(action->_scriptPath));

	// Set the action's UI display name. Chop of ".py" extension of filename.
	action->setText(fileName.chopped(3));

	// Give the modifier a status bar text.
	action->setStatusTip(tr("Insert this Python modifier into the data pipeline."));

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
		_allActions.push_back(action);

		// Register it with the global ActionManager.
		userInterface.actionManager()->addAction(action);
		OVITO_ASSERT(action->parent() == userInterface.actionManager());

		// Handle the insertion action.
		connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);
	}

	// Order actions list by category name.
	std::sort(_allActions.begin(), _allActions.end(), [](ModifierAction* a, ModifierAction* b) { return QString::localeAwareCompare(a->category(), b->category()) < 0; });

	// Sort actions into categories.
	for(ModifierAction* action : _allActions) {
		if(_categoryNames.empty() || _categoryNames.back() != action->category()) {
			_categoryNames.push_back(action->category());
			_actionsPerCategory.emplace_back();
		}
		_actionsPerCategory.back().push_back(action);
	}

	// Sort actions by name within each category.
	for(std::vector<ModifierAction*>& actions : _actionsPerCategory)
		std::sort(actions.begin(), actions.end(), [](ModifierAction* a, ModifierAction* b) { return QString::localeAwareCompare(a->text(), b->text()) < 0; });

	// Sort complete list of actions by name.
	std::sort(_allActions.begin(), _allActions.end(), [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });

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

		// Insert action into complete list, which is alphabetically sorted by name.
		auto insertionIter = std::lower_bound(_allActions.begin(), _allActions.end(), action, [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
		_allActions.insert(insertionIter, action);
	}

	// Listen for changes to the underlying modifier template list.
	connect(ModifierTemplates::get(), &QAbstractItemModel::rowsInserted, this, &ModifierListModel::refreshModifierTemplates);
	connect(ModifierTemplates::get(), &QAbstractItemModel::rowsRemoved, this, &ModifierListModel::refreshModifierTemplates);
	connect(ModifierTemplates::get(), &QAbstractItemModel::modelReset, this, &ModifierListModel::refreshModifierTemplates);
	connect(ModifierTemplates::get(), &QAbstractItemModel::dataChanged, this, &ModifierListModel::refreshModifierTemplates);

	// Add the built-in extension script directory below the application directory.
	QString prefixDir = QCoreApplication::applicationDirPath();
	_modifierScriptDirectories.push_back(prefixDir + QStringLiteral("/" OVITO_SCRIPT_EXTENSIONS_RELATIVE_PATH "/modifiers"));
	// Add the script directories in the user's home directory.
	for(const QString& configLocation : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
		_modifierScriptDirectories.push_back(configLocation + QStringLiteral("/Ovito/scripts/modifiers"));
#ifdef Q_OS_MACOS
	// For backward compatibility with OVITO 3.7.0:
	_modifierScriptDirectories.push_back(QDir::homePath() + QStringLiteral("/.config/Ovito/scripts/modifiers"));
#endif
	// Make sure our list doesn't contain the same directory twice.
	std::sort(_modifierScriptDirectories.begin(), _modifierScriptDirectories.end());
	_modifierScriptDirectories.erase(std::unique(_modifierScriptDirectories.begin(), _modifierScriptDirectories.end()), _modifierScriptDirectories.end());

	// Create category for script modifiers.
#ifndef OVITO_BUILD_BASIC
	_categoryNames.push_back(tr("Python modifiers"));
#else
	_categoryNames.push_back(tr("Python modifiers (Pro)"));
#endif
	_actionsPerCategory.emplace_back();

	// Load user-defined Python script modifiers.
	const QStringList nameFilters(QStringLiteral("*.py"));
	for(const QDir& scriptsDirectory : _modifierScriptDirectories) {
		QStringList scriptFiles = scriptsDirectory.entryList(nameFilters, QDir::Files, QDir::Name);
		for(const QString& fileName : scriptFiles) {

			// Create action for the modifier script.
			ModifierAction* action = ModifierAction::createForScript(fileName, scriptsDirectory);
			_actionsPerCategory.back().push_back(action);

			// Register it with the global ActionManager.
			userInterface.actionManager()->addAction(action);
			OVITO_ASSERT(action->parent() == userInterface.actionManager());

			// Handle the action.
			connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);

			// Insert action into complete list, which is alphabetically sorted by name.
			auto insertionIter = std::lower_bound(_allActions.begin(), _allActions.end(), action, [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
			_allActions.insert(insertionIter, action);
		}
	}

	// Define font, colors, etc.
	_categoryFont = QGuiApplication::font();
	_categoryFont.setBold(true);
#ifndef Q_OS_WIN
	if(_categoryFont.pixelSize() < 0)
		_categoryFont.setPointSize(_categoryFont.pointSize() * 4 / 5);
	else
		_categoryFont.setPixelSize(_categoryFont.pixelSize() * 4 / 5);
#endif
}

/******************************************************************************
* Updates the color brushes of the model.
******************************************************************************/
void ModifierListModel::updateColorPalette(const QPalette& palette)
{
	bool darkTheme = palette.color(QPalette::Active, QPalette::Window).lightness() < 100;
	_categoryBackgroundBrush = darkTheme ? palette.mid() : QBrush{Qt::lightGray, Qt::Dense4Pattern};
	_categoryForegroundBrush = QBrush(darkTheme ? QColor(Qt::blue).lighter() : QColor(Qt::blue));
}

/******************************************************************************
* Returns the action that belongs to the given model index.
******************************************************************************/
ModifierAction* ModifierListModel::actionFromIndex(int index) const
{
	if(index == 0) return nullptr; // Index 0 is the "Add modification..." item.
	index--;

	if(_useCategories) {
		for(const auto& categoryActions : _actionsPerCategory) {
			if(!categoryActions.empty()) {
				if(index == 0) return nullptr;
				index--;
				if(index < categoryActions.size())
					return categoryActions[index];
				index -= categoryActions.size();
			}
		}
	}
	else {
		if(index < _allActions.size())
			return _allActions[index];
	}

	return nullptr;
}

/******************************************************************************
* Returns the index of the modifier category to which the given list model index belongs.
******************************************************************************/
int ModifierListModel::categoryIndexFromListIndex(int index) const
{
	if(index == 0) return -1;
	index--;

	if(_useCategories) {
		int categoryIndex = 0;
		for(const auto& categoryActions : _actionsPerCategory) {
			if(index == 0)
				return categoryIndex;
			if(!categoryActions.empty())
				index -= categoryActions.size() + 1;
			categoryIndex++;
		}
	}

	return -1;
}

/******************************************************************************
* Returns the list model index where the given modifier category starts.
******************************************************************************/
int ModifierListModel::listIndexFromCategoryIndex(int categoryIndex) const
{
	if(_useCategories) {
		int index = 1;
		for(const auto& categoryActions : _actionsPerCategory) {
			if(categoryIndex == 0)
				return index;
			if(!categoryActions.empty())
				index += categoryActions.size() + 1;
			categoryIndex--;
		}
	}

	OVITO_ASSERT(false);
	return -1;
}

/******************************************************************************
* Returns the number of rows in the model.
******************************************************************************/
int ModifierListModel::rowCount(const QModelIndex& parent) const 
{
	int sum = 1; // First entry is the "Add modification..." item.

	if(_useCategories) {
		for(const auto& categoryActions : _actionsPerCategory)
			if(!categoryActions.empty())
				sum += categoryActions.size() + 1; 	// Take into account category header.
	}
	else {
		sum += _allActions.size();
	}

	return sum;
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
* Returns the data associated with a list item.
******************************************************************************/
QVariant ModifierListModel::data(const QModelIndex& index, int role) const
{
	if(role == Qt::DisplayRole) {
		if(ModifierAction* action = actionFromIndex(index)) {
			return action->text();
		}
		else {
			int categoryIndex = categoryIndexFromListIndex(index.row());
			if(categoryIndex < 0)
				return tr("Add modification...");
			else
				return _categoryNames[categoryIndex];
		}
	}
	else if(role == Qt::UserRole) {
		if(categoryIndexFromListIndex(index.row()) >= 0)
			return true;
		else
			return false;
	}
	else if(role == Qt::FontRole) {
		if(categoryIndexFromListIndex(index.row()) >= 0)
			return _categoryFont;
	}
	else if(role == Qt::ForegroundRole) {
		if(categoryIndexFromListIndex(index.row()) >= 0)
			return _categoryForegroundBrush;
	}
	else if(role == Qt::BackgroundRole) {
		if(categoryIndexFromListIndex(index.row()) >= 0)
			return _categoryBackgroundBrush;
	}
	else if(role == Qt::TextAlignmentRole) {
		if(categoryIndexFromListIndex(index.row()) >= 0)
			return Qt::AlignCenter;
	}
	return {};
}

/******************************************************************************
* Returns the flags for an item.
******************************************************************************/
Qt::ItemFlags ModifierListModel::flags(const QModelIndex& index) const
{
	if(categoryIndexFromListIndex(index.row()) >= 0)
		return Qt::ItemIsEnabled;
	else if(ModifierAction* action = actionFromIndex(index))
		return action->isEnabled() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;

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
		else if(!action->scriptPath().isEmpty()) {
			// Get the PythonScriptModifier modifier class.
			if(OvitoClassPtr clazz = PluginManager::instance().findClass({}, QStringLiteral("PythonScriptModifier"))) {
				if(!clazz->isAbstract() && clazz->isDerivedFrom(Modifier::OOClass())) {
					const Modifier::OOMetaClass* modifierClass = static_cast<const Modifier::OOMetaClass*>(clazz);

					// Instantiate the PythonScriptModifier class.
					OORef<Modifier> modifier = static_object_cast<Modifier>(modifierClass->createInstance());
					OVITO_CHECK_OBJECT_POINTER(modifier);
					{
						UndoSuspender noUndo;
						modifier->setTitle(action->text());

						// Load the script code from the template file.
						bool callSuccessful = QMetaObject::invokeMethod(modifier, "loadCodeTemplate", Qt::DirectConnection, Q_ARG(const QString&, action->scriptPath()));
						OVITO_ASSERT(callSuccessful);
					}

					// Insert modifier(s) into the data pipeline.
					_pipelineListModel->applyModifiers({modifier});
				}
			}
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
		if(_useCategories) {
			int startIndex = listIndexFromCategoryIndex(modifierTemplatesCategory());
			beginRemoveRows(QModelIndex(), startIndex, startIndex + templateActions.size());
		}
		for(ModifierAction* action : templateActions) {
			auto iter = std::find(_allActions.begin(), _allActions.end(), action);
			OVITO_ASSERT(iter != _allActions.end());
			int deleteIndex = 1 + std::distance(_allActions.begin(), iter);
			if(!_useCategories)
				beginRemoveRows(QModelIndex(), deleteIndex, deleteIndex);
			_allActions.erase(iter);
			if(!_useCategories)
				endRemoveRows();
			_userInterface.actionManager()->deleteAction(action);
		}
		templateActions.clear();
		if(_useCategories)
			endRemoveRows();
	}

	// Create new actions for the modifier templates.
	int count = ModifierTemplates::get()->templateList().size();
	if(count != 0) {
		if(_useCategories) {
			int startIndex = listIndexFromCategoryIndex(modifierTemplatesCategory());
			beginInsertRows(QModelIndex(), startIndex, startIndex + count);
		}
		for(const QString& templateName : ModifierTemplates::get()->templateList()) {
			// Create action for the modifier template.
			ModifierAction* action = ModifierAction::createForTemplate(templateName);
			templateActions.push_back(action);

			// Register it with the ActionManager.
			_userInterface.actionManager()->addAction(action);
			OVITO_ASSERT(action->parent() == _userInterface.actionManager());

			// Handle the action.
			connect(action, &QAction::triggered, this, &ModifierListModel::insertModifier);

			// Insert action into complete, alphabetically sorted list.
			auto insertionIter = std::lower_bound(_allActions.begin(), _allActions.end(), action, [](const ModifierAction* a, const ModifierAction* b) { return a->text().compare(b->text(), Qt::CaseInsensitive) < 0; });
			if(!_useCategories) {
				int insertionIndex = std::distance(_allActions.begin(), insertionIter);
				beginInsertRows(QModelIndex(), insertionIndex, insertionIndex);
			}
			_allActions.insert(insertionIter, action);
			if(!_useCategories)
				endInsertRows();
		}
		if(_useCategories)
			endInsertRows();
	}
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

	// Evaluate pipeline at the selected stage.
	if(currentItem) {
		if(AnimationSettings* anim = _userInterface.datasetContainer().activeAnimationSettings()) {
			_userInterface.handleExceptions([&] {
				if(PipelineObject* pipelineObject = dynamic_object_cast<PipelineObject>(currentItem->object())) {
					inputState = pipelineObject->evaluateSynchronous(PipelineEvaluationRequest(anim));
				}
				else if(PipelineSceneNode* pipeline = _pipelineListModel->selectedPipeline()) {
					inputState = pipeline->evaluatePipelineSynchronous(anim->currentTime(), false);
				}
			});
		}
	}

	// Update the actions.
	int row = 1;
	if(_useCategories) {
		for(const auto& categoryActions : _actionsPerCategory) {
			if(!categoryActions.empty()) 
				row++;
			for(ModifierAction* action : categoryActions) {
				if(action->updateState(inputState))
					Q_EMIT dataChanged(index(row), index(row));
				row++;
			}
		}
	}
	else {
		for(ModifierAction* action : _allActions) {
			if(action->updateState(inputState))
				Q_EMIT dataChanged(index(row), index(row));
			row++;
		}
	}
}

/******************************************************************************
* Sets whether available modifiers are storted by category instead of name.
******************************************************************************/
void ModifierListModel::setUseCategories(bool on)
{
	if(on != _useCategories) {
		beginResetModel();
		_useCategories = on;
		endResetModel();
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

}	// End of namespace
