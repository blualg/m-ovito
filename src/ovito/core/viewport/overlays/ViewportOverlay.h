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
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * \brief Abstract base class for all viewport layers types.
 */
class OVITO_CORE_EXPORT ViewportOverlay : public ActiveObject
{
    OVITO_CLASS(ViewportOverlay)

protected:

    /// \brief Constructor.
    explicit ViewportOverlay(ObjectInitializationFlags flags);

public:

    /// \brief This virtual method gets called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport) {}

    /// \brief This method asks the overlay to paint its contents over the rendered image.
    virtual void render(SceneRenderer* renderer, const QRect& logicalViewportRect, const QRect& physicalViewportRect) = 0;

    /// \brief Moves the position of the layer in the viewport by the given amount,
    ///        which is specified as a fraction of the viewport render size.
    ///
    /// Layer implementations should override this method if they support positioning.
    /// The default method implementation does nothing.
    virtual void moveLayerInViewport(const Vector2& delta) {}

    /// Helper method that checks whether the given Qt alignment value contains exactly one horizontal and one vertical alignment flag.
    void checkAlignmentParameterValue(int alignment) const;

    /// Informs the overlay that a new scene node has been inserted into the scene.
    virtual void sceneNodeAdded(SceneNode* node) {}
};

}   // End of namespace
