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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/StdObj.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectorPanel.h>
#include "LinesInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesInspectionApplet);
OVITO_CLASSINFO(LinesInspectionApplet, "DisplayName", "Lines");

/******************************************************************************
 * Lets the applet create the UI widget that is to be placed into the data
 * inspector panel.
 ******************************************************************************/
QWidget* LinesInspectionApplet::createWidget()
{
    createBaseWidgets();

    QSplitter* splitter = new QSplitter();
    // Side panel to select between different line objects
    splitter->addWidget(objectSelectionWidget());

    QWidget* panel = new QWidget();
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Filter Expression Box
    layout->addWidget(filterExpressionEdit(), 0, 1);
    // Data table view
    layout->addWidget(tableView(), 1, 0, 1, 2);

    layout->setRowStretch(1, 1);
    splitter->addWidget(panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    return splitter;
}

}  // namespace Ovito
