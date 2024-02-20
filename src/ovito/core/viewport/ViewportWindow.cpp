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
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/viewport/ViewportSuspender.h>
#include <ovito/core/viewport/ViewportGizmo.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/// Controls the margin size between the overlay render frame and the viewport border.
#define VIEWPORT_RENDER_FRAME_SIZE FloatType(0.93)

IMPLEMENT_ABSTRACT_OVITO_CLASS(ViewportWindow);
DEFINE_REFERENCE_FIELD(ViewportWindow, viewport);
DEFINE_REFERENCE_FIELD(ViewportWindow, renderer);

/******************************************************************************
* Associates this window with a viewport.
******************************************************************************/
void ViewportWindow::setViewport(Viewport* vp, UserInterface& userInterface)
{
    OVITO_ASSERT(vp);

    _userInterface = &userInterface;
    _viewport.set(this, PROPERTY_FIELD(viewport), vp);
    _scenePreparation = OORef<ScenePreparation>::create(userInterface, vp->scene());

    // Automatically rerender window whenever the scene is changed.
    connect(&scenePreparation(), &ScenePreparation::viewportUpdateRequest, this, &ViewportWindow::requestUpdate);
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void ViewportWindow::requestUpdate()
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    if(!_updateRequested) {
        _updateRequested = true;
        if(!userInterface().areViewportUpdatesSuspended()) {
            resumeViewportUpdates();
        }
    }
}

/******************************************************************************
* Asks the window to handle any pending update request now after viewport
* updates were temporarily suspended.
******************************************************************************/
void ViewportWindow::resumeViewportUpdates()
{
    OVITO_ASSERT(!userInterface().areViewportUpdatesSuspended());
    OVITO_ASSERT(QCoreApplication::instance());

    if(_updateRequested && !_updateScheduled && isVisible()) {
        _updateScheduled = true;
        QMetaObject::invokeMethod(this, "handleUpdateRequest", Qt::QueuedConnection);
    }
}

