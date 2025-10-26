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

/**
 * A Qt item delegate that can display one or more action buttons (e.g. delete, rename) next to each item in a list or table.
 */
class OVITO_GUI_EXPORT ActionsItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:

    /// Additional custom roles used by the item delegate to fetch the list of QActions from the model.
    enum ItemRoles {
        ActionsRole = Qt::UserRole + 1,
    };

    /// Constructor.
    explicit ActionsItemDelegate(QObject* parent);

    /// Paints an item.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Handles mouse events for an item.
    virtual bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    /// Returns the item view associated with this delegate.
    QAbstractItemView* view() const { return _view; }

    /// Returns the list of actions for the given model index.
    QVector<QAction*> actionsForIndex(const QModelIndex& index) const {
        return index.data(ActionsRole).value<QVector<QAction*>>();
    }

Q_SIGNALS:

    /// Is emited when the user requests the deletion of a list item.
    void itemDelete(int index);

    /// Is emited when the user requests the renaming of a list item.
    void itemRename(int index);

protected:

    /// Intercepts events of the item view widget.
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

private:

    /// Returns the rectangular area that is occupied by the i-th action button.
    QRect actionButtonRect(const QRect& itemRect, int actionIndex) const;

    QAbstractItemView* _view;
    QModelIndex _hoverIndex;
    mutable QIcon _deleteIcon;
    mutable QIcon _renameIcon;
    int _hoverActionIndex = -1;
};

}   // End of namespace
