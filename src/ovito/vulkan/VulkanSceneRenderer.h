////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include "VulkanContext.h"
#include "VulkanPipeline.h"

namespace Ovito {

/**
 * \brief An Vulkan-based scene renderer. This serves as base class for both the interactive renderer used
 *        by the viewports and the standard output renderer.
 */
class OVITO_VULKANRENDERER_EXPORT VulkanSceneRenderer : public SceneRenderer
{
public:

	/// Defines a metaclass specialization for this renderer class.
	class OVITO_VULKANRENDERER_EXPORT OOMetaClass : public SceneRenderer::OOMetaClass
	{
	public:
		/// Inherit standard constructor from base meta class.
		using SceneRenderer::OOMetaClass::OOMetaClass;

		/// Is called by OVITO to query the class for any information that should be included in the application's system report.
		virtual void querySystemInformation(QTextStream& stream, UserInterface& userInterface) const override;
	};

	OVITO_CLASS_META(VulkanSceneRenderer, OOMetaClass)

public:

	/// Constructor.
	explicit VulkanSceneRenderer(ObjectCreationParams params, std::shared_ptr<VulkanContext> vulkanContext, int concurrentFrameCount = 2);

	/// Destructor.
	virtual ~VulkanSceneRenderer();

	/// Returns the logical Vulkan context used by the renderer.
	const std::shared_ptr<VulkanContext>& context() const { return _context; }

	/// Returns the Vulkan logical device handle.
	VkDevice logicalDevice() const { return context()->logicalDevice(); }

	/// Returns the device-specific Vulkan function table. 
	QVulkanDeviceFunctions* deviceFunctions() const { return context()->deviceFunctions(); }

	/// This may be called on a renderer before startRender() to control its supersampling level.
	virtual void setAntialiasingHint(int antialiasingLevel) override { _antialiasingLevel = antialiasingLevel; }

	/// Returns the device pixel ratio of the output device we are rendering to.
	virtual qreal devicePixelRatio() const override { return antialiasingLevel() * SceneRenderer::devicePixelRatio(); }

	/// Renders the current animation frame.
	virtual bool renderFrame(const QRect& viewportRect, MainThreadOperation& operation) override;

	/// Renders the overlays/underlays of the viewport into the framebuffer.
	virtual bool renderOverlays(bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect, MainThreadOperation& operation) override;

