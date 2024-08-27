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
#include <ovito/core/rendering/FrameGraphBuilder.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/// Controls the margin size between the overlay render frame and the viewport border.
#define VIEWPORT_RENDER_FRAME_SIZE FloatType(0.93)

IMPLEMENT_ABSTRACT_OVITO_CLASS(ViewportWindow);
DEFINE_REFERENCE_FIELD(ViewportWindow, viewport);

/******************************************************************************
* Associates this window with a viewport.
******************************************************************************/
void ViewportWindow::setViewport(Viewport* vp, UserInterface& userInterface)
{
    OVITO_ASSERT(vp);

    _userInterface = &userInterface;
    _viewport.set(this, PROPERTY_FIELD(viewport), vp);
    if(vp) {
        if(!_scenePreparation) {
            _scenePreparation = OORef<ScenePreparation>::create(userInterface, vp->scene());
            // Automatically rerender window whenever the scene is changed.
            connect(&scenePreparation(), &ScenePreparation::viewportUpdateRequest, this, &ViewportWindow::requestUpdate);
            _scenePreparation->setAutoRestart(isVisible());
        }
        else {
            _scenePreparation->setScene(vp->scene());
        }
    }
    else {
        _scenePreparation.reset();
    }
}

/******************************************************************************
* Releases the renderer resources held by the viewport window and the renderer.
******************************************************************************/
void ViewportWindow::releaseResources()
{
    // Stop building a new frame graph.
    _frameGraphFuture.reset();

    // Release current frame graph and rendering job.
    setFrameGraph({});
    setRenderingJob({});
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void ViewportWindow::requestUpdate()
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    if(!_updateRequested && viewport()) {
        _updateRequested = true;
        if(!userInterface().areViewportUpdatesSuspended()) {
            resumeViewportUpdates();
        }
    }
}

/******************************************************************************
* If an update request is pending for this viewport window, immediately
* processes it and redraw the window contents.
******************************************************************************/
void ViewportWindow::processViewportUpdate()
{
    if(!viewport())
        return;

    if(QCoreApplication::instance()) {
        if(_updateRequested && _updateTimer.isActive()) {
            _updateTimer.stop();
            // Calling handleUpdateRequest() asynchronously because it is a long-running
            // operation that may start a local event loop. We can't allow user changes to the
            // list of viewports while processViewportUpdate() is being executed.
            QMetaObject::invokeMethod(this, "handleUpdateRequest", Qt::QueuedConnection);
        }
    }
    else {
        if(_updateRequested)
            handleUpdateRequest();
    }
}

/******************************************************************************
* Asks the window to handle any pending update request now after viewport
* updates were temporarily suspended.
******************************************************************************/
void ViewportWindow::resumeViewportUpdates()
{
    if(!viewport())
        return;
    OVITO_ASSERT(!userInterface().areViewportUpdatesSuspended());

    if(QCoreApplication::instance()) {
        if(_updateRequested && !_updateTimer.isActive() && isVisible()) {
            _updateTimer.start(10, Qt::CoarseTimer, this); // Refresh viewport window after a 10 ms delay to avoid excessive repaints.
        }
    }
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void ViewportWindow::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _updateTimer.timerId()) {
         _updateTimer.stop();
        handleUpdateRequest();
    }
}

