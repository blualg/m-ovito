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
#include <ovito/gui/desktop/dialogs/SaveImageFileDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "FrameBufferWindow.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBufferWindow::FrameBufferWindow(MainWindow& mainWindow, QWidget* parent) :
    QMainWindow(parent, (Qt::WindowFlags)(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint)),
    _mainWindow(mainWindow)
{
    // Note: The following setAttribute() call has been commented out, because it leads to sporadic program crashes (Qt 5.12.5).
    // setAttribute(Qt::WA_MacAlwaysShowToolWindow);

    QWidget* centralContainer = new QWidget(this);
    _centralLayout = new QStackedLayout(centralContainer);
    _centralLayout->setContentsMargins(0,0,0,0);
    _centralLayout->setStackingMode(QStackedLayout::StackAll);
    _frameBufferWidget = new FrameBufferWidget();
    _centralLayout->addWidget(_frameBufferWidget);
    setCentralWidget(centralContainer);

    QToolBar* toolBar = addToolBar(tr("Frame Buffer"));
    toolBar->setMovable(false);
    _saveToFileAction = toolBar->addAction(QIcon::fromTheme("framebuffer_save_picture"), tr("Save to file"), this, &FrameBufferWindow::saveImage);
    _copyToClipboardAction = toolBar->addAction(QIcon::fromTheme("framebuffer_copy_picture_to_clipboard"), tr("Copy to clipboard"), this, &FrameBufferWindow::copyImageToClipboard);
    toolBar->addSeparator();
    _autoCropAction = toolBar->addAction(QIcon::fromTheme("framebuffer_auto_crop"), tr("Auto-crop image"), this, &FrameBufferWindow::autoCrop);
    toolBar->addSeparator();
    toolBar->addAction(QIcon::fromTheme("framebuffer_zoom_out"), tr("Zoom out"), this, &FrameBufferWindow::zoomOut);
    toolBar->addAction(QIcon::fromTheme("framebuffer_zoom_in"), tr("Zoom in"), this, &FrameBufferWindow::zoomIn);
    toolBar->addSeparator();
    _cancelRenderingAction = toolBar->addAction(QIcon::fromTheme("framebuffer_cancel_rendering"), tr("Cancel"), this, &FrameBufferWindow::cancelRendering);
    _cancelRenderingAction->setEnabled(false);
    static_cast<QToolButton*>(toolBar->widgetForAction(_cancelRenderingAction))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Disable context menu in toolbar.
    setContextMenuPolicy(Qt::NoContextMenu);

    QWidget* progressWidgetContainer = new QWidget();
    progressWidgetContainer->setAttribute(Qt::WA_TransparentForMouseEvents);
    QGridLayout* progressWidgetContainerLayout = new QGridLayout(progressWidgetContainer);
    progressWidgetContainerLayout->setContentsMargins(0,0,0,0);
    progressWidgetContainer->hide();
    _centralLayout->addWidget(progressWidgetContainer);
    _centralLayout->setCurrentIndex(1);

    QWidget* progressWidget = new QWidget();
    progressWidget->setMinimumSize(420, 0);
    progressWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    progressWidget->setAutoFillBackground(true);
    QPalette pal = progressWidget->palette();
    QColor bgcolor = pal.color(QPalette::Window);
    bgcolor.setAlpha(170);
    pal.setColor(QPalette::Window, bgcolor);
    progressWidget->setPalette(std::move(pal));
    progressWidget->setBackgroundRole(QPalette::Window);
    progressWidgetContainerLayout->addWidget(progressWidget, 0, 0, Qt::AlignHCenter | Qt::AlignTop);
    _progressLayout = new QVBoxLayout(progressWidget);
    _progressLayout->setContentsMargins(16, 16, 16, 16);
    _progressLayout->setSpacing(0);
    _progressLayout->addStretch(1);
}

/******************************************************************************
* Creates a frame buffer of the requested size and adjusts the size of the window.
******************************************************************************/
const std::shared_ptr<FrameBuffer>& FrameBufferWindow::createFrameBuffer(int w, int h)
{
    // Can we return the existing frame buffer as is?
    if(frameBuffer() && frameBuffer()->size() == QSize(w, h))
        return frameBuffer();

    // First-time allocation of a frame buffer or resizing existing buffer.
    if(!frameBuffer())
        setFrameBuffer(std::make_shared<FrameBuffer>(w, h));
    else
        frameBuffer()->setSize(QSize(w, h));

    // Clear buffer contents.
    frameBuffer()->clear();

    // Adjust window size to frame buffer size.
    // Temporarily turn off the scrollbars, because they should not be included in the size hint calculation.
    _frameBufferWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _frameBufferWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    centralWidget()->updateGeometry();
    adjustSize();
    // Reenable the scrollbars, but only after a short delay, because otherwise
    // they interfere with the resizing of the viewport widget.
    QTimer::singleShot(0, _frameBufferWidget, [w = _frameBufferWidget]() {
        w->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        w->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    });

    return frameBuffer();
}

/******************************************************************************
* Shows and activates the frame buffer window.
******************************************************************************/
void FrameBufferWindow::showAndActivateWindow()
{
    if(isHidden()) {
        // Center frame buffer window in main window.
        if(parentWidget()) {
            QSize s = frameGeometry().size();
            QPoint position = parentWidget()->geometry().center() - QPoint(s.width() / 2, s.height() / 2);
            // Make sure the window's title bar doesn't move outside the screen area (issue #201):
            if(position.x() < 0) position.setX(0);
            if(position.y() < 0) position.setY(0);
            move(position);
        }
        show();
        updateGeometry();
    }
    activateWindow();
}

