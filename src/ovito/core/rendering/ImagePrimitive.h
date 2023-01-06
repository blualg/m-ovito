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

namespace Ovito {

/**
 * \brief A 2d image to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT ImagePrimitive final
{
	Q_GADGET

public:

	/// \brief Sets the mage to be rendered.
	void setImage(const QImage& image) { _image = image; }

	/// \brief Sets the mage to be rendered.
	void setImage(QImage&& image) { _image = std::move(image); }

	/// \brief Returns the image stored in the buffer.
	const QImage& image() const { return _image; }

	/// \brief Sets the destination rectangle for rendering the image in window coordinates.
	void setRectWindow(const Box2& rect) { _windowRect = rect; }

	/// \brief Sets the destination rectangle for rendering the image in window coordinates.
	void setRectWindow(const QRectF& rect) { _windowRect.minc = Point2(rect.left(), rect.top()); _windowRect.maxc = Point2(rect.right(), rect.bottom()); }

	/// \brief Sets the destination rectangle for rendering the image in viewport coordinates.
	void setRectViewport(const SceneRenderer* renderer, const Box2& rect);

	/// \brief Returns the destination rectangle in window coordinates.
	const Box2& windowRect() const { return _windowRect; }

private:

	/// The image to be rendered.
	QImage _image;

	/// The destination rectangle in window coordinates.
	Box2 _windowRect;
};

}	// End of namespace
