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
#include <ovito/core/rendering/FrameBuffer.h>
#include "FrameBufferWidget.h"

namespace Ovito {

/**
 * This window displays the contents of a FrameBuffer.
 */
class OVITO_GUI_EXPORT FrameBufferWindow : public QMainWindow
{
    Q_OBJECT

public:

    /// Constructor.
    FrameBufferWindow(MainWindow& mainWindow, QWidget* parent = nullptr);

    /// Return the FrameBuffer that is currently shown in the widget (can be NULL).
    const std::shared_ptr<FrameBuffer>& frameBuffer() const { return _frameBufferWidget->frameBuffer(); }

    /// Sets the FrameBuffer that is currently shown in the widget.
    void setFrameBuffer(const std::shared_ptr<FrameBuffer>& frameBuffer) { _frameBufferWidget->setFrameBuffer(frameBuffer); }

    /// Creates a frame buffer of the requested size and adjusts the size of the window.
    const std::shared_ptr<FrameBuffer>& createFrameBuffer(int w, int h);

    /// Shows and activates the frame buffer window.
    void showAndActivateWindow();

    /// Makes the framebuffer modal while a rendering operation is in progress
    /// and displays a progress indicator in the window.
    void showRenderingProgress(SharedFuture<void> renderingFuture);

public Q_SLOTS:

    /// This opens the file dialog and lets the user save the current contents of the frame buffer
    /// to an image file.
    void saveImage();

    /// This copies the current image to the clipboard.
    void copyImageToClipboard();

    /// Removes background color pixels along the outer edges of the rendered image.
    void autoCrop();

    /// Scales the image up.
    void zoomIn();

    /// Scales the image down.
    void zoomOut();

    /// Stops the rendering operation that is currently in progress.
    void cancelRendering();

protected Q_SLOTS:

    /// Is called during rendering whenever progress is made.
    void onTaskProgressUpdate();

protected:

    /// Is called when the user tries to close the window.
    virtual void closeEvent(QCloseEvent* event) override;

private:

    /// Is called when the rendering process ended.
    void onRenderingFinished(SharedFuture<void> future);

    /// The main window this child window is associated with.
    MainWindow& _mainWindow;

    /// The widget that displays the FrameBuffer.
    FrameBufferWidget* _frameBufferWidget;

    // Toolbar actions:
    QAction* _saveToFileAction;
    QAction* _copyToClipboardAction;
    QAction* _autoCropAction;
    QAction* _cancelRenderingAction;

    /// Layout manager of the central container widget.
    QStackedLayout* _centralLayout;

    /// The rendering operation currently in progress.
    SharedFuture<void> _renderingFuture;

    /// Layout component for displaying the progress of rendering operations.
    QVBoxLayout* _progressLayout;

    /// List of per-task display widgets.
    std::vector<std::pair<QLabel*, QProgressBar*>> _taskWidgets;

    /// Connection to the main window's taskProgressUpdate() signal.
    QMetaObject::Connection _taskProgressUpdateConnection;
};

}   // End of namespace
