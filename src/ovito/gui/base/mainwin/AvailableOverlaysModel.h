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
 * A Qt list model that list all available viewport layer types.
 */
class OVITO_GUIBASE_EXPORT AvailableOverlaysModel : public QAbstractListModel, public UserInterfaceComponent<UserInterface>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysModel(QObject* parent, UserInterface& ui, OverlayListModel* overlayListModel);

    /// Returns the number of rows in the model.
    virtual int rowCount(const QModelIndex& parent) const override;

    /// Returns the data associated with a list item.
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// Returns the flags for an item.
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Returns the action that belongs to the given model index.
    OverlayAction* actionFromIndex(int index) const { return (index >= 0 && index < _modelActions.size()) ? _modelActions[index] : nullptr; }

    /// Returns the action that belongs to the given model index.
    OverlayAction* actionFromIndex(const QModelIndex& index) const { return actionFromIndex(index.row()); }

    /// Returns the category index for the viewport layer templates.
    int templatesCategory() const { return (int)_actionsPerCategory.size() - 1; }

    /// Returns the list index where the "Get more layers..." item is located.
    int getMoreExtensionsItemIndex() const { return _getMoreExtensionsItemIndex; }

private Q_SLOTS:

    /// Updates the color brushes of the model.
    void updateColorPalette(const QPalette& palette);

    /// Rebuilds the list of actions for the viewport layer templates.
    void refreshTemplates();

    /// Signal handler that inserts the selected viewport layer into the active viewport.
    void insertViewportLayer();

    /// This handler is called whenever a new extension class has been registered at runtime.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// Rebuilds the internal list of model items.
    void updateModelLists();

    /// The list of viewport layer actions, sorted alphabetically.
    std::vector<OverlayAction*> _actions;

    /// The list of viewport layer actions, sorted by category.
    std::vector<std::vector<OverlayAction*>> _actionsPerCategory;

    /// The list of viewport layer categories.
    std::vector<QString> _categoryNames;

    /// The viewport layer actions as shown by the model.
    std::vector<OverlayAction*> _modelActions;

    /// The display strings as shown by the model.
    std::vector<QString> _modelStrings;

    /// The model representing the viewport layers of the active viewport.
    OverlayListModel* _overlayListModel;

    /// The list of directories searched for user-defined viewport layer scripts.
    QVector<QDir> _layerScriptDirectories;

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
