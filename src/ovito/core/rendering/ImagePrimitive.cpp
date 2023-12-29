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

#include <ovito/core/Core.h>
#include "ImagePrimitive.h"
#include "SceneRenderer.h"

namespace Ovito {

/******************************************************************************
* Sets the destination rectangle for rendering the image in viewport coordinates.
******************************************************************************/
void ImagePrimitive::setRectViewport(const SceneRenderer* renderer, const Box2& rect)
{
    OVITO_ASSERT(!rect.isEmpty());
    const QSize windowSize = renderer->viewportRect().size();
    Point2 minc((rect.minc.x() + 1.0) * windowSize.width() / 2.0, (-rect.maxc.y() + 1.0) * windowSize.height() / 2.0);
    Point2 maxc((rect.maxc.x() + 1.0) * windowSize.width() / 2.0, (-rect.minc.y() + 1.0) * windowSize.height() / 2.0);
    setRectWindow(Box2(minc, maxc));
}

}   // End of namespace
