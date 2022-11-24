////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include "FrameBufferWindow.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBufferWindow::FrameBufferWindow(QWidget* parent) :
	QMainWindow(parent, (Qt::WindowFlags)(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint))
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
	// they interfer with the resizing of the viewport widget.
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
* Makes the framebuffer modal while a rendering operation is in progress and displays the progress in the window.
******************************************************************************/
void FrameBufferWindow::showRenderingOperation(MainThreadOperation& renderingOperation)
{
	OVITO_ASSERT(!_renderingWatcher);
	_renderingWatcher = new TaskWatcher(this);

	connect(_renderingWatcher, &TaskWatcher::started, this, [this]() {
		parentWidget()->setEnabled(false);
		this->setEnabled(true);
		_saveToFileAction->setEnabled(false);
		_copyToClipboardAction->setEnabled(false);
		_autoCropAction->setEnabled(false);
		_cancelRenderingAction->setEnabled(true);
		_cancelRenderingAction->setVisible(true);
		_centralLayout->widget(1)->setVisible(true);
	});

	connect(_renderingWatcher, &TaskWatcher::finished, this, [this]() {
		parentWidget()->setEnabled(true);
		_saveToFileAction->setEnabled(true);
		_copyToClipboardAction->setEnabled(true);
		_autoCropAction->setEnabled(true);
		_cancelRenderingAction->setEnabled(false);
		_cancelRenderingAction->setVisible(false);
		_centralLayout->widget(1)->setVisible(false);
		_renderingWatcher->deleteLater();
	});

	_renderingWatcher->watch(renderingOperation.task());

	// Create UI for every running task.
	for(TaskWatcher* watcher : renderingOperation.userInterface().taskManager().runningTasks()) {
		createTaskProgressWidgets(watcher);
	}

	// Create a separate progress bar for every new active task.
	connect(&renderingOperation.userInterface().taskManager(), &TaskManager::taskStarted, this, &FrameBufferWindow::createTaskProgressWidgets, Qt::UniqueConnection);
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
			Exception ex(tr("Failed to save image to file '%1'.").arg(imageFilename));
			ex.reportError();
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QToolTip::showText(QCursor::pos(screen()), tr("Image has been copied to the clipboard"), nullptr, {}, 3000);
#endif
}

/******************************************************************************
* Removes background color pixels along the outer edges of the rendered image.
******************************************************************************/
void FrameBufferWindow::autoCrop()
{
	if(frameBuffer()) {
		if(!frameBuffer()->autoCrop()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			QToolTip::showText(QCursor::pos(screen()), tr("No background pixels found that can been removed"), nullptr, {}, 3000);
#endif
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
	if(_renderingWatcher)
		_renderingWatcher->cancel();
}

/******************************************************************************
* Is called when the user tries to close the window.
******************************************************************************/
void FrameBufferWindow::closeEvent(QCloseEvent* event)
{
	// Cancel the rendering operation if it is still in prrogress.
	cancelRendering();

	QMainWindow::closeEvent(event);
}

/******************************************************************************
* Creates the UI widgets for displaying the progress of one asynchronous task.
******************************************************************************/
void FrameBufferWindow::createTaskProgressWidgets(TaskWatcher* taskWatcher)
{
	// Helper function that sets up the UI widgets in the dialog for a newly started task.
	QLabel* statusLabel = new QLabel(taskWatcher->progressText());
	statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	QProgressBar* progressBar = new QProgressBar();
//	progressBar->setAttribute(Qt::WA_OpaquePaintEvent);
	progressBar->setMaximum(taskWatcher->progressMaximum());
	progressBar->setValue(taskWatcher->progressValue());
	if(statusLabel->text().isEmpty()) {
		statusLabel->hide();
		progressBar->hide();
	}
	_progressLayout->insertWidget(_progressLayout->count() - 1, statusLabel);
	_progressLayout->insertWidget(_progressLayout->count() - 1, progressBar);
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
}

}	// End of namespace
