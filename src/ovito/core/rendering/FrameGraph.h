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
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include "RendererResourceCache.h"
#include "LinePrimitive.h"
#include "ParticlePrimitive.h"
#include "TextPrimitive.h"
#include "ImagePrimitive.h"
#include "CylinderPrimitive.h"
#include "MeshPrimitive.h"
#include "MarkerPrimitive.h"

namespace Ovito {

/**
 * Abstract base class for per-object information used by the object picking system.
 */
class OVITO_CORE_EXPORT ObjectPickInfo : public OvitoObject
{
	OVITO_CLASS(ObjectPickInfo)

public:

	/// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
	virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) { return {}; }
};

/**
 * The FrameGraph class represents a sequence of rendering commands that are used to generate
 * a single frame of a 3D scene. The recorded rendering commands can be executed by a SceneRenderer to produce an image.
*/
class OVITO_CORE_EXPORT FrameGraph : public OvitoObject
{
	OVITO_CLASS(FrameGraph)

public:

    /// The type of layers that get rendered on top of each other. Each rendering command belongs to of one of these layers.
    enum RenderLayerType {
        UnderLayer,
        SceneLayer,
        HighlightLayer1,
        HighlightLayer2,
        OverLayer
    };

	/// Describes a single primitive to be rendered.
	class OVITO_CORE_EXPORT RenderingCommand
	{
	public:

		/// Bit-wise flags for rendering commands.
		enum Flag {
			NoFlags             = 0,
			ExcludeFromVisual   = (1<<0), // Skip the primitive in the visual rendering pass
			ExcludeFromPicking  = (1<<1), // Skip the primitive in the object picking rendering pass
		};
		Q_DECLARE_FLAGS(Flags, Flag);

		/// Constructor.
		explicit RenderingCommand(Flags flags, std::unique_ptr<RenderingPrimitive> primitive, const AffineTransformation& tm, OORef<const Pipeline> pipeline = {}, OORef<ObjectPickInfo> pickInfo = {}, uint32_t pickElementOffset = 0) :
			_primitive(std::move(primitive)), _tm(tm), _pipeline(std::move(pipeline)), _pickInfo(std::move(pickInfo)), _pickElementOffset(pickElementOffset), _flags(flags) {}

		/// Returns the graphics primitive rendered by this command.
		RenderingPrimitive* primitive() const { return _primitive.get(); }

		/// Replaces the graphics primitive with a new one.
		void setPrimitive(std::unique_ptr<RenderingPrimitive> primitive) { _primitive = std::move(primitive); }

		/// Returns the model-to-world transformation matrix to be applied to the graphics primitive.
		const AffineTransformation& modelWorldTM() const { return _tm; }

		/// The scene pipeline to which this rendering command belongs.
		const OORef<const Pipeline>& pipeline() const { return _pipeline; }

		/// An optional object that knows more about what is being rendered and which sub-elements it consists of.
		const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

		/// If this rendering command is part of a composite object that requires multiple rendering commands,
		/// then this offset indicates where this command's primitive elements start in the composite range.
		uint32_t pickElementOffset() const { return _pickElementOffset; }

		/// Determines whether this command should be skipped in object picking render mode.
		bool skipInPickingPass() const { return _flags.testFlag(ExcludeFromPicking); }

		/// Determines whether this command should be skipped in visual render mode.
		bool skipInVisualPass() const { return _flags.testFlag(ExcludeFromVisual); }

	private:

		/// The graphics primitive to be rendered.
		std::unique_ptr<RenderingPrimitive> _primitive;

		/// The model-to-world transformation matrix to be applied to the primitive.
		/// May be a null matrix to indicate that the primitive contains pre-projected coordinates.
		AffineTransformation _tm = AffineTransformation::Zero();

		/// The scene pipeline to which this rendering command belongs.
		/// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
		OORef<const Pipeline> _pipeline;

		/// An optional object that knows what high-level data is being represented by this render command and which sub-elements it consists of.
		OORef<ObjectPickInfo> _pickInfo;

		/// If this rendering command is part of a composite object that requires multiple rendering commands,
		/// then this offset indicates where this command's primitive elements start in the composite range.
		uint32_t _pickElementOffset;

