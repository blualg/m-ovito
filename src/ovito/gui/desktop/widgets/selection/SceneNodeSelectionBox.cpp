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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include "SceneNodeSelectionBox.h"
#include "SceneNodesListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget.
******************************************************************************/
SceneNodeSelectionBox::SceneNodeSelectionBox(MainWindow& mainWindow, QWidget* parent) : QComboBox(parent), _mainWindow(mainWindow)
{
    setInsertPolicy(QComboBox::NoInsert);
    setEditable(false);
#ifndef Q_OS_MACOS
    setMinimumContentsLength(40);
#else
    setMinimumContentsLength(32);
#endif
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setToolTip(tr("Pipeline selector"));
    setIconSize(QSize(24, 24));

    // Set the list model, which tracks the list of pipelines in the scene.
    setModel(new SceneNodesListModel(mainWindow, this));

    // Wire the combobox selection to the list model.
    connect(this, qOverload<int>(&QComboBox::activated), static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::activateItem);
    connect(static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::selectionChangeRequested, this, &QComboBox::setCurrentIndex);

    // Install a custom item delegate.
    setItemDelegate(new SceneNodeSelectionItemDelegate(this));
    connect(static_cast<SceneNodeSelectionItemDelegate*>(itemDelegate()), &SceneNodeSelectionItemDelegate::itemDelete, static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::deleteItem);
    connect(static_cast<SceneNodeSelectionItemDelegate*>(itemDelegate()), &SceneNodeSelectionItemDelegate::itemRename, this, &SceneNodeSelectionBox::renameSceneNode);

    // Install an event filter.
    view()->viewport()->installEventFilter(itemDelegate());
    view()->setTextElideMode(Qt::ElideRight);
}

/******************************************************************************
* Lets the user rename a list item.
******************************************************************************/
void SceneNodeSelectionBox::renameSceneNode(int index)
{
    if(OORef<SceneNode> sceneNode = static_cast<SceneNodesListModel*>(model())->sceneNodeFromListIndex(index)) {
        QString oldName = sceneNode->objectTitle();
        bool ok;
        QString newName = QInputDialog::getText(window(), tr("Change pipeline name"), tr("Pipeline name:                                         "), QLineEdit::Normal, oldName, &ok).trimmed();
        if(ok && newName != oldName) {
            _mainWindow.performTransaction(tr("Rename pipeline"), [&]() {
                sceneNode->setNodeName(newName);
            });
        }
    }
}

/******************************************************************************
* Paints an item in the combobox.
******************************************************************************/
void SceneNodeSelectionItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Paint buttons next to node items when mouse if over the item.
    if(SceneNode* node = qobject_cast<SceneNode*>(index.data(Qt::UserRole).value<QObject*>())) {
#ifdef Q_OS_WIN
        if(option.state & QStyle::State_MouseOver) {
#else
        if(option.state & QStyle::State_Selected) {
#endif
            // Shorten the text of the item to not overlap with the buttons.
            QStyleOptionViewItem reducedOption = option;
            initStyleOption(&reducedOption, index);
            QStyle* style = option.widget->style();
            QRect textRect = style->proxy()->subElementRect(QStyle::SE_ItemViewItemText, &reducedOption, reducedOption.widget);
            int textWidth = textRect.width() - 2 * option.rect.height();
            reducedOption.text = option.fontMetrics.elidedText(reducedOption.text, Qt::ElideRight, textWidth);
            reducedOption.textElideMode = Qt::ElideNone;
            option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &reducedOption, painter, option.widget);

            // Load the icons.
            if(_deleteIcon.isNull()) {
                _deleteIcon = QIcon::fromTheme("edit_delete_pipeline");
#ifndef Q_OS_WIN
//              _deleteIcon.addFile(":/guibase/actions/edit/delete_pipeline.white.svg", QSize(), QIcon::Disabled);
#endif
                _deleteIcon.setIsMask(true);
            }
            if(_renameIcon.isNull()) {
                _renameIcon = QIcon::fromTheme("edit_rename_pipeline");
#ifndef Q_OS_WIN
//              _renameIcon.addFile(":/guibase/actions/edit/rename_pipeline.bw.white.svg", QSize(), QIcon::Disabled);
#endif
                _renameIcon.setIsMask(true);
            }

            // Paint the icons.
            _deleteIcon.paint(painter, deleteButtonRect(option.rect), Qt::AlignTrailing | Qt::AlignVCenter, _deleteButtonHover ? QIcon::Active : QIcon::Disabled);
            _renameIcon.paint(painter, renameButtonRect(option.rect), Qt::AlignTrailing | Qt::AlignVCenter, _renameButtonHover ? QIcon::Active : QIcon::Disabled);

            return;
        }
    }
    QStyledItemDelegate::paint(painter, option, index);
}

