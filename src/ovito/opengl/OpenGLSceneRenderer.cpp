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
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/ColorCodingGradient.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingIdentifierMap.h>
#include "OpenGLSceneRenderer.h"
#include "OpenGLHelpers.h"
#include "OpenGLShaderHelper.h"
#include "OpenGLTexture.h"

#include <QOffscreenSurface>
#include <QSurface>
#include <QWindow>
#include <QScreen>
#include <QOpenGLFunctions_3_0>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLTexture>
#ifdef OVITO_DEBUG
    #include <QOpenGLDebugLogger>
#endif

// Called from Application::initialize() to register the embedded Qt resource files
// when running a statically linked executable. The Qt documentation says this
// needs to be placed outside of any C++ namespace.
static void registerQtResources()
{
#ifdef OVITO_BUILD_MONOLITHIC
    Q_INIT_RESOURCE(opengl);
#endif
}

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OpenGLSceneRenderer);

/// The vendor of the OpenGL implementation in use.
QByteArray OpenGLSceneRenderer::_openGLVendor;

/// The renderer name of the OpenGL implementation in use.
QByteArray OpenGLSceneRenderer::_openGLRenderer;

/// The version string of the OpenGL implementation in use.
QByteArray OpenGLSceneRenderer::_openGLVersion;

/// The version of the OpenGL shading language supported by the system.
QByteArray OpenGLSceneRenderer::_openGLSLVersion;

/// The current surface format used by the OpenGL implementation.
QSurfaceFormat OpenGLSceneRenderer::_openglSurfaceFormat;

/// The list of extensions supported by the OpenGL implementation.
QSet<QByteArray> OpenGLSceneRenderer::_openglExtensions;

/// Indicates whether the OpenGL implementation supports geometry shaders.
bool OpenGLSceneRenderer::_openGLSupportsGeometryShaders = false;