/******************************************************************************
* Is called when the viewport's scene has changed and a rerendering is required.
******************************************************************************/
void ViewportWindow::handleUpdateRequest()
{
    OVITO_ASSERT(_updateScheduled);
    OVITO_ASSERT(viewport());

    // Reset handler flag.
    _updateScheduled = false;

    // Skip if the viewport is currently hidden but keep the update request pending.
    if(!isVisible() || !viewport() || !renderer())
        return;

    // Do nothing if viewport updates are currently disabled.
    // The UserInterface will issue a new update request once updates are re-enabled.
    if(userInterface().areViewportUpdatesSuspended())
        return;

    // Reset update request flag.
    _updateRequested = false;

    // The dataset to be rendered.
    DataSet* dataset = userInterface().datasetContainer().currentSet();
    if(!dataset || !dataset->renderSettings())
        return;

    // The size of the viewport window.
    QSize windowSize = viewportWindowDeviceSize();
    if(windowSize.isEmpty())
        return;

    // Temporarily suspend viewport updates to avoid recursive updates.
    ViewportSuspender suspendUpdates(userInterface());

    // Inform the UI that rendering of an interactive viewport is in progress.
    userInterface().interactiveViewportRenderingStarted();

    // Graceful exception handling.
    bool success = userInterface().handleExceptions([&]() {

        // Set up preliminary projection without knowing the scene bounding box yet.
        AnimationTime time = viewport()->scene()->animationSettings()->currentTime();
        FloatType aspectRatio = (FloatType)windowSize.height() / windowSize.width();
        _projParams = viewport()->computeProjectionParameters(time, aspectRatio);
        if(this_task::isCanceled() || !viewport())
            return;

        // Frame generation control flags.
        const bool isInteractive = true;
        const bool isPreviewMode = viewport()->renderPreviewMode();
        const bool stopOnPipelineError = false;

        // Adjust projection if preview frame is enabled.
        ViewProjectionParameters noninteractiveProjParams = _projParams;
        if(isPreviewMode) {
            adjustProjectionForRenderFrame(dataset, _projParams, windowSize);
            noninteractiveProjParams = viewport()->computeProjectionParameters(time, viewport()->renderAspectRatio(dataset));
        }

        // Create a new fresh frame graph.
        std::unique_ptr<FrameGraph> frameGraph = std::make_unique<FrameGraph>(
            userInterface().datasetContainer().visCache()->acquireResourceFrame(),
            time, _projParams, viewportWindowDeviceIndependentSize(), isInteractive, isPreviewMode, stopOnPipelineError,
            renderer()->preferredImageFormat(), devicePixelRatio());

        // Set background color.
        if(!viewport()->renderPreviewMode())
            frameGraph->setClearColor(Viewport::viewportColor(ViewportSettings::COLOR_VIEWPORT_BKG));
        else
            frameGraph->setClearColor(dataset->renderSettings()->backgroundColorAt(time));

        // Render viewport "underlays".
        if(isPreviewMode) {
            if(boost::algorithm::any_of(viewport()->underlays(), [](ViewportOverlay* layer) { return layer->isEnabled(); })) {
                QRect viewportRect = dataset->renderSettings()->viewportFramebufferArea(viewport(), dataset->viewportConfig());
                if(!viewportRect.isEmpty()) {
                    QRect frameBox = previewFrameGeometry(dataset, windowSize);
                    if(!frameBox.isNull()) {
                        frameGraph->setCurrentRenderLayer(FrameGraph::RenderLayer::UnderLayer);
                        frameGraph->renderOverlays(viewport(), true, viewportRect, frameBox, noninteractiveProjParams);
                    }
                }
            }
        }

        // Render the 3d scene objects.
        frameGraph->setCurrentRenderLayer(FrameGraph::RenderLayer::SceneLayer);
        if(!frameGraph->renderSceneNode(viewport()->scene(), viewport()))
            return;

        // Render construction grid.
        if(viewport()->isGridVisible())
            renderConstructionGrid(*frameGraph);

        // Render visual representations of the modifiers.
        viewport()->scene()->visitPipelines([&](Pipeline* pipeline) {
            renderPipelineModifiers(viewport()->scene(), pipeline, *frameGraph);
            return true;
        });

        // Render viewport gizmos.
        for(ViewportGizmo* gizmo : viewportGizmos()) {
            gizmo->renderOverlay(viewport(), this, *frameGraph, dataset);
        }

        frameGraph->setCurrentRenderLayer(FrameGraph::RenderLayer::OverLayer);

        // Render viewport "overlays".
        if(isPreviewMode) {
            if(boost::algorithm::any_of(viewport()->overlays(), [](ViewportOverlay* layer) { return layer->isEnabled(); })) {
                QRect viewportRect = dataset->renderSettings()->viewportFramebufferArea(viewport(), dataset->viewportConfig());
                if(!viewportRect.isEmpty()) {
                    QRect frameBox = previewFrameGeometry(dataset, windowSize);
                    if(!frameBox.isNull()) {
                        frameGraph->renderOverlays(viewport(), false, viewportRect, frameBox, noninteractiveProjParams);
                    }
                }
            }
        }

        // Render UI elements on top (e.g. viewport caption).
        if(isPreviewMode) {
            renderPreviewFrame(*frameGraph, dataset, windowSize);
        }
        else {
            renderOrientationIndicator(*frameGraph, windowSize);
        }

        // Render viewport caption.
        if(isViewportTitleVisible())
            _contextMenuArea = renderViewportTitle(*frameGraph);
        else
            _contextMenuArea = QRectF();

        // Let the renderer implementation post-process the frame graph.
        renderer()->postprocessFrameGraph(*frameGraph);

        // Compute final projection based on the now known bounding box.
        _projParams = viewport()->computeProjectionParameters(time, aspectRatio, frameGraph->sceneBoundingBox());

        // Adjust projection if render frame is enabled.
        if(isPreviewMode)
            adjustProjectionForRenderFrame(dataset, _projParams, windowSize);

        frameGraph->setProjectionParams(_projParams);

        // Adopt newly generated frame graph.
        setFrameGraph(std::move(frameGraph));
    });

    // Inform the user interface that rendering of an interactive viewport has finished.
    userInterface().interactiveViewportRenderingFinished();

    // After the frame graph has been updated, show the new viewport contents on screen.
    if(success)
        refreshDisplay();
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ViewportWindow::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == viewport()) {
        if(event.type() == Viewport::ViewportWindowUpdateRequested) {
            requestUpdate();
        }
        else if(event.type() == Viewport::ViewportWindowResumeUpdatesRequested) {
            resumeViewportUpdates();
        }
        else if(event.type() == Viewport::ZoomToSceneExtentsRequested) {
            zoomToSceneExtents();
        }
        else if(event.type() == Viewport::ZoomToSelectionExtentsRequested) {
            zoomToSelectionExtents();
        }
        else if(event.type() == Viewport::ZoomToSceneExtentsWhenReadyRequested) {
            zoomToSceneExtentsWhenReady();
        }
        else if(event.type() == ReferenceEvent::ReferenceChanged) {
            const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
            if(refEvent.field() == PROPERTY_FIELD(Viewport::scene)) {
                // Keep our scene reference in sync with the viewport's.
                scenePreparation().setScene(viewport()->scene());
            }
        }
    }
    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Computes the geometry of the render preview frame, i.e., the cutout region of
