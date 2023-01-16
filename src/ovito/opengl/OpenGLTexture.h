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

#pragma once


#include <ovito/core/Core.h>

#include <QOpenGLContext>
#include <QOpenGLTexture>

namespace Ovito {

/**
 * \brief A wrapper class for OpenGL textures.
 * 
 * Note that we cannot simply use a plain QOpenGLTexture, because its implementation contains a bug,
 * which requires the QOpenGLContext in which the QOpenGLTexture was created to outlive the QOpenGLTexture.
 */
class OpenGLTexture : public QOpenGLTexture
{
public:

    /// Constructor.
    OpenGLTexture(const QImage& image, QOpenGLTexture::MipMapGeneration genMipMaps = QOpenGLTexture::GenerateMipMaps) : QOpenGLTexture(image, genMipMaps) {
        destroyTextureWithContext();
    }

    /// Constructor.
    OpenGLTexture(QOpenGLTexture::Target target) : QOpenGLTexture(target) {
        destroyTextureWithContext();
    }

    /// Destructor.
    ~OpenGLTexture() {
        // Uninstall signal handler.
        if(_signalConnection)
            QObject::disconnect(_signalConnection);
    }

private:

    /// Wraps the QOpenGLTexture::create() method and install a signal handler
    /// that automatically destroys the texture when then QOpenGLContext is destroyed.
    void destroyTextureWithContext() {
        OVITO_ASSERT(!_signalConnection);

        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        OVITO_ASSERT(ctx);
        QSurface* surface = ctx->surface();
        OVITO_ASSERT(surface);

        // When the QOpenGLContext::aboutToBeDestroyed signal gets fired, destroy this texture.
        _signalConnection = QObject::connect(ctx, &QOpenGLContext::aboutToBeDestroyed, [this, ctx, surface]() {
            OVITO_ASSERT(!QOpenGLContext::currentContext());
            ctx->makeCurrent(surface);
            destroy();
            ctx->doneCurrent();
            OpenGLTexture* self = this;
            QObject::disconnect(_signalConnection); // This may destroy the lambda function object currently being executed.
            self->_signalConnection = QMetaObject::Connection();
        });
    }

    QMetaObject::Connection _signalConnection;
};

}   // End of namespace