		/// Bit-wise flags of this rendering command.
		Flags _flags = NoFlags;
	};

	/// A group of rendering commands.
	class RenderingCommandGroup
	{
	public:

		/// Constructor.
		explicit RenderingCommandGroup(RenderLayerType layerType) : _layerType(layerType) {}

		/// Returns the type of layer this group belongs to.
		RenderLayerType layerType() const { return _layerType; }

		/// Returns the world-space bounding box of the command group.
		const Box3& boundingBox() const { return _boundingBox; }

		/// Returns the sequence of rendering commands in this group.
		const std::vector<RenderingCommand>& commands() const { return _commands; }

		/// Returns the mutable sequence of rendering commands in this group.
		std::vector<RenderingCommand>& commands() { return _commands; }

		/// Appends a rendering command to the group.
		template<typename... Args>
		void addCommand(Args&&... args) {
			_commands.emplace_back(std::forward<Args>(args)...);
		}

		/// Add a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
		/// Automatically computes the bounding box of the primitive and the model-to-world transformation.
		void addPrimitive(std::unique_ptr<RenderingPrimitive> primitive, const AffineTransformation& tm, const Box3& box, OORef<const Pipeline> pickablePipeline, OORef<ObjectPickInfo> pickInfo = {}, uint32_t pickElementOffset = 0);

		/// Add a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
		/// Automatically computes the bounding box of the primitive and the model-to-world transformation.
		void addPrimitiveNonpickable(std::unique_ptr<RenderingPrimitive> primitive, const AffineTransformation& tm, const Box3& box);

		/// Adds a primitive to the frame graph containing pre-projected coordinates.
		void addPrimitivePreprojected(std::unique_ptr<RenderingPrimitive> primitive);

		/// Renders a 2d polyline or polygon into an interactive viewport.
		void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed, const QSize& logicalViewportSize);

	private:

		/// The rendering commands in this group.
		std::vector<RenderingCommand> _commands;

		/// The world-space bounding box of the command group.
		Box3 _boundingBox;

		/// The kind of layer this group belongs to.
		RenderLayerType _layerType;
	};

