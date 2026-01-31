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
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

class OverlayListModel; // Defined in OverlayListModel.h

class OVITO_GUIBASE_EXPORT OverlayAction : public QAction
{
    Q_OBJECT

public:

    /// Constructs an action for a built-in layer class.
    static OverlayAction* createForClass(const ViewportOverlay::OOMetaClass* clazz);

    /// Constructs an action for a viewport layer template.
    static OverlayAction* createForTemplate(const QString& templateName);

    /// Returns the viewport layer's category.
    const QString& category() const { return _category; }

    /// Returns the overlay class descriptor if this action represents a built-in overlay type.
    OvitoClassPtr layerClass() const { return _layerClass; }

   /// The name of the viewport layer template if this action represents a saved template.
    const QString& templateName() const { return _templateName; }

    /// The absolute path of the modifier script if this action represents a Python-based modifier function.
    const QString& scriptPath() const { return _scriptPath; }

private:

    /// The Ovito class descriptor of the viewport layer subclass.
    OvitoClassPtr _layerClass = nullptr;

    /// The viewport layer's category.
    QString _category;

    /// The path to the overlay script on disk.
    QString _scriptPath;

    /// The name of the viewport layer template.
    QString _templateName;
};

/**
 * A Qt tree model that organizes all available viewport layer types by category.
 * Root items are categories, and child items are viewport layers within each category.
 */
class OVITO_GUIBASE_EXPORT AvailableOverlaysModel : public QAbstractItemModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysModel(QObject* parent, UserInterface& ui, OverlayListModel* overlayListModel);

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

    /// Returns the model's role names.
    virtual QHash<int, QByteArray> roleNames() const override;

    /// Returns the number of categories.
    int categoryCount() const { return (int)_categoryNames.size(); }

    /// Returns the name of the category at the given index.
    const QString& categoryName(int categoryIndex) const { return _categoryNames[categoryIndex]; }

    /// Returns the list of overlay actions for the given category.
    const std::vector<OverlayAction*>& categoryActions(int categoryIndex) const { return _actionsPerCategory[categoryIndex]; }

    /// Returns the action for an overlay at a given category and row.
    OverlayAction* actionAt(int categoryIndex, int overlayIndex) const;

    /// Returns the action for an overlay from a model index.
    OverlayAction* actionFromIndex(const QModelIndex& index) const;

    /// Returns the category index for the viewport layer templates.
    int templatesCategory() const { return (int)_actionsPerCategory.size() - 1; }

    /// Returns the complete list of overlay actions, sorted alphabetically.
    const std::vector<OverlayAction*>& allActions() const { return _actions; }

public Q_SLOTS:

    /// Inserts the overlay from the given model index into the current viewport.
    void insertOverlayByIndex(const QModelIndex& index);

private Q_SLOTS:

    /// Rebuilds the list of actions for the viewport layer templates.
    void refreshTemplates();

    /// Signal handler that inserts the selected viewport layer into the active viewport.
    void insertViewportLayer();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// The list of viewport layer actions, sorted alphabetically.
    std::vector<OverlayAction*> _actions;

    /// The list of viewport layer actions, sorted by category.
    std::vector<std::vector<OverlayAction*>> _actionsPerCategory;

    /// The list of viewport layer categories.
    std::vector<QString> _categoryNames;

    /// The model representing the viewport layers of the active viewport.
    OverlayListModel* _overlayListModel;

    /// The list of directories searched for user-defined viewport layer scripts.
    QVector<QDir> _layerScriptDirectories;
};

/**
 * A wrapper Qt list model that presents the tree-based AvailableOverlaysModel as a flat list.
 * This is used for QComboBox widgets that need a linear list of items.
 */
class OVITO_GUIBASE_EXPORT AvailableOverlaysListModel : public QAbstractListModel
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysListModel(AvailableOverlaysModel* sourceModel, QObject* parent = nullptr);

    /// Returns the number of rows in the model.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Returns the data associated with a list item.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// Returns the flags for an item.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Returns the model's role names.
    virtual QHash<int, QByteArray> roleNames() const override;

    /// Returns the action that belongs to the given model index.
    OverlayAction* actionFromIndex(int index) const;

    /// Returns the action that belongs to the given model index.
    OverlayAction* actionFromIndex(const QModelIndex& index) const { return actionFromIndex(index.row()); }

    /// Returns the list index where the "Get more layers..." item is located.
    int getMoreExtensionsItemIndex() const { return _getMoreExtensionsItemIndex; }

    /// Returns the underlying tree model.
    AvailableOverlaysModel* sourceModel() const { return _sourceModel; }

    /// Inserts the i-th overlay from this model into the current viewport.
    void insertOverlayByIndex(int index);

private Q_SLOTS:

    /// Updates the color brushes of the model.
    void updateColorPalette(const QPalette& palette);

    /// Called when the source model is about to be reset.
    void onSourceModelAboutToBeReset();

    /// Called when the source model has been reset.
    void onSourceModelReset();

private:

    /// Rebuilds the internal list of model items.
    void updateModelLists();

    /// The underlying tree model.
    AvailableOverlaysModel* _sourceModel;

    /// The overlay actions as shown by the model.
    std::vector<OverlayAction*> _modelActions;

    /// The display strings as shown by the model.
    std::vector<QString> _modelStrings;

    /// Font used for category header items.
    QFont _categoryFont;

    /// Colors used for category header items.
    QBrush _categoryBackgroundBrush;
    QBrush _categoryForegroundBrush;

    /// The font used for "Get more layers..." item.
    QFont _getMoreExtensionsFont;

    /// Color used for the "Get more layers..." item.
    QBrush _getMoreExtensionsForegroundBrush;

    /// The list index where the "Get more layers..." item is located.
    int _getMoreExtensionsItemIndex = -1;
};

}   // End of namespace
