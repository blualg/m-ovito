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
#include <ovito/core/rendering/FrameGraphBuilder.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include <ovito/core/utilities/concurrent/CoroutinePromise.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

/******************************************************************************
* Generates the frame graph contents for a scene.
******************************************************************************/
Future<void> FrameGraphBuilder::build(OORef<FrameGraph> frameGraph, Scene* scene, Viewport* viewport, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams)
{
    FrameGraphBuilder builder(frameGraph, scene, viewport);

    // Gather list of pipelines to render.
    builder.compilePipelinesList();

    // Render viewport underlays and overlays.
    bool hasAsyncOverlays = false;
    if(!logicalViewportRect.isEmpty() && !physicalViewportRect.isEmpty()) {
        builder.renderOverlays(viewport->underlays(), FrameGraph::UnderLayer, logicalViewportRect, physicalViewportRect, noninteractiveProjParams);
        builder.renderOverlays(viewport->overlays(), FrameGraph::OverLayer, logicalViewportRect, physicalViewportRect, noninteractiveProjParams);
        hasAsyncOverlays = !builder._asyncViewportLayersFutures.empty();
    }

    // Obtain the output of all visible scene pipelines.
    co_await FutureAwaiter(ObjectExecutor(frameGraph), builder.gatherPipelineResults());

    // Visit all vis elements and let them populate the frame graph.
    builder.renderVisElements();

    // Wait for all visual elements to finish rendering.
    co_await FutureAwaiter(ObjectExecutor(frameGraph), builder.waitForVisElements());

    // Wait for all viewport overlays to finish rendering.
    if(hasAsyncOverlays)
        co_await FutureAwaiter(ObjectExecutor(frameGraph), builder.waitForViewportLayers());

    // Compute combined scene bounding box.
    builder._fg->computeSceneBoundingBox();
}

/******************************************************************************
* Builds the list of visible scene pipeline nodes to be rendered.
******************************************************************************/
void FrameGraphBuilder::compilePipelinesList()
{
    // Visit all pipelines in the current scene.
    _scene->visitPipelines([&](Pipeline* pipeline) {

        // Skip pipeline if it is hidden in the current viewport.
        if(_viewport && pipeline->isHiddenInViewport(_viewport, false))
            return true;

        // Skip pipeline if it is the view node of the viewport or if it is the target of the view node.
        if(_viewport && _viewport->viewNode() && (_viewport->viewNode() == pipeline || _viewport->viewNode()->lookatTargetNode() == pipeline))
            return true;

        // Add the pipeline to the list of pipeline to be rendered.
        _visiblePipelines.push_back(pipeline);

        return true;
    });
}

/******************************************************************************
* Evaluates all visible pipelines in the scene to obtain their output data.
******************************************************************************/
Future<void> FrameGraphBuilder::gatherPipelineResults()
{
    for(const auto& pipeline : _visiblePipelines) {
        // Request pipeline result.
        PipelineEvaluationResult pipelineResult = pipeline->evaluatePipeline(PipelineEvaluationRequest(_fg->time(), _fg->stopOnPipelineError(), _fg->isInteractive()));

        // Flag the entire frame graph as preliminary if the pipeline output is preliminary.
        if(pipelineResult.evaluationTypes().testFlag(PipelineEvaluationResult::EvaluationType::Noninteractive) == false)
            _fg->setIsPreliminaryState(true);

        _pipelineResults.push_back(co_await FutureAwaiter(DeferredObjectExecutor(_fg), std::move(pipelineResult).asFuture()));
    }
}

/******************************************************************************
* Asks all visual elements to render.
******************************************************************************/
void FrameGraphBuilder::renderVisElements()
{
    OVITO_ASSERT(_pipelineResults.size() == _visiblePipelines.size());
    auto pipeline = _visiblePipelines.begin();
    ConstDataObjectPath dataObjectPath;
    for(const PipelineFlowState& state : _pipelineResults) {
        // Visit all vis elements of all data objects in the pipeline state.
        if(state)
            gatherVisElements(state.data(), *pipeline, state, dataObjectPath);
        OVITO_ASSERT(dataObjectPath.empty());
        ++pipeline;
    }
}

/******************************************************************************
* Visits all vis elements of all data objects in the collection and lets them
* populate the frame graph.
******************************************************************************/
void FrameGraphBuilder::gatherVisElements(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath)
{
    bool isOnStack = false;

    // Call all attached vis elements of the data object.
    for(DataVis* vis : dataObj->visElements()) {
        // Let the PipelineSceneNode substitute the vis element with another one.
        vis = pipeline->getReplacementVisElement(vis);
        if(vis->isEnabled()) {
            // Push the data object onto the stack.
            if(!isOnStack) {
                dataObjectPath.push_back(dataObj);
                isOnStack = true;
            }
            try {
                // Let the vis element do the rendering.
                auto result = vis->render(dataObjectPath, state, *_fg, pipeline);

                // Check if rendering happened synchronously or asynchronously.
                if(PipelineStatus* status = std::get_if<PipelineStatus>(&result)) {
                    handleRenderResult(vis, std::move(*status));
                }
                else if(Future<PipelineStatus>* future = std::get_if<Future<PipelineStatus>>(&result)) {
                    // Collect all unfinished futures in a list to await them later.
                    if(!future->isFinished()) {
                        _asyncVisElementFutures.push_back(std::move(*future));
                        _asyncVisElements.push_back(vis);
                    }
                    else
                        handleRenderResult(vis, future->result());
                }
                else OVITO_ASSERT(false);
            }
            catch(Exception& ex) {
                handleRenderException(vis, ex);
            }
        }
    }

    // Recursively visit the sub-objects of the data object.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        // Push the next data object onto the stack.
        if(!isOnStack) {
            dataObjectPath.push_back(dataObj);
            isOnStack = true;
        }
        gatherVisElements(subObject, pipeline, state, dataObjectPath);
        return false;
    });

    // Pop the data object from the stack.
    if(isOnStack)
        dataObjectPath.pop_back();
}

