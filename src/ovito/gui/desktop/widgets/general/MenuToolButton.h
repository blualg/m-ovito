
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
 * A drop-down menu for selecting an action.
 */
class OVITO_GUI_EXPORT MenuToolButton : public QToolButton
{
    Q_OBJECT

public:
    /// \brief Constructs the ToolButtonMenu.
    /// \param parent The parent widget for the ToolButtonMenu.
    MenuToolButton(QWidget* parent = nullptr);

    /// Creates a new action and adds it to the ToolButtonMenu.
    /// \param icon Icon for the action.
    /// \param label Human readable label for the action.
    /// \return The pointer to the QAction that was added to the menu.
    QAction* createAction(const QIcon& icon, const QString& label);

    /// \brief Creates a new separator in the menu.
    void createMenuSeparator();

private:
    QPointer<QMenu> _menu = nullptr;
};

}  // namespace Ovito