////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/viewport/ViewportGizmo.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(SceneRenderer);
IMPLEMENT_OVITO_CLASS(ObjectPickInfo);

/******************************************************************************
* Returns the device pixel ratio of the output device we are rendering to.
******************************************************************************/
qreal SceneRenderer::devicePixelRatio() const
{
    if(viewport() && isInteractive()) {
        // Query the device pixel ratio from the UI window associated with the viewport we are rendering into.
        if(ViewportWindowInterface* window = viewport()->window())
            return window->devicePixelRatio();
    }

    return 1.0;
}

/******************************************************************************
* Returns the line rendering width to use in object picking mode.
******************************************************************************/
FloatType SceneRenderer::defaultLinePickingWidth()
{
    return FloatType(6) * devicePixelRatio();
}

/******************************************************************************
* Computes the bounding box of the entire scene to be rendered.
******************************************************************************/
Box3 SceneRenderer::computeSceneBoundingBox(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp)
{
    OVITO_CHECK_OBJECT_POINTER(scene);
    OVITO_ASSERT(!_scene);

    try {
        _sceneBoundingBox.setEmpty();
        _isBoundingBoxPass = true;
        _time = time;
        _viewport = vp;
        _scene = scene;
        setProjParams(params);

        // Perform bounding box rendering pass.
        if(renderScene()) {

            // Include other visual content that is only visible in the interactive viewports.
            if(isInteractive())
                renderInteractiveContent();
        }

        _isBoundingBoxPass = false;
    }
    catch(...) {
        _isBoundingBoxPass = false;
        throw;
    }

    _scene = nullptr;
    return _sceneBoundingBox;
}

/******************************************************************************
* Prepares the renderer for rendering one or more frames.
******************************************************************************/
bool SceneRenderer::startRender(const RenderSettings* settings, const QSize& frameBufferSize, MixedKeyCache& visCache)
{
    OVITO_ASSERT_MSG(_renderSettings == nullptr, "SceneRenderer::startRender()", "startRender() called again without calling endRender() first.");
    _renderSettings = settings;
    _visCache = &visCache;
    return true;
}

/******************************************************************************
* Is called after rendering has finished.
******************************************************************************/
void SceneRenderer::endRender()
{
    _renderSettings = nullptr;
    _visCache = nullptr;
}

/******************************************************************************
* Sets the view projection parameters, the animation frame to render,
* and the viewport being rendered.
******************************************************************************/
void SceneRenderer::beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp,
                               const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    OVITO_ASSERT(!_scene);

    _time = time;
    _scene = scene;
    setProjParams(params);
    _viewport = vp;
    _viewportRect = viewportRect;
    _frameBuffer = frameBuffer;
    _modelWorldTM.setIdentity();
    _modelViewTM = projParams().viewMatrix;
}

/******************************************************************************
* Sets the view projection parameters, the animation frame to render,
* and the viewport being rendered.
******************************************************************************/
void SceneRenderer::endFrame(bool renderingSuccessful, const QRect& viewportRect)
{
    endPickObject();
    _scene.reset();
}

/******************************************************************************
* Renders all nodes in the scene
******************************************************************************/
bool SceneRenderer::renderScene()
{
    if(scene()) {
        // Recursively render all scene nodes.
        return renderNode(scene());
    }

    return true;
}

