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
#include <ovito/core/rendering/RenderingJob.h>

#include <QOpenGLContext>
#include <QOpenGLTexture>

namespace Ovito {

/**
 * \brief A wrapper class for OpenGL textures.
 *
 * Note that we cannot simply use a plain QOpenGLTexture, because its implementation contains a bug,
 * which requires the QOpenGLContext in which the QOpenGLTexture was created to outlive the QOpenGLTexture.
 */
class OpenGLTexture
{
public:

    /// Default constructor.
    OpenGLTexture() = default;

    /// Move constructor.
    OpenGLTexture(OpenGLTexture&& other) noexcept = default;

    /// Move assignment operator.
    OpenGLTexture& operator=(OpenGLTexture&& other) noexcept = default;

    /// Copy constructor is disabled.
    OpenGLTexture(const OpenGLTexture& other) = delete;

    /// Copy assignment operator is disabled.
    OpenGLTexture& operator=(const OpenGLTexture& other) = delete;

    /// Construction method I.
    void create(const QImage& image, QOpenGLTexture::MipMapGeneration genMipMaps = QOpenGLTexture::GenerateMipMaps) {
        OVITO_ASSERT(!_texture);
        _texture = std::make_unique<QOpenGLTexture>(image, genMipMaps);
        if(!_texture->isCreated())
            throw RendererException("Failed to create OpenGL texture object.");
        destroyTextureWithContext();
    }

    /// Construction method II.
    void create(QOpenGLTexture::Target target) {
        OVITO_ASSERT(!_texture);
        _texture = std::make_unique<QOpenGLTexture>(target);
        destroyTextureWithContext();
    }

    /// Destroys the OpenGL texture.
    void reset() {
        // Uninstall signal handler.
        if(_signalConnection)
            QObject::disconnect(_signalConnection);
        _texture.reset();
    }

    /// Destructor.
    ~OpenGLTexture() {
        reset();
    }

    // Checks if the texture still exists and has not been automatically destroyed because
    // the original QOpenGLContext was destroyed. This method is automatically called by the
    // RendererResourceCache class upon resource retrieval.
    bool isRendererResourceValid() const {
        return _texture && _texture->isCreated();
    }

    /// Accesses the underlying QOpenGLTexture object wrapped by this class.
    QOpenGLTexture& get() const {
        OVITO_ASSERT(_texture);
        return const_cast<QOpenGLTexture&>(*_texture);
    }

private:

    /// Wraps the QOpenGLTexture::create() method and install a signal handler
    /// that automatically destroys the texture when then QOpenGLContext is destroyed.
    void destroyTextureWithContext() {
        OVITO_ASSERT(!_signalConnection);
        OVITO_ASSERT(_texture);

        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        OVITO_ASSERT(ctx);
        QSurface* surface = ctx->surface();
        OVITO_ASSERT(surface);

        // When the QOpenGLContext::aboutToBeDestroyed signal gets fired, destroy this texture.
        _signalConnection = QObject::connect(ctx, &QOpenGLContext::aboutToBeDestroyed, [this, ctx, surface]() {
            OVITO_ASSERT(!QOpenGLContext::currentContext());
            ctx->makeCurrent(surface);
            _texture->destroy();
            ctx->doneCurrent();
            OpenGLTexture* self = this;
            QObject::disconnect(_signalConnection); // This may destroy the lambda function object currently being executed.
            self->_signalConnection = QMetaObject::Connection();
        });
    }

    std::unique_ptr<QOpenGLTexture> _texture; // Wrap QOpenGLTexture in unique_ptr to make this wrapper movable.
    QMetaObject::Connection _signalConnection;
};

}   // End of namespace
