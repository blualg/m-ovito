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
ActionsItemDelegate::ActionsItemDelegate(QObject* parent, int infoRole, int actionsRole) : InfoItemDelegate(parent, infoRole), _actionsRole(actionsRole)
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
    InfoItemDelegate::paint(painter, option, index);

    // Paint the action buttons on top of the item.
    if(_hoverIndex.isValid() && (spanEntireRow() ? index.row() == _hoverIndex.row() : index == _hoverIndex)) {
        QRect itemRect = option.rect;
        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        QStyleOptionViewItem actionOpt = option;
        int actionIndex = 0;
        for(QAction* action : actionsForIndex(index)) {
            if(!action || action->isVisible() == false || action->isSeparator())
                continue;
            QIcon icon = action->icon();
            if(icon.isNull())
                continue;

            // Draw the background
            actionOpt.rect = actionButtonRect(itemRect, actionIndex);
            if(actionOpt.backgroundBrush.style() == Qt::NoBrush) {
                // If the item has no background, use a semi-transparent version of the widget's base color.
                QColor bgColor = option.palette.color(QPalette::Base);
                bgColor.setAlphaF(0.5f);
                actionOpt.backgroundBrush = QBrush(bgColor);
            }
            style->proxy()->drawPrimitive(QStyle::PE_PanelItemViewItem, &actionOpt, painter, widget);

            // Paint icon.
            QPixmap iconPixmap = getIconPixmap(icon, actionOpt, actionIndex == _hoverActionIndex);
            style->proxy()->drawItemPixmap(painter, actionOpt.rect, Qt::AlignCenter, iconPixmap);

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
    int width = rect.height() - 2; // Make action buttons (roughly) square.
    rect.setRight(std::max(rect.right() - actionIndex * width, rect.left()));
    rect.setLeft(std::max(rect.right() - width, rect.left()));

    return rect;
}

/******************************************************************************
* Computes the visual rect of the given item, clipped to the viewport area.
******************************************************************************/
QRect ActionsItemDelegate::getClippedItemRect(const QModelIndex& index) const
{
    const QRect viewportRect = view()->viewport()->rect();
    QRect rect = view()->visualRect(index.model()->buddy(index));
    QRect clipped = rect & viewportRect;
    rect.setLeft(clipped.left());
    rect.setRight(clipped.right());
    return rect;
}

/******************************************************************************
* Generates a pixmap for the given icon, considering the style and state of the item.
******************************************************************************/
QPixmap ActionsItemDelegate::getIconPixmap(const QIcon& icon, const QStyleOptionViewItem& option, bool isActive) const
{
    // Pick the appropriate color for the icon depending on the state.
    QColor color = option.palette.color(QPalette::Normal, isActive ? QPalette::Highlight : QPalette::Text);
    color.setAlphaF(0.3f);

    // Check for existing pixmap cache key.
    auto cacheKey = std::make_pair(icon.cacheKey(), color.rgba());
    auto cacheIt = _iconPixmapCache.find(cacheKey);
    if(cacheIt != _iconPixmapCache.end()) {
        // Check global cache for existing pixmap.
        QPixmap cachedPixmap;
        if(QPixmapCache::find(cacheIt->second, &cachedPixmap))
            return cachedPixmap;
    }

    // Generate initial icon pixmap.
    int iconSize = std::min(_maxIconSize, option.rect.height());
    QPixmap pixmap = icon.pixmap(iconSize, QIcon::Normal);

    // Recolor the icon pixmap.
    QImage img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&img);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(0, 0, img.width(), img.height(), color);
    painter.end();
    pixmap = QPixmap::fromImage(std::move(img));

    // Store pixmap in cache.
    _iconPixmapCache.emplace(cacheKey, QPixmapCache::insert(pixmap));

    return pixmap;
}