/******************************************************************************
* Render a scene node (and all its children).
******************************************************************************/
bool SceneRenderer::renderNode(SceneNode* node)
{
    OVITO_ASSERT(scene());
    OVITO_CHECK_OBJECT_POINTER(node);

    // Skip node if it is hidden in the current viewport.
    if(viewport() && node->isHiddenInViewport(viewport(), false))
        return true;

    // Set up transformation matrix.
    TimeInterval interval;
    const AffineTransformation& nodeTM = node->getWorldTransform(time(), interval);
    setWorldTransform(nodeTM);

    if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(node)) {

        // Do not render node if it is the view node of the viewport or
        // if it is the target of the view node.
        if(!viewport() || !viewport()->viewNode() || (viewport()->viewNode() != node && viewport()->viewNode()->lookatTargetNode() != node)) {

            // Evaluate data pipeline of object node and render the results.
            PipelineEvaluationFuture pipelineEvaluation;
            if(waitForLongOperationsEnabled()) {
                pipelineEvaluation = pipeline->evaluateRenderingPipeline(PipelineEvaluationRequest(time()));
                if(!pipelineEvaluation.waitForFinished())
                    return false;

                // After the rendering process has been temporarily interrupted above, rendering is resumed now.
                // Give the renderer the opportunity to restore any state that must be active (e.g. the active OpenGL context).
                resumeRendering();
            }
            const PipelineFlowState& state = pipelineEvaluation.isValid() ?
                                                pipelineEvaluation.result() :
                                                pipeline->evaluatePipelineSynchronous(time(), true);

            if(state) {
                // Invoke all vis elements of all data objects in the pipeline state.
                ConstDataObjectPath dataObjectPath;
                renderDataObject(state.data(), pipeline, state, dataObjectPath);
                OVITO_ASSERT(dataObjectPath.empty());
            }
        }
    }

    // Render trajectory when node transformation is animated.
    if(isInteractive() && !isPicking()) {
        renderNodeTrajectory(node);
    }

    // Render child nodes.
    for(SceneNode* child : node->children()) {
        if(!renderNode(child))
            return false;
    }

    return true;
}

/******************************************************************************
* Renders a data object and all its sub-objects.
******************************************************************************/
void SceneRenderer::renderDataObject(const DataObject* dataObj, const PipelineSceneNode* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath)
{
    bool isOnStack = false;

    // Call all vis elements of the data object.
    for(DataVis* vis : dataObj->visElements()) {
        // Let the PipelineSceneNode substitude the vis element with another one.
        vis = pipeline->getReplacementVisElement(vis);
        if(vis->isEnabled()) {
            // Push the data object onto the stack.
            if(!isOnStack) {
                dataObjectPath.push_back(dataObj);
                isOnStack = true;
            }
            PipelineStatus status;
            try {
                // Let the vis element do the rendering.
                status = vis->render(time(), dataObjectPath, state, this, pipeline);
                // Pass error status codes to the exception handler below.
                if(status.type() == PipelineStatus::Error)
                    throw Exception(status.text());
                // In console mode, print warning messages to the terminal.
                if(status.type() == PipelineStatus::Warning && !status.text().isEmpty() && Application::instance()->consoleMode()) {
                    qWarning() << "WARNING: Visual element" << vis->objectTitle() << "reported:" << status.text();
                }
            }
            catch(SceneRenderer::RendererException&) {
                // Always interrupt rendering process by rethrowing the exception.
                throw;
            }
            catch(Exception& ex) {
                status = ex;
                ex.prependGeneralMessage(tr("Visual element '%1' reported an error during rendering.").arg(vis->objectTitle()));
                // If the vis element fails, interrupt rendering process in console mode; swallow exceptions in GUI mode.
                if(!isInteractive()) throw;
            }
            // Unless the vis element has indicated that it is in control of the status,
            // automatically adopt the outcome of the rendering operation as status code.
            if(!vis->manualErrorStateControl())
                vis->setStatus(status);
        }
    }

    // Recursively visit the sub-objects of the data object and render them as well.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        // Push the data object onto the stack.
        if(!isOnStack) {
            dataObjectPath.push_back(dataObj);
            isOnStack = true;
        }
        renderDataObject(subObject, pipeline, state, dataObjectPath);
        return false;
    });

    // Pop the data object from the stack.
    if(isOnStack) {
        dataObjectPath.pop_back();
    }
}

/******************************************************************************
* Renders the overlays/underlays of the viewport into the framebuffer.
******************************************************************************/
bool SceneRenderer::renderOverlays(bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect, MainThreadOperation& operation)
{
    OVITO_ASSERT(!isPicking());
    OVITO_ASSERT(viewport());

    for(ViewportOverlay* layer : (underlays ? viewport()->underlays() : viewport()->overlays())) {
        if(layer->isEnabled()) {
            layer->render(this, logicalViewportRect, physicalViewportRect, operation);
            if(operation.isCanceled())
                return false;
        }
    }
    return !operation.isCanceled();
}

