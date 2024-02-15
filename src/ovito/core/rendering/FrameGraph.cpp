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
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

/******************************************************************************
* Add a 3d graphics primitive to the frame graph.
******************************************************************************/
void FrameGraph::addPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive, const SceneNode* sceneNode, OORef<ObjectPickInfo> pickInfo, RenderingCommand::Flags flags)
{
    // Get world transformation matrix of scene node.
    TimeInterval interval;
    const AffineTransformation& nodeTM = sceneNode->getWorldTransform(time(), interval);
    addPrimitive(std::move(primitive), nodeTM, Box3{}, sceneNode, flags);
}

/******************************************************************************
* Add a 3d graphics primitive to the frame graph.
******************************************************************************/
void FrameGraph::addPrimitive(std::unique_ptr<FrameGraphPrimitive> primitive, const AffineTransformation& modelTM, const Box3& boundingBox, const SceneNode* sceneNode, RenderingCommand::Flags flags)
{
    OVITO_ASSERT(primitive);

    // Add the world-space bounding box of the primitive to the scene bounding box.
    _sceneBoundingBox.addBox((boundingBox.isEmpty() ? primitive->computeBoundingBox(visCache()) : boundingBox).transformed(modelTM));

    // Record the draw command.
    addCommand(std::move(primitive), modelTM, flags);
}

/******************************************************************************
* Generates the visual representation of a scene node (and all its children).
******************************************************************************/
bool FrameGraph::renderSceneNode(OORef<SceneNode> node, OORef<Viewport> viewport)
{
    OVITO_ASSERT(node);

    // Stop if rendering has been canceled.
    if(this_task::isCanceled())
        return false;

    // Skip node if it is hidden in the current viewport.
    if(viewport && node->isHiddenInViewport(viewport, false))
        return true;

    if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(node)) {
        // Do not render node if it is the view node of the viewport or
        // if it is the target of the view node.
        if(!viewport || !viewport->viewNode() || (viewport->viewNode() != node && viewport->viewNode()->lookatTargetNode() != node)) {
            // Evaluate pipeline and render the resulting data objects.
            PipelineEvaluationFuture pipelineEvaluation;
            if(!isInteractive()) {
                PipelineEvaluationRequest request(time());
                request.setThrowOnError(stopOnPipelineError());
                pipelineEvaluation = pipeline->evaluateRenderingPipeline(request);
                if(!pipelineEvaluation.waitForFinished())
                    return false;
            }
            const PipelineFlowState& state = pipelineEvaluation.isValid()
                                             ? pipelineEvaluation.result()
                                             : pipeline->evaluatePipelineSynchronous(time(), true);
            if(state) {
                // Invoke all vis elements of all data objects in the pipeline state.
                ConstDataObjectPath dataObjectPath;
                if(!renderDataObject(state.data(), pipeline, state, dataObjectPath))
                    return false;
                OVITO_ASSERT(dataObjectPath.empty());
            }
        }
    }

    // Render child nodes.
    for(SceneNode* child : node->children()) {
        if(!renderSceneNode(child, viewport))
            return false;
    }

    return !this_task::isCanceled();
}

/******************************************************************************
* Generates the visual representation of a data object and all its sub-objects.
******************************************************************************/
bool FrameGraph::renderDataObject(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath)
{
    // Stop if rendering has been canceled.
    if(this_task::isCanceled())
        return false;

    bool isOnStack = false;

    // Call all attached vis elements of the data object.
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
                status = vis->render(dataObjectPath, state, *this, pipeline);
                // Pass error status codes to the exception handler below.
                if(status.type() == PipelineStatus::Error)
                    throw Exception(status.text());
                // In console mode, print warning messages to the terminal.
                if(status.type() == PipelineStatus::Warning && !status.text().isEmpty() && Application::instance()->consoleMode()) {
                    qWarning() << "WARNING: Visual element" << vis->objectTitle() << "reported:" << status.text();
                }
            }
            catch(Exception& ex) {
                status = ex;
                ex.prependToMessage(QStringLiteral("Visual element '%1' reported an error during rendering: ").arg(vis->objectTitle()));
                // If the vis element fails, interrupt rendering process in console mode; swallow exceptions in GUI mode.
                if(stopOnPipelineError())
                    throw;
            }
            // Unless the vis element has indicated that it is in control of the status,
            // automatically adopt the outcome of the rendering operation as status code.
            if(!vis->manualErrorStateControl())
                vis->setStatus(status);
        }
    }

    // Recursively visit the sub-objects of the data object and render them as well.
    dataObj->visitSubObjects([&](const DataObject* subObject) {
        // Push the next data object onto the stack.
        if(!isOnStack) {
            dataObjectPath.push_back(dataObj);
            isOnStack = true;
        }
        return !renderDataObject(subObject, pipeline, state, dataObjectPath);
    });

    // Pop the data object from the stack.
    if(isOnStack)
        dataObjectPath.pop_back();

    return !this_task::isCanceled();
}

/******************************************************************************
* Render the overlays/underlays of a viewport.
******************************************************************************/
bool FrameGraph::renderOverlays(Viewport* viewport, bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams)
{
    for(ViewportOverlay* layer : (underlays ? viewport->underlays() : viewport->overlays())) {
        if(layer->isEnabled()) {
            layer->render(*this, logicalViewportRect, physicalViewportRect, noninteractiveProjParams);
            if(this_task::isCanceled())
                return false;
        }
    }

    return !this_task::isCanceled();
}