public:

	/// Constructor.
    void initializeObject(RendererResourceCache::ResourceFrame visCache, AnimationTime time, const ViewProjectionParameters& projectionParams, QSize viewportDeviceIndependentSize, bool isInteractive, bool isPreviewMode, bool stopOnPipelineError, QImage::Format preferredImageFormat, qreal devicePixelRatio) {
		OvitoObject::initializeObject();
		_visCache = std::move(visCache);
		_time = time;
		_projectionParams = projectionParams;
		_isInteractive = isInteractive;
		_isPreviewMode = isPreviewMode;
		_stopOnPipelineError = stopOnPipelineError;
		_preferredImageFormat = preferredImageFormat;
		_devicePixelRatio = devicePixelRatio;
		_viewportDeviceIndependentSize = viewportDeviceIndependentSize;
	}

	/// Returns the data cache to be used by visualization elements.
	const RendererResourceCache::ResourceFrame& visCache() const { OVITO_ASSERT(_visCache); return _visCache; }

	/// Releases the data cache of the frame graph after use.
	RendererResourceCache::ResourceFrame takeCacheFrame() { return std::move(_visCache); }

	/// Returns the animation time being rendered.
	AnimationTime time() const { return _time; }

	/// Returns whether we are rendering an interactive viewport or not.
	bool isInteractive() const { return _isInteractive; }

	/// Returns whether preview mode is active in the interactive viewport being rendered.
	bool isPreviewMode() const { return _isPreviewMode; }

	/// Returns whether the rendering should be stopped when an error occurs in a data pipeline.
	bool stopOnPipelineError() const { return _stopOnPipelineError; }

	/// Returns whether the rendered scene represents a preliminary pipeline state, i.e., a partial output
	/// of pipelines that have not been fully evaluated yet.
	bool isPreliminaryState() const { return _isPreliminaryState; }

	/// Specifies whether the rendered scene represents a preliminary pipeline state, i.e., a partial output
	/// of pipelines that have not been fully evaluated yet.
	void setIsPreliminaryState(bool isPreliminary) { _isPreliminaryState = isPreliminary; }

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	QImage::Format preferredImageFormat() const { return _preferredImageFormat; }

	/// Returns the device pixel ratio of the output device we are rendering to.
	qreal devicePixelRatio() const { return _devicePixelRatio; }

	/// Returns the 3d projection parameters to be used for rendering.
	const ViewProjectionParameters& projectionParams() const { return _projectionParams; }

	/// Changes the 3d projection parameters to be used for rendering.
	void setProjectionParams(const ViewProjectionParameters& params) { _projectionParams = params; }

	/// Sets the color to clear the framebuffer with.
	void setClearColor(const ColorA& c) { _clearColor = c; }

	/// Returns The color to clear the framebuffer with.
	const ColorA& clearColor() const { return _clearColor; }

	/// Returns the world-space bounding box of the 3d scene.
	const Box3& sceneBoundingBox() const { return _sceneBoundingBox; }

	/// Computes the combined scene bounding box from all command groups.
	void computeSceneBoundingBox();

	/// Returns the line rendering width to use in object picking mode.
	FloatType defaultLinePickingWidth() const;

	/// Returns the list of command groups.
	const std::deque<RenderingCommandGroup>& commandGroups() const { return _commandGroups; }

	/// Adds a new rendering command group to the graph.
	RenderingCommandGroup& addCommandGroup(RenderLayerType layerType) {
		return _commandGroups.emplace_back(layerType);
	}

	/// Add a 3d rendering primitive to the current layer of the frame graph.
	/// Automatically computes the bounding box of the primitive and the model-to-world transformation.
	void addPrimitive(RenderingCommandGroup& group, std::unique_ptr<RenderingPrimitive> primitive, OORef<const Pipeline> pipeline, OORef<ObjectPickInfo> pickInfo = {}, uint32_t pickElementOffset = 0);

	/// Add a 3d rendering primitive to the current layer of the frame graph.
	/// Automatically computes the bounding box of the primitive and the model-to-world transformation.
	void addPrimitiveNonpickable(RenderingCommandGroup& group, std::unique_ptr<RenderingPrimitive> primitive, const Pipeline* pipeline);

	/// Replaces all text primitives with (cached) image primitives.
	void renderTextAsImagePrimitives();

	/// Adjust wireframe line widths to match device pixel ratio.
	void adjustWireframeLineWidths();

    /// Computes the world size of an object that should appear always in the same size on the screen.
    FloatType nonScalingSize(const Point3& worldPosition) const {
		return projectionParams().nonScalingSize(worldPosition, _viewportDeviceIndependentSize);
	}

private:

	/// The data cache to be used by visualization elements.
	RendererResourceCache::ResourceFrame _visCache;

	/// The animation time being rendered.
	AnimationTime _time;

	/// The 3d projection parameters to be used for rendering.
	ViewProjectionParameters _projectionParams;

	/// Indicates whether we are rendering an interactive viewport or not.
	bool _isInteractive;

	/// Indicates that preview mode is active in the interactive viewport being rendered.
	bool _isPreviewMode;

	/// Indicates whether the rendering should stop when an error occurs in a data pipeline.
	bool _stopOnPipelineError;

	/// The best format for QImage to be used when creating an ImagePrimitive.
	QImage::Format _preferredImageFormat;

	/// The device pixel ratio of the output device we are rendering to.
	qreal _devicePixelRatio;

	/// The size of the viewport in device-independent pixels.
	QSize _viewportDeviceIndependentSize;

	/// The color to clear the framebuffer with.
	ColorA _clearColor = ColorA(0, 0, 0, 0);

	/// The list of recorded rendering commands groups.
	/// Ordering is important, as the groups are rendered in sequence within each RenderLayerType.
	/// Using a deque instead of a vector, because addresses must be stable.
	std::deque<RenderingCommandGroup> _commandGroups;

	/// The world-space bounding box of the 3d scene.
	Box3 _sceneBoundingBox;

	/// Indicates whether the rendered scene represents a preliminary or the fully evaluated pipeline state.
	bool _isPreliminaryState = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FrameGraph::RenderingCommand::Flags);

}	// End of namespace

#include <ovito/core/dataset/scene/Pipeline.h>
