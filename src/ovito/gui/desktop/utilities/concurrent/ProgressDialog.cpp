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
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include "ProgressDialog.h"

namespace Ovito {

/******************************************************************************
* Initializes the dialog window.
******************************************************************************/
ProgressDialog::ProgressDialog(QWidget* parent, TaskPtr task, const QString& dialogTitle) : QDialog(parent), _task(std::move(task))
{
    OVITO_ASSERT(_task);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(dialogTitle);

    QVBoxLayout* layout = new QVBoxLayout(this);

#if 0
#ifdef Q_OS_MACOS
    // On macOS, the progress dialog has no title bar (it's a Qt::Sheet).
    // Insert our own header text label into the dialog.
    if(parent && !dialogTitle.isEmpty()) {
        QLabel* titleLabel = new QLabel(dialogTitle);
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

    // Helper function that sets up the UI widgets in the dialog for a newly started task.
    auto createUIForTask = [this, layout](const TaskPtr& task) {
        // Don't display the task if it is already finished.
        if(task->isFinished())
            return;
        // Create a TaskWatcher object that tracks the progress of the task.
        TaskWatcher* taskWatcher = new TaskWatcher(this);
        // Self-destruction after task has finished.
        connect(taskWatcher, &TaskWatcher::finished, taskWatcher, &QObject::deleteLater);

        QLabel* statusLabel = new QLabel();
        statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QProgressBar* progressBar = new QProgressBar();
        layout->insertWidget(layout->count() - 2, statusLabel);
        layout->insertWidget(layout->count() - 2, progressBar);
        connect(taskWatcher, &TaskWatcher::progressChanged, progressBar, [progressBar](qlonglong progress, qlonglong maximum) {
            progressBar->setMaximum(maximum);
            progressBar->setValue(progress);
        });
        connect(taskWatcher, &TaskWatcher::progressTextChanged, statusLabel, &QLabel::setText);
        connect(taskWatcher, &TaskWatcher::progressTextChanged, statusLabel, [statusLabel, progressBar](const QString& text) {
            statusLabel->setVisible(!text.isEmpty());
            progressBar->setVisible(!text.isEmpty());
        });

        // Remove progress display when this task finished.
        connect(taskWatcher, &TaskWatcher::finished, progressBar, &QObject::deleteLater);
        connect(taskWatcher, &TaskWatcher::finished, statusLabel, &QObject::deleteLater);

        // Begin watching the task.
        taskWatcher->watch(task);

        // Initial update of the progress display.
        statusLabel->setText(taskWatcher->progressText());
        progressBar->setMaximum(taskWatcher->progressMaximum());
        progressBar->setValue(taskWatcher->progressValue());
        if(statusLabel->text().isEmpty()) {
            statusLabel->hide();
            progressBar->hide();
        }
    };

    // Create UI for every already running task.
    TaskManager& taskManager = ExecutionContext::current().ui().taskManager();
    taskManager.visitRegisteredTasks([&](const TaskPtr& task) {
        createUIForTask(task);
    });

    // Expand dialog window to minimum width.
    QRect g = geometry();
    if(g.width() < 450) {
        g.setWidth(450);
        setGeometry(g);
    }

    // Center dialog in parent window.
    if(parent) {
        QSize s = frameGeometry().size();
        QPoint position = parent->geometry().center() - QPoint(s.width() / 2, s.height() / 2);
        // Make sure the window's title bar doesn't move outside the screen area:
        if(position.x() < 0) position.setX(0);
        if(position.y() < 0) position.setY(0);
        move(position);
    }

    // Create a separate progress bar for every new active task.
    connect(&taskManager, &TaskManager::taskRegistered, this, std::move(createUIForTask), Qt::QueuedConnection);

    // Show the dialog with a short delay.
    // This prevents the dialog from showing up for short tasks that terminate very quickly.
    QTimer::singleShot(200, this, &QDialog::open);
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
