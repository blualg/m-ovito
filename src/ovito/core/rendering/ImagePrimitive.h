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
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A 2d image to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT ImagePrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Default constructor.
    ImagePrimitive() = default;

    /// Constructor taking an image and a window rectangle.
    ImagePrimitive(const QImage& image, const Box2& windowRect) : _image(image), _windowRect(windowRect) {}

    /// Constructor taking an image and a window rectangle.
    ImagePrimitive(const QImage& image, const QRectF& windowRect) : _image(image) { setRectWindow(windowRect); }

    /// Sets the mage to be rendered.
    void setImage(const QImage& image) { _image = image; }

    /// Sets the mage to be rendered.
    void setImage(QImage&& image) { _image = std::move(image); }

    /// Returns the image stored in the buffer.
    const QImage& image() const { return _image; }

    /// Sets the destination rectangle for rendering the image in window coordinates.
    void setRectWindow(const Box2& rect) { _windowRect = rect; }

    /// Sets the destination rectangle for rendering the image in window coordinates.
    void setRectWindow(const QRectF& rect) { _windowRect.minc = Point2(rect.left(), rect.top()); _windowRect.maxc = Point2(rect.right(), rect.bottom()); }

    /// Returns the destination rectangle in window coordinates.
    const Box2& windowRect() const { return _windowRect; }

private:

    /// The image to be rendered.
    QImage _image;

    /// The destination rectangle in window coordinates.
    Box2 _windowRect;
};

}   // End of namespace