* the interactive viewport window that will be visible in a rendered image.
******************************************************************************/
QRect ViewportWindow::previewFrameGeometry(DataSet* dataset, const QSize& windowSize) const
{
    if(windowSize.isEmpty())
        return {};

    // Aspect ratio of the viewport rectangle in the rendered output image.
    FloatType renderAspectRatio = viewport()->renderAspectRatio(dataset);
    if(renderAspectRatio == 0.0)
        return {};

    // Compute a rectangle fitted into the viewport window that has the same aspect ratio as the rendered viewport image.
    FloatType windowAspectRatio = (FloatType)windowSize.height() / windowSize.width();
    FloatType frameWidth, frameHeight;
    if(renderAspectRatio < windowAspectRatio) {
        frameWidth = VIEWPORT_RENDER_FRAME_SIZE;
        frameHeight = frameWidth / windowAspectRatio * renderAspectRatio;
    }
    else {
        frameHeight = VIEWPORT_RENDER_FRAME_SIZE;
        frameWidth = frameHeight / renderAspectRatio * windowAspectRatio;
    }
    Box2 frameRect(-frameWidth, -frameHeight, frameWidth, frameHeight);

    // Convert rectangle from viewport to window coordinates.
    return QRectF(
        (frameRect.minc.x() + 1.0) * windowSize.width() / 2.0,
        (frameRect.minc.y() + 1.0) * windowSize.height() / 2.0,
        frameRect.width() * windowSize.width() / 2.0,
        frameRect.height() * windowSize.height() / 2.0).toRect();
}

/******************************************************************************
* Modifies the projection such that the render frame painted over the 3d scene exactly
* matches the true visible area.
******************************************************************************/
void ViewportWindow::adjustProjectionForRenderFrame(DataSet* dataset, ViewProjectionParameters& params, const QSize& windowSize)
{
    if(windowSize.isEmpty())
        return;

    FloatType renderAspectRatio = viewport()->renderAspectRatio(dataset);
    if(renderAspectRatio == 0.0)
        return;

    FloatType windowAspectRatio = (FloatType)windowSize.height() / windowSize.width();

    if(params.isPerspective) {
        if(renderAspectRatio < windowAspectRatio)
            params.fieldOfView = std::atan(std::tan(params.fieldOfView/2) / (VIEWPORT_RENDER_FRAME_SIZE / windowAspectRatio * renderAspectRatio))*2;
        else
            params.fieldOfView = std::atan(std::tan(params.fieldOfView/2) / VIEWPORT_RENDER_FRAME_SIZE)*2;
        params.projectionMatrix = Matrix4::perspective(params.fieldOfView, FloatType(1) / params.aspectRatio, params.znear, params.zfar);
    }
    else {
        if(renderAspectRatio < windowAspectRatio)
            params.fieldOfView /= VIEWPORT_RENDER_FRAME_SIZE / windowAspectRatio * renderAspectRatio;
        else
            params.fieldOfView /= VIEWPORT_RENDER_FRAME_SIZE;
        params.projectionMatrix = Matrix4::ortho(-params.fieldOfView / params.aspectRatio, params.fieldOfView / params.aspectRatio,
                            -params.fieldOfView, params.fieldOfView,
                            params.znear, params.zfar);
    }
    params.inverseProjectionMatrix = params.projectionMatrix.inverse();
}

