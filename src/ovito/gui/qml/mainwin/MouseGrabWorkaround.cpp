////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/gui/qml/GUI.h>
#include "MouseGrabWorkaround.h"

namespace Ovito {

/******************************************************************************
* Actives the workaround, which means receiving mouse events will be disabled 
* for all Qt Quick items. 
******************************************************************************/
void MouseGrabWorkaround::setActive(bool active, QQuickItem* activeItem)
{
    if(_isActive == active)
        return;
    _isActive = active;
#ifdef Q_OS_WASM
    if(active) {
        OVITO_ASSERT(_savedState.empty());
        if(_container) {
//          qDebug() << "Activating mouse grabber for container" << _container << "and active item" << activeItem;
            disableMouseEventHandling(_container, activeItem);
        }
    }
    else {
        for(const auto& state : _savedState) {
            if(QQuickItem* item = state.first.data()) {
                item->setAcceptedMouseButtons(state.second);
            }
        }
        _savedState.clear();
    }
#endif
}

void MouseGrabWorkaround::disableMouseEventHandling(QQuickItem* item, QQuickItem* activeItem)
{
    for(QQuickItem* childItem : item->childItems()) {
        if(childItem == activeItem) {
            continue;
        }
        if(childItem->acceptedMouseButtons() != Qt::NoButton) {
//          qDebug() << "Overriding mouse state of item" << childItem << childItem->acceptedMouseButtons();
            _savedState.emplace_back(childItem, childItem->acceptedMouseButtons());
            childItem->setAcceptedMouseButtons(Qt::NoButton);
        }
        disableMouseEventHandling(childItem, activeItem);
    }
}

}   // End of namespace
