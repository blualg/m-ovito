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


#include <ovito/core/Core.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportLayout.h>

namespace Ovito {

/**
 * \brief This class holds a collection of Viewport objects.
 *
 * It also keeps track of the current viewport and the maximized viewport.
 */
class OVITO_CORE_EXPORT ViewportConfiguration : public RefTarget
{
    OVITO_CLASS(ViewportConfiguration)

public:

    /// Constructor.
    explicit ViewportConfiguration(ObjectInitializationFlags flags);

    /// Registers a viewport with the configuration object so that it takes part in the automatic viewport refresh mechanism.
    /// This method is currently used in the implementation of the Viewport.create_qt_widget() Python method.
    void registerViewport(Viewport* vp) {
        OVITO_ASSERT(vp);
        if(!viewports().contains(vp))
            _viewports.push_back(this, PROPERTY_FIELD(viewports), vp);
    }

    /// Determines the effective rectangles for all the viewports in the layout hierarchy.
    std::vector<std::pair<Viewport*, QRectF>> getViewportRectangles(const QRectF& rect = QRectF(0,0,1,1), const QSizeF& borderSize = QSizeF(0,0)) const {
        std::vector<std::pair<Viewport*, QRectF>> viewportRects;
        if(layoutRootCell())
            layoutRootCell()->getViewportRectangles(rect, viewportRects, borderSize);
        return viewportRects;
    }

    /// \brief Zooms all viewports to the extents of the currently selected nodes.
    void zoomToSelectionExtents();

    /// \brief Zooms to the extents of the scene.
    void zoomToSceneExtents();

    /// \brief Zooms all viewports to the extents of the scene
    ///        when all scene pipelines have been fully evaluated and the extents are known.
    void zoomToSceneExtentsWhenReady();

protected:

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// Rebuilds the linear list of all viewports that are part of the current viewport layout tree.
    void updateListOfViewports();

    /// List of all viewports which get automatically refreshed whenever their scene changes.
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(Viewport*, viewports, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_WEAK_REF);

    /// The active viewport. May be null.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Viewport*, activeViewport, setActiveViewport, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_WEAK_REF);

    /// The maximized viewport if any.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Viewport*, maximizedViewport, setMaximizedViewport, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_WEAK_REF);

    /// The viewport layout tree's root node.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<ViewportLayoutCell>, layoutRootCell, setLayoutRootCell);
};

}   // End of namespace
