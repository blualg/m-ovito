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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "OpenGLRenderer.h"
#include "OffscreenOpenGLRenderingJob.h"

#include <QOffscreenSurface>
#include <QSurface>
#include <QWindow>
#include <QScreen>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>

// Called on first use of the OpenGL renderer to register the embedded Qt resource files
// when running a statically linked executable. The Qt documentation says this
// needs to be placed outside of any C++ namespace.
static void registerQtResources()
{
#ifdef OVITO_BUILD_MONOLITHIC
    Q_INIT_RESOURCE(opengl);
#endif
}

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpenGLRenderer);
OVITO_CLASSINFO(OpenGLRenderer, "ClassNameAlias", "OpenGLSceneRenderer");  // For backward compatibility with OVITO 3.10
OVITO_CLASSINFO(OpenGLRenderer, "ClassNameAlias", "StandardSceneRenderer");  // For backward compatibility with OVITO 3.10
OVITO_CLASSINFO(OpenGLRenderer, "DisplayName", "OpenGL");
OVITO_CLASSINFO(OpenGLRenderer, "Description", "Hardware-accelerated rendering engine, also used by OVITO's interactive viewports. The OpenGL renderer is fast and has the smallest memory footprint.");
DEFINE_PROPERTY_FIELD(OpenGLRenderer, antialiasingLevel);
DEFINE_PROPERTY_FIELD(OpenGLRenderer, orderIndependentTransparency);
SET_PROPERTY_FIELD_LABEL(OpenGLRenderer, antialiasingLevel, "Antialiasing level");
SET_PROPERTY_FIELD_LABEL(OpenGLRenderer, orderIndependentTransparency, "Order-independent transparency");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OpenGLRenderer, antialiasingLevel, IntegerParameterUnit, 1, 6);

/// The vendor of the OpenGL implementation in use.
QByteArray OpenGLRenderer::_openGLVendor;

/// The renderer name of the OpenGL implementation in use.
QByteArray OpenGLRenderer::_openGLRenderer;

/// The version string of the OpenGL implementation in use.
QByteArray OpenGLRenderer::_openGLVersion;

/// The version of the OpenGL shading language supported by the system.
QByteArray OpenGLRenderer::_openGLSLVersion;

/// The current surface format used by the OpenGL implementation.
QSurfaceFormat OpenGLRenderer::_openglSurfaceFormat;

/// The list of extensions supported by the OpenGL implementation.
QSet<QByteArray> OpenGLRenderer::_openglExtensions;

/// Indicates whether the OpenGL implementation supports geometry shaders.
bool OpenGLRenderer::_openGLSupportsGeometryShaders = false;