/******************************************************************************
* Zooms to the extents of the given bounding box.
******************************************************************************/
void ViewportWindow::zoomToBox(const Box3& box)
{
    // Obtain the aspect ratio from the UI window associated with the viewport or the current render settings.
    FloatType aspectRatio = 0;
    if(viewport()->renderPreviewMode()) {
        aspectRatio = viewport()->renderAspectRatio(userInterface().datasetContainer().currentSet());
    }
    if(aspectRatio == 0) {
        QSize windowSize = viewportWindowDeviceSize();
        if(windowSize.width() > 0)
            aspectRatio = (FloatType)windowSize.height() / windowSize.width();
    }
    if(aspectRatio == 0)
        aspectRatio = 1;

    viewport()->zoomToBox(box, aspectRatio);
}

/******************************************************************************
* Zooms to the extents of the scene.
******************************************************************************/
void ViewportWindow::zoomToSceneExtents()
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(Scene* scene = viewport()->scene()) {
        Box3 sceneBoundingBox = scene->worldBoundingBox(scene->animationSettings()->currentTime(), viewport());
        zoomToBox(sceneBoundingBox);
    }
}

/******************************************************************************
* Zooms to the extents of the currently selected nodes.
******************************************************************************/
void ViewportWindow::zoomToSelectionExtents()
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(Scene* scene = viewport()->scene()) {
        Box3 selectionBoundingBox;
        for(SceneNode* node : scene->selection()->nodes()) {
            selectionBoundingBox.addBox(node->worldBoundingBox(scene->animationSettings()->currentTime(), viewport()));
        }

        if(!selectionBoundingBox.isEmpty())
            zoomToBox(selectionBoundingBox);
        else
            zoomToSceneExtents();
    }
}

/******************************************************************************
* Zooms to the extents of the scene once all scene pipelines have been computed.
******************************************************************************/
void ViewportWindow::zoomToSceneExtentsWhenReady()
{
    if(viewport()) {
        scenePreparation().future().finally([self = OOWeakRef<ViewportWindow>(this)](Task& task) noexcept {
            if(!task.isCanceled()) {
                if(OORef<ViewportWindow> window = self.lock())
                    QMetaObject::invokeMethod(window.get(), "zoomToSceneExtents", Qt::AutoConnection);
            }
        });
    }
}