/******************************************************************************
* Handles mouse events for a list item.
******************************************************************************/
bool ActionsItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if(event->type() == QEvent::MouseMove && !_mousePressIndex.isValid()) {
        if(index.isValid()) {
            QPoint mousePos = static_cast<QMouseEvent*>(event)->pos();
            QRect itemRect = getClippedItemRect(index);
            int hoverActionIndex = -1;
            QModelIndex hoverIndex;
            int actionIndex = 0;
            for(QAction* action : actionsForIndex(index)) {
                if(!action || action->isVisible() == false || action->isSeparator())
                    continue;
                QIcon icon = action->icon();
                if(icon.isNull())
                    continue;
                QRect actionRect = actionButtonRect(itemRect, actionIndex);
                hoverIndex = index;
                if(actionRect.contains(mousePos)) {
                    hoverActionIndex = actionIndex;
                    QToolTip::showText(view()->viewport()->mapToGlobal(actionRect.bottomLeft()), action->text(), view()->viewport(), actionRect);
                    break;
                }
                actionIndex++;
            }
            if(hoverActionIndex != _hoverActionIndex || hoverIndex != _hoverIndex) {
                if(_hoverIndex.isValid())
                    view()->update(_hoverIndex);
                _hoverIndex = hoverIndex;
                _hoverActionIndex = hoverActionIndex;
                view()->update(hoverIndex);
            }
            if(_hoverActionIndex != -1)
                return true;
        }
        else if(_hoverActionIndex != -1 || _hoverIndex.isValid()) {
            if(_hoverIndex.isValid())
                view()->update(_hoverIndex);
            _hoverIndex = QModelIndex();
            _hoverActionIndex = -1;
        }
    }
    else if(event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton && index.isValid() && !_mousePressIndex.isValid()) {
        QPoint mousePos = static_cast<QMouseEvent*>(event)->pos();
        QRect itemRect = getClippedItemRect(index);
        int clickedActionIndex = -1;
        QModelIndex clickedIndex;
        int actionIndex = 0;
        for(QAction* action : actionsForIndex(index)) {
            if(!action || action->isVisible() == false || action->isSeparator())
                continue;
            QIcon icon = action->icon();
            if(icon.isNull())
                continue;
            QRect actionRect = actionButtonRect(itemRect, actionIndex);
            if(actionRect.contains(mousePos)) {
                clickedActionIndex = actionIndex;
                _mousePressIndex = index;
                _mousePressActionRect = actionRect;
                _mousePressAction = action;
                break;
            }
            actionIndex++;
        }
        if(clickedActionIndex != -1) {
            if(_hoverIndex.isValid() && _hoverIndex != index)
                view()->update(_hoverIndex);
            _hoverIndex = index;
            _hoverActionIndex = clickedActionIndex;
            view()->update(index);
            return true;
        }
    }
    return InfoItemDelegate::editorEvent(event, model, option, index);
}

/******************************************************************************
* Intercepts events of the item view widget.
******************************************************************************/
bool ActionsItemDelegate::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonRelease && _mousePressIndex.isValid()) {
        OVITO_ASSERT(_mousePressAction);
        if(_mousePressActionRect.contains(static_cast<QMouseEvent*>(event)->pos())) {
            QModelIndex index = std::exchange(_mousePressIndex, QModelIndex());
            QAction* action = std::exchange(_mousePressAction, nullptr);
            event->accept();
            view()->setCurrentIndex(index);
            if(QComboBox* cb = qobject_cast<QComboBox*>(parent()))
                cb->hidePopup();
            action->trigger();
            if(ItemAction* itemAction = qobject_cast<ItemAction*>(action))
                Q_EMIT itemAction->triggeredForItem(index);
        }
        else {
            if(_hoverIndex.isValid())
                view()->update(_hoverIndex);
            _hoverIndex = QModelIndex();
            _hoverActionIndex = -1;
            _mousePressIndex = QModelIndex();
            _mousePressAction = nullptr;
        }
        return true;
    }
    else if(event->type() == QEvent::MouseMove) {
        if(_mousePressIndex.isValid())
            return true;
        QModelIndex index = view()->indexAt(static_cast<QMouseEvent*>(event)->pos());
        if(index != _hoverIndex && (!index.isValid() || view()->itemDelegateForIndex(index) != this)) {
            if(_hoverIndex.isValid())
                view()->update(_hoverIndex);
            _hoverIndex = index;
            _hoverActionIndex = -1;
        }
    }
    else if(event->type() == QEvent::Leave) {
        OVITO_ASSERT(!_mousePressIndex.isValid());
        if(_hoverActionIndex != -1 || _hoverIndex.isValid()) {
            if(_hoverIndex.isValid())
                view()->update(_hoverIndex);
            _hoverIndex = QModelIndex();
            _hoverActionIndex = -1;
        }
    }
    else if(event->type() == QEvent::Hide) {
        _hoverIndex = QModelIndex();
        _hoverActionIndex = -1;
        _mousePressIndex = QModelIndex();
        _mousePressAction = nullptr;
    }

    return InfoItemDelegate::eventFilter(obj, event);
}

}   // End of namespace