/******************************************************************************
* Returns the rectangular area that is occupied by the delete button of a list item.
******************************************************************************/
QRect SceneNodeSelectionItemDelegate::deleteButtonRect(const QRect& itemRect) const
{
    QRect rect = itemRect;
    rect.setLeft(std::max(rect.right() - rect.height(), rect.left()));
    return rect;
}

/******************************************************************************
* Returns the rectangular area that is occupied by the rename button of a list item.
******************************************************************************/
QRect SceneNodeSelectionItemDelegate::renameButtonRect(const QRect& itemRect) const
{
    QRect rect = itemRect;
    rect.setRight(std::max(rect.right() - rect.height(), rect.left()));
    rect.setLeft(std::max(rect.right() - rect.height(), rect.left()));
    return rect;
}

/******************************************************************************
* Handles mouse events for a list item.
******************************************************************************/
bool SceneNodeSelectionItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
        if(SceneNode* node = qobject_cast<SceneNode*>(index.data(Qt::UserRole).value<QObject*>())) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            QAbstractItemView* view = static_cast<QComboBox*>(parent())->view();
            QRect itemRect = option.rect;
            int maxWidth = view->viewport()->size().width();
            itemRect.setWidth(qMin(maxWidth, itemRect.width()));
            QRect deleteRect = deleteButtonRect(itemRect);
            QRect renameRect = renameButtonRect(itemRect);
            _deleteButtonHover = deleteRect.contains(mouseEvent->pos());
            _renameButtonHover = renameRect.contains(mouseEvent->pos());
            if(_deleteButtonHover)
                QToolTip::showText(view->viewport()->mapToGlobal(deleteRect.bottomRight()), tr("Delete"), view->viewport(), deleteRect);
            if(_renameButtonHover)
                QToolTip::showText(view->viewport()->mapToGlobal(renameRect.bottomRight()), tr("Rename"), view->viewport(), renameRect);
            return true;
        }
        else {
            if(_deleteButtonHover || _renameButtonHover) {
                _deleteButtonHover = false;
                _renameButtonHover = false;
                QAbstractItemView* view = static_cast<QComboBox*>(parent())->view();
                view->viewport()->update();
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

/******************************************************************************
* Intercepts events of the combox view widget.
******************************************************************************/
bool SceneNodeSelectionItemDelegate::eventFilter(QObject* obj, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        QAbstractItemView* view = static_cast<QComboBox*>(parent())->view();
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex indexUnderMouse = view->indexAt(mouseEvent->pos());
        if(SceneNode* node = qobject_cast<SceneNode*>(indexUnderMouse.data(Qt::UserRole).value<QObject*>())) {
            QRect itemRect = view->visualRect(indexUnderMouse);
            int maxWidth = view->viewport()->size().width();
            itemRect.setWidth(qMin(maxWidth, itemRect.width()));
            if(deleteButtonRect(itemRect).contains(mouseEvent->pos())) {
                static_cast<QComboBox*>(parent())->hidePopup();
                Q_EMIT itemDelete(indexUnderMouse.row());
                return true;
            }
            if(renameButtonRect(itemRect).contains(mouseEvent->pos())) {
                static_cast<QComboBox*>(parent())->hidePopup();
                Q_EMIT itemRename(indexUnderMouse.row());
                return true;
            }
        }
    }
    else if(event->type() == QEvent::Hide || event->type() == QEvent::Leave) {
        if(_deleteButtonHover || _renameButtonHover) {
            _deleteButtonHover = false;
            _renameButtonHover = false;
            QAbstractItemView* view = static_cast<QComboBox*>(parent())->view();
            view->viewport()->update();
        }
    }

    return QStyledItemDelegate::eventFilter(obj, event);
}

}   // End of namespace
