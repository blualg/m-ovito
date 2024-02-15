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
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/oo/RefTargetListener.h>
#include "ViewportInputMode.h"

namespace Ovito {

/******************************************************************************
* The default input mode for the viewports. This mode lets the user
* select scene nodes.
******************************************************************************/
class OVITO_GUIBASE_EXPORT SelectionMode : public ViewportInputMode
{
    OVITO_CLASS(SelectionMode)
    Q_OBJECT

public:

    /// Constructor.
    using ViewportInputMode::ViewportInputMode;

    /// \brief Returns the activation behavior of this input mode.
    virtual InputModeType modeType() override { return ExclusiveMode; }

    /// \brief Handles the mouse down event for the given viewport.
    virtual void mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// \brief Handles the mouse up event for the given viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// \brief Handles the mouse move event for the given viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// \brief Returns the cursor that is used by OVITO's viewports to indicate a selection.
    static QCursor& selectionCursor() {
#ifndef Q_OS_WASM
        static QCursor hoverCursor(QPixmap(QStringLiteral(":/guibase/cursor/editing/cursor_mode_select.png")));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        static QCursor hoverCursor(Qt::CrossCursor);
#endif
        return hoverCursor;
    }

protected:

    /// \brief This is called by the system after the input handler is
    ///        no longer the active handler.
    virtual void deactivated(bool temporary) override;

protected:

    /// The mouse position.
    QPointF _clickPoint;

    /// The current viewport we are working in.
    Viewport* _viewport = nullptr;
};

}   // End of namespace