/******************************************************************************
 * Gets the trajectory of motion of a node. The returned data buffer stores an
 * array of Point3 (if the node's position is animated) or a null pointer
 * (if the node's position is static).
 ******************************************************************************/
ConstDataBufferPtr SceneRenderer::getNodeTrajectory(const SceneNode* node)
{
    Controller* ctrl = node->transformationController();
    if(ctrl && ctrl->isAnimated()) {
        AnimationSettings* animSettings = scene()->animationSettings();
        int firstFrame = animSettings->firstFrame();
        int lastFrame = animSettings->lastFrame();
        OVITO_ASSERT(lastFrame >= firstFrame);
        BufferFactory<Point3G> vertices(lastFrame - firstFrame + 1);
        auto v = vertices.begin();
        for(int frame = firstFrame; frame <= lastFrame; frame++) {
            TimeInterval iv;
            const Vector3& pos = node->getWorldTransform(AnimationTime::fromFrame(frame), iv).translation();
            *v++ = Point3G::Origin() + pos.toDataType<GraphicsFloatType>();
        }
        OVITO_ASSERT(v == vertices.end());
        return vertices.take();
    }
    return {};
}

/******************************************************************************
* Renders the trajectory of motion of a node in the interactive viewports.
******************************************************************************/
void SceneRenderer::renderNodeTrajectory(const SceneNode* node)
{
    // Do not render the trajectory of the camera node of the viewport.
    if(viewport() && viewport()->viewNode() == node) return;

    if(ConstDataBufferPtr trajectory = getNodeTrajectory(node)) {
        setWorldTransform(AffineTransformation::Identity());

        if(!isBoundingBoxPass()) {

            // Render lines connecting the trajectory points.
            if(trajectory->size() >= 2) {
                BufferFactory<Point3G> lineVertices((trajectory->size() - 1) * 2);
                BufferReadAccess<Point3G> trajectoryPoints(trajectory);
                for(size_t index = 0; index < trajectory->size(); index++) {
                    if(index != 0)
                        lineVertices[index * 2 - 1] = trajectoryPoints[index];
                    if(index != trajectory->size() - 1)
                        lineVertices[index * 2] = trajectoryPoints[index];
                }
                LinePrimitive trajLine;
                trajLine.setPositions(lineVertices.take());
                trajLine.setUniformColor(ColorA(1.0, 0.8, 0.4));
                renderLines(trajLine);
            }

            // Render the trajectory points themselves using marker primitives.
            MarkerPrimitive frameMarkers(MarkerPrimitive::DotShape);
            frameMarkers.setPositions(std::move(trajectory));
            frameMarkers.setColor(ColorA(1, 1, 1));
            renderMarkers(frameMarkers);
        }
        else {
            Box3G bb;
            bb.addPoints(BufferReadAccess<Point3G>(trajectory));
            addToLocalBoundingBox(bb.toDataType<FloatType>());
        }
    }
}

/******************************************************************************
* This virtual method is responsible for rendering additional content that is only
* visible in the interactive viewports.
******************************************************************************/
void SceneRenderer::renderInteractiveContent()
{
    OVITO_ASSERT(viewport());
    OVITO_ASSERT(scene());

    // Render construction grid.
    if(viewport()->isGridVisible())
        renderGrid();

    // Render visual 3D representation of the modifiers.
    renderModifiers(false);

    // Render visual 2D representation of the modifiers.
    renderModifiers(true);

    // Render viewport gizmos.
    if(ViewportWindowInterface* viewportWindow = viewport()->window()) {
        // First, render 3D content.
        for(ViewportGizmo* gizmo : viewportWindow->viewportGizmos()) {
            gizmo->renderOverlay3D(viewport(), this);
        }
        // Then, render 2D content on top.
        for(ViewportGizmo* gizmo : viewportWindow->viewportGizmos()) {
            gizmo->renderOverlay2D(viewport(), this);
        }
    }
}

