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
#include "ProgressDialog.h"

namespace Ovito {

/******************************************************************************
* Initializes the dialog window.
******************************************************************************/
ProgressDialog::ProgressDialog(MainWindow& mainWindow, QWidget* parent, TaskPtr task, const QString& dialogTitle) : QDialog(parent), _mainWindow(mainWindow), _task(std::move(task))
{
    OVITO_ASSERT(_task);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(dialogTitle);

    // Show the dialog with a short delay.
    // This prevents the dialog from showing up for short tasks that terminate very quickly.
    QTimer::singleShot(200, this, &QDialog::open);
}

/******************************************************************************
* Is called when the dialog is shown.
******************************************************************************/
void ProgressDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    QVBoxLayout* layout = new QVBoxLayout(this);

#if 0
#ifdef Q_OS_MACOS
    // On macOS, the progress dialog has no title bar (it's a Qt::Sheet).
    // Insert our own header text label into the dialog.
    if(parentWidget() && !windowTitle().isEmpty()) {
        QLabel* titleLabel = new QLabel(windowTitle());
        QFont boldFont;
        boldFont.setWeight(QFont::Bold);
        titleLabel->setFont(std::move(boldFont));
        layout->addWidget(titleLabel);
        QFrame* headerLine = new QFrame();
        headerLine->setFrameShape(QFrame::HLine);
        layout->addWidget(headerLine);
    }
#endif
#endif

    layout->addStretch(1);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    // Cancel the running task when user presses the cancel button.
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ProgressDialog::reject);

    // Update the list of tasks if their status or number changes.
    connect(&_mainWindow, &MainWindow::taskProgressUpdate, this, &ProgressDialog::updateTaskList);

    // Build the initial list of tasks.
    updateTaskList();

    // Expand dialog window to minimum width.
    QRect g = geometry();
    if(g.width() < 450) {
        g.setWidth(450);
        setGeometry(g);
    }

    // Center dialog in parent window.
    if(parentWidget()) {
        QSize s = frameGeometry().size();
        QPoint position = parentWidget()->geometry().center() - QPoint(s.width() / 2, s.height() / 2);
        // Make sure the window's title bar doesn't move outside the screen area:
        if(position.x() < 0) position.setX(0);
        if(position.y() < 0) position.setY(0);
        move(position);
    }
}

/******************************************************************************
* Updates the displayed list of running tasks in the dialog.
******************************************************************************/
void ProgressDialog::updateTaskList()
{
    size_t index = 0;
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());

    _mainWindow.visitRunningTasks([&](Task& task, const QString& text, int progressValue, int progressMaximum) {
        if(text.isEmpty())
            return;
        QLabel* statusLabel;
        QProgressBar* progressBar;
        if(index == _taskWidgets.size()) {
            statusLabel = new QLabel();
            progressBar = new QProgressBar();
            statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            layout->insertWidget(layout->count() - 2, statusLabel);
            layout->insertWidget(layout->count() - 2, progressBar);
            _taskWidgets.emplace_back(statusLabel, progressBar);
        }
        else {
            std::tie(statusLabel, progressBar) = _taskWidgets[index];
        }
        statusLabel->setText(text);
        progressBar->setMaximum(progressMaximum);
        progressBar->setValue(progressValue);
        index++;
    });

    // Hide any remaining task widgets that are no longer needed.
    while(index < _taskWidgets.size()) {
        auto [statusLabel, progressBar] = _taskWidgets.back();
        delete statusLabel;
        delete progressBar;
        _taskWidgets.pop_back();
    }
}

/******************************************************************************
* Is called when the user tries to close the dialog.
******************************************************************************/
void ProgressDialog::closeEvent(QCloseEvent* event)
{
    // Cancel the root operation associated with this dialog.
    _task->cancel();

    if(event->spontaneous())
        event->ignore();

    QDialog::closeEvent(event);
}

/******************************************************************************
* Is called when the user tries to close the dialog.
******************************************************************************/
void ProgressDialog::reject()
{
    // Cancel the root operation associated with this dialog.
    _task->cancel();
}

}   // End of namespace
