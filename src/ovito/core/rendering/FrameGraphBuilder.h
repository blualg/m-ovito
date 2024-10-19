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
#include "FrameGraph.h"

namespace Ovito {

/**
 * A helper object that takes care of building a FrameGraph for rendering a scene.
*/
class OVITO_CORE_EXPORT FrameGraphBuilder
{
    Q_DISABLE_COPY(FrameGraphBuilder)

public:

	/// Generates the frame graph contents for a scene.
	static Future<void> build(OORef<FrameGraph> frameGraph, Scene* scene, Viewport* viewport, const QRect& logicalViewportRect = {}, const QRect& physicalViewportRect = {}, const ViewProjectionParameters& noninteractiveProjParams = {});

	/// Constructor (for internal use only).
	FrameGraphBuilder(OORef<FrameGraph> frameGraph, Scene* scene, Viewport* viewport) :
		_fg(std::move(frameGraph)),
		_scene(scene),
		_viewport(viewport) {}

private:

	/// Builds the list of visible scene pipeline nodes to be rendered.
	void compilePipelinesList();

	/// Evaluates all visible pipelines in the scene to obtain their output data.
	Future<void> gatherPipelineResults();

	/// Asks all visual elements to render.
	void renderVisElements();

	/// Visits all vis elements of all data objects in the collection and lets them populate the frame graph.
	void gatherVisElements(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath);

	/// Waits for all visual elements to finish rendering.
	Future<void> waitForVisElements();

	/// Waits for all viewport overlays to finish rendering.
	Future<void> waitForViewportLayers();

	/// Handles the status returned by a vis element's or overlay's render() method.
	void handleRenderResult(ActiveObject* object, PipelineStatus&& status);

	/// Handles an exception thrown by a vis element's or overlay's render() method.
	void handleRenderException(ActiveObject* object, Exception& ex);

	/// Render the overlays/underlays of a viewport.
	void renderOverlays(const QVector<OORef<ViewportOverlay>>& overlays, FrameGraph::RenderLayerType layerType, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams);

private:

	/// The frame graph to build.
	OORef<FrameGraph> _fg;

	/// The scene to be rendered.
	OORef<Scene> _scene;

	/// The viewport to be rendered.
	OORef<Viewport> _viewport;

	/// The list of visible pipeline scene nodes to be rendered.
	std::vector<OORef<Pipeline>> _visiblePipelines;

	/// The results obtained from the pipelines in the scene.
	std::vector<PipelineFlowState> _pipelineResults;

	/// The list of asynchronous visual elements.
	std::vector<OORef<DataVis>> _asyncVisElements;

	/// The list of asynchronous visual element rendering tasks.
	std::vector<Future<PipelineStatus>> _asyncVisElementFutures;

	/// The list of asynchronous viewport layers.
	std::vector<OORef<ViewportOverlay>> _asyncViewportLayers;

	/// The list of asynchronous viewport layer rendering tasks.
	std::vector<Future<PipelineStatus>> _asyncViewportLayersFutures;
};

}	// End of namespace