/******************************************************************************
* Render the axis tripod symbol in the corner of the viewport that indicates
* the coordinate system orientation.
******************************************************************************/
void ViewportWindow::renderOrientationIndicator(FrameGraph& frameGraph, const QSize& windowSize)
{
    constexpr GraphicsFloatType tripodSize = 80.0f;          // device-independent pixels
    constexpr GraphicsFloatType tripodArrowSize = 0.17f;     // percentage of the above value.

    // Matrix describing the orientation of the axes.
    Matrix3G orientation = frameGraph.projectionParams().viewMatrix.linear().toDataType<GraphicsFloatType>();

    // Set up spacial projection matrix.
    const GraphicsFloatType tripodPixelSize = tripodSize * frameGraph.devicePixelRatio();
    Matrix4G viewportScalingTM = Matrix4G::Identity();
    viewportScalingTM(0,0) = tripodPixelSize / windowSize.width();
    viewportScalingTM(1,1) = tripodPixelSize / windowSize.height();
    viewportScalingTM(0,3) = -1.0 + viewportScalingTM(0,0);
    viewportScalingTM(1,3) = -1.0 + viewportScalingTM(1,1);
    Matrix4G projectionMatrix = viewportScalingTM * Matrix4G::ortho(-1.4, 1.4, -1.4, 1.4, -2.0, 2.0);

    static const ColorA axisColors[3] = { ColorA(1.0, 0.0, 0.0), ColorA(0.0, 1.0, 0.0), ColorA(0.4, 0.4, 1.0) };
    static const QString labelTexts[3] = { QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z") };

    // Cache line and text drawing primitives as long as camera hasn't moved and window size hasn't changed.
    auto& [lines, labels] = frameGraph.visCache().lookup<std::tuple<LinePrimitive, std::array<TextPrimitive,3>>>(RendererResourceKey<struct OrientionIndicatorCache, Matrix3G, GraphicsFloatType, GraphicsFloatType>{
        orientation, viewportScalingTM(0,0), viewportScalingTM(1,1)
    });

    // Create line primitive for the coordinate axis arrows.
    if(!lines.positions()) {
        BufferFactory<ColorAG> vertexColors(18);
        std::fill(vertexColors.begin() + 0,  vertexColors.begin() + 6,  axisColors[0].toDataType<GraphicsFloatType>());
        std::fill(vertexColors.begin() + 6,  vertexColors.begin() + 12, axisColors[1].toDataType<GraphicsFloatType>());
        std::fill(vertexColors.begin() + 12, vertexColors.end(),        axisColors[2].toDataType<GraphicsFloatType>());
        lines.setColors(vertexColors.take());

        // Update geometry of coordinate axis arrows.
        BufferFactory<Point3G> vertices(18);
        for(size_t axis = 0, index = 0; axis < 3; axis++) {
            Vector3G dir = orientation.column(axis).normalized().toDataType<GraphicsFloatType>();
            vertices[index++] = projectionMatrix * (Point3G::Origin());
            vertices[index++] = projectionMatrix * (Point3G::Origin() + dir);
            vertices[index++] = projectionMatrix * (Point3G::Origin() + dir);
            vertices[index++] = projectionMatrix * (Point3G::Origin() + (dir + tripodArrowSize * Vector3G(dir.y() - dir.x(), -dir.x() - dir.y(), dir.z())));
            vertices[index++] = projectionMatrix * (Point3G::Origin() + dir);
            vertices[index++] = projectionMatrix * (Point3G::Origin() + (dir + tripodArrowSize * Vector3G(-dir.y() - dir.x(), dir.x() - dir.y(), dir.z())));
        }
        lines.setPositions(vertices.take());
    }

    // Render coordinate axis arrows as 2d element.
    frameGraph.addCommand(std::make_unique<LinePrimitive>(lines));

    // Render x,y,z labels.
    for(int axis = 0; axis < 3; axis++) {

        // Initialize the graphics primitives for rendering the text labels.
        // This needs to be done only once.
        if(labels[axis].text().isEmpty()) {
            labels[axis].setFont(ViewportSettings::getSettings().viewportFont());
            labels[axis].setColor(axisColors[axis]);
            labels[axis].setText(labelTexts[axis]);
            labels[axis].setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        // Compute the projected position of the label in NDC space.
        Point3G p = Point3G::Origin() + orientation.column(axis).resized(1.23);
        Point3G ndcPoint = projectionMatrix * p;

        // Convert position to window space.
        labels[axis].setPositionWindow(Point2{
            ( ndcPoint.x() + 1.0) * windowSize.width() / 2.0,
            (-ndcPoint.y() + 1.0) * windowSize.height() / 2.0
        });

        frameGraph.addCommand(std::make_unique<TextPrimitive>(labels[axis]));
    }
}

/******************************************************************************
* Computes a point in the given coordinate system based on the given screen
* position and the current snapping settings.
******************************************************************************/
bool ViewportWindow::snapPoint(const QPointF& screenPoint, Point3& snapPoint, const AffineTransformation& snapSystem) const
{
    // Compute the intersection point of the ray with the X-Y plane of the snapping coordinate system.
    Ray3 ray = snapSystem.inverse() * screenRay(screenPoint);

    Plane3 plane(Vector3(0, 0, 1), 0);
    FloatType t = plane.intersectionT(ray, FloatType(1e-3));
    if(t == FLOATTYPE_MAX)
        return false;

    if(isPerspectiveProjection() && t <= 0)
        return false;

    snapPoint = ray.point(t);
    snapPoint.z() = 0;

    return true;
}

/******************************************************************************
* Computes a point in the grid coordinate system based on a screen position and
* the current snap settings.
******************************************************************************/
bool ViewportWindow::snapPoint(const QPointF& screenPoint, Point3& snapPoint) const
{
    return this->snapPoint(screenPoint, snapPoint, viewport()->gridMatrix());
}

/******************************************************************************
* Computes a ray in world space going through a pixel of the viewport window.
******************************************************************************/
Ray3 ViewportWindow::screenRay(const QPointF& screenPoint) const
{
    QSize windowSize = viewportWindowDeviceIndependentSize();

    return projectionParams().viewportRay(Point2(
            (FloatType)screenPoint.x() / windowSize.width() * FloatType(2) - FloatType(1),
            FloatType(1) - (FloatType)screenPoint.y() / windowSize.height() * FloatType(2)));
}

/******************************************************************************
* Paints the rectangular frame on top of the scene to indicate the visible image area.
******************************************************************************/
void ViewportWindow::renderPreviewFrame(FrameGraph& frameGraph, DataSet* dataset, const QSize& windowSize)
{
    // The render frame in viewport coordinates.
    QRect frameRect = previewFrameGeometry(dataset, windowSize);
    if(frameRect.isNull())
        return;

    // Create a 1x1 pixel semi-transparent image, which is used to fill rectangular areas with a uniform color.
    static const QImage image = [&]() {
        QImage image(1, 1, frameGraph.preferredImageFormat());
        if(image.format() == QImage::Format_RGBA8888 || image.format() == QImage::Format_ARGB32)
            image.fill(0xA0A0A0A0);
        else
            image.fill(QColor(0xA0, 0xA0, 0xA0, 0xA0));
        return image;
    }();

    // Fill area around frame rectangle with semi-transparent color.
    // Use four rectangles to form the outer frame.
    frameGraph.addCommand(std::make_unique<ImagePrimitive>(image, Box2(Point2(0, 0), Point2(frameRect.left(), windowSize.height()))));
    frameGraph.addCommand(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.right(), 0), Point2(windowSize.width(), windowSize.height()))));
    frameGraph.addCommand(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.left(), 0), Point2(frameRect.right(), frameRect.top()))));
    frameGraph.addCommand(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.left(), frameRect.bottom()), Point2(frameRect.right(), windowSize.height()))));
}

