////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/gui/base/actions/ActionManager.h>
#include "CommandPanel.h"
#include "RenderCommandPage.h"
#include "ModifyCommandPage.h"
#include "OverlayCommandPage.h"

namespace Ovito {

/******************************************************************************
* The constructor of the command panel class.
******************************************************************************/
CommandPanel::CommandPanel(MainWindow& mainWindow, QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    // Create tab widget
    _tabWidget = new QTabWidget(this);
    layout->addWidget(_tabWidget, 1);

    // Create the tabs.
    _tabWidget->setDocumentMode(true);
    _tabWidget->addTab(_modifyPage = new ModifyCommandPage(mainWindow, _tabWidget), QIcon::fromTheme("command_panel_tab_modify"), QString());
    _tabWidget->addTab(_renderPage = new RenderCommandPage(mainWindow, _tabWidget), QIcon::fromTheme("command_panel_tab_render"), QString());
    _tabWidget->addTab(_overlayPage = new OverlayCommandPage(mainWindow, _tabWidget), QIcon::fromTheme("command_panel_tab_overlays"), QString());
    _tabWidget->setTabToolTip(0, tr("Pipelines"));
    _tabWidget->setTabToolTip(1, tr("Rendering"));
    _tabWidget->setTabToolTip(2, tr("Viewport layers"));
    setCurrentPage(MainWindow::MODIFY_PAGE);

    QAction* showModifyPageAction = mainWindow.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_MODIFY, tr("Pipeline editor"), {}, tr("Switches to the pipeline editing tab."));
    connect(showModifyPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::MODIFY_PAGE); });

    QAction* showRenderPageAction = mainWindow.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_RENDER, tr("Render settings"), {}, tr("Switches to the image & animation rendering tab."));
    connect(showRenderPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::RENDER_PAGE); });

    QAction* showOverlayPageAction = mainWindow.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_OVERLAYS, tr("Viewport layers"), {}, tr("Switches to the viewport layers tab."));
    connect(showOverlayPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::OVERLAY_PAGE); });
}

/******************************************************************************
* Loads the layout of the widgets from the settings store.
******************************************************************************/
void CommandPanel::restoreLayout()
{
    _modifyPage->restoreLayout();
    _renderPage->restoreLayout();
    _overlayPage->restoreLayout();
}

/******************************************************************************
* Saves the layout of the widgets to the settings store.
******************************************************************************/
void CommandPanel::saveLayout()
{
    _modifyPage->saveLayout();
    _renderPage->saveLayout();
    _overlayPage->saveLayout();
}


/******************************************************************************
* This Qt item delegate class renders the list items of the pipeline editor and other list views.
* It extends the QStyledItemDelegate base class by displaying the
* PipelineStatus::shortInfo() value next to the title of each pipeline entry.
******************************************************************************/
void ExtendedListItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Render the item exactly like QStyledItemDelegate::paint().
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);

    if(!(opt.state & QStyle::State_Editing)) {

        // Obtain the value of the PipelineStatus::shortInfo() field from the PipelineListModel.
        if(QVariant info = index.data(_shortInfoRole); info.isValid()) {
            painter->save();
            painter->setClipRegion(opt.rect);

            if(info.typeId() == QMetaType::QColor) {
                // Display a QColor as a small filled rectangle.
                QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
                const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;
                const int titleWidth = opt.fontMetrics.horizontalAdvance(opt.text + QStringLiteral("   "));
                QRect rect = textRect.adjusted(textMargin + titleWidth, 6, -textMargin, -6); // remove width padding
                rect.setWidth(rect.height());
                painter->fillRect(rect, info.value<QColor>());
            }
            else if(info.canConvert<QString>()) {
                // Render textual information as a text label with dimmed coloring.
                opt.font = option.widget->font();
                painter->setFont(opt.font);

                // The following is adopted from QCommonStyle::drawControl().
                QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
                if(cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
                    cg = QPalette::Inactive;
                QPalette::ColorRole textRole = (opt.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;
                QPalette::ColorRole backgroundRole = (opt.state & QStyle::State_Selected) ? QPalette::Highlight : QPalette::Window;
                painter->setPen(blendColors(
                    opt.palette.color(cg, textRole),
                    opt.palette.color(cg, backgroundRole),
                    0.75));

                QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, option.widget);
                const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, option.widget) + 1;
                const int titleWidth = opt.fontMetrics.horizontalAdvance(opt.text + QStringLiteral("   "));
                textRect.adjust(textMargin + titleWidth, 0, -textMargin, 0); // remove width padding

                QString text = opt.fontMetrics.elidedText(info.toString(), Qt::ElideRight, textRect.width());
                painter->drawText(textRect, opt.displayAlignment, text);
            }

            painter->restore();
        }
    }
}

}   // End of namespace
