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
#include <ovito/core/app/Application.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/ColorCodingGradient.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ObjectPickingIdentifierMap.h>
#include "OpenGLRenderingJob.h"
#include "OpenGLRenderingFrameBuffer.h"
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

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OpenGLRenderingJob);

/******************************************************************************
 * Constructor.
 ******************************************************************************/
OpenGLRenderingJob::OpenGLRenderingJob(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache,
                                       int multisamplingLevel, bool orderIndependentTransparency)
    : RenderingJob(flags, std::move(visCache)),
      _multisamplingLevel(multisamplingLevel),
      _orderIndependentTransparency(orderIndependentTransparency)
{
}

/******************************************************************************
 * Called when this object is being destroyed.
 ******************************************************************************/
void OpenGLRenderingJob::aboutToBeDeleted()
{
    RenderingJob::aboutToBeDeleted();

    // Release all cached OpenGL resources from the last frame rendered.
    // This may require an active GL context.
    if(_currentResourceFrame) {
        OpenGLContextRestore contextRestore = activateContext();
        _currentResourceFrame = {};
    }
}

/******************************************************************************
 * Creates a new abstract target frame buffer for rendering into.
 ******************************************************************************/
OORef<AbstractRenderingFrameBuffer> OpenGLRenderingJob::createOffscreenFrameBuffer(const QRect& viewportRect,
                                                                                   const std::shared_ptr<FrameBuffer>& frameBuffer)
{
    // Creating an OpenGL framebuffer requires an active OpenGL context.
    OpenGLContextRestore contextRestore = activateContext();

    return OORef<OpenGLRenderingFrameBuffer>::create(this, viewportRect, frameBuffer);
}

/******************************************************************************
 * Renders an image of the given frame graph into the given target frame buffer.
 ******************************************************************************/
