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

namespace Ovito {

/**
 * \brief A specialized QTableView widget that supports copying the selected contents of the table to the clipboard.
 */
class OVITO_GUI_EXPORT CopyableTableView : public QTableView
{
public:

    /// Constructor.
    CopyableTableView(QWidget* parent = nullptr) : QTableView(parent) {
        setWordWrap(false);
    }

protected:

    /// Handles key press events for this widget.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

/**
 * \brief A specialized QTableWidget widget that supports copying the selected contents of the table to the clipboard.
 */
class OVITO_GUI_EXPORT CopyableTableWidget : public QTableWidget
{
public:

    /// Constructor.
    CopyableTableWidget(QWidget* parent = nullptr) : QTableWidget(parent) {
        setWordWrap(false);
    }

    /// Constructor.
    CopyableTableWidget(int rows, int columns, QWidget* parent = nullptr) : QTableWidget(rows, columns, parent) {
        setWordWrap(false);
    }

protected:

    /// Handles key press events for this widget.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

}   // End of namespace
