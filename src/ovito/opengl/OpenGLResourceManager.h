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

/**
 * \file OpenGLResourceManager.h
 * \brief Contains the definition of the Ovito::OpenGLResourceManager class.
 */

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

#include <QOpenGLTexture>

namespace Ovito {

class OVITO_OPENGLRENDERER_EXPORT OpenGLResourceManager : public RendererResourceCache
{
    Q_DISABLE_COPY(OpenGLResourceManager);

public:

    /// Returns the thread-local instance of the class.
    static OpenGLResourceManager* instance(); 

    /// Default constructor.
    OpenGLResourceManager() = default;

    /// Creates an OpenGL texture object for a QImage.
    QOpenGLTexture* uploadImage(const QImage& image, ResourceFrameHandle resourceFrame, QOpenGLTexture::MipMapGeneration genMipMaps = QOpenGLTexture::DontGenerateMipMaps);

    /// Creates a 1-D OpenGL texture object for a ColorCodingGradient.
    QOpenGLTexture* uploadColorMap(ColorCodingGradient* gradient, ResourceFrameHandle resourceFrame);
};

}   // End of namespace