/******************************************************************************
* Waits for all visual elements to finish rendering.
******************************************************************************/
Future<void> FrameGraphBuilder::waitForVisElements()
{
    // Can return immediately if there are no asynchronous visual elements.
    if(_asyncVisElementFutures.empty())
        co_return;

    // Wait for all asynchronous visual elements to finish rendering.
    std::vector<Future<PipelineStatus>> asyncVisElementFutures = co_await FutureAwaiter(ObjectExecutor(_fg), when_all_futures(std::move(_asyncVisElementFutures)));

    // Once all future results are available, handle them one by one.
    OVITO_ASSERT(asyncVisElementFutures.size() == _asyncVisElements.size());
    for(size_t i = 0; i < asyncVisElementFutures.size(); ++i) {
        DataVis* vis = _asyncVisElements[i];
        try {
            if(!asyncVisElementFutures[i].isCanceled())
                handleRenderResult(vis, asyncVisElementFutures[i].result());
        }
        catch(Exception& ex) {
            handleRenderException(vis, ex);
        }
    }
}

/******************************************************************************
* Waits for all viewport overlays to finish rendering.
******************************************************************************/
Future<void> FrameGraphBuilder::waitForViewportLayers()
{
    OVITO_ASSERT(!_asyncViewportLayersFutures.empty());

    // Wait for all asynchronous viewport layers to finish rendering.
    std::vector<Future<PipelineStatus>> asyncViewportLayersFutures = co_await FutureAwaiter(ObjectExecutor(_fg), when_all_futures(std::move(_asyncViewportLayersFutures)));

    // Once all future results are available, handle them one by one.
    OVITO_ASSERT(asyncViewportLayersFutures.size() == _asyncViewportLayers.size());
    for(size_t i = 0; i < asyncViewportLayersFutures.size(); ++i) {
        ViewportOverlay* overlay = _asyncViewportLayers[i];
        try {
            if(!asyncViewportLayersFutures[i].isCanceled())
                handleRenderResult(overlay, asyncViewportLayersFutures[i].result());
        }
        catch(Exception& ex) {
            handleRenderException(overlay, ex);
        }
    }
}

/******************************************************************************
* Handles the status returned by a vis element's or overlay's render() method.
******************************************************************************/
void FrameGraphBuilder::handleRenderResult(ActiveObject* object, PipelineStatus&& status)
{
    // Pass error status codes to the exception handler.
    if(status.type() == PipelineStatus::Error)
        throw Exception(status.text());

    // In console mode, print warning messages to the terminal.
    if(status.type() == PipelineStatus::Warning && !status.text().isEmpty() && !Application::guiMode()) {
        if(dynamic_object_cast<DataVis>(object))
            qWarning() << "WARNING: Visual element" << object->objectTitle() << "reported:" << status.text();
        else if(dynamic_object_cast<ViewportOverlay>(object))
            qWarning() << "WARNING: Viewport layer" << object->objectTitle() << "reported:" << status.text();
        else
            OVITO_ASSERT(false);
    }

    // Display current status in the GUI.
    object->setStatus(std::move(status));
}

/******************************************************************************
* Handles an exception thrown by a vis element's or overlay's render() method.
******************************************************************************/
void FrameGraphBuilder::handleRenderException(ActiveObject* object, Exception& ex)
{
    // Indicate error status in the GUI.
    object->setStatus(ex);

    // If the rendering of the element failed, interrupt entire rendering process in console mode; swallow exceptions in GUI mode.
    if(_fg->stopOnPipelineError()) {
        if(dynamic_object_cast<DataVis>(object))
            throw ex.prependToMessage(QStringLiteral("Visual element '%1' reported an error: ").arg(object->objectTitle()));
        else if(dynamic_object_cast<ViewportOverlay>(object))
            throw ex.prependToMessage(QStringLiteral("Viewport layer '%1' reported an error: ").arg(object->objectTitle()));
        else
            OVITO_ASSERT(false);
    }
}

/******************************************************************************
* Render the overlays/underlays of a viewport.
******************************************************************************/
void FrameGraphBuilder::renderOverlays(const QVector<OORef<ViewportOverlay>>& overlays, FrameGraph::RenderLayerType layerType, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams)
{
    for(ViewportOverlay* layer : overlays) {
        if(layer->isEnabled()) {
            try {
                // Let the layer do the rendering.
                auto result = layer->render(*_fg, _fg->addCommandGroup(layerType), logicalViewportRect, physicalViewportRect, noninteractiveProjParams, _scene);

                // Check if rendering happened synchronously or asynchronously.
                if(PipelineStatus* status = std::get_if<PipelineStatus>(&result)) {
                    handleRenderResult(layer, std::move(*status));
                }
                else if(Future<PipelineStatus>* future = std::get_if<Future<PipelineStatus>>(&result)) {
                    // Collect all unfinished futures in a list to await them later.
                    if(!future->isFinished()) {
                        _asyncViewportLayersFutures.push_back(std::move(*future));
                        _asyncViewportLayers.push_back(layer);
                    }
                    else
                        handleRenderResult(layer, future->result());
                }
                else OVITO_ASSERT(false);
            }
            catch(Exception& ex) {
                handleRenderException(layer, ex);
            }
        }
    }
}

}   // End of namespace