/******************************************************************************
* Renders the visual representation of the modifiers.
******************************************************************************/
void SceneRenderer::renderModifiers(bool renderOverlay)
{
    // Visit all objects in the scene.
    if(scene()) {
        scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
            renderModifiers(pipeline, renderOverlay);
            return true;
        });
    }
}

/******************************************************************************
* Renders the visual representation of the modifiers in a pipeline.
******************************************************************************/
void SceneRenderer::renderModifiers(PipelineSceneNode* pipeline, bool renderOverlay)
{
    ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(pipeline->dataProvider());
    while(modApp) {
        Modifier* mod = modApp->modifier();

        // Setup local transformation.
        TimeInterval interval;
        setWorldTransform(pipeline->getWorldTransform(time(), interval));

        try {
            // Render modifier.
            mod->renderModifierVisual(ModifierEvaluationRequest(scene()->animationSettings(), modApp), pipeline, this, renderOverlay);
        }
        catch(const Exception& ex) {
            // Swallow exceptions, because we are in interactive rendering mode.
            ex.logError();
        }

        // Traverse up the pipeline.
        modApp = dynamic_object_cast<ModifierApplication>(modApp->input());
    }
}

/******************************************************************************
* Renders a 2d polyline in the viewport.
******************************************************************************/
void SceneRenderer::render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed)
{
    if(isBoundingBoxPass())
        return;
    OVITO_ASSERT(count >= 2);

    LinePrimitive primitive;
    primitive.setUniformColor(color);

    BufferFactory<Point3G> vertices((closed ? count : count-1) * 2);
    Point3G* lineSegment = vertices.begin();
    for(int i = 0; i < count - 1; i++, lineSegment += 2) {
        lineSegment[0] = Point3G(points[i].x(), points[i].y(), 0.0);
        lineSegment[1] = Point3G(points[i+1].x(), points[i+1].y(), 0.0);
    }
    if(closed) {
        lineSegment[0] = Point3G(points[count-1].x(), points[count-1].y(), 0.0);
        lineSegment[1] = Point3G(points[0].x(), points[0].y(), 0.0);
        lineSegment += 2;
    }
    OVITO_ASSERT(lineSegment == vertices.end());
    primitive.setPositions(vertices.take());

    // Set up model-view-projection matrices.
    ViewProjectionParameters originalProjParams = projParams();
    ViewProjectionParameters newProjParams;
    newProjParams.aspectRatio = originalProjParams.aspectRatio;
    newProjParams.projectionMatrix = Matrix4::ortho(viewportRect().left(), viewportRect().right() + 1, viewportRect().bottom() + 1, viewportRect().top(), -1.0, 1.0);
    newProjParams.inverseProjectionMatrix = newProjParams.projectionMatrix.inverse();
    setProjParams(newProjParams);
    setWorldTransform(AffineTransformation::Identity());

    setDepthTestEnabled(false);
    renderLines(primitive);
    setDepthTestEnabled(true);

    setProjParams(originalProjParams);
}

/******************************************************************************
* Computes the world-space radius of an object located at the given world-space position,
* which should appear exactly one pixel wide in the rendered image.
******************************************************************************/
FloatType SceneRenderer::projectedPixelSize(const Point3& worldPosition) const
{
    // Get window size in device pixels.
    int height = viewportRect().height();
    if(height == 0) return 0;

    // The projected size in pixels:
    const FloatType baseSize = 1.0 * devicePixelRatio();

    if(projParams().isPerspective) {

        Point3 p = projParams().viewMatrix * worldPosition;
        if(p.z() == 0) return 1;

        Point3 p1 = projParams().projectionMatrix * p;
        Point3 p2 = projParams().projectionMatrix * (p + Vector3(1,0,0));

        return baseSize / (p1 - p2).length() / (FloatType)height;
    }
    else {
        return projParams().fieldOfView / (FloatType)height * baseSize;
    }
}