/******************************************************************************
* Is called by OVITO to query the class for any information that should be
* included in the application's system report.
******************************************************************************/
void OpenGLSceneRenderer::OOMetaClass::querySystemInformation(QTextStream& stream, UserInterface& userInterface) const
{
    if(this == &OpenGLSceneRenderer::OOClass()) {
        OpenGLSceneRenderer::determineOpenGLInfo();

        stream << "======= OpenGL info =======" << "\n";
        const QSurfaceFormat& format = OpenGLSceneRenderer::openglSurfaceFormat();
        stream << "Vendor: " << OpenGLSceneRenderer::openGLVendor() << "\n";
        stream << "Renderer: " << OpenGLSceneRenderer::openGLRenderer() << "\n";
        stream << "Version number: " << format.majorVersion() << QStringLiteral(".") << format.minorVersion() << "\n";
        stream << "Version string: " << OpenGLSceneRenderer::openGLVersion() << "\n";
        stream << "Profile: " << (format.profile() == QSurfaceFormat::CoreProfile ? "core" : (format.profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
        stream << "Swap behavior: " << (format.swapBehavior() == QSurfaceFormat::SingleBuffer ? QStringLiteral("single buffer") : (format.swapBehavior() == QSurfaceFormat::DoubleBuffer ? QStringLiteral("double buffer") : (format.swapBehavior() == QSurfaceFormat::TripleBuffer ? QStringLiteral("triple buffer") : QStringLiteral("other")))) << "\n";
        stream << "Depth buffer size: " << format.depthBufferSize() << "\n";
        stream << "Stencil buffer size: " << format.stencilBufferSize() << "\n";
        stream << "Shading language: " << OpenGLSceneRenderer::openGLSLVersion() << "\n";
        stream << "Deprecated functions: " << (format.testOption(QSurfaceFormat::DeprecatedFunctions) ? "yes" : "no") << "\n";
        stream << "Geometry shader support: " << (OpenGLSceneRenderer::openGLSupportsGeometryShaders() ? "yes" : "no") << "\n";
        stream << "Alpha: " << format.hasAlpha() << "\n";
#if 0
        stream << "Supported extensions:\n";
        QStringList extensionList;
        for(const QByteArray& extension : OpenGLSceneRenderer::openglExtensions())
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
OpenGLSceneRenderer::OpenGLSceneRenderer(ObjectInitializationFlags flags) : SceneRenderer(flags)
{
    registerQtResources();

    // Determine which transparency rendering method has been selected by the user in the
    // application settings dialog.
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings applicationSettings;
    if(applicationSettings.value("rendering/transparency_method").toInt() == 2) {
        // Activate the Weighted Blended Order-Independent Transparency method.
        _orderIndependentTransparency = true;
    }
#endif
}

/******************************************************************************
* Determines the capabilities of the current OpenGL implementation.
******************************************************************************/
void OpenGLSceneRenderer::determineOpenGLInfo()
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

/******************************************************************************
* Renders a single frame.
******************************************************************************/
void OpenGLSceneRenderer::renderFrame(std::shared_ptr<const FrameGraph> frameGraph, const QRect& viewportRect, std::shared_ptr<FrameBuffer> frameBuffer, std::shared_ptr<ObjectPickingIdentifierMap> pickingIdentifierMap)
{
    // Make sure we have a valid cache frame set for the resource manager during this render pass.
    OVITO_ASSERT(hasCurrentResourceFrame());

    // The OpenGL renderer can only render to a GL framebuffer.
    if(frameBuffer)
        throw RendererException(tr("The OpenGLSceneRenderer cannot directly render into a FrameBuffer."));

    // Store a pointer internally.
    _objectPickingIdentifierMap = pickingIdentifierMap.get();

    // Convert viewport rect from logical device coordinates to OpenGL framebuffer coordinates.
    _viewportRect = QRect(viewportRect.x() * multisamplingLevel(), viewportRect.y() * multisamplingLevel(), viewportRect.width() * multisamplingLevel(), viewportRect.height() * multisamplingLevel());

    // Adopt viewport projection parameters.
    _projParams = frameGraph->projectionParams();

    // OpenGL rendering requires a Qt GUI application.
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        throw RendererException(tr(
                "OVITO's OpenGLRenderer cannot be used in headless mode, that is if the application is running without access to a graphics environment. "
                "Please use a different rendering backend or see https://docs.ovito.org/python/modules/ovito_vis.html#ovito.vis.OpenGLRenderer for instructions "
                "on how to enable OpenGL rendering in Python script environments."));
    }

    // Are we rendering the contents of an interactive viewport window?
    _isInteractive = frameGraph->isInteractive();

    // Get the GL context being used for the current rendering pass.
    _glcontext = QOpenGLContext::currentContext();
    if(!_glcontext)
        throw RendererException(tr("Cannot render scene: There is no active OpenGL context"));
    _glcontextGroup = _glcontext->shareGroup();
    _glsurface = _glcontext->surface();
    OVITO_ASSERT(_glsurface != nullptr);

    // Check OpenGL version.
    if(_glcontext->format().majorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MAJOR || (_glcontext->format().majorVersion() == OVITO_OPENGL_MINIMUM_VERSION_MAJOR && _glcontext->format().minorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MINOR)) {
        throw RendererException(tr(
                "The OpenGL implementation available on this system does not support OpenGL version %4.%5 or newer.\n\n"
                "Ovito requires modern graphics hardware to accelerate 3d rendering. You current system configuration is not compatible with Ovito.\n\n"
                "To avoid this error message, please install the newest graphics driver, or upgrade your graphics card.\n\n"
                "The currently installed OpenGL graphics driver reports the following information:\n\n"
                "OpenGL Vendor: %1\n"
                "OpenGL Renderer: %2\n"
                "OpenGL Version: %3\n\n"
                "Ovito requires OpenGL version %4.%5 or higher.")
                .arg(QString(OpenGLSceneRenderer::openGLVendor()))
                .arg(QString(OpenGLSceneRenderer::openGLRenderer()))
                .arg(QString(OpenGLSceneRenderer::openGLVersion()))
                .arg(OVITO_OPENGL_MINIMUM_VERSION_MAJOR)
                .arg(OVITO_OPENGL_MINIMUM_VERSION_MINOR));
    }

    // Prepare a functions table allowing us to call OpenGL functions in a platform-independent way.
    initializeOpenGLFunctions();
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Obtain surface format.
    OVITO_REPORT_OPENGL_ERRORS(this);
    _glformat = _glcontext->format();

    // Get the OpenGL version.
    _glversion = QT_VERSION_CHECK(_glformat.majorVersion(), _glformat.minorVersion(), 0);

#ifdef OVITO_DEBUG
//  _glversion = QT_VERSION_CHECK(4, 1, 0);
//  _glversion = QT_VERSION_CHECK(3, 2, 0);
//  _glversion = QT_VERSION_CHECK(3, 1, 0);
//  _glversion = QT_VERSION_CHECK(2, 1, 0);

    // Initialize debug logger.
    if(_glformat.testOption(QSurfaceFormat::DebugContext)) {
        QOpenGLDebugLogger* logger = _glcontext->findChild<QOpenGLDebugLogger*>();
        if(!logger) {
            logger = new QOpenGLDebugLogger(_glcontext);
            QObject::connect(logger, &QOpenGLDebugLogger::messageLogged, [](const QOpenGLDebugMessage& debugMessage) {
                qDebug() << debugMessage;
            });
        }
        logger->initialize();
        logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        logger->enableMessages();
    }
#endif

    // Get optional function pointers.
    glMultiDrawArrays = reinterpret_cast<void (QOPENGLF_APIENTRY *)(GLenum, const GLint*, const GLsizei*, GLsizei)>(_glcontext->getProcAddress("glMultiDrawArrays"));
    glMultiDrawArraysIndirect = reinterpret_cast<void (QOPENGLF_APIENTRY *)(GLenum, const void*, GLsizei, GLsizei)>(_glcontext->getProcAddress("glMultiDrawArraysIndirect"));
#ifndef Q_OS_WASM
    OVITO_ASSERT(glMultiDrawArrays); // glMultiDrawArrays() should always be available in desktop OpenGL 2.0+.
#endif

    // Set up a vertex array object (VAO). An active VAO is required during rendering according to the OpenGL 3.2 core profile.
    std::optional<QOpenGLVertexArrayObject> vertexArrayObject;
    if(glformat().majorVersion() >= 3) {
        vertexArrayObject.emplace();
        OVITO_CHECK_OPENGL(this, vertexArrayObject->create());
        OVITO_CHECK_OPENGL(this, vertexArrayObject->bind());
    }
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Put the GL context into its default initial state before rendering a frame begins.
    OVITO_CHECK_OPENGL(this, this->glDisable(GL_STENCIL_TEST));
    OVITO_CHECK_OPENGL(this, this->glDisable(GL_BLEND));
    OVITO_CHECK_OPENGL(this, this->glEnable(GL_DEPTH_TEST));
    OVITO_CHECK_OPENGL(this, this->glDepthFunc(GL_LESS));
    OVITO_CHECK_OPENGL(this, this->glDepthRangef(0, 1));
    OVITO_CHECK_OPENGL(this, this->glClearDepthf(1));
    OVITO_CHECK_OPENGL(this, this->glDepthMask(GL_TRUE));
    OVITO_CHECK_OPENGL(this, this->glDisable(GL_SCISSOR_TEST));

    // Set up OpenGL render viewport.
    OVITO_CHECK_OPENGL(this, this->glViewport(_viewportRect.x(), _viewportRect.y(), _viewportRect.width(), _viewportRect.height()));

    // Clear frame buffer.
    if(!isPickingPass()) {
        OVITO_CHECK_OPENGL(this, this->glClearColor(frameGraph->clearColor().r(), frameGraph->clearColor().g(), frameGraph->clearColor().b(), frameGraph->clearColor().a()));
    }
    else {
        OVITO_CHECK_OPENGL(this, this->glClearColor(0, 0, 0, 0));
#if 1
        // Object IDs start at 1 when rendering for object picking.
        constexpr quint32 startObjectID = 1;
#else
        // This can be enabled during debugging to avoid alpha!=1 pixels in the picking render buffer.
        constexpr quint32 startObjectID = 0xEF000000;
#endif
        // Reset the object IDs assigned to the object picking groups.
        objectPickingIdentifierMap()->prepare(*frameGraph, startObjectID);
    }
    OVITO_CHECK_OPENGL(this, this->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Render 2d background graphics.
    _isTransparencyPass = false;
    renderFrameGraph(*frameGraph, FrameGraph::RenderLayer::UnderLayer);

    // Render opaque 3d geometry.
    if(renderFrameGraph(*frameGraph, FrameGraph::RenderLayer::SceneLayer)) {

        // Render translucent 3d geometry in a second pass.
        renderTransparentGeometry(*frameGraph);
    }

    // Render foreground graphics.
    renderFrameGraph(*frameGraph, FrameGraph::RenderLayer::OverLayer);

#ifdef OVITO_DEBUG
    // Stop debug logger.
    if(_glcontext) {
        if(QOpenGLDebugLogger* logger = _glcontext->findChild<QOpenGLDebugLogger*>()) {
            logger->stopLogging();
        }
    }
#endif
    _glcontext = nullptr;
}

/******************************************************************************
* Renders all semi-transparent geometry in a second rendering pass.
******************************************************************************/
void OpenGLSceneRenderer::renderTransparentGeometry(const FrameGraph& frameGraph)
{
    // Semi-transparent geomertry should never get rendered in a picking render pass.
    OVITO_ASSERT(!isPickingPass());

    // Prepare for order-independent transparency pass.
    if(orderIndependentTransparency()) {
        // Implementation of the "Weighted Blended Order-Independent Transparency" method.

        if(!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit())
            throw RendererException(tr("Your OpenGL graphics driver does not support framebuffer blit operations needed for order-independent transparency."));
        if(!openGLFeatures().testFlag(QOpenGLFunctions::MultipleRenderTargets))
            throw RendererException(tr("Your OpenGL graphics driver does not support multiple render targets, which are required for order-independent transparency."));

        // Create additional offscreen OpenGL framebuffer.
        if(!_oitFramebuffer || !_oitFramebuffer->isValid() || _oitFramebuffer->size() != viewportRect().size()) {
            QOpenGLFramebufferObjectFormat framebufferFormat;
            framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            framebufferFormat.setInternalTextureFormat(GL_RGBA16F);
            _oitFramebuffer = std::make_unique<QOpenGLFramebufferObject>(viewportRect().size(), framebufferFormat);
            _oitFramebuffer->addColorAttachment(_oitFramebuffer->size(), GL_R16F);
        }

        // Clear OpenGL error state and verify validity of framebuffer.
        while(this->glGetError() != GL_NO_ERROR);
        if(!_oitFramebuffer->isValid())
            throw RendererException(tr("Failed to create offscreen OpenGL framebuffer object for order-independent transparency."));

        // Bind OpenGL framebuffer.
        if(!_oitFramebuffer->bind())
            throw RendererException(tr("Failed to bind OpenGL framebuffer object for order-independent transparency."));

        // Render to the two output textures simultaneously.
        constexpr GLenum drawBuffersList[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        OVITO_CHECK_OPENGL(this, this->glDrawBuffers(2, drawBuffersList));

        // Clear the contents of the OIT buffer.
        OVITO_CHECK_OPENGL(this, this->glClearColor(0, 0, 0, 1));
        OVITO_CHECK_OPENGL(this, this->glClear(GL_COLOR_BUFFER_BIT));

        // Blit depth buffer from primary FBO to transparency FBO.
        OVITO_CHECK_OPENGL(this, this->glBindFramebuffer(GL_READ_FRAMEBUFFER, _primaryFramebuffer));
        OVITO_CHECK_OPENGL(this, this->glBlitFramebuffer(0, 0, _oitFramebuffer->width(), _oitFramebuffer->height(), 0, 0, _oitFramebuffer->width(), _oitFramebuffer->height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST));
        OVITO_CHECK_OPENGL(this, this->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));

        // Disable writing to the depth buffer.
        OVITO_CHECK_OPENGL(this, this->glDepthMask(GL_FALSE));

        // Enable blending.
        OVITO_CHECK_OPENGL(this, this->glEnable(GL_BLEND));
        OVITO_CHECK_OPENGL(this, this->glBlendEquation(GL_FUNC_ADD));
        OVITO_CHECK_OPENGL(this, this->glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA));
    }

    _isTransparencyPass = true;
    renderFrameGraph(frameGraph, FrameGraph::RenderLayer::SceneLayer);
    _isTransparencyPass = false;

    if(orderIndependentTransparency()) {

        // Switch back to the primary rendering buffer.
        OVITO_CHECK_OPENGL(this, this->glBindFramebuffer(GL_FRAMEBUFFER, _primaryFramebuffer));
        constexpr GLenum drawBuffersList[] = { GL_COLOR_ATTACHMENT0 };
        this->glDrawBuffers(1, drawBuffersList);

        OVITO_ASSERT(this->glIsEnabled(GL_BLEND));
        OVITO_CHECK_OPENGL(this, this->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE));

        // Perform 2D compositing step.
        this->glDisable(GL_DEPTH_TEST);

        // Activate the OpenGL shader program for drawing a screen-filling quad.
        OpenGLShaderHelper shader(this);
        shader.load("oit_compose", "image/oit_compose.vert", "image/oit_compose.frag");
        shader.setVerticesPerInstance(4);
        shader.setInstanceCount(1);

        // Bind the OIT framebuffer as textures.
        QVector<GLuint> textureIds = _oitFramebuffer->textures();
        OVITO_ASSERT(textureIds.size() == 2);
        OVITO_CHECK_OPENGL(this, this->glActiveTexture(GL_TEXTURE0));
        OVITO_CHECK_OPENGL(this, this->glBindTexture(GL_TEXTURE_2D, textureIds[0]));
        shader.setUniformValue("accumulationTex", 0);
        OVITO_CHECK_OPENGL(this, this->glActiveTexture(GL_TEXTURE1));
        OVITO_CHECK_OPENGL(this, this->glBindTexture(GL_TEXTURE_2D, textureIds[1]));
        shader.setUniformValue("revealageTex", 1);
        OVITO_CHECK_OPENGL(this, this->glActiveTexture(GL_TEXTURE0));

        // Draw a quad with 4 vertices.
        shader.draw(GL_TRIANGLE_STRIP);

        this->glBindTexture(GL_TEXTURE_2D, 0);
        this->glDepthMask(GL_TRUE);
        this->glDisable(GL_BLEND);
        this->glEnable(GL_DEPTH_TEST);
    }
}

/******************************************************************************
* Executes the rendering commands stored in the given frame graph.
******************************************************************************/
bool OpenGLSceneRenderer::renderFrameGraph(const FrameGraph& frameGraph, FrameGraph::RenderLayer renderLayer)
{
    bool hasTransparentGeometry = false;

    for(const FrameGraph::RenderingCommand& command : frameGraph.commands()) {

        // Skip commands that are not part of the current render layer.
        if(command.renderLayer() != renderLayer)
            continue;

        // Skip commands that are not relevant for the current rendering pass.
        if(isPickingPass()) {
            if(command.skipInPickingPass())
                continue;
        }
        else {
            if(command.skipInVisualPass())
                continue;
        }

        // Enable/disable depth testing as needed.
        if(command.noDepthTesting()) {
            OVITO_CHECK_OPENGL(this, glDisable(GL_DEPTH_TEST));
        }
        else {
            OVITO_CHECK_OPENGL(this, glEnable(GL_DEPTH_TEST));
        }

        // Set up the model-view transformation matrix.
        if(command.modelWorldTM() != AffineTransformation::Zero()) {
            _preprojectedCoordinates = false;
            _modelViewTM = projParams().viewMatrix * command.modelWorldTM();
        }
        else {
            _preprojectedCoordinates = true;
            _modelViewTM.setZero();
        }

        if(const ParticlePrimitive* primitive = dynamic_cast<const ParticlePrimitive*>(command.primitive())) {
            hasTransparentGeometry |= renderParticles(*primitive, command.pickingGroupId());
        }
        else if(const CylinderPrimitive* primitive = dynamic_cast<const CylinderPrimitive*>(command.primitive())) {
            hasTransparentGeometry |= renderCylinders(*primitive, command.pickingGroupId());
        }
        else if(const MeshPrimitive* primitive = dynamic_cast<const MeshPrimitive*>(command.primitive())) {
            hasTransparentGeometry |= renderMesh(*primitive, command.pickingGroupId());
        }
        else if(!isTransparencyPass()) {
            if(const LinePrimitive* primitive = dynamic_cast<const LinePrimitive*>(command.primitive())) {
                renderLinesImplementation(*primitive, command.pickingGroupId());
            }
            else if(const ImagePrimitive* primitive = dynamic_cast<const ImagePrimitive*>(command.primitive())) {
                renderImageImplementation(*primitive);
            }
            else if(const MarkerPrimitive* primitive = dynamic_cast<const MarkerPrimitive*>(command.primitive())) {
                renderMarkersImplementation(*primitive, command.pickingGroupId());
            }
        }
        OVITO_REPORT_OPENGL_ERRORS(this);
    }
    return hasTransparentGeometry;
}

/******************************************************************************
* Renders a particles primitive.
******************************************************************************/
bool OpenGLSceneRenderer::renderParticles(const ParticlePrimitive& primitive, int pickingGroupID)
{
    // Render particles immediately if they are all fully opaque. Otherwise defer rendering to a later time.
    if(isPickingPass() || isTransparencyPass() != (!primitive.transparencies())) {
        renderParticlesImplementation(primitive, pickingGroupID);
        return false;
    }
    else {
        if(orderIndependentTransparency() && primitive.transparencies()) {
            // The order-independent transparency method does not support fully opaque geometry (transparency=0) very well.
            // Any such geometry still appears translucent and does not fully occlude the objects behind it. To mitigate the problem,
            // we render the fully opaque geometry already during the first rendering pass to fill the z-buffer.
            struct OpaqueParticlesCache {
                ConstDataBufferPtr opaqueIndices;
                bool initialized = false;
            };
            auto& cache = currentResourceFrame().lookup<OpaqueParticlesCache>(
                RendererResourceKey<struct OpaqueParticlesCacheKey, ConstDataBufferPtr, ConstDataBufferPtr>(primitive.transparencies(), primitive.indices()));
            if(!cache.initialized) {
                cache.initialized = true;
                // Are there any particles having a non-positive transparency value?
                std::vector<int32_t> fullyOpaqueIndices;
                if(!primitive.indices()) {
                    int index = 0;
                    for(FloatType t : BufferReadAccess<GraphicsFloatType>(primitive.transparencies())) {
                        if(t <= 0) fullyOpaqueIndices.push_back(index);
                        index++;
                    }
                }
                else {
                    BufferReadAccess<GraphicsFloatType> transparencies(primitive.transparencies());
                    for(auto index : BufferReadAccess<int32_t>(primitive.indices())) {
                        if(transparencies[index] <= 0) fullyOpaqueIndices.push_back(index);
                    }
                }
                if(!fullyOpaqueIndices.empty()) {
                    cache.opaqueIndices = BufferFactory<int32_t>(fullyOpaqueIndices.begin(), fullyOpaqueIndices.end()).take();
                }
            }
            if(cache.opaqueIndices) {
                ParticlePrimitive opaqueParticles = primitive;
                opaqueParticles.setTransparencies({});
                opaqueParticles.setIndices(cache.opaqueIndices);
                renderParticlesImplementation(opaqueParticles, pickingGroupID);
            }
        }
        return true;
    }
}

/******************************************************************************
* Renders a cylinders primitive.
******************************************************************************/
bool OpenGLSceneRenderer::renderCylinders(const CylinderPrimitive& primitive, int pickingGroupID)
{
    // Render primitives immediately if they are all fully opaque. Otherwise defer rendering to a later time.
    if(isPickingPass() || isTransparencyPass() != (!primitive.transparencies())) {
        renderCylindersImplementation(primitive, pickingGroupID);
        return false;
    }
    return true;
}

/******************************************************************************
* Renders a triangle mesh primitive.
******************************************************************************/
bool OpenGLSceneRenderer::renderMesh(const MeshPrimitive& primitive, int pickingGroupID)
{
    // Render mesh immediately if it is fully opaque. Otherwise defer rendering to a later time.
    if(isPickingPass() || isTransparencyPass() != primitive.isFullyOpaque()) {
        renderMeshImplementation(primitive, pickingGroupID);
        return false;
    }
    return true;
}

/******************************************************************************
* Loads an OpenGL shader program.
******************************************************************************/
QOpenGLShaderProgram* OpenGLSceneRenderer::loadShaderProgram(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile)
{
    QOpenGLContextGroup* contextGroup = QOpenGLContextGroup::currentContextGroup();
    OVITO_ASSERT(contextGroup);

    OVITO_ASSERT(QThread::currentThread() == contextGroup->thread());
    OVITO_ASSERT(QOpenGLShaderProgram::hasOpenGLShaderPrograms());
    OVITO_ASSERT(QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Vertex));
    OVITO_ASSERT(QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Fragment));

    // Are we doing the transparency pass for "Weighted Blended Order-Independent Transparency"?
    bool isWBOITPass = (isTransparencyPass() && orderIndependentTransparency());

    // Compile a modified version of each shader for the transparency pass.
    // This is accomplished by giving the shader a unique identifier.
    QString mangledId = id;
    if(isWBOITPass)
        mangledId += QStringLiteral(".wboi_transparency");

    // Each OpenGL shader is only created once per OpenGL context group.
    std::unique_ptr<QOpenGLShaderProgram> program(contextGroup->findChild<QOpenGLShaderProgram*>(mangledId));
    if(program)
        return program.release();

    // The program's source code hasn't been compiled so far. Do it now and cache the shader program.
    program = std::make_unique<QOpenGLShaderProgram>();
    program->setObjectName(mangledId);

    // Load and compile vertex shader source.
    loadShader(program.get(), QOpenGLShader::Vertex, vertexShaderFile, isWBOITPass);

    // Load and compile fragment shader source.
    loadShader(program.get(), QOpenGLShader::Fragment, fragmentShaderFile, isWBOITPass);

    // Load and compile geometry shader source.
    if(!geometryShaderFile.isEmpty()) {
        loadShader(program.get(), QOpenGLShader::Geometry, geometryShaderFile, isWBOITPass);
    }

    // Make the shader program a child object of the GL context group.
    program->setParent(contextGroup);
    OVITO_ASSERT(contextGroup->findChild<QOpenGLShaderProgram*>(mangledId));

    // Compile the shader program.
    if(!program->link()) {
        RendererException ex(QString("The OpenGL shader program %1 failed to link.").arg(mangledId));
        ex.appendDetailMessage(program->log());
        throw ex;
    }

    OVITO_REPORT_OPENGL_ERRORS(this);

    return program.release();
}

