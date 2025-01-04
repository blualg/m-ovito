////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <QOpenGLFunctions>
#include <QOpenGLTexture>

namespace Ovito {

/**
 * \brief A wrapper class for OpenGL textures.
 */
class OpenGLTexture
{
public:

    /// Default constructor.
    OpenGLTexture() = default;

    /// Move constructor.
    OpenGLTexture(OpenGLTexture&& other) noexcept :
        _textureId(std::exchange(other._textureId, 0)),
        _size(std::exchange(other._size, QSize())),
        _shareGroup(std::exchange(other._shareGroup, nullptr)),
        _target(other._target) {}

    /// Move assignment operator.
    OpenGLTexture& operator=(OpenGLTexture&& other) noexcept {
        std::swap(_textureId, other._textureId);
        _size = other._size;
        _shareGroup = other._shareGroup;
        _target = other._target;
        return *this;
    }

    /// Copy constructor is disabled.
    OpenGLTexture(const OpenGLTexture& other) = delete;

    /// Copy assignment operator is disabled.
    OpenGLTexture& operator=(const OpenGLTexture& other) = delete;

    /// Destructor.
    ~OpenGLTexture() { reset(); }

    /// Returns true if this texture has been created.
    explicit operator bool() const { return _textureId != 0; }

    /// Construction method I.
    void create(QOpenGLTexture::Target target) {
        OVITO_ASSERT(!_textureId);
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        OVITO_ASSERT(ctx);
        _shareGroup = ctx->shareGroup();
        functions()->glGenTextures(1, &_textureId);
        OVITO_ASSERT(_textureId != 0);
        _target = target;
    }

    void create(const QImage& image, bool generateMipmaps = false) {
        OVITO_ASSERT(image.format() == QImage::Format_RGBA8888);
        create(QOpenGLTexture::Target2D);
        setData(QOpenGLTexture::RGBA8_UNorm, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, image.width(), image.height(), image.bits(), generateMipmaps);
    }

    void setMinMagFilters(QOpenGLTexture::Filter minificationFilter, QOpenGLTexture::Filter magnificationFilter) {
        bind();
        functions()->glTexParameteri(_target, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minificationFilter));
        functions()->glTexParameteri(_target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magnificationFilter));
        release();
    }

    void setWrapMode(QOpenGLTexture::WrapMode mode) {
        bind();
        functions()->glTexParameteri(_target, GL_TEXTURE_WRAP_S, static_cast<GLint>(mode));
        if(_target == QOpenGLTexture::Target2D)
            functions()->glTexParameteri(_target, GL_TEXTURE_WRAP_T, static_cast<GLint>(mode));
        release();
    }

    void setData(QOpenGLTexture::TextureFormat internalFormat, QOpenGLTexture::PixelFormat sourceFormat, QOpenGLTexture::PixelType sourceType, int width, int height, const void* data, bool generateMipmaps) {
        bind();
        if(_target == QOpenGLTexture::Target1D) {
            //functions()->glTexImage1D(_target, 0, internalFormat, width, 0, dataFormat, GL_UNSIGNED_BYTE, data);
            OVITO_ASSERT(false);
        }
        else if(_target == QOpenGLTexture::Target2D) {
            functions()->glTexImage2D(_target, 0, internalFormat, width, height, 0, sourceFormat, sourceType, data);
        }
        else OVITO_ASSERT(false);
        _size = QSize(width, height);
        if(generateMipmaps)
            functions()->glGenerateMipmap(_target);
        release();
    }

    /// Destroys the OpenGL texture.
    void reset() {
        if(_textureId) {
            functions()->glDeleteTextures(1, &_textureId);
            _textureId = 0;
            _shareGroup = nullptr;
            _size = {};
        }
    }

    void bind() const {
        OVITO_ASSERT(_textureId);
        functions()->glBindTexture(_target, _textureId);
    }

    void release() const {
        functions()->glBindTexture(_target, 0);
    }

    auto width() const { return _size.width(); }
    auto height() const { return _size.height(); }
    auto textureId() const { return _textureId; }

private:

    QOpenGLFunctions* functions() const {
        OVITO_ASSERT(QOpenGLContext::currentContext());
        OVITO_ASSERT(_shareGroup);
        OVITO_ASSERT(_shareGroup == QOpenGLContext::currentContext()->shareGroup());
        return QOpenGLContext::currentContext()->functions();
    }

private:

    GLuint _textureId = 0;
    QSize _size;
    QOpenGLContextGroup* _shareGroup = nullptr;
    QOpenGLTexture::Target _target = QOpenGLTexture::Target2D;
};

}   // End of namespace