/******************************************************************************
* When picking mode is active, this registers an object being rendered.
******************************************************************************/
quint32 SceneRenderer::beginPickObject(const PipelineSceneNode* objNode, ObjectPickInfo* pickInfo)
{
    if(isPicking()) {
        _currentObjectPickingRecord.objectNode = const_cast<PipelineSceneNode*>(objNode);
        _currentObjectPickingRecord.pickInfo = pickInfo;
        _currentObjectPickingRecord.baseObjectID = _nextAvailablePickingID;
        return _currentObjectPickingRecord.baseObjectID;
    }
    return 0;
}

/******************************************************************************
* Registers a range of sub-IDs belonging to the current object being rendered.
******************************************************************************/
quint32 SceneRenderer::registerSubObjectIDs(quint32 subObjectCount, const ConstDataBufferPtr& indices)
{
    OVITO_ASSERT(isPicking());

    quint32 baseObjectID = _nextAvailablePickingID;
    if(indices)
        _currentObjectPickingRecord.indexedRanges.push_back(std::make_pair(indices, _nextAvailablePickingID - _currentObjectPickingRecord.baseObjectID));
    _nextAvailablePickingID += subObjectCount;
    return baseObjectID;
}

/******************************************************************************
* Call this when rendering of a pickable object is finished.
******************************************************************************/
void SceneRenderer::endPickObject()
{
    if(isPicking()) {
        if(_currentObjectPickingRecord.objectNode) {
            _objectPickingRecords.push_back(std::move(_currentObjectPickingRecord));
        }
        _currentObjectPickingRecord.baseObjectID = 0;
        _currentObjectPickingRecord.objectNode = nullptr;
        _currentObjectPickingRecord.pickInfo = nullptr;
        _currentObjectPickingRecord.indexedRanges.clear();
    }
}

/******************************************************************************
* Resets the internal state of the picking renderer and clears the stored object records.
******************************************************************************/
void SceneRenderer::resetPickingBuffer()
{
    endPickObject();
    _objectPickingRecords.clear();
#if 1
    _nextAvailablePickingID = 1;
#else
    // This can be enabled during debugging to avoid alpha!=1 pixels in the picking render buffer.
    _nextAvailablePickingID = 0xEF000000;
#endif
}

/******************************************************************************
* Given an object picking ID, looks up the corresponding record.
******************************************************************************/
const SceneRenderer::ObjectPickingRecord* SceneRenderer::lookupObjectPickingRecord(quint32 objectID) const
{
    if(objectID == 0 || _objectPickingRecords.empty())
        return nullptr;

    for(auto iter = _objectPickingRecords.begin(); iter != _objectPickingRecords.end(); iter++) {
        if(iter->baseObjectID > objectID) {
            OVITO_ASSERT(iter != _objectPickingRecords.begin());
            OVITO_ASSERT(objectID >= (iter-1)->baseObjectID);
            return &*std::prev(iter);
        }
    }

    OVITO_ASSERT(objectID >= _objectPickingRecords.back().baseObjectID);
    return &_objectPickingRecords.back();
}

/******************************************************************************
* Determines the range of the construction grid to display.
******************************************************************************/
std::tuple<FloatType, Box2I> SceneRenderer::determineGridRange(Viewport* vp)
{
    // Determine the area of the construction grid that is visible in the viewport.
    static const Point2 testPoints[] = {
        {-1,-1}, {1,-1}, {1, 1}, {-1, 1}, {0,1}, {0,-1}, {1,0}, {-1,0},
        {0,1}, {0,-1}, {1,0}, {-1,0}, {-1, 0.5}, {-1,-0.5}, {1,-0.5}, {1,0.5}, {0,0}
    };

    // Compute intersection points of test rays with grid plane.
    Box2 visibleGridRect;
    size_t numberOfIntersections = 0;
    for(size_t i = 0; i < sizeof(testPoints)/sizeof(testPoints[0]); i++) {
        Point3 p;
        if(vp->computeConstructionPlaneIntersection(testPoints[i], p, 0.1f)) {
            numberOfIntersections++;
            visibleGridRect.addPoint(p.x(), p.y());
        }
    }

    if(numberOfIntersections < 2) {
        // Cannot determine visible parts of the grid.
        return std::tuple<FloatType, Box2I>(0.0f, Box2I());
    }

    // Determine grid spacing adaptively.
    Point3 gridCenter(visibleGridRect.center().x(), visibleGridRect.center().y(), 0);
    FloatType gridSpacing = vp->nonScalingSize(vp->gridMatrix() * gridCenter) * 2.0f;
    // Round to nearest power of 10.
    gridSpacing = pow((FloatType)10, floor(log10(gridSpacing)));

    // Determine how many grid lines need to be rendered.
    int xstart = (int)floor(visibleGridRect.minc.x() / (gridSpacing * 10)) * 10;
    int xend = (int)ceil(visibleGridRect.maxc.x() / (gridSpacing * 10)) * 10;
    int ystart = (int)floor(visibleGridRect.minc.y() / (gridSpacing * 10)) * 10;
    int yend = (int)ceil(visibleGridRect.maxc.y() / (gridSpacing * 10)) * 10;

    return std::tuple<FloatType, Box2I>(gridSpacing, Box2I(Point2I(xstart, ystart), Point2I(xend, yend)));
}