Future<void> OpenGLRenderingJob::renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<AbstractRenderingFrameBuffer> frameBuffer,
                                             std::shared_ptr<ObjectPickingIdentifierMap> pickingMap)
{
    // OpenGL rendering requires a Qt GUI application.
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        throw RendererException(
            tr("OVITO's OpenGLRenderer cannot be used in headless mode, that is if the application is running without access to a graphics "
               "environment. "
               "Please use a different rendering backend or see "
               "https://docs.ovito.org/python/modules/ovito_vis.html#ovito.vis.OpenGLRenderer for instructions "
               "on how to enable OpenGL rendering in Python script environments."));
    }

    // Rendering requires an active GL context.
    OpenGLContextRestore contextRestore = activateContext();
    _glcontext = QOpenGLContext::currentContext();
    if(!_glcontext) throw RendererException(tr("Cannot render scene: There is no active OpenGL context"));

    // Prepare a functions table allowing us to call OpenGL functions in a platform-independent way.
    initializeOpenGLFunctions();
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Bind offscreen OpenGL framebuffer for rendering.
    OORef<OpenGLRenderingFrameBuffer> glFrameBuffer = static_object_cast<OpenGLRenderingFrameBuffer>(std::move(frameBuffer));
    if(glFrameBuffer->framebufferObject() && !glFrameBuffer->framebufferObject()->bind())
        throw RendererException(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

    // Store physical framebuffer size.
    _framebufferSize = glFrameBuffer->framebufferSize();

    // Store a pointer internally.
    _frameGraph = frameGraph.get();
    _objectPickingIdentifierMap = pickingMap.get();

    // Obtain surface format.
    _glformat = _glcontext->format();
    OVITO_REPORT_OPENGL_ERRORS(this);

    // OpenGL in a VirtualBox machine Windows guest reports "2.1 Chromium 1.9" as version string, which is
    // not correctly parsed by Qt. We have to workaround this.
    QByteArray openGLVersionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    QByteArray openGLRendererString = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if(openGLVersionString.startsWith("2.1 ")) {
        _glformat.setMajorVersion(2);
        _glformat.setMinorVersion(1);
    }
    if(glformat().majorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MAJOR || (glformat().majorVersion() == OVITO_OPENGL_MINIMUM_VERSION_MAJOR &&
                                                                          glformat().minorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MINOR)) {
        throw RendererException(tr("The OpenGL graphics driver installed on this system does not support OpenGL version %6.%7 or newer.\n\n"
                                   "Ovito requires modern graphics hardware and up-to-date graphics drivers to render 3D graphics. Your "
                                   "current system configuration is not compatible with Ovito.\n\n"
                                   "To avoid this error, please install the newest graphics driver of the hardware vendor or, if "
                                   "necessary, consider replacing your graphics card with a newer model.\n\n"
                                   "The installed OpenGL graphics driver reports the following information:\n\n"
                                   "OpenGL vendor: %1\n"
                                   "OpenGL renderer: %2\n"
                                   "OpenGL version: %3.%4 (%5)\n\n"
                                   "Ovito requires at least OpenGL version %6.%7.")
                                    .arg(QString::fromUtf8(reinterpret_cast<const char*>(glGetString(GL_VENDOR))))
                                    .arg(QString::fromUtf8(openGLRendererString))
                                    .arg(glformat().majorVersion())
                                    .arg(glformat().minorVersion())
                                    .arg(QString::fromUtf8(openGLVersionString))
                                    .arg(OVITO_OPENGL_MINIMUM_VERSION_MAJOR)
                                    .arg(OVITO_OPENGL_MINIMUM_VERSION_MINOR));
    }

#ifdef Q_OS_WIN
    if(openGLRendererString == "Intel(R) HD Graphics" || openGLRendererString == "Intel(R) HD Graphics 2000" ||
       openGLRendererString == "Intel(R) HD Graphics 3000" || openGLRendererString == "Intel(R) HD Graphics 4400") {
        throw RendererException(tr("The graphics chip installed in this system is not compatible with OVITO, unfortunately.\n\n"
                                   "Intel(R) HD Graphics, an integrated graphics chip released in the years 2010/2011/2012, does not "
                                   "support the specific OpenGL functions required by OVITO. "
                                   "There is no known workaround to make OVITO work on systems with this particular graphics unit. Please "
                                   "use OVITO on a computer with a more modern graphics processor.\n\n"
                                   "Detected graphics interface:\n\n"
                                   "OpenGL vendor: %1\n"
                                   "OpenGL renderer: %2\n"
                                   "OpenGL version: %3.%4 (%5)")
                                    .arg(QString::fromUtf8(reinterpret_cast<const char*>(glGetString(GL_VENDOR))))
                                    .arg(QString::fromUtf8(openGLRendererString))
                                    .arg(glformat().majorVersion())
                                    .arg(glformat().minorVersion())
                                    .arg(QString::fromUtf8(openGLVersionString)));
    }
#endif

    // Open a new cache frame for the OpenGL resource managament.
    _currentResourceFrame = visCache()->acquireResourceFrame();

    // Get the OpenGL version.
    _glversion = QT_VERSION_CHECK(glformat().majorVersion(), glformat().minorVersion(), 0);

#ifdef OVITO_DEBUG
    //  _glversion = QT_VERSION_CHECK(4, 1, 0);
    //  _glversion = QT_VERSION_CHECK(3, 2, 0);
    //  _glversion = QT_VERSION_CHECK(3, 1, 0);
    //  _glversion = QT_VERSION_CHECK(2, 1, 0);

    // Initialize debug logger.
    if(glformat().testOption(QSurfaceFormat::DebugContext)) {
        QOpenGLDebugLogger* logger = glcontext()->findChild<QOpenGLDebugLogger*>();
        if(!logger) {
            logger = new QOpenGLDebugLogger(glcontext());
            QObject::connect(logger, &QOpenGLDebugLogger::messageLogged,
                             [](const QOpenGLDebugMessage& debugMessage) { qDebug() << debugMessage; });
        }
        logger->initialize();
        logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
        logger->enableMessages();
    }
#endif

    // Get optional function pointers.
    glMultiDrawArrays = reinterpret_cast<void(QOPENGLF_APIENTRY*)(GLenum, const GLint*, const GLsizei*, GLsizei)>(
        glcontext()->getProcAddress("glMultiDrawArrays"));
    glMultiDrawArraysIndirect = reinterpret_cast<void(QOPENGLF_APIENTRY*)(GLenum, const void*, GLsizei, GLsizei)>(
        glcontext()->getProcAddress("glMultiDrawArraysIndirect"));
#ifndef Q_OS_WASM
    OVITO_ASSERT(glMultiDrawArrays);  // glMultiDrawArrays() should always be available in desktop OpenGL 2.0+.
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
    OVITO_CHECK_OPENGL(this, this->glViewport(0, 0, framebufferSize().width(), framebufferSize().height()));

    // Clear frame buffer.
    if(!isPickingPass()) {
        OVITO_CHECK_OPENGL(this, this->glClearColor(frameGraph->clearColor().r(), frameGraph->clearColor().g(),
                                                    frameGraph->clearColor().b(), frameGraph->clearColor().a()));
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
        renderTransparentGeometry(*frameGraph, *glFrameBuffer);
    }

    // Render foreground graphics.
    renderFrameGraph(*frameGraph, FrameGraph::RenderLayer::OverLayer);

    // Store the resource cache frame in the target frame buffer object to keep OpenGL resources alive
    // for subsequent frames.
    glFrameBuffer->storePreviousResourceFrame(std::move(_currentResourceFrame));

    // Flush the contents to the FBO before extracting image.
    if(glFrameBuffer->framebufferObject()) {
        glcontext()->swapBuffers(glcontext()->surface());
    }

    // Read the rendered image from the OpenGL framebuffer and paint it into to the output frame buffer.
    if(glFrameBuffer->outputFrameBuffer() && glFrameBuffer->framebufferObject()) {
        const QRect& viewportRect = glFrameBuffer->outputViewportRect();

        // Clear destination area in the framebuffer (only necessary if OpenGL image is not fully opaque).
        FrameBuffer& outputFrameBuffer = *glFrameBuffer->outputFrameBuffer();
        if(frameGraph->clearColor().a() != 1 && !outputFrameBuffer.image().isNull())
            outputFrameBuffer.clear(frameGraph->clearColor(), viewportRect);

        // Fetch rendered image from OpenGL framebuffer.
        QImage renderedImage = glFrameBuffer->framebufferObject()->toImage();
        OVITO_ASSERT(renderedImage.size() == framebufferSize());
        // We need it in ARGB32 format for best results.
        renderedImage.reinterpretAsFormat(QImage::Format_ARGB32);
        // Rescale supersampled image to output size.
        QImage scaledImage = renderedImage.scaled(viewportRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // Transfer OpenGL image to the output frame buffer.
        if(!outputFrameBuffer.image().isNull()) {
            QPainter painter(&outputFrameBuffer.image());
            painter.drawImage(viewportRect, scaledImage,
                              QRect(0, scaledImage.height() - viewportRect.height(), viewportRect.width(), viewportRect.height()));
        }
        else {
            outputFrameBuffer.image() = scaledImage;
        }
        outputFrameBuffer.update(viewportRect);
        outputFrameBuffer.commitChanges();
    }

#ifdef OVITO_DEBUG
    // Stop debug logger.
    if(QOpenGLDebugLogger* logger = glcontext()->findChild<QOpenGLDebugLogger*>()) {
        logger->stopLogging();
    }
#endif
    _glcontext = nullptr;

    return Future<void>::createImmediateEmpty();
}

/******************************************************************************
 * Renders all semi-transparent geometry in a second rendering pass.
 ******************************************************************************/
void OpenGLRenderingJob::renderTransparentGeometry(const FrameGraph& frameGraph, OpenGLRenderingFrameBuffer& frameBuffer)
{
    // Semi-transparent geometry should never get rendered in a picking render pass.
    OVITO_ASSERT(!isPickingPass());

    // Implementation of the "Weighted Blended Order-Independent Transparency" method.
    if(orderIndependentTransparency()) frameBuffer.beginOITRendering();

    _isTransparencyPass = true;
    renderFrameGraph(frameGraph, FrameGraph::RenderLayer::SceneLayer);
    _isTransparencyPass = false;

    // Second phase of the "Weighted Blended Order-Independent Transparency" method.
    if(orderIndependentTransparency()) frameBuffer.endOITRendering();
}

/******************************************************************************
 * Executes the rendering commands stored in the given frame graph.
 ******************************************************************************/
bool OpenGLRenderingJob::renderFrameGraph(const FrameGraph& frameGraph, FrameGraph::RenderLayer renderLayer)
{
    bool hasTransparentGeometry = false;

    for(const FrameGraph::RenderingCommand& command : frameGraph.commands()) {
        // Skip commands that are not part of the current render layer.
        if(command.renderLayer() != renderLayer) continue;

        // Skip commands that are not relevant for the current rendering pass.
        if(isPickingPass()) {
            if(command.skipInPickingPass()) continue;
        }
        else {
            if(command.skipInVisualPass()) continue;
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
            _modelViewTM = frameGraph.projectionParams().viewMatrix * command.modelWorldTM();
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
bool OpenGLRenderingJob::renderParticles(const ParticlePrimitive& primitive, int pickingGroupID)
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
                RendererResourceKey<struct OpaqueParticlesCacheKey, ConstDataBufferPtr, ConstDataBufferPtr>(primitive.transparencies(),
                                                                                                            primitive.indices()));
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
bool OpenGLRenderingJob::renderCylinders(const CylinderPrimitive& primitive, int pickingGroupID)
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
bool OpenGLRenderingJob::renderMesh(const MeshPrimitive& primitive, int pickingGroupID)
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
QOpenGLShaderProgram* OpenGLRenderingJob::loadShaderProgram(const QString& id, const QString& vertexShaderFile,
                                                            const QString& fragmentShaderFile, const QString& geometryShaderFile)
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
    if(isWBOITPass) mangledId += QStringLiteral(".wboi_transparency");

    // Each OpenGL shader is only created once per OpenGL context group.
    std::unique_ptr<QOpenGLShaderProgram> program(contextGroup->findChild<QOpenGLShaderProgram*>(mangledId));
    if(program) return program.release();

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
void OpenGLRenderingJob::loadShader(QOpenGLShaderProgram* program, QOpenGLShader::ShaderType shaderType, const QString& filename,
                                    bool isWBOITPass)
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
                    else
                        qWarning() << "WARNING: Could not resolve OpenGL 3.0 API functions.";
                }
            }
        }
    }

    // Helper function that appends a source code line to the buffer after preprocessing it.
    auto preprocessShaderLine = [&](QByteArray& line) {
        if(_glversion < QT_VERSION_CHECK(3, 0, 0)) {
            // Automatically back-port shader source code to make it compatible with OpenGL 2.1 (GLSL 1.20):
            if(shaderType == QOpenGLShader::Vertex) {
                if(line.startsWith("in "))
                    line = QByteArrayLiteral("attribute") + line.mid(2);
                else if(line.startsWith("out "))
                    line = QByteArrayLiteral("varying") + line.mid(3);
                else if(line.startsWith("flat out "))
                    line = QByteArrayLiteral("varying") + line.mid(8);
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
                if(line.startsWith("in "))
                    line = QByteArrayLiteral("varying") + line.mid(2);
                else if(line.startsWith("flat in "))
                    line = QByteArrayLiteral("varying") + line.mid(7);
                else if(line.startsWith("out "))
                    return;
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
        else if(line.contains("<fragDepth>")) {  // For GLES2:
            line.replace("<fragDepth>", "gl_FragDepthEXT");
            line.prepend(QByteArrayLiteral("#if defined(GL_EXT_frag_depth)\n"));
            line.append(QByteArrayLiteral("#endif\n"));
        }

        // Old GLSL versions do not provide an inverse() function for mat3 matrices.
        // Replace calls to the inverse() function with a custom implementation.
        if(_glversion < QT_VERSION_CHECK(3, 3, 0))
            line.replace("<inverse_mat3>", "inverse_mat3");  //  Emulate inverse(mat3) with own function.
        else
            line.replace("<inverse_mat3>", "inverse");  // inverse(mat3) is natively supported.

        // The per-instance vertex ID.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<VertexID>", "int(mod(vertexID + 0.5, float(vertices_per_instance)))");  // gl_VertexID is not available, requires
                                                                                                   // a VBO with explicit vertex IDs
        else if(!useInstancedArrays())
            line.replace("<VertexID>", "(gl_VertexID % vertices_per_instance)");  // gl_VertexID is available but no instanced arrays.
        else
            line.replace("<VertexID>", "gl_VertexID");  // gl_VertexID is fully supported.

        // The instance ID.
        if(_glversion < QT_VERSION_CHECK(3, 0, 0))
            line.replace("<InstanceID>", "(int(vertexID) / vertices_per_instance)");  // Compute the instance ID from the running vertex
                                                                                      // index, which is read from a VBO array.
        else if(!useInstancedArrays())
            line.replace("<InstanceID>",
                         "(gl_VertexID / vertices_per_instance)");  // Compute the instance ID from the running vertex index.
        else
            line.replace("<InstanceID>", "gl_InstanceID");  // gl_InstanceID is fully supported.

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
                return;  // Skip view ray calculation in vertex/geometry shader and let the fragement shader do the full calculation for
                         // each fragment.
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
                line.replace("<flat_normal.output>", "flat_normal_fs");  // Note: "flat_normal_fs" is defined in "flat_normal.vert".
            }
            else {
                // Pass view-space coordinates of vertex to fragment shader as texture coordintes.
                if(!isGLES)
                    line = "gl_TexCoord[1] = inverse_projection_matrix * gl_Position;\n";
                else
                    line = "tex_coords = (inverse_projection_matrix * gl_Position).xyz;\n";
            }
        }

        // Flat surface normal calculation in fragment shaders.
        if(line.contains("<flat_normal.input>")) {
            if(_glversion >= QT_VERSION_CHECK(3, 0, 0)) {
                line.replace("<flat_normal.input>", "flat_normal_fs");  // Note: "flat_normal_fs" is defined in "flat_normal.frag".
            }
            else {
                // Calculate surface normal from cross product of UV tangents.
                line.replace("<flat_normal.input>", !isGLES ? "normalize(cross(dFdx(gl_TexCoord[1].xyz), dFdy(gl_TexCoord[1].xyz))"
                                                            : "normalize(cross(dFdx(tex_coords), dFdy(tex_coords))");
            }
        }

        shaderSource.append(line);
    };

    // Load actual shader source code.
    QFile shaderSourceFile(filename);
    if(!shaderSourceFile.open(QFile::ReadOnly)) throw RendererException(QString("Unable to open shader source file %1.").arg(filename));

    // Parse each line of the shader file and process #include directives.
    while(!shaderSourceFile.atEnd()) {
        QByteArray line = shaderSourceFile.readLine();
        if(line.startsWith("#include")) {
            QString includeFilePath;

            // Special include statement which require preprocessing.
            if(line.contains("<shading.frag>")) {
                if(!isWBOITPass)
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/shading.frag");
                else
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/shading_transparency.frag");
            }
            else if(line.contains("<view_ray.vert>")) {
                if(_glversion < QT_VERSION_CHECK(3, 0, 0))
                    continue;  // Skip this include file, because view ray calculation is performed by the fragment shaders in old GLSL
                               // versions.
                includeFilePath = QStringLiteral(":/openglrenderer/glsl/view_ray.vert");
            }
            else if(line.contains("<view_ray.frag>")) {
                if(_glversion < QT_VERSION_CHECK(3, 0, 0))
                    continue;  // Skip this include file, because view ray calculation is performed by the fragment shaders in old GLSL
                               // versions.
                includeFilePath = QStringLiteral(":/openglrenderer/glsl/view_ray.frag");
            }
            else if(line.contains("<flat_normal.vert>")) {
                if(_glversion >= QT_VERSION_CHECK(3, 0, 0))
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.vert");
                else if(isGLES)
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.GLES.vert");
                else
                    continue;
            }
            else if(line.contains("<flat_normal.frag>")) {
                if(_glversion >= QT_VERSION_CHECK(3, 0, 0))
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.frag");
                else if(isGLES)
                    includeFilePath = QStringLiteral(":/openglrenderer/glsl/flat_normal.GLES.frag");
                else
                    continue;
            }
            else {
                // Resolve relative file paths.
                QFileInfo includeFile(QFileInfo(shaderSourceFile).dir(), QString::fromUtf8(line.mid(8).replace('\"', "").trimmed()));
                includeFilePath = includeFile.filePath();
            }

            // Load the secondary shader file and insert it into the source of the primary shader.
            QFile secondarySourceFile(includeFilePath);
            if(!secondarySourceFile.open(QFile::ReadOnly))
                throw RendererException(QString("Unable to open shader source file %1 referenced by include directive in shader file %2.")
                                            .arg(includeFilePath)
                                            .arg(filename));
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

#if 0  // TODO
/******************************************************************************
* Activates the special highlight rendering mode.
******************************************************************************/
void OpenGLRenderingJob::setHighlightMode(int pass)
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
        case 0x8031 /*GL_TABLE_TOO_LARGE*/:
            return "GL_TABLE_TOO_LARGE - The specified table exceeds the implementation's maximum supported table size.";
        case 0x0506 /*GL_INVALID_FRAMEBUFFER_OPERATION*/:
            return "GL_INVALID_FRAMEBUFFER_OPERATION - The read and draw framebuffers are not framebuffer complete.";
        default: return "Unknown OpenGL error code.";
    }
}