/******************************************************************************
* Is called when the viewport's scene has changed and a rerendering is required.
******************************************************************************/
void ViewportWindow::handleUpdateRequest()
{
    OVITO_ASSERT(viewport());

    // Skip if the viewport is currently hidden but keep the update request pending.
    if(!isVisible() || !viewport())
        return;

    // Do nothing if viewport updates are currently suspended.
    // The UserInterface will issue a new update request once updates are resumed.
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

    userInterface().handleExceptions<true>([&]() {

        // Interactive viewport rendering is performed with a higher priority than other tasks.
        this_task::get()->setHighPriorityTask();

        // Set up preliminary projection without knowing the scene bounding box yet.
        AnimationTime time = viewport()->scene()->animationSettings()->currentTime();
        FloatType aspectRatio = (FloatType)windowSize.height() / windowSize.width();
        ViewProjectionParameters projParams = viewport()->computeProjectionParameters(time, aspectRatio);

        // Adjust projection if preview frame is enabled.
        ViewProjectionParameters noninteractiveProjParams = projParams;
        if(viewport()->renderPreviewMode()) {
            adjustProjectionForRenderPreviewFrame(dataset, projParams, windowSize);
            noninteractiveProjParams = viewport()->computeProjectionParameters(time, viewport()->renderAspectRatio(dataset));
        }

        // Initialize the window's rendering job. Job may be null, e.g., in a JupyterViewportWindow.
        OORef<RenderingJob> renderingJob = this->renderingJob();

        // Create a new fresh frame graph.
        OORef<FrameGraph> frameGraph = OORef<FrameGraph>::create(
            userInterface().datasetContainer().visCache()->acquireResourceFrame(),
            time,
            projParams,
            viewportWindowDeviceIndependentSize(),
            true, // isInteractive
            viewport()->renderPreviewMode(),
            false, // stopOnPipelineError
            renderingJob ? renderingJob->preferredImageFormat() : QImage::Format_ARGB32_Premultiplied,
            devicePixelRatio());

        // Set viewport background color.
        frameGraph->setClearColor(viewport()->renderPreviewMode()
            ? dataset->renderSettings()->backgroundColorAt(time)
            : Viewport::viewportColor(ViewportSettings::COLOR_VIEWPORT_BKG));

        // Render construction grid.
        if(viewport()->isGridVisible())
            renderConstructionGrid(*frameGraph);

        // Render visual representations of the modifiers.
        viewport()->scene()->visitPipelines([&](Pipeline* pipeline) {
            renderPipelineModifiers(pipeline, *frameGraph);
            return true;
        });

        // Render interactive viewport gizmos.
        for(ViewportGizmo* gizmo : viewportGizmos()) {
            gizmo->renderOverlay(viewport(), this, *frameGraph, dataset);
        }

        QRect logicalViewportRect = dataset->renderSettings()->viewportFramebufferArea(viewport(), dataset->viewportConfig());
        QRect physicalViewportRect = previewFrameGeometry(dataset, windowSize);

        // Let the FrameGraphBuilder class do the heavy lifting and generate the frame graph for the current scene.
        Future<OORef<FrameGraph>> frameGraphFuture = FrameGraphBuilder::build(std::move(frameGraph), viewport()->scene(), viewport(), logicalViewportRect, physicalViewportRect, noninteractiveProjParams);

        // After the frame graph has been built for the scene, finish and then render it.
        // Note: Temporarily suspending viewport updates while building the frame graph to prevent recursive updates.
        _frameGraphFuture = frameGraphFuture.then(*this, [this, suspendUpdates=ViewportSuspender(userInterface())](OORef<FrameGraph> frameGraph) mutable {

            DataSet* dataset = userInterface().datasetContainer().currentSet();
            QSize windowSize = viewportWindowDeviceSize();
            if(!viewport() || !dataset || !dataset->renderSettings() || windowSize.isEmpty())
                return;

            // Create a command group for the UI element rendering commands.
            FrameGraph::RenderingCommandGroup& uiCommandGroup = frameGraph->addCommandGroup(FrameGraph::OverLayer);

            // Render UI elements on top (e.g. viewport caption).
            if(viewport()->renderPreviewMode()) {
                renderPreviewFrame(*frameGraph, uiCommandGroup, dataset, windowSize);
            }
            else {
                if(isOrientationIndicatorVisible())
                    renderOrientationIndicator(*frameGraph, uiCommandGroup, windowSize);
            }

            // Render viewport caption.
            if(isViewportTitleVisible())
                _contextMenuArea = renderViewportTitle(*frameGraph, uiCommandGroup);
            else
                _contextMenuArea = QRectF();

            // Let the renderer implementation post-process the frame graph.
            this->renderingJob()->postprocessFrameGraph(*frameGraph);

            // Compute final projection based on the now known bounding box.
            _projParams = viewport()->computeProjectionParameters(frameGraph->time(), (FloatType)windowSize.height() / windowSize.width(), frameGraph->sceneBoundingBox());

            // Adjust projection if render frame is enabled.
            if(viewport()->renderPreviewMode())
                adjustProjectionForRenderPreviewFrame(dataset, _projParams, windowSize);
            frameGraph->setProjectionParams(_projParams);

            // Adopt newly generated frame graph for this window.
            setFrameGraph(std::move(frameGraph));

            // After the frame graph has been rebuilt, let window implementation render and image and update the on-screen display.
            rerender();
        });
    });
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
        else if(event.type() == Viewport::ViewportWindowHandleUpdatesRequested) {
            processViewportUpdate();
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
* Modifies the projection such that the render preview frame painted over
* the 3d scene exactly matches the true visible area.
******************************************************************************/
void ViewportWindow::adjustProjectionForRenderPreviewFrame(DataSet* dataset, ViewProjectionParameters& params, const QSize& windowSize)
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
        // Fire-and-forget task that will zoom to the scene extents once the scene is ready.
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
void ViewportWindow::renderOrientationIndicator(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QSize& windowSize)
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
    Matrix4G projectionMatrix = viewportScalingTM * Matrix4G::ortho(-1.4f, 1.4f, -1.4f, 1.4f, -2.0f, 2.0f);

    static const ColorA axisColors[3] = { ColorA(1.0, 0.0, 0.0), ColorA(0.0, 1.0, 0.0), ColorA(0.4, 0.4, 1.0) };
    static const QString labelTexts[3] = { QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z") };

    // Cache line and text drawing primitives as long as camera hasn't moved and window size hasn't changed.
    const auto& lines = frameGraph.visCache().lookup<LinePrimitive>(
        RendererResourceKey<struct OrientionIndicatorCache, Matrix3G, GraphicsFloatType, GraphicsFloatType>{
            orientation, viewportScalingTM(0,0), viewportScalingTM(1,1)
        },
        [&](LinePrimitive& lines) {
            // Create line primitive for the coordinate axis arrows.
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
        });

    // Render coordinate axis arrows as 2d element.
    commandGroup.addPrimitivePreprojected(std::make_unique<LinePrimitive>(lines));

    // Render x,y,z labels.
    for(int axis = 0; axis < 3; axis++) {
        auto label = std::make_unique<TextPrimitive>();

        // Initialize the graphics primitives for rendering the text labels.
        // This needs to be done only once.
        label->setFont(ViewportSettings::getSettings().viewportFont());
        label->setColor(axisColors[axis]);
        label->setText(labelTexts[axis]);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        // Compute the projected position of the label in NDC space.
        Point3G p = Point3G::Origin() + orientation.column(axis).resized(1.23f);
        Point3G ndcPoint = projectionMatrix * p;

        // Convert position to window space.
        label->setPositionWindow(Point2{
            ( ndcPoint.x() + 1.0) * windowSize.width() / 2.0,
            (-ndcPoint.y() + 1.0) * windowSize.height() / 2.0
        });

        commandGroup.addPrimitivePreprojected(std::move(label));
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
void ViewportWindow::renderPreviewFrame(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, DataSet* dataset, const QSize& windowSize)
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
    commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(image, Box2(Point2(0, 0), Point2(frameRect.left(), windowSize.height()))));
    commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.right(), 0), Point2(windowSize.width(), windowSize.height()))));
    commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.left(), 0), Point2(frameRect.right(), frameRect.top()))));
    commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(image, Box2(Point2(frameRect.left(), frameRect.bottom()), Point2(frameRect.right(), windowSize.height()))));
}

/******************************************************************************
* Renders the viewport caption text.
******************************************************************************/
QRectF ViewportWindow::renderViewportTitle(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup)
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

    commandGroup.addPrimitivePreprojected(std::move(primitive));

    return textBounds;
}

/******************************************************************************
* Sets a flag indicating whether the mouse cursor is currently located in the
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
void ViewportWindow::renderPipelineModifiers(Pipeline* pipeline, FrameGraph& frameGraph)
{
    OORef<ModificationNode> node = dynamic_object_cast<ModificationNode>(pipeline->head());
    while(node) {
        try {
            // Render modifier.
            if(OORef<Modifier> mod = node->modifier())
                mod->renderModifierVisual(node, pipeline, frameGraph);
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
    for(size_t i = 0; i < std::size(testPoints); i++) {
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
    frameGraph.addCommandGroup(FrameGraph::SceneLayer).addPrimitiveNonpickable(std::move(primitive), viewport()->gridMatrix(), Box3(Point3(xstartF, ystartF, 0), Point3(xendF, yendF, 0)));
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