/******************************************************************************
* Renders the viewport caption text.
******************************************************************************/
QRectF ViewportWindow::renderViewportTitle(FrameGraph& frameGraph)
{
    std::unique_ptr<TextPrimitive> primitive = std::make_unique<TextPrimitive>();
    primitive->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    if(_cursorInContextMenuArea) {
        QFont font = ViewportSettings::getSettings().viewportFont();
        font.setUnderline(true);
        primitive->setFont(font);
    }
    else {
        primitive->setFont(ViewportSettings::getSettings().viewportFont());
    }

    QString str = viewport()->viewportTitle();
    if(viewport()->renderPreviewMode())
        str += Viewport::tr(" (preview)");
#ifdef OVITO_DEBUG
    str += QStringLiteral(" [%1]").arg(++_renderDebugCounter);
#endif
    primitive->setText(str);
    Color textColor = Viewport::viewportColor(ViewportSettings::COLOR_VIEWPORT_CAPTION);
    if(viewport()->renderPreviewMode() && textColor == frameGraph.clearColor().rgb())
        textColor = Vector3(1,1,1) - (Vector3)textColor;
    primitive->setColor(textColor);

    Point2 pos = Point2(2, 2) * frameGraph.devicePixelRatio();
    primitive->setPositionWindow(pos);

    // Compute the area covered by the caption text.
    QRectF textBounds = primitive->queryLocalBounds(1.0);
    textBounds.moveTo(QPointF(2,2));
    textBounds.setWidth(std::max(textBounds.width(), 30.0));
    textBounds.adjust(-2, -2, 2, 2);

    frameGraph.addCommand(std::move(primitive));

    return textBounds;
}

/******************************************************************************
* Sets a flag indicatring whether the mouse cursor is currently located in the
* viewport window area that activates the context menu.
******************************************************************************/
void ViewportWindow::setCursorInContextMenuArea(bool flag)
{
    if(_cursorInContextMenuArea != flag) {
        _cursorInContextMenuArea = flag;
        viewport()->updateViewport();
    }
}

