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

#pragma once


#include <ovito/gui/base/GUIBase.h>

namespace Ovito {

class PipelineListModel;    // Defined in PipelineListModel.h

class OVITO_GUIBASE_EXPORT ModifierAction : public QAction
{
    Q_OBJECT

public:

    /// Constructs an action for a built-in modifier class.
    static ModifierAction* createForClass(ModifierClassPtr clazz);

    /// Constructs an action for a modifier template.
    static ModifierAction* createForTemplate(const QString& templateName);

    /// Returns the modifier's category.
    const QString& category() const { return _category; }

    /// Returns the modifier class descriptor if this action represents a built-in modifier.
    ModifierClassPtr modifierClass() const { return _modifierClass; }

    /// The name of the modifier template if this action represents a saved modifier template.
    const QString& templateName() const { return _templateName; }

    /// Updates the actions enabled/disabled state depending on the current data pipeline.
    bool updateState(const PipelineFlowState& input);

private:

    /// The Ovito class descriptor of the modifier subclass.
    ModifierClassPtr _modifierClass = nullptr;

    /// The modifier's category.
    QString _category;

    /// The name of the modifier template.
    QString _templateName;
};

/**
 * A Qt tree model that organizes all available modifier types by category.
 * Root items are categories, and child items are modifiers within each category.
 */
class OVITO_GUIBASE_EXPORT AvailableModifiersModel : public QAbstractItemModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableModifiersModel(QObject* parent, UserInterface& ui, PipelineListModel* pipelineListModel);

    /// Returns the model index for the item at the given row and column under the given parent.
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the parent of the model item with the given index.
    virtual QModelIndex parent(const QModelIndex& index) const override;

    /// Returns the number of rows under the given parent.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the number of columns for the children of the given parent.
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the data associated with an item.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// Returns the flags for an item.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Returns the name of the category at the given index.
    const QString& categoryName(int categoryIndex) const { return _categoryNames[categoryIndex]; }

    /// Returns the list of modifier actions for the given category.
    const std::vector<QAction*>& categoryActions(int categoryIndex) const { return _actionsPerCategory[categoryIndex]; }

    /// Returns the action for a modifier at a given category and row.
    QAction* actionAt(int categoryIndex, int modifierIndex) const;

    /// Returns the action for a modifier from a model index.
    QAction* actionFromIndex(const QModelIndex& index) const;

    /// Returns the category index for the modifier templates.
    int templatesCategory() const { return (int)_actionsPerCategory.size() - 1; }

public Q_SLOTS:

    /// Updates the enabled/disabled state of all modifier actions based on the current pipeline.
    void updateActionState();

private Q_SLOTS:

    /// Signal handler that inserts the selected modifier into the current pipeline.
    void insertModifier();

    /// Rebuilds the list of actions for the modifier templates.
    void refreshTemplates();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// The list of modifier actions, sorted by category.
    std::vector<std::vector<QAction*>> _actionsPerCategory;

    /// The list of modifier categories.
    std::vector<QString> _categoryNames;

    /// Model representing the current data pipeline.
    PipelineListModel* _pipelineListModel;

    /// Wrapper action for managing modifier templates.
    QAction* _manageTemplatesAction = nullptr;
};

}   // End of namespace
