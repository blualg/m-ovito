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

#include <ovito/gui/desktop/GUI.h>
#include "ActionsItemDelegate.h"

namespace Ovito {

/******************************************************************************
* Constructor
******************************************************************************/
ActionsItemDelegate::ActionsItemDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
    if(QAbstractItemView* view = qobject_cast<QAbstractItemView*>(parent)) {
        _view = view;
    }
    else if(QComboBox* cb = qobject_cast<QComboBox*>(parent)) {
        _view = cb->view();
    }
    else {
        qWarning("ActionsItemDelegate: Parent object is not a QAbstractItemView or QComboBox.");
        OVITO_ASSERT(false);
        return;
    }

    _view->viewport()->setMouseTracking(true);
    _view->viewport()->installEventFilter(this);
}

/******************************************************************************
* Paints an item.
******************************************************************************/
void ActionsItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // First, paint the standard item appearance.
    QStyledItemDelegate::paint(painter, option, index);

    // Paint the action buttons on top of the item.
    if(index == _hoverIndex) {
        QRect itemRect = view()->visualRect(index);
        int actionIndex = 0;
        for(QAction* action : actionsForIndex(index)) {
            if(!action || action->isVisible() == false || action->isSeparator())
                continue;
            QIcon icon = action->icon();
            if(icon.isNull())
                continue;
            icon.paint(painter,
                    actionButtonRect(itemRect, actionIndex),
                    Qt::AlignTrailing | Qt::AlignVCenter,
                    (_hoverActionIndex == actionIndex) ? QIcon::Active : QIcon::Disabled);
            ++actionIndex;
        }
    }
}

/******************************************************************************
* Returns the rectangular area that is occupied by the i-th action button.
******************************************************************************/
QRect ActionsItemDelegate::actionButtonRect(const QRect& itemRect, int actionIndex) const
{
    QRect rect = itemRect;
    rect.setRight(std::max(rect.right() - actionIndex * rect.height(), rect.left()));
    rect.setLeft(std::max(rect.right() - (actionIndex + 1) * rect.height(), rect.left()));
    return rect;
}

/******************************************************************************
* Handles mouse events for a list item.
******************************************************************************/
bool ActionsItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
        if(index.isValid()) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            QRect itemRect = option.rect;
            int hoverActionIndex = -1;
            int actionIndex = 0;
            for(QAction* action : actionsForIndex(index)) {
                if(!action || action->isVisible() == false || action->isSeparator())
                    continue;
                QIcon icon = action->icon();
                if(icon.isNull())
                    continue;
                QRect actionRect = actionButtonRect(itemRect, actionIndex);
                if(actionRect.contains(mouseEvent->pos())) {
                    hoverActionIndex = actionIndex;
                    QToolTip::showText(view()->viewport()->mapToGlobal(actionRect.bottomRight()), action->text(), view()->viewport(), actionRect);
                    break;
                }
                actionIndex++;
            }
            if(hoverActionIndex != _hoverActionIndex || index != _hoverIndex) {
                if(_hoverIndex.isValid())
                    view()->update(_hoverIndex);
                _hoverIndex = index;
                _hoverActionIndex = hoverActionIndex;
                view()->update(index);
            }
            if(_hoverActionIndex != -1) {
                return true;
            }
        }
        else {
            if(_hoverActionIndex != -1 || _hoverIndex.isValid()) {
                if(_hoverIndex.isValid())
                    view()->update(_hoverIndex);
                _hoverIndex = QModelIndex();
                _hoverActionIndex = -1;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

/******************************************************************************
* Intercepts events of the item view widget.
******************************************************************************/
bool ActionsItemDelegate::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex index = view()->indexAt(mouseEvent->pos());
        if(index.isValid()) {
            QRect itemRect = view()->visualRect(index);
            int actionIndex = 0;
            for(QAction* action : actionsForIndex(index)) {
                if(!action || action->isVisible() == false || action->isSeparator())
                    continue;
                QIcon icon = action->icon();
                if(icon.isNull())
                    continue;
                QRect actionRect = actionButtonRect(itemRect, actionIndex);
                if(actionRect.contains(mouseEvent->pos())) {
                    mouseEvent->accept();
                    if(QComboBox* cb = qobject_cast<QComboBox*>(parent()))
                        cb->hidePopup();
                    action->trigger();
                    return true;
                }
                actionIndex++;
            }
        }
    }
    else if(event->type() == QEvent::Hide || event->type() == QEvent::Leave) {
        if(_hoverActionIndex != -1 || _hoverIndex.isValid()) {
            if(_hoverIndex.isValid())
                view()->update(_hoverIndex);
            _hoverIndex = QModelIndex();
            _hoverActionIndex = -1;
        }
    }

    return QStyledItemDelegate::eventFilter(obj, event);
}

}   // End of namespace
