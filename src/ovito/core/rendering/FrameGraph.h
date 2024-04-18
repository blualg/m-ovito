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
	virtual QString infoString(Pipeline* pipeline, quint32 subobjectId) { return {}; }
};

/**
 * The FrameGraph class represents a sequence of rendering commands that are used to generate
 * a single frame of a 3D scene. The recorded rendering commands can be executed by a SceneRenderer to produce an image.
*/
class OVITO_CORE_EXPORT FrameGraph
{
public:

    /// The render layers that get rendered on top of each other.
    enum RenderLayer {
        UnderLayer,
        SceneLayer,
        OverLayer
    };

	class OVITO_CORE_EXPORT RenderingCommand
	{
	public:

		/// Bit-wise flags for rendering commands.
		enum Flag {
			NoFlags             = 0,
			NoDepthTesting      = (1<<0), // Render the primitive without depth testing
			ExcludeFromVisual   = (1<<1), // Skip the primitive in the visual rendering pass
		};
		Q_DECLARE_FLAGS(Flags, Flag);

		/// Constructor taking a 3d rendering primitive.
		explicit RenderingCommand(RenderLayer renderLayer, std::unique_ptr<FrameGraphPrimitive> primitive, const AffineTransformation& tm, int pickingGroupId, Flags flags) :
			_primitive(std::move(primitive)), _tm(tm), _pickingGroupId(pickingGroupId), _flags(flags), _renderLayer(renderLayer) {}

		/// Constructor taking a 2d rendering primitive.
		explicit RenderingCommand(RenderLayer renderLayer, std::unique_ptr<FrameGraphPrimitive> primitive, Flags flags = Flags(NoDepthTesting)) : _primitive(std::move(primitive)), _flags(flags), _renderLayer(renderLayer) {}

		/// Returns the graphics primitive rendered by this command.
		FrameGraphPrimitive* primitive() const { return _primitive.get(); }

		/// Replaces the graphics primitive with a new one.
		void setPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive) { _primitive = std::move(primitive); }

		/// Returns the model-to-world transformation matrix to be applied to the graphics primitive.
		const AffineTransformation& modelWorldTM() const { return _tm; }

		/// Returns the image layer the primitive should be rendered on.
		RenderLayer renderLayer() const { return _renderLayer; }

		/// Returns whether depth testing should be disabled while rendering the primitive.
		bool noDepthTesting() const { return _flags.testFlag(NoDepthTesting); }

		/// Returns whether this command should be skipped in object picking render mode.
		bool skipInPickingPass() const { return pickingGroupId() == 0; }

		/// Returns whether this command should be skipped in visual render mode.
		bool skipInVisualPass() const { return _flags.testFlag(ExcludeFromVisual); }

		/// Returns the object picking group to which the primitive belongs.
		int pickingGroupId() const { return _pickingGroupId; }

	private:

		/// The graphics primitive to be rendered.
		std::unique_ptr<FrameGraphPrimitive> _primitive;

		/// The model-to-world transformation matrix.
		AffineTransformation _tm = AffineTransformation::Zero();

		/// The object picking group to which the primitive belongs.
		int _pickingGroupId = 0;

		/// The bit-wise flags for the rendering command.
		Flags _flags = NoFlags;

		/// The image layer the primitive should be rendered on.
		RenderLayer _renderLayer = SceneLayer;
	};

	class OVITO_CORE_EXPORT ObjectPickingGroup
	{
	public:

		/// Constructor.
		ObjectPickingGroup(OORef<Pipeline> pipeline, OORef<ObjectPickInfo> pickInfo) :
			_pipeline(std::move(pipeline)),
			_pickInfo(std::move(pickInfo)) {}

		/// Returns the scene node associated with this group.
		const OORef<Pipeline>& pipeline() const { return _pipeline; }

		/// Returns the option picking info object, which determines the part of the dataset that was picked.
		const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

	private:

		/// The scene pipeline being rendered.
		/// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
		OORef<Pipeline> _pipeline;

		/// An additional picking info object, which determines the part of the dataset being rendered.
		OORef<ObjectPickInfo> _pickInfo;
	};