/******************************************************************************
* Renders the construction grid.
******************************************************************************/
void SceneRenderer::renderGrid()
{
    if(isPicking())
        return;

    FloatType gridSpacing;
    Box2I gridRange;
    std::tie(gridSpacing, gridRange) = determineGridRange(viewport());
    if(gridSpacing <= 0) return;

    // Determine how many grid lines need to be rendered.
    int xstart = gridRange.minc.x();
    int ystart = gridRange.minc.y();
    int numLinesX = gridRange.size(0) + 1;
    int numLinesY = gridRange.size(1) + 1;

    FloatType xstartF = (FloatType)xstart * gridSpacing;
    FloatType ystartF = (FloatType)ystart * gridSpacing;
    FloatType xendF = (FloatType)(xstart + numLinesX - 1) * gridSpacing;
    FloatType yendF = (FloatType)(ystart + numLinesY - 1) * gridSpacing;

    setWorldTransform(viewport()->gridMatrix());

    if(!isBoundingBoxPass()) {

        // Allocate vertex buffer.
        int numVertices = 2 * (numLinesX + numLinesY);

        BufferFactory<Point3G> vertexPositions(numVertices);
        BufferFactory<ColorAG> vertexColors(numVertices);

        // Build lines array.
        const ColorAG color = Viewport::viewportColor(ViewportSettings::COLOR_GRID).toDataType<GraphicsFloatType>();
        const ColorAG majorColor = Viewport::viewportColor(ViewportSettings::COLOR_GRID_INTENS).toDataType<GraphicsFloatType>();
        const ColorAG majorMajorColor = Viewport::viewportColor(ViewportSettings::COLOR_GRID_AXIS).toDataType<GraphicsFloatType>();

        Point3G* v = vertexPositions.begin();
        ColorAG* c = vertexColors.begin();
        FloatType x = xstartF;
        for(int i = xstart; i < xstart + numLinesX; i++, x += gridSpacing, c += 2) {
            *v++ = Point3G(x, ystartF, 0);
            *v++ = Point3G(x, yendF, 0);
            if((i % 10) != 0)
                c[0] = c[1] = color;
            else if(i != 0)
                c[0] = c[1] = majorColor;
            else
                c[0] = c[1] = majorMajorColor;
        }
        FloatType y = ystartF;
        for(int i = ystart; i < ystart + numLinesY; i++, y += gridSpacing, c += 2) {
            *v++ = Point3G(xstartF, y, 0);
            *v++ = Point3G(xendF, y, 0);
            if((i % 10) != 0)
                c[0] = c[1] = color;
            else if(i != 0)
                c[0] = c[1] = majorColor;
            else
                c[0] = c[1] = majorMajorColor;
        }
        OVITO_ASSERT(c == vertexColors.end());

        // Render grid lines.
        LinePrimitive primitive;
        primitive.setPositions(vertexPositions.take());
        primitive.setColors(vertexColors.take());
        renderLines(primitive);
    }
    else {
        addToLocalBoundingBox(Box3(Point3(xstartF, ystartF, 0), Point3(xendF, yendF, 0)));
    }
}