	/// This method is called just before renderFrame() is called.
	virtual void beginFrame(TimePoint time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

	/// Temporarily enables/disables the depth test while rendering.
	virtual void setDepthTestEnabled(bool enabled) override;

	/// Activates the special highlight rendering mode.
	virtual void setHighlightMode(int pass) override;

    /// Returns the number of frames that can be potentially active at the same time.
	int concurrentFrameCount() const { return _concurrentFrameCount; }

    /// Returns the current Vulkan swap chain frame index in the range [0, concurrentFrameCount() - 1].
	int currentSwapChainFrame() const { return _currentSwapChainFrame; }

	/// Returns the monotonically increasing identifier of the current Vulkan frame being rendered.
	VulkanContext::ResourceFrameHandle currentResourceFrame() const { return _currentResourceFrame; }

	/// Sets monotonically increasing identifier of the current Vulkan frame being rendered.
	void setCurrentResourceFrame(VulkanContext::ResourceFrameHandle frame) { _currentResourceFrame = frame; }

	/// Returns the active Vulkan command buffer.
	VkCommandBuffer currentCommandBuffer() const { return _currentCommandBuffer; }

	/// Sets the active Vulkan command buffer.
	void setCurrentCommandBuffer(VkCommandBuffer cmdBuf) { _currentCommandBuffer = cmdBuf; }

    /// Sets the current Vulkan swap chain frame index.
	void setCurrentSwapChainFrame(int frame) { _currentSwapChainFrame = frame; }

	/// Returns the default Vulkan render pass used by the renderer.
    VkRenderPass defaultRenderPass() const { return _defaultRenderPass; }

	/// Sets the default Vulkan render pass to be used by the renderer.
    void setDefaultRenderPass(VkRenderPass renderpass) { _defaultRenderPass = renderpass; }

	/// Returns the size in pixels of the Vulkan frame buffer we are rendering into.
	const QSize& frameBufferSize() const { return _frameBufferSize; }

	/// Sets the size in pixels of the Vulkan frame buffer we are rendering into.
	void setFrameBufferSize(const QSize& size) { _frameBufferSize = size; }

	/// Returns the sample count used by the current Vulkan target rendering buffer.
    VkSampleCountFlagBits sampleCount() const { return _sampleCount; }

	/// Renders a line primitive.
	virtual void renderLines(const LinePrimitive& primitive) override;

	/// Renders a particles primitive.
	virtual void renderParticles(const ParticlePrimitive& primitive) override;

	/// Renders the cylinder or arrow primitives.
	virtual void renderCylinders(const CylinderPrimitive& primitive) override;

	/// Renders an image primitive.
	virtual void renderImage(const ImagePrimitive& primitive) override;

	/// Renders a text primitive.
	virtual void renderText(const TextPrimitive& primitive) override;

	/// Renders a mesh primitive.
	virtual void renderMesh(const MeshPrimitive& primitive) override;

	/// Returns a 4x4 matrix that can be used to correct for coordinate system differences between OpenGL and Vulkan.
    const Matrix4& clipCorrection() const { return _clipCorrection; } 

	/// Returns the descriptor set layout for the global uniforms buffer.
	VkDescriptorSetLayout globalUniformsDescriptorSetLayout();

	/// Returns the descriptor set layout for color gradient maps.
	VkDescriptorSetLayout colorMapDescriptorSetLayout();

	/// Returns the Vulkan descriptor set for the global uniforms structure, which can be bound to a pipeline. 
	VkDescriptorSet getGlobalUniformsDescriptorSet();

	/// Uploads a color coding map to the Vulkan device as a uniforms buffer.
	VkDescriptorSet uploadColorMap(ColorCodingGradient* gradient);

protected:

	/// This method is called after the reference counter of this object has reached zero
	/// and before the object is being finally deleted. 
	virtual void aboutToBeDeleted() override;

	/// Returns the supersampling level.
	int antialiasingLevel() const { return _antialiasingLevel; }

protected Q_SLOTS:

	/// Releases all Vulkan resources held by the renderer class.
    virtual void releaseVulkanDeviceResources();

private:

	/// Creates the Vulkan resources needed by this renderer.
	void initResources();

	/// Renders a set of cylinders or arrow glyphs.
	void renderCylindersImplementation(const CylinderPrimitive& primitive);

	/// Renders a set of lines.
	void renderLinesImplementation(const LinePrimitive& primitive);

	/// Renders a set of lines using GL_LINES mode.
	void renderThinLinesImplementation(const LinePrimitive& primitive);

	/// Renders a set of lines using triangle strips.
	void renderThickLinesImplementation(const LinePrimitive& primitive);

	/// Renders a 2d pixel image into the output framebuffer.
	void renderImageImplementation(const ImagePrimitive& primitive);

	/// Renders a set of particles.
	void renderParticlesImplementation(const ParticlePrimitive& primitive);

	/// Renders a triangle mesh.
	void renderMeshImplementation(const MeshPrimitive& primitive);

	/// Renders just the edges of a triangle mesh as a wireframe model.
	void renderMeshWireframeImplementation(const MeshPrimitive& primitive, const QMatrix4x4& mvp);

	/// Generates the wireframe line elements for the visible edges of a mesh.
	ConstDataBufferPtr generateMeshWireframeLines(const MeshPrimitive& primitive);

	/// Prepares the OpenGL buffer with the per-instance transformation matrices for 
	/// rendering a set of meshes.
	VkBuffer getMeshInstanceTMBuffer(const MeshPrimitive& primitive);

	/// Creates the Vulkan pipelines for the line rendering primitive.
	VulkanPipeline& createLinePrimitivePipeline(VulkanPipeline& pipeline);

	/// Creates the Vulkan pipelines for the cylinder rendering primitive.
	VkPipelineLayout createCylinderPrimitivePipeline(VulkanPipeline& pipeline);

	/// Creates the Vulkan pipelines for the mesh rendering primitive.
	VulkanPipeline& createMeshPrimitivePipeline(VulkanPipeline& pipeline);

	/// Creates the Vulkan pipelines for the particle rendering primitive.
	VulkanPipeline& createParticlePrimitivePipeline(VulkanPipeline& pipeline);

	/// Creates the Vulkan pipelines for the image rendering primitive.
	void initImagePrimitivePipelines();

	/// Destroys the Vulkan pipelines for the line rendering primitive.
	void releaseLinePrimitivePipelines();

	/// Destroys the Vulkan pipelines for the particle rendering primitive.
	void releaseParticlePrimitivePipelines();

	/// Destroys the Vulkan pipelines for the cylinder rendering primitive.
	void releaseCylinderPrimitivePipelines();

	/// Destroys the Vulkan pipelines for the mesh rendering primitive.
	void releaseMeshPrimitivePipelines();

	/// Destroys the Vulkan pipelines for the image rendering primitive.
	void releaseImagePrimitivePipelines();

private:

	/// The logical Vulkan device used by the renderer.
	std::shared_ptr<VulkanContext> _context;

	/// Controls the number of sub-pixels to render.
	int _antialiasingLevel = 1;

	/// The number of frames that can be potentially active at the same time.
    int _concurrentFrameCount = 2;

    /// The current Vulkan swap chain frame index.
    uint32_t _currentSwapChainFrame = 0;

	/// Indicates whether depth testing is currently enabled for drawing commands.
	bool _depthTestEnabled = true;

	/// The default Vulkan render pass to be used by the renderer.
    VkRenderPass _defaultRenderPass = VK_NULL_HANDLE;

	/// The active command buffer for the current swap chain image.
	VkCommandBuffer _currentCommandBuffer = VK_NULL_HANDLE;

	/// The sample count used by the current Vulkan target rendering buffer.
    VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;

	/// The size of the frame buffer we are rendering into.
	QSize _frameBufferSize;

	/// The monotonically increasing identifier of the current Vulkan frame being rendered.
	VulkanContext::ResourceFrameHandle _currentResourceFrame = 0;

	/// List of semi-transparent particles primitives collected during the first rendering pass, which need to be rendered during the second pass.
	std::vector<std::pair<AffineTransformation, ParticlePrimitive>> _translucentParticles;

	/// List of semi-transparent cylinder primitives collected during the first rendering pass, which need to be rendered during the second pass.
	std::vector<std::pair<AffineTransformation, CylinderPrimitive>> _translucentCylinders;

	/// List of semi-transparent particles primitives collected during the first rendering pass, which need to be rendered during the second pass.
	std::vector<std::pair<AffineTransformation, MeshPrimitive>> _translucentMeshes;

	/// Indicates that the Vulkan resources needed by this renderer have been created.
	bool _resourcesInitialized = false;

	/// Data structure holding the Vulkan pipelines used by the line drawing primitive.
	struct {
		VulkanPipeline thinWithColors;
		VulkanPipeline thinUniformColor;
		VulkanPipeline thinPicking;
	} _linePrimitivePipelines;

	/// Data structure holding the Vulkan pipelines used by the particle drawing primitive.
	struct VulkanParticlePrimitivePipelines {
		VulkanPipeline cube;
		VulkanPipeline cube_picking;
		VulkanPipeline sphere;
		VulkanPipeline sphere_picking;
		VulkanPipeline square;
		VulkanPipeline square_picking;
		VulkanPipeline circle;
		VulkanPipeline circle_picking;
		VulkanPipeline imposter;
		VulkanPipeline imposter_picking;
		VulkanPipeline box;
		VulkanPipeline box_picking;
		VulkanPipeline ellipsoid;
		VulkanPipeline ellipsoid_picking;
		VulkanPipeline superquadric;
		VulkanPipeline superquadric_picking;
	} _particlePrimitivePipelines;

	/// Data structure holding the Vulkan pipelines used by the cylinder drawing primitive.
	struct {
		VulkanPipeline cylinder;
		VulkanPipeline cylinder_picking;
		VulkanPipeline cylinder_flat;
		VulkanPipeline cylinder_flat_picking;
		VulkanPipeline arrow_head;
		VulkanPipeline arrow_head_picking;
		VulkanPipeline arrow_tail;
		VulkanPipeline arrow_tail_picking;
		VulkanPipeline arrow_flat;
		VulkanPipeline arrow_flat_picking;
	} _cylinderPrimitivePipelines;

	/// Data structure holding the Vulkan pipelines used by the image drawing primitive.
	struct {
		VulkanPipeline imageQuad;
		VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	} _imagePrimitivePipelines;

	/// Data structure holding the Vulkan pipelines used by the mesh drawing primitive.
	struct VulkanMeshPrimitivePipelines {
		VulkanPipeline mesh;
		VulkanPipeline mesh_picking;
		VulkanPipeline mesh_wireframe;
		VulkanPipeline mesh_wireframe_instanced;
		VulkanPipeline mesh_instanced;
		VulkanPipeline mesh_instanced_picking;
		VulkanPipeline mesh_instanced_with_colors;
		VulkanPipeline mesh_color_mapping;
	} _meshPrimitivePipelines;

	/// A 4x4 matrix that can be used to correct for coordinate system differences between OpenGL and Vulkan.
	/// By pre-multiplying the projection matrix with this matrix, applications can
	/// continue to assume that Y is pointing upwards, and can set minDepth and
	/// maxDepth in the viewport to 0 and 1, respectively, without having to do any
	/// further corrections to the vertex Z positions. Geometry from OpenGL
	/// applications can then be used as-is, assuming a rasterization state matching
	/// OpenGL culling and front face settings.
    const Matrix4 _clipCorrection{1.0, 0.0, 0.0, 0.0,
									0.0, -1.0, 0.0, 0.0,
									0.0, 0.0, 0.5, 0.5,
									0.0, 0.0, 0.0, 1.0};

	/// Data structure with some slowly or not varying data, which is made available to all shaders.
	struct GlobalUniforms
	{
		Matrix_4<float> projectionMatrix;
		Matrix_4<float> inverseProjectionMatrix;
		Point_2<float> viewportOrigin; 			// Corner of the current viewport rectangle in window coordinates.
		Vector_2<float> inverseViewportSize;	// One over the width/height of the viewport rectangle in window space.
		float znear, zfar;

		bool operator==(const GlobalUniforms& other) const {
			return projectionMatrix == other.projectionMatrix && viewportOrigin == other.viewportOrigin 
				&& inverseViewportSize == other.inverseViewportSize && znear == other.znear && zfar == other.zfar;
		}
	};

	VkDescriptorSetLayout _globalUniformsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout _colorMapDescriptorSetLayout = VK_NULL_HANDLE;
};

}	// End of namespace
