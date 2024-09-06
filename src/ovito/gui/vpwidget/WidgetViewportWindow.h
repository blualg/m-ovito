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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/viewport/BaseViewportWindow.h>

#include <QWidget>

namespace Ovito {

/**
 * \brief Generic base class for viewport windows that implements mouse input handling.
 */
class OVITO_VIEWPORTWIDGET_EXPORT WidgetViewportWindow : public BaseViewportWindow
{
    Q_OBJECT
	OVITO_CLASS(WidgetViewportWindow)

public:

    /// Associates this window with a viewport and creates the UI widget.
    void initializeWindow(Viewport* vp, UserInterface& userInterface, QWidget* parent);

    /// Returns the QWidget encapsulated by this ViewportWindow object.
    QWidget* widget() const { return _widget; }

    /// Indicates whether the window is currently shown or not.
    virtual bool isVisible() const override {
        return widget() && widget()->isVisible();
    }

    /// Sets the mouse cursor shape for the window.
    virtual void setCursor(const QCursor& cursor) override {
        widget()->setCursor(cursor);
    }

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() const override {
        return widget()->mapFromGlobal(QCursor::pos());
    }

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() const override {
        return widget()->size() * devicePixelRatio();
    }

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() const override {
        return widget()->size();
    }

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() const override {
        return widget()->devicePixelRatioF();
    }

protected:

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    virtual void aboutToBeDeleted() override;

    /// Creates the Qt widget that is associated with this viewport window.
    virtual QWidget* createQtWidget(QWidget* parent) = 0;

    /// Tells the window implementation to present a rendered frame on screen.
    virtual void presentFrame() override {
        if(widget())
            widget()->update();
    }

    /// Filters events sent to the widget.
    bool eventFilter(QObject* obj, QEvent* event) override;

private:

    /// The actual Qt GUI widget of the viewport window.
    QPointer<QWidget> _widget;
};

}   // End of namespace
