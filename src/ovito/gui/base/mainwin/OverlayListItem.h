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
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

/**
 * An item of the OverlayListModel representing a ViewportOverlay attached to a Viewport.
 */
class OVITO_GUIBASE_EXPORT OverlayListItem : public QObject, public RefMaker
{
    OVITO_CLASS(OverlayListItem)
    Q_OBJECT

public:

    enum OverlayItemType {
        Layer,
        ViewportHeader,
        SceneLayer,
    };

public:

    /// Constructor.
    explicit OverlayListItem(ViewportOverlay* overlay, OverlayItemType itemType);

    /// Returns the status of the object represented by the list item.
    const PipelineStatus& status() const;

    /// Returns the title text for this list item.
    QString title(Viewport* selectedViewport) const;

    /// Returns the type of this list item.
    OverlayItemType itemType() const { return _itemType; }

    /// Returns a short piece information (typically a string) to be displayed next to the object's title in the UI.
    QVariant shortInfo(Viewport* selectedViewport) const;

Q_SIGNALS:

    /// This signal is emitted when this item has changed.
    void itemChanged(OverlayListItem* item);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The overlay represented by this item in the list box.
    DECLARE_REFERENCE_FIELD(ViewportOverlay*, overlay);

    /// The type of this list item.
    OverlayItemType _itemType;
};

}   // End of namespace
