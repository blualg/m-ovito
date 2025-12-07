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
#include <ovito/gui/desktop/widgets/general/ElidedTextLabel.h>
#include "TaskDisplayWidget.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget and associates it with the main window.
******************************************************************************/
TaskDisplayWidget::TaskDisplayWidget(MainWindow* mainWindow) : _mainWindow(mainWindow)
{
    QHBoxLayout* progressWidgetLayout = new QHBoxLayout(this);
    progressWidgetLayout->setContentsMargins(10,0,0,0);
    progressWidgetLayout->setSpacing(0);
    _progressTextDisplay = new ElidedTextLabel(Qt::ElideLeft);
    _progressTextDisplay->setLineWidth(0);
    _progressTextDisplay->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    _progressTextDisplay->setAutoFillBackground(true);
    _progressTextDisplay->setMargin(2);
    _progressTextDisplay->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    progressWidgetLayout->addWidget(_progressTextDisplay);
    _progressBar = new QProgressBar(this);
    _progressBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    progressWidgetLayout->addWidget(_progressBar);
    progressWidgetLayout->addStrut(_progressTextDisplay->sizeHint().height());
    setMinimumHeight(_progressTextDisplay->minimumSizeHint().height());

    connect(mainWindow, &MainWindow::taskProgressUpdate, this, &TaskDisplayWidget::updateIndicator);
    connect(this, &QObject::destroyed, _progressTextDisplay, &QObject::deleteLater);

    updateIndicator();
}

/******************************************************************************
* Shows or hides the progress indicator widgets and updates the displayed information.
******************************************************************************/
void TaskDisplayWidget::updateIndicator()
{
    QString activeText;
    int activeValue;
    int activeMaximum;

    // Visit all in-progress tasks and pick the one that should be displayed in the status bar.
    _mainWindow->ui().visitRunningTasks([&](const QString& text, int progressValue, int progressMaximum) {
        if(!text.isEmpty() && activeText.isEmpty()) {
            activeText = text;
            activeValue = progressValue;
            activeMaximum = progressMaximum;
        }
    });

    // Update display.
    _progressTextDisplay->setText(activeText);
    if(!activeText.isEmpty()) {
        _progressBar->setRange(0, activeMaximum);
        _progressBar->setValue(activeValue);
        show();
    }
    else {
        hide();
    }
}

}   // End of namespace