public:

	/// Constructor.
	FrameGraph(RendererResourceCache::ResourceFrame visCache, AnimationTime time, const ViewProjectionParameters& projectionParams, QSize viewportDeviceIndependentSize, bool isInteractive, bool isPreviewMode, bool stopOnPipelineError, QImage::Format preferredImageFormat, qreal devicePixelRatio) :
		_visCache(std::move(visCache)),
		_time(time),
		_projectionParams(projectionParams),
		_isInteractive(isInteractive),
		_isPreviewMode(isPreviewMode),
		_stopOnPipelineError(stopOnPipelineError),
		_preferredImageFormat(preferredImageFormat),
		_devicePixelRatio(devicePixelRatio),
		_viewportDeviceIndependentSize(viewportDeviceIndependentSize) {}

	/// Returns the data cache to be used by visualization elements.
	const RendererResourceCache::ResourceFrame& visCache() const { OVITO_ASSERT(_visCache); return _visCache; }

	/// Releases the data cache of the frame graph after use.
	RendererResourceCache::ResourceFrame takeVisCache() { return std::move(_visCache); }

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

	/// Returns the line rendering width to use in object picking mode.
	FloatType defaultLinePickingWidth() const;

	/// Returns the layer new rendering primitives should be added to.
	void setCurrentRenderLayer(RenderLayer layer) { _currentRenderLayer = layer; }

	/// Returns the sequence of recorded rendering commands.
	const std::vector<RenderingCommand>& commands() const { return _commands; }

	/// Returns the list of defined object picking groups.
	const std::vector<ObjectPickingGroup>& pickingGroups() const { return _pickingGroups; }

	/// Appends a rendering command to the frame graph.
	template<typename... Args>
	void addCommand(Args&&... args) {
		_commands.emplace_back(_currentRenderLayer, std::forward<Args>(args)...);
		// Make sure that the picking groups are numbered and used correctly.
		OVITO_ASSERT(_commands.back().skipInPickingPass() || _commands.back().pickingGroupId() == _pickingGroups.size());
	}

	/// Add a 3d graphics primitive to the frame graph.
	void addPrimitive(
		std::unique_ptr<FrameGraphPrimitive> primitive,
		const Pipeline* pipeline,
		int pickingGroup = 0,
		const Box3& boundingBox = {},
		RenderingCommand::Flags flags = RenderingCommand::NoFlags);

	/// Add a 3d graphics primitive to the frame graph.
	void addPrimitive(
		std::unique_ptr<FrameGraphPrimitive> primitive,
		const AffineTransformation& modelTM,
		int pickingGroup,
		const Box3& boundingBox = {},
		RenderingCommand::Flags flags = RenderingCommand::NoFlags);

	/// Add a 3d graphics primitive to the frame graph, which is excluded from object picking.
	void addPrimitive(
		std::unique_ptr<FrameGraphPrimitive> primitive,
		const AffineTransformation& modelTM,
		const Box3& boundingBox = {}) {
			addPrimitive(std::move(primitive), modelTM, 0, boundingBox);
		}

	/// Creates a new object picking group.
	int addPickingGroup(const Pipeline* pipeline, OORef<ObjectPickInfo> pickInfo = {}) {
		_pickingGroups.emplace_back(const_cast<Pipeline*>(pipeline), std::move(pickInfo));
		return _pickingGroups.size(); // Return 1-based group ID.
	}

	/// Generates the visual representation of a scene node (and all its children).
	void renderSceneNode(OORef<SceneNode> node, OORef<Viewport> viewport);

	/// Generates the visual representation of a data object and all its sub-objects.
	void renderDataObject(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath);

	/// Render the overlays/underlays of a viewport.
	void renderOverlays(Viewport* viewport, bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams);

	/// Renders a 2d polyline or polygon into an interactive viewport.
	void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed, const QSize& logicalViewportSize);

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

	/// The recorded rendering commands.
	std::vector<RenderingCommand> _commands;

	/// The object picking groups.
	std::vector<ObjectPickingGroup> _pickingGroups;

	/// The color to clear the framebuffer with.
	ColorA _clearColor = ColorA(0, 0, 0, 0);

	/// The world-space bounding box of the 3d scene.
	Box3 _sceneBoundingBox;

	/// The current layer rendering primitives should be added to.
	RenderLayer _currentRenderLayer = SceneLayer;

	/// Indicates whether the rendered scene represents a preliminary or the fully evaluated pipeline state.
	bool _isPreliminaryState = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FrameGraph::RenderingCommand::Flags);

}	// End of namespace

#include <ovito/core/dataset/scene/Pipeline.h>
