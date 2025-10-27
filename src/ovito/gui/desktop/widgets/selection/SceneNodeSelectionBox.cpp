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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ActionsItemDelegate.h>
#include "SceneNodeSelectionBox.h"
#include "SceneNodesListModel.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget.
******************************************************************************/
SceneNodeSelectionBox::SceneNodeSelectionBox(MainWindowUI& ui, QWidget* parent) : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui)
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
    setModel(new SceneNodesListModel(ui, this));

    // Wire the combobox selection to the list model.
    connect(this, qOverload<int>(&QComboBox::activated), static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::activateItem);
    connect(static_cast<SceneNodesListModel*>(model()), &SceneNodesListModel::selectionChangeRequested, this, &QComboBox::setCurrentIndex);

    // Configure the view.
    view()->setTextElideMode(Qt::ElideRight);

    // Install custom item delegate to add action buttons to the combo box.
    ActionsItemDelegate* delegate = new ActionsItemDelegate(this, SceneNodesListModel::InfoRole, SceneNodesListModel::ActionsRole);
    setItemDelegate(delegate);
}

}   // End of namespace