/******************************************************************************
* Makes the framebuffer modal while a rendering operation is in progress and
* displays the progress in the window.
******************************************************************************/
void FrameBufferWindow::showRenderingProgress()
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(ExecutionContext::current().isValid());
    _renderingTask = this_task::get()->shared_from_this();

    // Update UI whenever the progress state of the rendering task(s) changes.
    _taskProgressUpdateConnection = connect(&_mainWindow, &MainWindow::taskProgressUpdate, this, &FrameBufferWindow::onTaskProgressUpdate);

    // Disable OVITO main window while rendering is in progress,
    // then immediately re-enable this floating child window.
    parentWidget()->setEnabled(false);
    this->setEnabled(true);

    // Disable toolbar actions during rendering.
    _saveToFileAction->setEnabled(false);
    _copyToClipboardAction->setEnabled(false);
    _autoCropAction->setEnabled(false);
    _cancelRenderingAction->setEnabled(true);
    _cancelRenderingAction->setVisible(true);
    _centralLayout->widget(1)->setVisible(true);

    // Show UI for the rendering progress.
    onTaskProgressUpdate();

    // Start watching the rendering task. Re-enable the window after rendering is done.
    _renderingTask->finally(_mainWindow, [self = QPointer<FrameBufferWindow>(this)]() noexcept {
        if(!self.isNull()) {
            self->_renderingTask.reset();
            self->onRenderingFinished();
        }
    });
}

/******************************************************************************
* Is called when the rendering process ended.
******************************************************************************/
void FrameBufferWindow::onRenderingFinished()
{
    disconnect(_taskProgressUpdateConnection);
    // Hide any remaining task widgets.
    for(auto& taskWidgets : _taskWidgets) {
        auto [statusLabel, progressBar] = taskWidgets;
        delete statusLabel;
        delete progressBar;
    }
    _taskWidgets.clear();
    parentWidget()->setEnabled(true);
    _saveToFileAction->setEnabled(true);
    _copyToClipboardAction->setEnabled(true);
    _autoCropAction->setEnabled(true);
    _cancelRenderingAction->setEnabled(false);
    _cancelRenderingAction->setVisible(false);
    _centralLayout->widget(1)->setVisible(false);
}

/******************************************************************************
* This opens the file dialog and lets the suer save the current contents of the frame buffer
* to an image file.
******************************************************************************/
void FrameBufferWindow::saveImage()
{
    if(!frameBuffer())
        return;

    SaveImageFileDialog fileDialog(this, tr("Save image"));
    if(fileDialog.exec()) {
        QString imageFilename = fileDialog.imageInfo().filename();
        if(!frameBuffer()->image().save(imageFilename, fileDialog.imageInfo().format())) {
            _mainWindow.reportError(tr("Failed to save image to file '%1'.").arg(imageFilename), this);
        }
    }
}

/******************************************************************************
* This copies the current image to the clipboard.
******************************************************************************/
void FrameBufferWindow::copyImageToClipboard()
{
    if(!frameBuffer())
        return;

    QApplication::clipboard()->setImage(frameBuffer()->image());
    QToolTip::showText(QCursor::pos(screen()), tr("Image has been copied to the clipboard"), nullptr, {}, 3000);
}

/******************************************************************************
* Removes background color pixels along the outer edges of the rendered image.
******************************************************************************/
void FrameBufferWindow::autoCrop()
{
    if(frameBuffer()) {
        if(!frameBuffer()->autoCrop()) {
            QToolTip::showText(QCursor::pos(screen()), tr("No background pixels found that can been removed"), nullptr, {}, 3000);
        }
    }
}

/******************************************************************************
* Scales the image up.
******************************************************************************/
void FrameBufferWindow::zoomIn()
{
    _frameBufferWidget->zoomIn();
}

/******************************************************************************
* Scales the image down.
******************************************************************************/
void FrameBufferWindow::zoomOut()
{
    _frameBufferWidget->zoomOut();
}

/******************************************************************************
* Stops the rendering operation that is currently in progress.
******************************************************************************/
void FrameBufferWindow::cancelRendering()
{
    if(_renderingTask)
        _renderingTask->cancel();
}

/******************************************************************************
* Is called when the user tries to close the window.
******************************************************************************/
void FrameBufferWindow::closeEvent(QCloseEvent* event)
{
    // Cancel the rendering operation if it is still in progress.
    cancelRendering();

    QMainWindow::closeEvent(event);
}

/******************************************************************************
* Is called during rendering whenever progress is made.
******************************************************************************/
void FrameBufferWindow::onTaskProgressUpdate()
{
    size_t index = 0;

    _mainWindow.visitRunningTasks([&](Task& task, const QString& text, int progressValue, int progressMaximum) {
        if(text.isEmpty())
            return;
        QLabel* statusLabel;
        QProgressBar* progressBar;
        if(index == _taskWidgets.size()) {
            statusLabel = new QLabel();
            progressBar = new QProgressBar();
            statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            _progressLayout->addWidget(statusLabel);
            _progressLayout->addWidget(progressBar);
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

}   // End of namespace