/******************************************************************************
 * Renders a text primitive by means of a cached image primitive.
 ******************************************************************************/
void SceneRenderer::renderTextDefaultImplementation(const TextPrimitive& primitive, QImage::Format preferredImageFormat)
{
    if(primitive.text().isEmpty() || isPicking())
        return;

    qreal devicePixelRatio = this->devicePixelRatio();

    // Look up the image primitive for the text label in the cache.
    auto& [imagePrimitive, offset] = visCache().get<std::tuple<ImagePrimitive, QPointF>>(
        RendererResourceKey<struct TextImageCache, QString, ColorA, ColorA, FloatType, FloatType, qreal, QString, bool,
                            int, Qt::TextFormat>{primitive.text(), primitive.color(),
                                                 primitive.outlineColor(), primitive.outlineWidth(), primitive.rotation(),
                                                 devicePixelRatio, primitive.font().key(), primitive.useTightBox(),
                                                 primitive.alignment(), primitive.textFormat()});

    if(imagePrimitive.image().isNull()) {
        Qt::TextFormat resolvedTextFormat = primitive.resolvedTextFormat();
        qreal outlineWidth = primitive.effectiveOutlineWidth(devicePixelRatio);

        // Measure text size in local text coordinate system (does NOT include alignment/offset/rotation/outline).
        // Bounds are calculated as if text was drawn at base coordinates (0,0).
        QRectF textBounds = primitive.queryLocalBounds(devicePixelRatio, resolvedTextFormat);

        // Compute axis-aligned bounding box in absolute window coordinate system.
        QRectF boundingBox = primitive.computeBoundingBox(textBounds.size(), devicePixelRatio);

        // Generate texture image.
        QRect pixelBounds = boundingBox.toAlignedRect();
        QImage textureImage(pixelBounds.width(), pixelBounds.height(), preferredImageFormat);
        textureImage.setDevicePixelRatio(devicePixelRatio);
        textureImage.fill(0);
        QPainter painter(&textureImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        painter.translate(
            (primitive.position().x() - boundingBox.left()) / devicePixelRatio,
            (primitive.position().y() - boundingBox.top()) / devicePixelRatio);

        // Start with top-left alignment.
        QPointF textOffset(-textBounds.left(), -textBounds.top());

        // Apply horizontal alignment.
        if(primitive.alignment() & Qt::AlignRight)
            textOffset.rx() += -textBounds.width();
        else if(primitive.alignment() & Qt::AlignHCenter)
            textOffset.rx() += -textBounds.width() / 2;

        // Apply vertical alignment.
        if(primitive.alignment() & Qt::AlignBottom)
            textOffset.ry() += -textBounds.height();
        else if(primitive.alignment() & Qt::AlignVCenter)
            textOffset.ry() += -textBounds.height() / 2;

        if(primitive.rotation() != 0) {
            // Rotate around point given by the primitive's position.
            qreal x = textOffset.x() * std::cos(primitive.rotation()) - textOffset.y() * std::sin(primitive.rotation());
            qreal y = textOffset.x() * std::sin(primitive.rotation()) + textOffset.y() * std::cos(primitive.rotation());
            painter.translate(x / devicePixelRatio, y / devicePixelRatio);
            painter.rotate(qRadiansToDegrees(primitive.rotation()));
        }
        else {
            painter.translate(textOffset.x() / devicePixelRatio, textOffset.y() / devicePixelRatio);
        }

        // Draw text.
        primitive.draw(painter, resolvedTextFormat, textBounds.width() / devicePixelRatio);
        painter.end();

        // Store image primitive in cache including offset vector relative to primitive position.
        imagePrimitive.setImage(std::move(textureImage));
        offset = boundingBox.topLeft() - QPointF(primitive.position().x(), primitive.position().y());
    }

    // Compute absolute image paint position by adding precomputed offset vector to current primitive position.
    QPoint alignedPos = (QPointF(primitive.position().x(), primitive.position().y()) + offset).toPoint();
    imagePrimitive.setRectWindow(QRect(alignedPos, imagePrimitive.image().size()));

    // Render.
    renderImage(imagePrimitive);
}

}   // End of namespace
