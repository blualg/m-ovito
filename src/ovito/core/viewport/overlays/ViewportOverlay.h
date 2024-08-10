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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/ActiveObject.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * \brief Abstract base class for all viewport layers types.
 */
class OVITO_CORE_EXPORT ViewportOverlay : public ActiveObject
{
public:

    /// A meta-class for viewport layers (i.e. classes derived from ViewportOverlay).
    class OVITO_CORE_EXPORT OOMetaClass : public ActiveObject::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ActiveObject::OOMetaClass::OOMetaClass;

        /// \brief Returns the category under which the layer will be displayed in the drop-down list box.
        virtual QString viewportOverlayCategory() const;
    };

    OVITO_CLASS_META(ViewportOverlay, OOMetaClass);

public:

    /// This virtual method gets called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport);

    /// This method asks the overlay to paint its contents over the rendered image.
    virtual void render(FrameGraph& frameGraph, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene) = 0;

    /// Moves the position of the layer in the viewport by the given amount,
    /// which is specified as a fraction of the viewport render size.
    ///
    /// Layer implementations should override this method if they support positioning.
    /// The default method implementation does nothing.
    virtual void moveLayerInViewport(const Vector2& delta) {}

    /// Helper method that checks whether the given Qt alignment value contains exactly one horizontal and one vertical alignment flag.
    void checkAlignmentParameterValue(int alignment) const;

    /// Informs the overlay that a new scene node has been inserted into the scene.
    virtual void sceneNodeAdded(SceneNode* node);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The pipeline generating the data that is being used by the overlay (optional).
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Pipeline*, pipeline, setPipeline, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_WEAK_REF | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);
};

}   // End of namespace
