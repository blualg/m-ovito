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


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

class SceneNodeSelectionItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:

    /// Constructor.
    using QStyledItemDelegate::QStyledItemDelegate;

    /// Paints an item in the combobox.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Handles mouse events for a list item.
    virtual bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

Q_SIGNALS:

    /// Is emited when the user requests the deletion of a list item.
    void itemDelete(int index);

    /// Is emited when the user requests the renaming of a list item.
    void itemRename(int index);

protected:

    /// Intercepts events of the combox view widget.
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

private:

    /// Returns the rectangular area that is occupied by the delete button of a list item.
    QRect deleteButtonRect(const QRect& itemRect) const;

    /// Returns the rectangular area that is occupied by the rename button of a list item.
    QRect renameButtonRect(const QRect& itemRect) const;

    mutable QIcon _deleteIcon;
    mutable QIcon _renameIcon;
    bool _deleteButtonHover = false;
    bool _renameButtonHover = false;
};

/**
 * A combo-box widget that displays the current scene node selection
 * and allows to select scene nodes.
 */
class SceneNodeSelectionBox : public QComboBox
{
    Q_OBJECT

public:

    /// Constructs the widget.
    SceneNodeSelectionBox(MainWindow& mainWindow, QWidget* parent = nullptr);

private Q_SLOTS:

    /// Lets the user rename a list item.
    void renameSceneNode(int index);

private:

    MainWindow& _mainWindow;
};

}   // End of namespace