/******************************************************************************
* Loads and compiles a GLSL shader and adds it to the given program object.
******************************************************************************/
void OpenGLSceneRenderer::loadShader(QOpenGLShaderProgram* program, QOpenGLShader::ShaderType shaderType, const QString& filename, bool isWBOITPass)
{
    QByteArray shaderSource;
    bool isGLES = QOpenGLContext::currentContext()->isOpenGLES();
    int glslVersion = 0;

    // Insert GLSL version string at the top.
    // Pick GLSL language version based on current OpenGL version.
    if(!isGLES) {
        // Inject GLSL version directive into shader source.
        if(_glversion >= QT_VERSION_CHECK(3, 3, 0)) {
            shaderSource.append("#version 330\n");
            glslVersion = QT_VERSION_CHECK(3, 3, 0);
        }
        else if(shaderType == QOpenGLShader::Geometry || _glversion >= QT_VERSION_CHECK(3, 2, 0)) {
            shaderSource.append("#version 150\n");
            glslVersion = QT_VERSION_CHECK(1, 5, 0);
        }
        else if(_glversion >= QT_VERSION_CHECK(3, 1, 0)) {
            shaderSource.append("#version 140\n");
            glslVersion = QT_VERSION_CHECK(1, 4, 0);
        }
        else if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
            shaderSource.append("#version 130\n");
            glslVersion = QT_VERSION_CHECK(1, 3, 0);
        }
        else {
            shaderSource.append("#version 120\n");
            glslVersion = QT_VERSION_CHECK(1, 2, 0);
        }
    }
    else {
        // Using OpenGL ES context.
        // Inject GLSL version directive into shader source.
        if(glformat().majorVersion() >= 3) {
            shaderSource.append("#version 300 es\n");
            glslVersion = QT_VERSION_CHECK(3, 0, 0);
        }
        else {
            glslVersion = QT_VERSION_CHECK(1, 2, 0);
            shaderSource.append("precision highp float;\n");

            if(shaderType == QOpenGLShader::Fragment) {
                // OpenGL ES 2.0 has no built-in support for gl_FragDepth.
                // Need to request EXT_frag_depth extension in such a case.
                shaderSource.append("#extension GL_EXT_frag_depth : enable\n");
                // Computation of local normal vectors in fragment shaders requires GLSL
                // derivative functions dFdx, dFdy.
                shaderSource.append("#extension GL_OES_standard_derivatives : enable\n");
            }

            // Provide replacements of some missing GLSL functions in OpenGL ES Shading Language.
            shaderSource.append("mat3 transpose(in mat3 tm) {\n");
            shaderSource.append("    vec3 i0 = tm[0];\n");
            shaderSource.append("    vec3 i1 = tm[1];\n");
            shaderSource.append("    vec3 i2 = tm[2];\n");
            shaderSource.append("    mat3 out_tm = mat3(\n");
            shaderSource.append("         vec3(i0.x, i1.x, i2.x),\n");
            shaderSource.append("         vec3(i0.y, i1.y, i2.y),\n");
            shaderSource.append("         vec3(i0.z, i1.z, i2.z));\n");
            shaderSource.append("    return out_tm;\n");
            shaderSource.append("}\n");
        }
    }

    if(_glversion < QT_VERSION_CHECK(3, 0, 0)) {
        // This is needed to emulate the special shader variables 'gl_VertexID' and 'gl_InstanceID' in GLSL 1.20:
        if(shaderType == QOpenGLShader::Vertex) {
            // Note: Data type 'float' is used for the vertex attribute, because some OpenGL implementation have poor support for integer
            // vertex attributes.
            shaderSource.append("attribute float vertexID;\n");
            shaderSource.append("uniform int vertices_per_instance;\n");
        }
    }
    else if(!useInstancedArrays()) {
        // This is needed to compute the special shader variable 'gl_VertexID' when instanced arrays are not supported:
        if(shaderType == QOpenGLShader::Vertex) {
            shaderSource.append("uniform int vertices_per_instance;\n");
        }
    }

    if(!isWBOITPass) {
        // Declare the fragment color output variable referenced by the <fragColor> placeholder.
        if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
            if(shaderType == QOpenGLShader::Fragment) {
                shaderSource.append("out vec4 fragColor;\n");
            }
        }
    }
    else {
        // Declare the fragment output variables referenced by the <fragAccumulation> and <fragRevealage> placeholders.
        if(shaderType == QOpenGLShader::Fragment) {
            if(glslVersion >= QT_VERSION_CHECK(3, 0, 0)) {
                if(glslVersion >= QT_VERSION_CHECK(3, 3, 0)) {
                    shaderSource.append("layout(location = 0) out vec4 fragAccumulation;\n");
                    shaderSource.append("layout(location = 1) out float fragRevealage;\n");
                }
                else {
                    shaderSource.append("out vec4 fragAccumulation;\n");
                    shaderSource.append("out float fragRevealage;\n");
                    if(QOpenGLFunctions_3_0* glfunc30 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_0>(glcontext())) {
                        OVITO_CHECK_OPENGL(this, glfunc30->glBindFragDataLocation(program->programId(), 0, "fragAccumulation"));
                        OVITO_CHECK_OPENGL(this, glfunc30->glBindFragDataLocation(program->programId(), 1, "fragRevealage"));
                    }
                    else qWarning() << "WARNING: Could not resolve OpenGL 3.0 API functions.";
                }
            }
        }
    }

    // Helper function that appends a source code line to the buffer after preprocessing it.
    auto preprocessShaderLine = [&](QByteArray& line) {
        if(_glversion < QT_VERSION_CHECK(3, 0, 0)) {
            // Automatically back-port shader source code to make it compatible with OpenGL 2.1 (GLSL 1.20):
            if(shaderType == QOpenGLShader::Vertex) {
                if(line.startsWith("in ")) line = QByteArrayLiteral("attribute") + line.mid(2);
                else if(line.startsWith("out ")) line = QByteArrayLiteral("varying") + line.mid(3);
                else if(line.startsWith("flat out ")) line = QByteArrayLiteral("varying") + line.mid(8);
                else {
                    if(!isGLES) {
                        line.replace("float(objectID & 0xFF)", "floor(mod(objectID, 256.0))");
                        line.replace("float((objectID >> 8) & 0xFF)", "floor(mod(objectID / 256.0, 256.0))");
                        line.replace("float((objectID >> 16) & 0xFF)", "floor(mod(objectID / 65536.0, 256.0))");
                        line.replace("float((objectID >> 24) & 0xFF)", "floor(mod(objectID / 16777216.0, 256.0))");
                    }
                    else {
                        line.replace("float(objectID & 0xFF)", "floor(mod(float(objectID), 256.0))");
                        line.replace("float((objectID >> 8) & 0xFF)", "floor(mod(float(objectID) / 256.0, 256.0))");
                        line.replace("float((objectID >> 16) & 0xFF)", "floor(mod(float(objectID) / 65536.0, 256.0))");
                        line.replace("float((objectID >> 24) & 0xFF)", "floor(mod(float(objectID) / 16777216.0, 256.0))");
                    }
                }
            }
            else if(shaderType == QOpenGLShader::Fragment) {
                if(line.startsWith("in ")) line = QByteArrayLiteral("varying") + line.mid(2);
                else if(line.startsWith("flat in ")) line = QByteArrayLiteral("varying") + line.mid(7);
                else if(line.startsWith("out ")) return;
            }
        }

        if(!isWBOITPass) {
            // Writing to the fragment color output variable.
            if(_glversion < QT_VERSION_CHECK(3, 0, 0))
                line.replace("<fragColor>", "gl_FragColor");
            else
                line.replace("<fragColor>", "fragColor");
        }
        else {
            if(glslVersion < QT_VERSION_CHECK(3, 0, 0))
                line.replace("<fragAccumulation>", "gl_FragData[0]");
            else
                line.replace("<fragAccumulation>", "fragAccumulation");

            if(glslVersion < QT_VERSION_CHECK(3, 0, 0))
                line.replace("<fragRevealage>", "gl_FragData[1].r");
            else
                line.replace("<fragRevealage>", "fragRevealage");
        }

        // Writing to the fragment depth output variable.
        if(_glversion >= QT_VERSION_CHECK(3, 0, 0) || isGLES == false)
            line.replace("<fragDepth>", "gl_FragDepth");
        else if(line.contains("<fragDepth>")) { // For GLES2:
            line.replace("<fragDepth>", "gl_FragDepthEXT");
            line.prepend(QByteArrayLiteral("#if defined(GL_EXT_frag_depth)\n"));
            line.append(QByteArrayLiteral("#endif\n"));
        }

        // Old GLSL versions do not provide an inverse() function for mat3 matrices.
        // Replace calls to the inverse() function with a custom implementation.
        if(_glversion < QT_VERSION_CHECK(3, 3, 0))
            line.replace("<inverse_mat3>", "inverse_mat3"); //  Emulate inverse(mat3) with own function.
        else
            line.replace("<inverse_mat3>", "inverse"); // inverse(mat3) is natively supported.

        // The per-instance vertex ID.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<VertexID>", "int(mod(vertexID + 0.5, float(vertices_per_instance)))"); // gl_VertexID is not available, requires a VBO with explicit vertex IDs
        else if(!useInstancedArrays())
            line.replace("<VertexID>", "(gl_VertexID % vertices_per_instance)"); // gl_VertexID is available but no instanced arrays.
        else
            line.replace("<VertexID>", "gl_VertexID"); // gl_VertexID is fully supported.

        // The instance ID.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<InstanceID>", "(int(vertexID) / vertices_per_instance)"); // Compute the instance ID from the running vertex index, which is read from a VBO array.
        else if(!useInstancedArrays())
            line.replace("<InstanceID>", "(gl_VertexID / vertices_per_instance)"); // Compute the instance ID from the running vertex index.
        else
            line.replace("<InstanceID>", "gl_InstanceID"); // gl_InstanceID is fully supported.

        // 1-D texture sampler.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<texture1D>", "texture1D");
        else
            line.replace("<texture1D>", "texture");

        // 2-D texture sampler.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<texture2D>", "texture2D");
        else
            line.replace("<texture2D>", "texture");

        // View ray calculation in vertex and geometry shaders.
        if(line.contains("<calculate_view_ray_through_vertex>")) {
            if(_glversion >= QT_VERSION_CHECK(3, 0, 0))
                line.replace("<calculate_view_ray_through_vertex>", "calculate_view_ray_through_vertex()");
            else
                return; // Skip view ray calculation in vertex/geometry shader and let the fragement shader do the full calculation for each fragment.
        }

        // View ray calculation in fragment shaders.
        if(line.contains("<calculate_view_ray_through_fragment>")) {
            if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
                // Calculate view ray based on interpolated values coming from the vertex shader.
                line.replace("<calculate_view_ray_through_fragment>", "vec3 ray_dir_norm = normalize(ray_dir);");
            }
            else {
                // Perform full view ray computation in the fragment shader's main function.
                line.replace("<calculate_view_ray_through_fragment>",
                    "vec2 viewport_position = ((gl_FragCoord.xy - viewport_origin) * inverse_viewport_size) - 1.0;\n"
                    "vec4 _near = inverse_projection_matrix * vec4(viewport_position, -1.0, 1.0);\n"
                    "vec4 _far = _near + inverse_projection_matrix[2];\n"
                    "vec3 ray_origin = _near.xyz / _near.w;\n"
                    "vec3 ray_dir_norm = normalize(_far.xyz / _far.w - ray_origin);\n");
            }
        }

        // Flat surface normal calculation in vertex and geometry shaders.
        if(line.contains("<flat_normal.output>")) {
            if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
                line.replace("<flat_normal.output>", "flat_normal_fs"); // Note: "flat_normal_fs" is defined in "flat_normal.vert".
            }
            else {
                // Pass view-space coordinates of vertex to fragment shader as texture coordintes.
                if(!isGLES) line = "gl_TexCoord[1] = inverse_projection_matrix * gl_Position;\n";
                else line = "tex_coords = (inverse_projection_matrix * gl_Position).xyz;\n";
            }
        }

        // Flat surface normal calculation in fragment shaders.
        if(line.contains("<flat_normal.input>")) {
            if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
                line.replace("<flat_normal.input>", "flat_normal_fs"); // Note: "flat_normal_fs" is defined in "flat_normal.frag".
            }
            else {
                // Calculate surface normal from cross product of UV tangents.
                line.replace("<flat_normal.input>",
                    !isGLES ? "normalize(cross(dFdx(gl_TexCoord[1].xyz), dFdy(gl_TexCoord[1].xyz))"
                        : "normalize(cross(dFdx(tex_coords), dFdy(tex_coords))");
            }
        }

        shaderSource.append(line);
    };

    // Load actual shader source code.
    QFile shaderSourceFile(filename);
    if(!shaderSourceFile.open(QFile::ReadOnly))
        throw RendererException(QString("Unable to open shader source file %1.").arg(filename));

    // Parse each line of the shader file and process #include directives.
    while(!shaderSourceFile.atEnd()) {
        QByteArray line = shaderSourceFile.readLine();
        if(line.startsWith("#include")) {
            QString includeFilePath;

            // Special include statement which require preprocessing.
            if(line.contains("<shading.frag>")) {
                if(!isWBOITPass) includeFilePath = QStringLiteral(":/openglrenderer/glsl/shading.frag");
                else includeFilePath = QStringLiteral(":/openglrenderer/glsl/shading_transparency.frag");
            }
            else if(line.contains("<view_ray.vert>")) {
                if(_glversion < QT_VERSION_CHECK(3, 0, 0)) continue; // Skip this include file, because view ray calculation is performed by the fragment shaders in old GLSL versions.
                includeFilePath = QStringLiteral(":/openglrenderer/glsl/view_ray.vert");
            }
            else if(line.contains("<view_ray.frag>")) {
                if(_glversion < QT_VERSION_CHECK(3, 0, 0)) continue; // Skip this include file, because view ray calculation is performed by the fragment shaders in old GLSL versions.
                includeFilePath = QStringLiteral(":/openglrenderer/glsl/view_ray.frag");
            }
            else if(line.contains("<flat_normal.vert>")) {
                if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.vert");
                else if(isGLES) includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.GLES.vert");
                else continue;
            }
            else if(line.contains("<flat_normal.frag>")) {
                if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.frag");
                else if(isGLES) includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.GLES.frag");
                else continue;
            }
            else {
                // Resolve relative file paths.
                QFileInfo includeFile(QFileInfo(shaderSourceFile).dir(), QString::fromUtf8(line.mid(8).replace('\"', "").trimmed()));
                includeFilePath = includeFile.filePath();
            }

            // Load the secondary shader file and insert it into the source of the primary shader.
            QFile secondarySourceFile(includeFilePath);
            if(!secondarySourceFile.open(QFile::ReadOnly))
                throw RendererException(QString("Unable to open shader source file %1 referenced by include directive in shader file %2.").arg(includeFilePath).arg(filename));
            while(!secondarySourceFile.atEnd()) {
                line = secondarySourceFile.readLine();
                preprocessShaderLine(line);
            }
            shaderSource.append('\n');
        }
        else {
            preprocessShaderLine(line);
        }
    }

    // Load and compile vertex shader source.
    if(!program->addShaderFromSourceCode(shaderType, shaderSource)) {
        RendererException ex(QString("The shader source file %1 failed to compile.").arg(filename));
        ex.appendDetailMessage(program->log());
        ex.appendDetailMessage(QStringLiteral("Problematic shader source:"));
        ex.appendDetailMessage(shaderSource);
        throw ex;
    }

    OVITO_REPORT_OPENGL_ERRORS(this);
}

