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

class OVITO_CORE_EXPORT FrameGraph
{
public:

	class OVITO_CORE_EXPORT RenderingCommand
	{
	public:

		/// Bit-wise flags for rendering commands.
		enum Flag {
			NoFlags          = 0,
			SkipDepthTesting = (1<<0), // Render the primitive without depth testing
			SkipVisualPass   = (1<<1), // Skip the primitive in the visual pass
			SkipPickingPass  = (1<<2), // Skip the primitive in the object picking pass
		};
		Q_DECLARE_FLAGS(Flags, Flag);

		/// Constructor taking a 3d rendering primitive.
		explicit RenderingCommand(std::unique_ptr<FrameGraphPrimitive> primitive, const AffineTransformation& tm, Flags flags = NoFlags) : _primitive(std::move(primitive)), _tm(tm), _flags(flags) {}

		/// Constructor taking a 2d rendering primitive.
		explicit RenderingCommand(std::unique_ptr<FrameGraphPrimitive> primitive, Flags flags = SkipDepthTesting) : _primitive(std::move(primitive)), _flags(flags) {}

		/// Returns the graphics primitive.
		FrameGraphPrimitive* primitive() const { return _primitive.get(); }

		/// Replaces the graphics primitive with a new one.
		void setPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive) { _primitive = std::move(primitive); }

		/// Returns the model-to-world transformation matrix.
		const AffineTransformation& modelWorldTM() const { return _tm; }

		/// Returns whether depth testing should be disabled while rendering the primitive.
		bool skipDepthTesting() const { return _flags.testFlag(SkipDepthTesting); }

	private:

		/// The graphics primitive.
		std::unique_ptr<FrameGraphPrimitive> _primitive;

		/// The world transformation matrix.
		AffineTransformation _tm = AffineTransformation::Zero();

		/// The bit-wise flags for the rendering command.
		Flags _flags = NoFlags;
	};

public:

	/// Constructor.
	FrameGraph(RendererResourceCache::ResourceFrame visCache, AnimationTime time, const ViewProjectionParameters& projectionParams, QSize viewportDeviceIndependentSize, bool isInteractive, bool isPreviewMode, bool stopOnPipelineError, QImage::Format preferredImageFormat, qreal devicePixelRatio) :
		_visCache(std::move(visCache)),
		_time(time),
		_projectionParams(projectionParams),
		_viewportDeviceIndependentSize(viewportDeviceIndependentSize),
		_isInteractive(isInteractive),
		_isPreviewMode(isPreviewMode),
		_stopOnPipelineError(stopOnPipelineError),
		_preferredImageFormat(preferredImageFormat),
		_devicePixelRatio(devicePixelRatio) {}

	/// Returns the data cache to be used by visualization elements.
	const RendererResourceCache::ResourceFrame& visCache() const { return _visCache; }

	/// Releases the data cache of the frame graph after use.
	RendererResourceCache::ResourceFrame takeVisCache() && {
		_commands.clear();
		return std::move(_visCache);
	}

	/// Returns the animation time being rendered.
	AnimationTime time() const { return _time; }

	/// Returns whether we are rendering an interactive viewport or not.
	bool isInteractive() const { return _isInteractive; }

	/// Returns whether preview mode is active in the interactive viewport being rendered.
	bool isPreviewMode() const { return _isPreviewMode; }

	/// Returns whether the rendering should be stopped when an error occurs in a data pipeline.
	bool stopOnPipelineError() const { return _stopOnPipelineError; }

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

	/// Returns the sequence of recorded rendering commands.
	const std::vector<RenderingCommand>& commands() const { return _commands; }

	/// Appends a rendering command to the frame graph.
	template<typename... Args>
	void addCommand(Args&&... args) { _commands.emplace_back(std::forward<Args>(args)...); }

	/// Add a 3d graphics primitive to the frame graph.
	void addPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive, const SceneNode* sceneNode, OORef<ObjectPickInfo> pickInfo = {}, RenderingCommand::Flags flags = RenderingCommand::NoFlags);

	/// Add a 3d graphics primitive to the frame graph.
	void addPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive, const AffineTransformation& modelTM, const Box3& boundingBox = {}, const SceneNode* sceneNode = {}, RenderingCommand::Flags flags = RenderingCommand::NoFlags);

	/// Generates the visual representation of a scene node (and all its children).
	bool renderSceneNode(OORef<SceneNode> node, OORef<Viewport> viewport);

	/// Generates the visual representation of a data object and all its sub-objects.
	bool renderDataObject(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath);

	/// Render the overlays/underlays of a viewport.
	bool renderOverlays(Viewport* viewport, bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams);

	/// Renders a 2d polyline or polygon into an interactive viewport.
	void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed);

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

	/// The color to clear the framebuffer with.
	ColorA _clearColor = ColorA(0, 0, 0, 0);

	/// The world-space bounding box of the 3d scene.
	Box3 _sceneBoundingBox;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FrameGraph::RenderingCommand::Flags);

}	// End of namespace
