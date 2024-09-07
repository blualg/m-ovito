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
#include "OpenGLHelpers.h"
#include "OpenGLTexture.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>

namespace Ovito {

class OpenGLShaderHelper; // defined in OpenGLShaderHelper.h
class OpenGLRenderingFrameBuffer; // defined in OpenGLRenderingFrameBuffer.h

/**
 * \brief A RAII utility class that restores the previous OpenGL context when the object goes out of scope.
*/
class OVITO_OPENGLRENDERER_EXPORT OpenGLContextRestore
{
    Q_DISABLE_COPY(OpenGLContextRestore)
public:

    /// Constructor, which remembers the previous context.
    OpenGLContextRestore() noexcept : _initialized(true), _context(QOpenGLContext::currentContext()), _surface(_context ? _context->surface() : nullptr) {}
    /// Move constructor.
    OpenGLContextRestore(OpenGLContextRestore&& other) noexcept : _initialized(std::exchange(other._initialized, false)), _context(std::exchange(other._context, nullptr)), _surface(std::exchange(other._surface, nullptr)) {}

    /// Destructor, which restores the previous OpenGL context.
    ~OpenGLContextRestore() {
        if(_initialized) {
            if(_context && _surface)
                _context->makeCurrent(_surface);
            else if(QOpenGLContext* context = QOpenGLContext::currentContext())
                context->doneCurrent();
        }
    }

private:
    bool _initialized = false;
    QOpenGLContext* _context = nullptr;
    QSurface* _surface = nullptr;
};

/**
 * \brief A rendering job of the OpenGL renderer.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLRenderingJob : public RenderingJob, public QOpenGLExtraFunctions
{
    OVITO_CLASS(OpenGLRenderingJob)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, int multisamplingLevel, bool orderIndependentTransparency);

    /// Called when this object is being destroyed.
    virtual void aboutToBeDeleted() override;

    /// Returns the cache managing rendering resources.
    const std::shared_ptr<RendererResourceCache>& visCache() const { return _visCache; }

	/// Creates a new abstract target frame buffer for rendering into.
	virtual OORef<AbstractRenderingFrameBuffer> createOffscreenFrameBuffer(const QRect& viewportRect, const std::shared_ptr<FrameBuffer>& frameBuffer) override;

	/// Renders an image of the given frame graph into the given target frame buffer.
	[[nodiscard]] virtual Future<void> renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<AbstractRenderingFrameBuffer> frameBuffer, std::shared_ptr<ObjectPickingIdentifierMap> pickingMap = {}) override;

	/// Returns the multi-sampling level used to reduce anti-aliasing artifacts during offscreen rendering.
	virtual int multisamplingLevel() const override { return _multisamplingLevel; }

    /// Performs post-processing of a newly generated frame graph to be rendered by this implementation.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) override {
        // Convert all text rendering primitives into image primitives.
        frameGraph.renderTextAsImagePrimitives();
        // Adjust the line widths of all wireframe primitives.
        frameGraph.adjustWireframeLineWidths();
    }

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const override { return QImage::QImage::Format_RGBA8888; }

    /// Requests the rendering job to make its OpenGL context current, e.g. for releasing OpenGL resources that require an active context.
    [[nodiscard]] virtual OpenGLContextRestore activateContext() = 0;

protected:

    /// May combine the framebuffer contents from multiple renderers.
    /// This can be implemented by derived classes.
    virtual void performFrameCompositing() {}

    /// Decides whether a command from the render graph should be executed by the renderer.
    virtual bool filterRenderingCommand(const FrameGraph::RenderingCommand& command, const FrameGraph::RenderingCommandGroup& commandGroup);

    /// Sets up the model-view transformation matrix for the given rendering command.
    void setupModelViewTransformation(const FrameGraph::RenderingCommand& command);

    /// Returns the resource cache frame used by the renderer to manage OpenGL resources.
    RendererResourceCache::ResourceFrame& currentResourceFrame() { return _currentResourceFrame; }

    /// Returns the OpenGL context this renderer uses.
    QOpenGLContext* glcontext() const { return _glcontext; }

    /// Returns the surface format of the current OpenGL context.
    const QSurfaceFormat& glformat() const { return _glformat; }

    /// Reports OpenGL error status codes.
    void checkOpenGLErrorStatus(const char* command, const char* sourceFile, int sourceLine);

    /// Loads and compiles an OpenGL shader program.
    QOpenGLShaderProgram* loadShaderProgram(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile = QString());

    /// Loads and compiles a GLSL shader and adds it to the given program object.
    void loadShader(QOpenGLShaderProgram* program, QOpenGLShader::ShaderType shaderType, const QString& filename, bool isWBOITPass);

    /// Returns the OpenGL context version encoded as an integer.
    quint32 glversion() const { return _glversion; }

    /// Indicates whether OpenGL geometry shaders are supported.
    bool useGeometryShaders() const { return !_disableGeometryShaders && QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Geometry, glcontext()); }

    /// Indicates that we have OpenGL support for instanced arrays (requires OpenGL 3.3+).
    bool useInstancedArrays() const { return !_disableInstancedArrays && glversion() >= QT_VERSION_CHECK(3, 3, 0); }

    /// Indicates that we have OpenGL support for glMultiDrawArraysIndirect (requires OpenGL 4.3+).
    bool useMultiDrawArraysIndirect() const { return !_disableMultiDrawArraysIndirect && glversion() >= QT_VERSION_CHECK(4, 3, 0); }

    /// Creates an OpenGL texture object for a QImage.
    const OpenGLTexture& uploadImage(const QImage& image);

    /// Creates a 1-D OpenGL texture object for a ColorCodingGradient.
    const OpenGLTexture& uploadColorMap(const ColorCodingGradient* gradient);

    /// Returns the frame graph we are currently rendering.
    const FrameGraph* frameGraph() const { OVITO_ASSERT(_frameGraph); return _frameGraph; }

    /// Returns whether we are currently rendering semi-transparent geometry.
    bool isTransparencyPass() const { return _isTransparencyPass; }

    /// Executes the rendering commands stored in the given frame graph.
    bool renderFrameGraph(FrameGraph::RenderLayerType layerType);

    /// Render all semi-transparent geometry in a second rendering pass.
    void renderTransparentGeometry(OpenGLRenderingFrameBuffer& frameBuffer);

    /// Renders a particles primitive.
    bool renderParticles(const ParticlePrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a cylinders primitive.
    bool renderCylinders(const CylinderPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a triangle mesh primitive.
    bool renderMesh(const MeshPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a set of particles.
    void renderParticlesImplementation(const ParticlePrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a triangle mesh.
    void renderMeshImplementation(const MeshPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders just the edges of a triangle mesh as a wireframe model.
    void renderMeshWireframeImplementation(const MeshPrimitive& primitive);

    /// Generates the wireframe line elements for the visible edges of a mesh.
    ConstDataBufferPtr generateMeshWireframeLines(const MeshPrimitive& primitive);

    /// Prepares the OpenGL buffer with the per-instance transformation matrices for rendering a set of meshes.
    QOpenGLBuffer getMeshInstanceTMBuffer(const MeshPrimitive& primitive, OpenGLShaderHelper& shader);

    /// Renders a set of markers.
    void renderMarkersImplementation(const MarkerPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a set of lines.
    void renderLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a set of lines using GL_LINES mode.
    void renderThinLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a set of lines using triangle strips.
    void renderThickLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a set of cylinders or arrow glyphs.
    void renderCylindersImplementation(const CylinderPrimitive& primitive, const FrameGraph::RenderingCommand& command);

    /// Renders a 2d pixel image into the output framebuffer.
    void renderImageImplementation(const ImagePrimitive& primitive);

    /// Returns whether the renderer is using a two-pass OIT method.
    bool orderIndependentTransparency() const { return _orderIndependentTransparency; }

    /// Returns the mapping of frame buffer object IDs to object picking groups.
    ObjectPickingIdentifierMap* objectPickingIdentifierMap() const { return _objectPickingIdentifierMap; }

    /// Indicates that we are currently rendering a false-color image for object picking.
    bool isPickingPass() const { return objectPickingIdentifierMap() != nullptr; }

    /// Returns the model-view transformation matrix for the current graphics primitive being rendered.
    const AffineTransformation& modelViewTM() const { return _modelViewTM; }

	/// Returns the output area in the OpenGL framebuffer (in device pixels).
	const QSize& framebufferSize() const { return _framebufferSize; }

private:

    /// Controls the level of multisampling used to reduce antialiasing effects.
    int _multisamplingLevel = 1;

    /// Controls whether a two-pass OIT method is used to render semi-transparent geometry.
    bool _orderIndependentTransparency = false;

    /// The OpenGL context being used for rendering.
    QOpenGLContext* _glcontext = nullptr;

    /// The OpenGL surface format.
    QSurfaceFormat _glformat;

    /// The OpenGL version of the context encoded as an integer.
    quint32 _glversion;

    /// The cache managing rendering resources.
    std::shared_ptr<RendererResourceCache> _visCache;

    /// Keeps alive the OpenGL resources that get created during frame rendering
    /// such that they can be re-used in subsequent frames.
    /// Note: OpenGL objects must be released while an OpenGL context is current.
    /// The renderer needs to have control of when resources get released.
    RendererResourceCache::ResourceFrame _currentResourceFrame;

    /// Pointer to the glMultiDrawArrays() function. Requires OpenGL 2.0.
    void (QOPENGLF_APIENTRY *glMultiDrawArrays)(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) = nullptr;

    /// Pointer to the optional glMultiDrawArraysIndirect() function. Requires OpenGL 4.3.
    void (QOPENGLF_APIENTRY *glMultiDrawArraysIndirect)(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) = nullptr;

    /// The mapping of frame buffer object IDs to object picking groups.
    ObjectPickingIdentifierMap* _objectPickingIdentifierMap = nullptr;

    /// Indicates whether we are currently rendering semi-transparent geometry.
    bool _isTransparencyPass = false;

    /// Indicates that the use of geometry shaders has explicitly been disabled.
    bool _disableGeometryShaders = (qEnvironmentVariableIntValue("OVITO_DISABLE_GEOMETRY_SHADERS") != 0);

    /// Indicates that the use of OpenGL instanced arrays has explicitly been disabled.
    bool _disableInstancedArrays = (qEnvironmentVariableIntValue("OVITO_DISABLE_INSTANCED_ARRAYS") != 0);

    /// Indicates that the use of glMultiDrawArraysIndirect() has explicitly been disabled.
    bool _disableMultiDrawArraysIndirect = (qEnvironmentVariableIntValue("OVITO_DISABLE_MULTI_DRAW_ARRAYS_INDIRECT") != 0);

    /// Indicates whether the renderer uses shader non-perspective attribute interpolation to compute fragment rays.
    bool _useInterpolatedRayDirections = (qEnvironmentVariableIntValue("OVITO_DISABLE_INTERPOLATED_RAY_DIRS") == 0);

    /// The frame graph we are currently rendering.
    const FrameGraph* _frameGraph = nullptr;

    /// The model-view transformation matrix for the current graphics primitive being rendered.
    AffineTransformation _modelViewTM = AffineTransformation::Identity();

    /// Indicates that the current primitive being rendered is using preprojected NDC coordinates.
    bool _preprojectedCoordinates = false;

	/// The output area in the OpenGL framebuffer (in device pixels).
	QSize _framebufferSize;

    friend class OpenGLShaderHelper;
    friend class OpenGLRenderingFrameBuffer;
};

}   // End of namespace
