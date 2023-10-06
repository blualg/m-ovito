////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#include <ovito/gui/qml/GUI.h>

namespace Ovito {

/**
 * \brief Helper object for working around a mouse grabbing issue on the WebAssembly platform.
 */
class MouseGrabWorkaround : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickItem* container READ container WRITE setContainer)

public:

    /// Constructor.
    using QObject::QObject;

    QQuickItem* container() const { return _container; }
    void setContainer(QQuickItem* container) { _container = container; }

    /// Actives the workaround, which means receiving mouse events will be disabled for all Qt Quick items.
    Q_INVOKABLE void setActive(bool active, QQuickItem* activeItem);
    bool isActive() const { return _isActive; }

private:

    void disableMouseEventHandling(QQuickItem* item, QQuickItem* activeItem);

    bool _isActive = false;
    QQuickItem* _container = nullptr;

    std::vector<std::pair<QPointer<QQuickItem>, Qt::MouseButtons>> _savedState;
};

}   // End of namespace
