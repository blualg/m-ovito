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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>

namespace Ovito {

/**
 * The command panel in the main window.
 */
class OVITO_GUI_EXPORT CommandPanel : public QWidget
{
    Q_OBJECT

public:

    /// Constructor.
    CommandPanel(MainWindow& mainWindow, QWidget* parent);

    /// Activates one of the command pages.
    void setCurrentPage(MainWindow::CommandPanelPage newPage) {
        OVITO_ASSERT(newPage < _tabWidget->count());
        _tabWidget->setCurrentIndex((int)newPage);
    }

    /// Returns the currently active command page.
    MainWindow::CommandPanelPage currentPage() const { return static_cast<MainWindow::CommandPanelPage>(_tabWidget->currentIndex()); }

    /// Returns the modification page contained in the command panel.
    ModifyCommandPage* modifyPage() const { return _modifyPage; }

    /// Returns the rendering page contained in the command panel.
    RenderCommandPage* renderPage() const { return _renderPage; }

    /// Returns the viewport overlay page contained in the command panel.
    OverlayCommandPage* overlayPage() const { return _overlayPage; }

    /// Returns the utilities page contained in the command panel.
    UtilityCommandPage* utilityPage() const { return _utilityPage; }

    /// Returns the default size for the command panel.
    virtual QSize sizeHint() const { return QSize(336, 300); }

    /// Loads the layout of the widgets from the settings store.
    void restoreLayout();

    /// Saves the layout of the widgets to the settings store.
    void saveLayout();

private:

    QTabWidget* _tabWidget;
    ModifyCommandPage* _modifyPage;
    RenderCommandPage* _renderPage;
    OverlayCommandPage* _overlayPage;
    UtilityCommandPage* _utilityPage;
};

/**
 * This Qt item delegate class renders the list items of the pipeline editor and other list views.
 * It extends the QStyledItemDelegate base class by displaying the
 * PipelineStatus::shortInfo() value next to the title of each pipeline entry.
 */
class ExtendedListItemDelegate : public QStyledItemDelegate
{
public:

    /// Constructor.
    ExtendedListItemDelegate(QObject* parent, int shortInfoRole) : QStyledItemDelegate(parent), _shortInfoRole(shortInfoRole) {}

    /// Renders the list item.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:

    int _shortInfoRole;

    /// Blend two RGB colors.
    static QColor blendColors(const QColor& color1, const QColor& color2, qreal ratio)
    {
        int r = color1.red() * (1 - ratio) + color2.red() * ratio;
        int g = color1.green() * (1 - ratio) + color2.green() * ratio;
        int b = color1.blue() * (1 - ratio) + color2.blue() * ratio;
        return QColor(r, g, b);
    }
};

}   // End of namespace
