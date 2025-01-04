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
#include <ovito/core/rendering/SceneRenderer.h>

#include <QSurfaceFormat>

namespace Ovito {

/**
 * \brief An OpenGL-based scene renderer. This serves as base class for both the interactive renderer used
 *        by the viewports and the standard output renderer.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLRenderer : public SceneRenderer
{
public:

    /// Defines a metaclass specialization for this renderer class.
    class OVITO_OPENGLRENDERER_EXPORT OOMetaClass : public SceneRenderer::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using SceneRenderer::OOMetaClass::OOMetaClass;

        /// Is called by OVITO to query the class for any information that should be included in the application's system report.
        virtual void querySystemInformation(QTextStream& stream, UserInterface& userInterface) const override;
    };

    OVITO_CLASS_META(OpenGLRenderer, OOMetaClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

	/// Creates a new renderer-specific rendering job for offscreen rendering.
	virtual OORef<RenderingJob> createOffscreenRenderingJob() override;

    /// Returns the vendor name of the OpenGL implementation in use.
    static const QByteArray& openGLVendor() { return _openGLVendor; }

    /// Returns the renderer name of the OpenGL implementation in use.
    static const QByteArray& openGLRenderer() { return _openGLRenderer; }

    /// Returns the version string of the OpenGL implementation in use.
    static const QByteArray& openGLVersion() { return _openGLVersion; }

    /// Returns the version of the OpenGL shading language supported by the system.
    static const QByteArray& openGLSLVersion() { return _openGLSLVersion; }

    /// Returns the current surface format used by the OpenGL implementation.
    static const QSurfaceFormat& openglSurfaceFormat() { return _openglSurfaceFormat; }

    /// Returns the list of extensions supported by the OpenGL implementation.
    static const QSet<QByteArray>& openglExtensions() { return _openglExtensions; }

    /// Returns whether the OpenGL implementation supports geometry shaders.
    static bool openGLSupportsGeometryShaders() { return _openGLSupportsGeometryShaders; }

    /// Determines the capabilities of the current OpenGL implementation.
    static void determineOpenGLInfo();

private:

    /// Controls the number of sub-pixels to render.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{3}, antialiasingLevel, setAntialiasingLevel, PROPERTY_FIELD_RESETTABLE);

    /// Activates the order-independent rendering method for semi-transparent objects (implemented by the OpenGL renderer).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, orderIndependentTransparency, setOrderIndependentTransparency, PROPERTY_FIELD_RESETTABLE);

    /// The vendor of the OpenGL implementation in use.
    static QByteArray _openGLVendor;

    /// The renderer name of the OpenGL implementation in use.
    static QByteArray _openGLRenderer;

    /// The version string of the OpenGL implementation in use.
    static QByteArray _openGLVersion;

    /// The version of the OpenGL shading language supported by the system.
    static QByteArray _openGLSLVersion;

    /// The current surface format used by the OpenGL implementation.
    static QSurfaceFormat _openglSurfaceFormat;

    /// The list of extensions supported by the OpenGL implementation.
    static QSet<QByteArray> _openglExtensions;

    /// Indicates whether the OpenGL implementation supports geometry shaders.
    static bool _openGLSupportsGeometryShaders;
};

}   // End of namespace
