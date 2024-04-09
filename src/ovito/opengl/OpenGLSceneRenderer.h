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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include "OpenGLHelpers.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLTexture>

namespace Ovito {

class OpenGLShaderHelper; // defined in OpenGLShaderHelper.h

/**
 * \brief An OpenGL-based scene renderer. This serves as base class for both the interactive renderer used
 *        by the viewports and the standard output renderer.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLSceneRenderer : public SceneRenderer, public QOpenGLExtraFunctions
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

    OVITO_CLASS_META(OpenGLSceneRenderer, OOMetaClass)

public:

    /// Constructor.
    explicit OpenGLSceneRenderer(ObjectInitializationFlags flags);

    /// Renders a single frame.
    virtual void renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

    /// Indicates whether we are rendering the contents of an interactive viewport window.
    bool isInteractive() const { return _isInteractive; }

    /// This may be called on a renderer before startRender() to control its supersampling level.
    virtual void setMultisamplingLevel(int multisamplingLevel) override { _multisamplingLevel = multisamplingLevel; }

	/// Returns the multisampling level currently used by the renderer.
	virtual int multisamplingLevel() const override final { return _multisamplingLevel; }

	/// This may be called on a renderer before startRender() to control the rendering method for semi-transparent objects.
	virtual void setOrderIndependentTransparencyHint(bool orderIndependent) override { _orderIndependentTransparency = orderIndependent; }

    /// Returns the OpenGL context this renderer uses.
    QOpenGLContext* glcontext() const { return _glcontext; }

    /// Returns the surface format of the current OpenGL context.
    const QSurfaceFormat& glformat() const { return _glformat; }

    /// Loads and compiles an OpenGL shader program.
    QOpenGLShaderProgram* loadShaderProgram(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile = QString());

    /// Reports OpenGL error status codes.
    void checkOpenGLErrorStatus(const char* command, const char* sourceFile, int sourceLine);

    /// Returns the cache used by the renderer to manage OpenGL resources.
    RendererResourceCache::ResourceFrame& currentResourceFrame() {
        OVITO_ASSERT(_currentResourceFrame);
        return _currentResourceFrame;
    }

    /// Returns whether the renderer has an in-flight frame for OpenGL resource caching.
    bool hasCurrentResourceFrame() const {
        return (bool)_currentResourceFrame;
    }

    /// Sets the cache to be used by the renderer to manage OpenGL resources.
    RendererResourceCache::ResourceFrame setCurrentResourceFrame(RendererResourceCache::ResourceFrame frame) {
        return std::exchange(_currentResourceFrame, std::move(frame));
    }

    /// Returns the OpenGL context version encoded as an integer.
    quint32 glversion() const { return _glversion; }

    /// Indicates whether OpenGL geometry shaders are supported.
    bool useGeometryShaders() const { return !_disableGeometryShaders && QOpenGLShader::hasOpenGLShaders(QOpenGLShader::Geometry, glcontext()); }

    /// Indicates that we have OpenGL support for instanced arrays (requires OpenGL 3.3+).
    bool useInstancedArrays() const { return !_disableInstancedArrays && glversion() >= QT_VERSION_CHECK(3, 3, 0); }

    /// Indicates that we have OpenGL support for glMultiDrawArraysIndirect (requires OpenGL 4.3+).
    bool useMultiDrawArraysIndirect() const { return !_disableMultiDrawArraysIndirect && glversion() >= QT_VERSION_CHECK(4, 3, 0); }

    /// Sets the primary framebuffer to be used by the renderer.
    void setPrimaryFramebuffer(GLuint primaryFramebuffer) { _primaryFramebuffer = primaryFramebuffer; }

    /// Creates an OpenGL texture object for a QImage.
    QOpenGLTexture* uploadImage(const QImage& image, QOpenGLTexture::MipMapGeneration genMipMaps = QOpenGLTexture::DontGenerateMipMaps);

    /// Creates a 1-D OpenGL texture object for a ColorCodingGradient.
    QOpenGLTexture* uploadColorMap(ColorCodingGradient* gradient);

    /// Lets the renderer implementation perform post-processing of a newly generated frame graph.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) override {
        // Convert all text rendering primitives into image primitives.
        frameGraph.renderTextAsImagePrimitives();
        // Adjust the line widths of all wireframe primitives.
        frameGraph.adjustWireframeLineWidths();
    }

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const override { return QImage::QImage::Format_RGBA8888; }

    /// Indicates that we are currently rendering a false-color image for object picking.
    bool isPickingPass() const { return _isPickingPass; }

    /// Activates picking render mode.
    void setPickingPass(bool enable) { _isPickingPass = enable; }

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

protected:

    /// Returns the model-view transformation matrix for the current graphics primitive being rendered.
    const AffineTransformation& modelViewTM() const { return _modelViewTM; }

	/// Returns the rectangular region of the framebuffer we are rendering into (in device coordinates).
	const QRect& viewportRect() const { return _viewportRect; }

    /// Returns the current viewport projection parameter.
    const ViewProjectionParameters& projParams() const { return _projParams; }

    /// Loads and compiles a GLSL shader and adds it to the given program object.
    void loadShader(QOpenGLShaderProgram* program, QOpenGLShader::ShaderType shaderType, const QString& filename, bool isWBOITPass);

private:

    /// Returns whether we are currently rendering semi-transparent geometry.
    bool isTransparencyPass() const { return _isTransparencyPass; }

    /// Executes the rendering commands stored in the given frame graph.
    bool renderFrameGraph(FrameGraph& frameGraph, FrameGraph::RenderLayer renderLayer);

    /// Render all semi-transparent geometry in a second rendering pass.
    void renderTransparentGeometry(FrameGraph& frameGraph);

    /// Renders a particles primitive.
    bool renderParticles(const ParticlePrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a cylinders primitive.
    bool renderCylinders(const CylinderPrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a triangle mesh primitive.
    bool renderMesh(const MeshPrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a set of particles.
    void renderParticlesImplementation(const ParticlePrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a triangle mesh.
    void renderMeshImplementation(const MeshPrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders just the edges of a triangle mesh as a wireframe model.
    void renderMeshWireframeImplementation(const MeshPrimitive& primitive);

    /// Generates the wireframe line elements for the visible edges of a mesh.
    ConstDataBufferPtr generateMeshWireframeLines(const MeshPrimitive& primitive);

    /// Prepares the OpenGL buffer with the per-instance transformation matrices for
    /// rendering a set of meshes.
    QOpenGLBuffer getMeshInstanceTMBuffer(const MeshPrimitive& primitive, OpenGLShaderHelper& shader);

    /// Renders a set of markers.
    void renderMarkersImplementation(const MarkerPrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a set of lines.
    void renderLinesImplementation(const LinePrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a set of lines using GL_LINES mode.
    void renderThinLinesImplementation(const LinePrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a set of lines using triangle strips.
    void renderThickLinesImplementation(const LinePrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a set of cylinders or arrow glyphs.
    void renderCylindersImplementation(const CylinderPrimitive& primitive, FrameGraph::ObjectPickingGroup* pickingGroup);

    /// Renders a 2d pixel image into the output framebuffer.
    void renderImageImplementation(const ImagePrimitive& primitive);

    /// Returns whether the renderer is using a two-pass OIT method.
    bool orderIndependentTransparency() const { return _orderIndependentTransparency; }

	/// Registers a range of unique IDs for the current object picking group being rendered.
	quint32 allocateObjectPickingIDs(FrameGraph::ObjectPickingGroup* pickingGroup, quint32 objectCount, const ConstDataBufferPtr& indices = {});

private:

    /// The OpenGL context this renderer uses.
    QOpenGLContext* _glcontext = nullptr;

    /// The GL context group this renderer uses.
    QPointer<QOpenGLContextGroup> _glcontextGroup;

    /// The surface used by the GL context.
    QSurface* _glsurface = nullptr;

    /// Pointer to the glMultiDrawArrays() function. Requires OpenGL 2.0.
    void (QOPENGLF_APIENTRY *glMultiDrawArrays)(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) = nullptr;

    /// Pointer to the optional glMultiDrawArraysIndirect() function. Requires OpenGL 4.3.
    void (QOPENGLF_APIENTRY *glMultiDrawArraysIndirect)(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) = nullptr;

    /// The OpenGL surface format.
    QSurfaceFormat _glformat;

    /// The OpenGL version of the context encoded as an integer.
    quint32 _glversion;

    /// Controls the level of multisampling used by the renderer to reduce antialiasing effects.
    int _multisamplingLevel = 1;

    /// Controls whether the renderer is using a two-pass OIT method.
    bool _orderIndependentTransparency = false;

    /// Indicates whether we are currently rendering a false-color image for object picking.
    bool _isPickingPass = false;

    /// Indicates whether we are currently rendering semi-transparent geometry.
    bool _isTransparencyPass = false;

    /// Indicates that the use of geometry shaders has explicitly been disabled.
    bool _disableGeometryShaders = (qEnvironmentVariableIntValue("OVITO_DISABLE_GEOMETRY_SHADERS") != 0);

    /// Indicates that the use of OpenGL instanced arrays has explicitly been disabled.
    bool _disableInstancedArrays = (qEnvironmentVariableIntValue("OVITO_DISABLE_INSTANCED_ARRAYS") != 0);

    /// Indicates that the use of glMultiDrawArraysIndirect() has explicitly been disabled.
    bool _disableMultiDrawArraysIndirect = (qEnvironmentVariableIntValue("OVITO_DISABLE_MULTI_DRAW_ARRAYS_INDIRECT") != 0);

    /// The primary framebuffer used by the renderer. The FBO's lifetime is managed by the subclass.
    /// It may be null when rendering to the system framebuffer provided by QOpenGLWidget.
    GLuint _primaryFramebuffer = 0;

    /// The additional framebuffer used for the OIT transparency pass.
    std::unique_ptr<QOpenGLFramebufferObject> _oitFramebuffer;

    /// The renderer uses this to manage the OpenGL resources created during frame rendering
    /// and re-use them in subsequent frames. OpenGL objects must be released only while an OpenGL context
    /// is current, which is why we cannot simply use the FrameGraph's resource cache for
    /// this purpose. The renderer needs to have control of when resources get released.
    RendererResourceCache::ResourceFrame _currentResourceFrame;

    /// Indicates whether we are rendering the contents of an interactive viewport window.
    bool _isInteractive = false;

    /// The model-view transformation matrix for the current graphics primitive being rendered.
    AffineTransformation _modelViewTM = AffineTransformation::Identity();

	/// The rectangular region of the framebuffer we are rendering into (in device coordinates).
	QRect _viewportRect;

    /// The current viewport projection.
    ViewProjectionParameters _projParams;

    /// Indicates that the current primitive being rendered is using preprojected NDC coordinates.
    bool _preprojectedCoordinates = false;

	/// The next available object ID to be used for object picking.
	quint32 _nextAvailablePickingID;

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

    friend class OpenGLShaderHelper;
};

}   // End of namespace
