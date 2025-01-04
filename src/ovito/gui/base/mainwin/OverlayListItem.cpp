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
#include <ovito/core/viewport/Viewport.h>
#include "OverlayListItem.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OverlayListItem);
DEFINE_REFERENCE_FIELD(OverlayListItem, overlay);

/******************************************************************************
* Constructor.
******************************************************************************/
void OverlayListItem::initializeObject(ViewportOverlay* overlay, OverlayItemType itemType)
{
    RefMaker::initializeObject();

    _itemType = itemType;
    _overlay.set(this, PROPERTY_FIELD(overlay), overlay);
}

/******************************************************************************
* This method is called when the object presented by the modifier
* list item generates a message.
******************************************************************************/
bool OverlayListItem::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    /// Update item if it has been enabled/disabled, its status has changed, or its title has changed.
    if(event.type() == ReferenceEvent::TargetEnabledOrDisabled || event.type() == ReferenceEvent::ObjectStatusChanged || event.type() == ReferenceEvent::TitleChanged) {
        Q_EMIT itemChanged(this);
    }

    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Returns the status of the object represented by the list item.
******************************************************************************/
const PipelineStatus& OverlayListItem::status() const
{
    if(overlay()) {
        return overlay()->status();
    }
    else {
        static const PipelineStatus defaultStatus;
        return defaultStatus;
    }
}

/******************************************************************************
* Returns the text for this list item.
******************************************************************************/
QString OverlayListItem::title(Viewport* selectedViewport) const
{
    OVITO_ASSERT(selectedViewport);
    switch(_itemType) {
    case Layer:
        return overlay() ? overlay()->objectTitle() : QString();
    case ViewportHeader: return tr("Active viewport: %1").arg(selectedViewport->viewportTitle());
    case SceneLayer: return tr("3D scene layer");
    default: return {};
    }
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant OverlayListItem::shortInfo(Viewport* selectedViewport) const
{
    OVITO_ASSERT(this_task::get());
    if(overlay()) {
        if(Scene* scene = selectedViewport->scene()) {
            return overlay()->getPipelineEditorShortInfo(scene);
        }
    }
    return {};
}

}   // End of namespace
