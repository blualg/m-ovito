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
#include "InfoItemDelegate.h"

namespace Ovito {

/**
 * A QAction subclass representing an action associated with an item in a QAbstractListModel.
 */
class OVITO_GUI_EXPORT ItemAction : public QAction
{
    Q_OBJECT

public:

    /// Constructor.
    using QAction::QAction;

Q_SIGNALS:

    /// This signal is emitted when the action is triggered for the given list model item.
    void triggeredForItem(const QModelIndex& index);
};

/**
 * A Qt item delegate that can display one or more action buttons (e.g. delete, rename) next to each item in a list or table.
 *
 * To use this delegate with a QAbstractItemView widget, subclass QAbstractItemModel and reimplement the data() method to return a QList<QAction*>
 * for the custom `actionsRole` specified in the ActionsItemDelegate constructor. The delegate will then render the action buttons next to each item
 * and handle mouse events to trigger the corresponding actions.
 *
 * Use the ItemAction class for actions that need to know the specific item they were triggered for.
 */
class OVITO_GUI_EXPORT ActionsItemDelegate : public InfoItemDelegate
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ActionsItemDelegate(QObject* parent, int infoRole, int actionsRole);

    /// Determines whether the action buttons should be displayed when hovering over any part of the item row (true)
    /// or only when hovering over the item contents (false, default).
    void setSpanEntireRow(bool enable) { _spanEntireRow = enable; }

    /// Returns whether the action buttons span the entire row.
    bool spanEntireRow() const { return _spanEntireRow; }

    /// Paints an item.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Handles mouse events for an item.
    virtual bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    /// Returns the item view associated with this delegate.
    QAbstractItemView* view() const { return _view; }

    /// Returns the Qt data role used to obtain the item actions from the model.
    int actionsRole() const { return _actionsRole; }

    /// Returns the list of actions for the given model index.
    QList<QAction*> actionsForIndex(const QModelIndex& index) const {
        return index.data(actionsRole()).value<QList<QAction*>>();
    }

Q_SIGNALS:

    /// Is emited when the user requests the deletion of a list item.
    void itemDelete(int index);

    /// Is emited when the user requests the renaming of a list item.
    void itemRename(int index);

protected:

    /// Intercepts events of the item view widget.
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

    /// Computes the visual rect of the given item, clipped to the viewport area.
    QRect getClippedItemRect(const QModelIndex& index) const;

    /// Generates a pixmap for the given icon, considering the style and state of the item.
    QPixmap getIconPixmap(const QIcon& icon, const QStyleOptionViewItem& option, bool isActive) const;

private:

    /// Returns the rectangular area that is occupied by the i-th action button.
    QRect actionButtonRect(const QRect& itemRect, int actionIndex) const;

    int _actionsRole;
    QAbstractItemView* _view;
    QModelIndex _hoverIndex;
    QModelIndex _mousePressIndex;
    QRect _mousePressActionRect;
    QAction* _mousePressAction = nullptr;
    int _hoverActionIndex = -1;
    bool _spanEntireRow = false;
    int _maxIconSize = 22; // Maximum size of action button icons.
    mutable std::map<std::pair<qint64, QRgb>, QPixmapCache::Key> _iconPixmapCache;
};

}   // End of namespace