#if 0 // TODO
/******************************************************************************
* Activates the special highlight rendering mode.
******************************************************************************/
void OpenGLSceneRenderer::setHighlightMode(int pass)
{
    if(pass == 1) {
        this->glEnable(GL_DEPTH_TEST);
        this->glClearStencil(0);
        this->glClear(GL_STENCIL_BUFFER_BIT);
        this->glEnable(GL_STENCIL_TEST);
        this->glStencilFunc(GL_ALWAYS, 0x1, 0x1);
        this->glStencilMask(0x1);
        this->glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM)
        // Partial workaround for a bug in the MacOS/arm64 OpenGL implementation.
        // Fragment shaders discarding fragments (via conditional "discard") still modify the stencil buffer, which is unexpected.
        // See also: https://developer.apple.com/forums/thread/721988
        this->glStencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);
#endif
        this->glDepthFunc(GL_LEQUAL);
    }
    else if(pass == 2) {
        this->glDisable(GL_DEPTH_TEST);
        this->glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
        this->glStencilMask(0x1);
        this->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    else {
        this->glDepthFunc(GL_LESS);
        this->glEnable(GL_DEPTH_TEST);
        this->glDisable(GL_STENCIL_TEST);
    }
}
#endif


/******************************************************************************
* Translates an OpenGL error code to a human-readable message string.
******************************************************************************/
static const char* openglErrorString(GLenum errorCode)
{
    switch(errorCode) {
    case GL_NO_ERROR: return "GL_NO_ERROR - No error has been recorded.";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument.";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE - A numeric argument is out of range.";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION - The specified operation is not allowed in the current state.";
    case 0x0503 /*GL_STACK_OVERFLOW*/: return "GL_STACK_OVERFLOW - This command would cause a stack overflow.";
    case 0x0504 /*GL_STACK_UNDERFLOW*/: return "GL_STACK_UNDERFLOW - This command would cause a stack underflow.";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY - There is not enough memory left to execute the command.";
    case 0x8031 /*GL_TABLE_TOO_LARGE*/: return "GL_TABLE_TOO_LARGE - The specified table exceeds the implementation's maximum supported table size.";
    case 0x0506 /*GL_INVALID_FRAMEBUFFER_OPERATION*/: return "GL_INVALID_FRAMEBUFFER_OPERATION - The read and draw framebuffers are not framebuffer complete.";
    default: return "Unknown OpenGL error code.";
    }
}