/******************************************************************************
 * Reports OpenGL error status codes.
 ******************************************************************************/
void OpenGLRenderingJob::checkOpenGLErrorStatus(const char* command, const char* sourceFile, int sourceLine)
{
    GLenum error;
    while((error = this->glGetError()) != GL_NO_ERROR) {
        qDebug() << "WARNING: OpenGL call" << command
                 << "failed "
                    "in line"
                 << sourceLine << "of file" << sourceFile << "with error" << openglErrorString(error);
    }
}

/******************************************************************************
 * Create an OpenGL texture object for a QImage.
 ******************************************************************************/
QOpenGLTexture* OpenGLRenderingJob::uploadImage(const QImage& image, QOpenGLTexture::MipMapGeneration genMipMaps)
{
    OVITO_ASSERT(!image.isNull());

    // Check if this image has already been uploaded to the GPU.
    RendererResourceKey<struct ImageCache, quint64, QOpenGLContextGroup*> cacheKey{image.cacheKey(),
                                                                                   QOpenGLContextGroup::currentContextGroup()};
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
QOpenGLTexture* OpenGLRenderingJob::uploadColorMap(ColorCodingGradient* gradient)
{
    // Check if this color map has already been uploaded to the GPU.
    RendererResourceKey<struct ColorMapCache, OORef<ColorCodingGradient>, QOpenGLContextGroup*> cacheKey{
        gradient, QOpenGLContextGroup::currentContextGroup()};
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

}  // namespace Ovito
