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
#include "OpenGLResourceManager.h"
#include "OpenGLTexture.h"

#include <QThreadStorage>
#include <QOpenGLContextGroup>

namespace Ovito {

static QThreadStorage<OpenGLResourceManager*> glContextManagerStorage;

OpenGLResourceManager* OpenGLResourceManager::instance()
{
    if(!glContextManagerStorage.hasLocalData()) {
        glContextManagerStorage.setLocalData(new OpenGLResourceManager());
    }
    return glContextManagerStorage.localData();
}

/******************************************************************************
* Create an OpenGL texture object for a QImage.
******************************************************************************/
QOpenGLTexture* OpenGLResourceManager::uploadImage(const QImage& image, ResourceFrameHandle resourceFrame, QOpenGLTexture::MipMapGeneration genMipMaps)
{
    OVITO_ASSERT(!image.isNull());

    // Check if this image has already been uploaded to the GPU.
    RendererResourceKey<struct ImageCache, quint64, QOpenGLContextGroup*> cacheKey{ image.cacheKey(), QOpenGLContextGroup::currentContextGroup() };
    std::unique_ptr<OpenGLTexture>& texture = lookup<std::unique_ptr<OpenGLTexture>>(cacheKey, resourceFrame);

    // Create the texture object.
    if(!texture || !texture->isCreated()) {
        texture = std::make_unique<OpenGLTexture>(image, genMipMaps);
        if(genMipMaps == QOpenGLTexture::DontGenerateMipMaps) {
            texture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
        }
    }

    return texture.get();
}

/******************************************************************************
* Creates a 1-D OpenGL texture object for a ColorCodingGradient.
******************************************************************************/
QOpenGLTexture* OpenGLResourceManager::uploadColorMap(ColorCodingGradient* gradient, ResourceFrameHandle resourceFrame)
{
    // Check if this color map has already been uploaded to the GPU.
    RendererResourceKey<struct ColorMapCache, OORef<ColorCodingGradient>, QOpenGLContextGroup*> cacheKey{ gradient, QOpenGLContextGroup::currentContextGroup() };
    std::unique_ptr<OpenGLTexture>& texture = lookup<std::unique_ptr<OpenGLTexture>>(cacheKey, resourceFrame);

    if(!texture || !texture->isCreated()) {
        // Sample the color gradient to produce a row of RGB pixel data.
        int resolution;
        std::vector<uint8_t> pixelData;

        if(gradient) {
            resolution = 256;
            pixelData.resize(resolution * 3);
            for(int x = 0; x < resolution; x++) {
                Color c = gradient->valueToColor((FloatType)x / (resolution - 1));
                pixelData[x * 3 + 0] = (uint8_t)(255 * c.r());
                pixelData[x * 3 + 1] = (uint8_t)(255 * c.g());
                pixelData[x * 3 + 2] = (uint8_t)(255 * c.b());
            }
        }
        else {
            resolution = 1;
            pixelData.resize(3, 255);
        }

        // Create the 1-d texture object.
        texture = std::make_unique<OpenGLTexture>(QOpenGLTexture::Target1D);
        texture->setFormat(QOpenGLTexture::RGB8_UNorm);
        texture->setSize(resolution);
        texture->allocateStorage(QOpenGLTexture::RGB, QOpenGLTexture::UInt8);
        texture->setAutoMipMapGenerationEnabled(true);
        texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, pixelData.data());
    }

    return texture.get();
}

}   // End of namespace