/******************************************************************************
* Is called by OVITO to query the class for any information that should be
* included in the application's system report.
******************************************************************************/
void OpenGLRenderer::OOMetaClass::querySystemInformation(QTextStream& stream, UserInterface& userInterface) const
{
    if(this == &OpenGLRenderer::OOClass()) {
        OpenGLRenderer::determineOpenGLInfo();

        stream << "======= OpenGL info =======" << "\n";
        const QSurfaceFormat& format = OpenGLRenderer::openglSurfaceFormat();
        stream << "Vendor: " << OpenGLRenderer::openGLVendor() << "\n";
        stream << "Renderer: " << OpenGLRenderer::openGLRenderer() << "\n";
        stream << "Version number: " << format.majorVersion() << QStringLiteral(".") << format.minorVersion() << "\n";
        stream << "Version string: " << OpenGLRenderer::openGLVersion() << "\n";
        stream << "Profile: " << (format.profile() == QSurfaceFormat::CoreProfile ? "core" : (format.profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
        stream << "Swap behavior: " << (format.swapBehavior() == QSurfaceFormat::SingleBuffer ? QStringLiteral("single buffer") : (format.swapBehavior() == QSurfaceFormat::DoubleBuffer ? QStringLiteral("double buffer") : (format.swapBehavior() == QSurfaceFormat::TripleBuffer ? QStringLiteral("triple buffer") : QStringLiteral("other")))) << "\n";
        stream << "Depth buffer size: " << format.depthBufferSize() << "\n";
        stream << "Stencil buffer size: " << format.stencilBufferSize() << "\n";
        stream << "Shading language: " << OpenGLRenderer::openGLSLVersion() << "\n";
        stream << "Deprecated functions: " << (format.testOption(QSurfaceFormat::DeprecatedFunctions) ? "yes" : "no") << "\n";
        stream << "Geometry shader support: " << (OpenGLRenderer::openGLSupportsGeometryShaders() ? "yes" : "no") << "\n";
#if 0
        stream << "Supported extensions:\n";
        QStringList extensionList;
        for(const QByteArray& extension : OpenGLRenderer::openglExtensions())
            extensionList << extension;
        extensionList.sort();
        for(const QString& extension : extensionList)
            stream << extension << "\n";
#endif
    }
}

/******************************************************************************
* Constructor.
******************************************************************************/
void OpenGLRenderer::initializeObject(ObjectInitializationFlags flags)
{
    SceneRenderer::initializeObject(flags);

    registerQtResources();

    if(this_task::isInteractive()) {
        // Check which transparency rendering method has been selected by the user in the application settings dialog.
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings applicationSettings;
        if(applicationSettings.value("rendering/transparency_method").toInt() == 2) {
            // Activate the Weighted Blended Order-Independent Transparency method.
            setOrderIndependentTransparency(true);
        }
#endif
    }
}

/******************************************************************************
* Creates a new renderer-specific rendering job for offscreen rendering.
******************************************************************************/
OORef<RenderingJob> OpenGLRenderer::createOffscreenRenderingJob()
{
    return OORef<OffscreenOpenGLRenderingJob>::create(
        this_task::ui()->datasetContainer().visCache(), // Note: It's valid to use the global vis cache here, because the OpenGL renderer runs in the main thread.
        this, std::max(1, antialiasingLevel()));
}

/******************************************************************************
* Determines the capabilities of the current OpenGL implementation.
******************************************************************************/
void OpenGLRenderer::determineOpenGLInfo()
{
    if(!_openGLVendor.isEmpty())
        return;     // Already done.

    // Create a temporary GL context and an offscreen surface if necessary.
    QOpenGLContext tempContext;
    QOffscreenSurface offscreenSurface;
    std::unique_ptr<QWindow> window;
    QOpenGLContext* currentContext = QOpenGLContext::currentContext();
    if(!currentContext) {
        if(!tempContext.create())
            throw RendererException(tr("Failed to create an OpenGL context. Please check your graphics driver installation to make sure your system supports OpenGL applications. "
                                "Sometimes this may only be a temporary error after an automatic operating system update was installed in the background. In this case, simply rebooting your computer can help."));

        if(qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
            // Create a hidden, temporary window to make the GL context current.
            window.reset(new QWindow());
            window->setSurfaceType(QSurface::OpenGLSurface);
            window->setFormat(tempContext.format());
            window->create();
            if(!tempContext.makeCurrent(window.get()))
                throw RendererException(tr("Failed to make OpenGL context current. Cannot query OpenGL information."));
        }
        else {
            // Create temporary offscreen buffer to make GL context current.
            offscreenSurface.setFormat(tempContext.format());
            offscreenSurface.create();
            if(!offscreenSurface.isValid())
                throw RendererException(tr("Failed to create temporary offscreen rendering surface. Cannot query OpenGL information."));
            if(!tempContext.makeCurrent(&offscreenSurface))
                throw RendererException(tr("Failed to make OpenGL context current on offscreen rendering surface. Cannot query OpenGL information."));
        }
        OVITO_ASSERT(QOpenGLContext::currentContext() == &tempContext);
        currentContext = &tempContext;
    }

    _openGLVendor = reinterpret_cast<const char*>(currentContext->functions()->glGetString(GL_VENDOR));
    _openGLRenderer = reinterpret_cast<const char*>(currentContext->functions()->glGetString(GL_RENDERER));
    _openGLVersion = reinterpret_cast<const char*>(currentContext->functions()->glGetString(GL_VERSION));
    _openGLSLVersion = reinterpret_cast<const char*>(currentContext->functions()->glGetString(GL_SHADING_LANGUAGE_VERSION));
    _openglSurfaceFormat = currentContext->format();
    _openglExtensions = currentContext->extensions();
    _openGLSupportsGeometryShaders = QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Geometry);
}

}   // End of namespace
