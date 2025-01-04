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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include "WidgetViewportWindow.h"

#include <QApplication>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(WidgetViewportWindow);

/******************************************************************************
* This method is called after the reference counter of this object has reached zero
* and before the object is being finally deleted.
******************************************************************************/
void WidgetViewportWindow::aboutToBeDeleted()
{
    // Release the UI widget.
    delete widget();

    BaseViewportWindow::aboutToBeDeleted();
}

/******************************************************************************
* Associates this window with a viewport and creates the UI widget.
******************************************************************************/
void WidgetViewportWindow::initializeWindow(Viewport* vp, UserInterface& userInterface, QWidget* parent)
{
    setViewport(vp, userInterface);

    OVITO_ASSERT(!_widget);
    _widget = createQtWidget(parent);

    widget()->installEventFilter(this);
    widget()->setMouseTracking(true);
    widget()->setFocusPolicy(Qt::StrongFocus);
    widget()->setAcceptDrops(false); // File drop events are handled by the root main window. This makes them propagate up the widget hierarchy.

    // Make sure the viewport window releases its resources before the application shuts down, e.g. due to a Python script error.
    connect(QCoreApplication::instance(), &QObject::destroyed, this, &BaseViewportWindow::releaseResources);
}

/******************************************************************************
* Filters events sent to the widget.
******************************************************************************/
bool WidgetViewportWindow::eventFilter(QObject* obj, QEvent* event)
{
    switch(event->type()) {
    case QEvent::Show:
        showEvent(static_cast<QShowEvent*>(event));
        break;
    case QEvent::Hide:
        hideEvent(static_cast<QHideEvent*>(event));
        break;
    case QEvent::Leave:
        leaveEvent(event);
        break;
    case QEvent::MouseButtonDblClick:
        if(widget()->isEnabled())
            mouseDoubleClickEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::MouseButtonPress:
        if(widget()->isEnabled())
            mousePressEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::MouseButtonRelease:
        if(widget()->isEnabled())
            mouseReleaseEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::MouseMove:
        if(widget()->isEnabled())
            mouseMoveEvent(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::Wheel:
        if(widget()->isEnabled())
            wheelEvent(static_cast<QWheelEvent*>(event));
        break;
    case QEvent::FocusOut:
        focusOutEvent(static_cast<QFocusEvent*>(event));
        break;
    case QEvent::Resize:
        resizeEvent(static_cast<QResizeEvent*>(event));
        break;
    case QEvent::KeyPress:
        if(widget()->isEnabled())
            keyPressEvent(static_cast<QKeyEvent*>(event));
        break;
    default:
        // Pass the event on to the base class.
        break;
    }
    return false;
}

}   // End of namespace