/******************************************************************************
* Renders the visual representation of the modifiers in a pipeline.
******************************************************************************/
void ViewportWindow::renderPipelineModifiers(Scene* scene, Pipeline* pipeline, FrameGraph& frameGraph)
{
    ModificationNode* node = dynamic_object_cast<ModificationNode>(pipeline->head());
    while(node) {
        Modifier* mod = node->modifier();

        try {
            // Render modifier.
            mod->renderModifierVisual(ModifierEvaluationRequest(scene->animationSettings(), node), pipeline, frameGraph);
        }
        catch(const Exception& ex) {
            // Swallow exceptions, because we are in interactive rendering mode.
            ex.logError();
        }

        // Traverse up the pipeline.
        node = dynamic_object_cast<ModificationNode>(node->input());
    }
}

/******************************************************************************
* Determines the range of the construction grid to display.
******************************************************************************/
std::tuple<FloatType, Box2I> ViewportWindow::determineConstructionGridRange()
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
        if(computeConstructionPlaneIntersection(testPoints[i], p, 0.1f)) {
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
    FloatType gridSpacing = projectionParams().nonScalingSize(viewport()->gridMatrix() * gridCenter, viewportWindowDeviceIndependentSize()) * 2;

    // Round to nearest power of 10.
    gridSpacing = std::pow((FloatType)10, std::floor(log10(gridSpacing)));

    // Determine how many grid lines need to be rendered.
    int xstart = (int)std::floor(visibleGridRect.minc.x() / (gridSpacing * 10)) * 10;
    int xend = (int)std::ceil(visibleGridRect.maxc.x() / (gridSpacing * 10)) * 10;
    int ystart = (int)std::floor(visibleGridRect.minc.y() / (gridSpacing * 10)) * 10;
    int yend = (int)std::ceil(visibleGridRect.maxc.y() / (gridSpacing * 10)) * 10;

    return std::tuple<FloatType, Box2I>(gridSpacing, Box2I(Point2I(xstart, ystart), Point2I(xend, yend)));
}

/******************************************************************************
* Renders the construction grid.
******************************************************************************/
void ViewportWindow::renderConstructionGrid(FrameGraph& frameGraph)
{
    auto [gridSpacing, gridRange] = determineConstructionGridRange();
    if(gridSpacing <= 0)
        return;

    // Determine how many grid lines need to be rendered.
    int xstart = gridRange.minc.x();
    int ystart = gridRange.minc.y();
    int numLinesX = gridRange.size(0) + 1;
    int numLinesY = gridRange.size(1) + 1;

    FloatType xstartF = (FloatType)xstart * gridSpacing;
    FloatType ystartF = (FloatType)ystart * gridSpacing;
    FloatType xendF = (FloatType)(xstart + numLinesX - 1) * gridSpacing;
    FloatType yendF = (FloatType)(ystart + numLinesY - 1) * gridSpacing;

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
    std::unique_ptr<LinePrimitive> primitive = std::make_unique<LinePrimitive>();
    primitive->setPositions(vertexPositions.take());
    primitive->setColors(vertexColors.take());
    frameGraph.addPrimitive(std::move(primitive), viewport()->gridMatrix(), Box3(Point3(xstartF, ystartF, 0), Point3(xendF, yendF, 0)));
}

/******************************************************************************
* Computes the intersection of a ray going through a point in the
* viewport projection plane and the grid plane.
*
* Returns true if an intersection has been found.
******************************************************************************/
bool ViewportWindow::computeConstructionPlaneIntersection(const Point2& viewportPosition, Point3& intersectionPoint, FloatType epsilon)
{
    // The construction plane in grid coordinates.
    Plane3 gridPlane = Plane3(Vector3(0,0,1), 0);

    // Compute the ray and transform it to the grid coordinate system.
    Ray3 ray = viewport()->gridMatrix().inverse() * projectionParams().viewportRay(viewportPosition);

    // Compute intersection point.
    FloatType t = gridPlane.intersectionT(ray, epsilon);
    if(t == std::numeric_limits<FloatType>::max())
        return false;
    if(projectionParams().isPerspective && t <= 0)
        return false;

    intersectionPoint = ray.point(t);
    intersectionPoint.z() = 0;

    return true;
}

}   // End of namespace