/******************************************************************************
* Reports OpenGL error status codes.
******************************************************************************/
void OpenGLSceneRenderer::checkOpenGLErrorStatus(const char* command, const char* sourceFile, int sourceLine)
{
    GLenum error;
    while((error = this->glGetError()) != GL_NO_ERROR) {
        qDebug() << "WARNING: OpenGL call" << command << "failed "
                "in line" << sourceLine << "of file" << sourceFile
                << "with error" << openglErrorString(error);
    }
}

/******************************************************************************
* Create an OpenGL texture object for a QImage.
******************************************************************************/
QOpenGLTexture* OpenGLSceneRenderer::uploadImage(const QImage& image, QOpenGLTexture::MipMapGeneration genMipMaps)
{
    OVITO_ASSERT(!image.isNull());

    // Check if this image has already been uploaded to the GPU.
    RendererResourceKey<struct ImageCache, quint64, QOpenGLContextGroup*> cacheKey{ image.cacheKey(), QOpenGLContextGroup::currentContextGroup() };
    std::unique_ptr<OpenGLTexture>& texture = currentResourceFrame().lookup<std::unique_ptr<OpenGLTexture>>(cacheKey);

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
QOpenGLTexture* OpenGLSceneRenderer::uploadColorMap(ColorCodingGradient* gradient)
{
    // Check if this color map has already been uploaded to the GPU.
    RendererResourceKey<struct ColorMapCache, OORef<ColorCodingGradient>, QOpenGLContextGroup*> cacheKey{ gradient, QOpenGLContextGroup::currentContextGroup() };
    std::unique_ptr<OpenGLTexture>& texture = currentResourceFrame().lookup<std::unique_ptr<OpenGLTexture>>(cacheKey);

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