/******************************************************************************
* Renders a 2d polyline in the viewport.
******************************************************************************/
void FrameGraph::render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed)
{
    OVITO_ASSERT(count >= 2);

#if 0 // TODO
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
#endif
}

/******************************************************************************
 * Replaces all text primitives with (cached) image primitives.
 ******************************************************************************/
void FrameGraph::renderTextAsImagePrimitives()
{
    for(RenderingCommand& command : _commands) {
        if(const TextPrimitive* primitive = dynamic_cast<const TextPrimitive*>(command.primitive())) {
            if(!primitive->text().isEmpty()) {
                // Look up the Qt image for the text in the cache.
                auto& [image, offset] = visCache().lookup<std::tuple<QImage, QPointF>>(
                    RendererResourceKey<struct TextImageCache, QString, ColorA, ColorA, FloatType, FloatType, qreal, QString, bool, int, Qt::TextFormat>{
                                                            primitive->text(), primitive->color(),
                                                            primitive->outlineColor(), primitive->outlineWidth(), primitive->rotation(),
                                                            devicePixelRatio(), primitive->font().key(), primitive->useTightBox(),
                                                            primitive->alignment(), primitive->textFormat()});

                if(image.isNull()) {
                    Qt::TextFormat resolvedTextFormat = primitive->resolvedTextFormat();
                    qreal outlineWidth = primitive->effectiveOutlineWidth(devicePixelRatio());

                    // Measure text size in local text coordinate system (does NOT include alignment/offset/rotation/outline).
                    // Bounds are calculated as if text was drawn at base coordinates (0,0).
                    QRectF textBounds = primitive->queryLocalBounds(devicePixelRatio(), resolvedTextFormat);

                    // Compute axis-aligned bounding box in absolute window coordinate system.
                    QRectF boundingBox = primitive->computeBoundingBox(textBounds.size(), devicePixelRatio());

                    // Generate texture image.
                    QRect pixelBounds = boundingBox.toAlignedRect();
                    image = QImage(pixelBounds.width(), pixelBounds.height(), preferredImageFormat());
                    image.setDevicePixelRatio(devicePixelRatio());
                    image.fill(0);
                    QPainter painter(&image);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setRenderHint(QPainter::TextAntialiasing);

                    painter.translate(
                        (primitive->position().x() - boundingBox.left()) / devicePixelRatio(),
                        (primitive->position().y() - boundingBox.top()) / devicePixelRatio());

                    // Start with top-left alignment.
                    QPointF textOffset(-textBounds.left(), -textBounds.top());

                    // Apply horizontal alignment.
                    if(primitive->alignment() & Qt::AlignRight)
                        textOffset.rx() += -textBounds.width();
                    else if(primitive->alignment() & Qt::AlignHCenter)
                        textOffset.rx() += -textBounds.width() / 2;

                    // Apply vertical alignment.
                    if(primitive->alignment() & Qt::AlignBottom)
                        textOffset.ry() += -textBounds.height();
                    else if(primitive->alignment() & Qt::AlignVCenter)
                        textOffset.ry() += -textBounds.height() / 2;

                    if(primitive->rotation() != 0) {
                        // Rotate around point given by the primitive's position.
                        qreal x = textOffset.x() * std::cos(primitive->rotation()) - textOffset.y() * std::sin(primitive->rotation());
                        qreal y = textOffset.x() * std::sin(primitive->rotation()) + textOffset.y() * std::cos(primitive->rotation());
                        painter.translate(x / devicePixelRatio(), y / devicePixelRatio());
                        painter.rotate(qRadiansToDegrees(primitive->rotation()));
                    }
                    else {
                        painter.translate(textOffset.x() / devicePixelRatio(), textOffset.y() / devicePixelRatio());
                    }

                    // Draw text.
                    primitive->draw(painter, resolvedTextFormat, textBounds.width() / devicePixelRatio());
                    painter.end();

                    // Store image primitive in cache including offset vector relative to primitive position.
                    offset = boundingBox.topLeft() - QPointF(primitive->position().x(), primitive->position().y());
                }

                // Compute absolute image paint position by adding precomputed offset vector to current primitive position.
                QPoint alignedPos = (QPointF(primitive->position().x(), primitive->position().y()) + offset).toPoint();
                std::unique_ptr<ImagePrimitive> imagePrimitive = std::make_unique<ImagePrimitive>();
                imagePrimitive->setRectWindow(QRect(alignedPos, image.size()));
                imagePrimitive->setImage(image);

                // Replace original text primitive with the image primitive.
                command.setPrimitive(std::move(imagePrimitive));
            }
            else {
                // Remove empty text primitives.
                command.setPrimitive({});
            }
        }
    }
}

/******************************************************************************
* Adjust wireframe line widths to match device pixel ratio.
******************************************************************************/
void FrameGraph::adjustWireframeLineWidths()
{
    for(RenderingCommand& command : _commands) {
        if(LinePrimitive* primitive = dynamic_cast<LinePrimitive*>(command.primitive())) {
            if(primitive->lineWidth() <= 0) {
                // Make the line one device-independent pixel wide.
                primitive->setLineWidth(devicePixelRatio());
            }
        }
    }
}

/******************************************************************************
* Returns the line rendering width to use in object picking mode.
******************************************************************************/
FloatType FrameGraph::defaultLinePickingWidth() const
{
    return FloatType(6) * devicePixelRatio();
}

}   // End of namespace
