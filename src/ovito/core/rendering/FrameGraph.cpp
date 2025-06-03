////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ObjectPickInfo);
IMPLEMENT_ABSTRACT_OVITO_CLASS(FrameGraph);

#ifndef OVITO_BUILD_MONOLITHIC
void RenderingPrimitive::__key_function() {}
void ParticlePrimitive::__key_function() {}
void CylinderPrimitive::__key_function() {}
void MeshPrimitive::__key_function() {}
void LinePrimitive::__key_function() {}
void TextPrimitive::__key_function() {}
void MarkerPrimitive::__key_function() {}
void ImagePrimitive::__key_function() {}
void VolumePrimitive::__key_function() {}
#endif

/******************************************************************************
 * Adds a 3d rendering primitive to the current layer of the frame graph.
 * Automatically computes the bounding box of the primitive and the model-to-world transformation.
 * Optional: A FrameGraph::RenderingCommand::Flag can be give, default is "NoFlags"
 ******************************************************************************/
FrameGraph::RenderingCommand& FrameGraph::addPrimitive(RenderingCommandGroup& group, std::unique_ptr<RenderingPrimitive> primitive,
                                                       OORef<const SceneNode> sceneNode, OORef<ObjectPickInfo> pickInfo,
                                                       uint32_t pickElementOffset, RenderingCommand::Flags flags)
{
    OVITO_ASSERT(sceneNode);
    OVITO_ASSERT(this_task::isMainThread()); // Must be called from main thread, because we are accessing the pipeline.

    const AffineTransformation& tm = sceneNode->getWorldTransform(time());
    Box3 boundingBox = primitive->computeBoundingBox(visCache());
    return group.addPrimitive(std::move(primitive), tm, boundingBox, std::move(sceneNode), std::move(pickInfo), pickElementOffset, flags);
}

/******************************************************************************
 * Adds a 3d rendering primitive to the current layer of the frame graph.
 * Automatically computes the bounding box of the primitive and the model-to-world transformation.
 ******************************************************************************/
FrameGraph::RenderingCommand& FrameGraph::addPrimitiveNonpickable(RenderingCommandGroup& group,
                                                                  std::unique_ptr<RenderingPrimitive> primitive, const SceneNode* sceneNode)
{
    OVITO_ASSERT(sceneNode);
    OVITO_ASSERT(this_task::isMainThread()); // Must be called from main thread, because we are accessing the pipeline.

    Box3 boundingBox = primitive->computeBoundingBox(visCache());
    return group.addPrimitiveNonpickable(std::move(primitive), sceneNode->getWorldTransform(time()), boundingBox);
}

/******************************************************************************
 * Adds a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
 * Automatically computes the bounding box of the primitive and the model-to-world transformation.\
 * Optional: A FrameGraph::RenderingCommand::Flag can be give, default is "NoFlags"
 ******************************************************************************/
FrameGraph::RenderingCommand& FrameGraph::RenderingCommandGroup::addPrimitive(std::unique_ptr<RenderingPrimitive> primitive,
                                                                              const AffineTransformation& tm, const Box3& box,
                                                                              OORef<const SceneNode> pickableSceneNode,
                                                                              OORef<ObjectPickInfo> pickInfo, uint32_t pickElementOffset,
                                                                              RenderingCommand::Flags flags)
{
    // Add the world-space bounding box of the primitive to the group's bounding box.
    _boundingBox.addBox(box.transformed(tm));

    return addCommand(flags, std::move(primitive), tm, std::move(pickableSceneNode), std::move(pickInfo), pickElementOffset);
}

/******************************************************************************
 * Adds a 3d rendering primitive to the current layer of the frame graph with a pre-computed bounding box.
 * Automatically computes the bounding box of the primitive and the model-to-world transformation.
 ******************************************************************************/
FrameGraph::RenderingCommand& FrameGraph::RenderingCommandGroup::addPrimitiveNonpickable(std::unique_ptr<RenderingPrimitive> primitive,
                                                                                         const AffineTransformation& tm, const Box3& box)
{
    // Add the world-space bounding box of the primitive to the group's bounding box.
    _boundingBox.addBox(box.transformed(tm));

    return addCommand(RenderingCommand::ExcludeFromPicking, std::move(primitive), tm);
}

/******************************************************************************
 * Adds a primitive to the frame graph containing pre-projected coordinates.
 ******************************************************************************/
FrameGraph::RenderingCommand& FrameGraph::RenderingCommandGroup::addPrimitivePreprojected(std::unique_ptr<RenderingPrimitive> primitive)
{
    return addCommand(RenderingCommand::ExcludeFromPicking, std::move(primitive), AffineTransformation::Zero());
}

/******************************************************************************
 * Computes the combined scene bounding box from all command groups.
 ******************************************************************************/
void FrameGraph::computeSceneBoundingBox()
{
    for(const RenderingCommandGroup& group : _commandGroups) {
        _sceneBoundingBox.addBox(group.boundingBox());
    }
}

/******************************************************************************
 * Renders a 2d polyline in the viewport.
 ******************************************************************************/
void FrameGraph::RenderingCommandGroup::render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed,
                                                         const QSize& logicalViewportSize)
{
    OVITO_ASSERT(count >= 2);
    OVITO_ASSERT(layerType() == OverLayer || layerType() == UnderLayer);

    FloatType w = logicalViewportSize.width();
    FloatType h = logicalViewportSize.height();
    auto projectPoint = [&](const Point2& p) { return Point3G(2 * p.x() / w - 1, 1 - 2 * p.y() / h, 0.0); };

    BufferFactory<Point3G> vertices((closed ? count : count - 1) * 2);
    Point3G* lineSegment = vertices.begin();
    for(int i = 0; i < count - 1; i++, lineSegment += 2) {
        lineSegment[0] = projectPoint(points[i]);
        lineSegment[1] = projectPoint(points[i + 1]);
    }
    if(closed) {
        lineSegment[0] = projectPoint(points[count - 1]);
        lineSegment[1] = projectPoint(points[0]);
        lineSegment += 2;
    }
    OVITO_ASSERT(lineSegment == vertices.end());

    std::unique_ptr<LinePrimitive> primitive = std::make_unique<LinePrimitive>();
    primitive->setUniformColor(color);
    primitive->setPositions(vertices.take());

    addPrimitivePreprojected(std::move(primitive));
}

/******************************************************************************
 * Replaces all text primitives with (cached) image primitives.
 ******************************************************************************/
void FrameGraph::renderTextAsImagePrimitives()
{
    for(RenderingCommandGroup& commandGroup : _commandGroups) {
        for(RenderingCommand& command : commandGroup.commands()) {
            if(const TextPrimitive* primitive = dynamic_cast<const TextPrimitive*>(command.primitive())) {
                if(!primitive->text().isEmpty()) {
                    // Look up the Qt image for the text in the cache.
                    const auto& [image, offset] = visCache().lookup<std::tuple<QImage, QPointF>>(
                        RendererResourceKey<struct TextImageCache, QString, ColorA, ColorA, FloatType, FloatType, qreal, QString, bool, int,
                                            Qt::TextFormat>{primitive->text(), primitive->color(), primitive->outlineColor(),
                                                            primitive->outlineWidth(), primitive->rotation(), devicePixelRatio(),
                                                            primitive->font().key(), primitive->useTightBox(), primitive->alignment(),
                                                            primitive->textFormat()},
                        [&](QImage& image, QPointF& offset) {
                            Qt::TextFormat resolvedTextFormat = primitive->resolvedTextFormat();

                            // Measure text size in local text coordinate system (does NOT include alignment/offset/rotation/outline).
                            // Bounds are calculated as if text was drawn at base coordinates (0,0).
                            QRectF textBounds = primitive->queryLocalBounds(devicePixelRatio(), resolvedTextFormat);

                            // Compute axis-aligned bounding box in absolute window coordinate system.
                            QRectF boundingBox = primitive->computeBounds(textBounds.size(), devicePixelRatio());

                            // Generate texture image.
                            QRect pixelBounds = boundingBox.toAlignedRect();
                            image = QImage(pixelBounds.width(), pixelBounds.height(), preferredImageFormat());
                            image.setDevicePixelRatio(devicePixelRatio());
                            image.fill(0);
                            QPainter painter(&image);
                            painter.setRenderHint(QPainter::Antialiasing);
                            painter.setRenderHint(QPainter::TextAntialiasing);

                            painter.translate((primitive->position().x() - boundingBox.left()) / devicePixelRatio(),
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
                                qreal x =
                                    textOffset.x() * std::cos(primitive->rotation()) - textOffset.y() * std::sin(primitive->rotation());
                                qreal y =
                                    textOffset.x() * std::sin(primitive->rotation()) + textOffset.y() * std::cos(primitive->rotation());
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
                        });

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
}

/******************************************************************************
 * Adjust wireframe line widths to match device pixel ratio.
 ******************************************************************************/
void FrameGraph::adjustWireframeLineWidths()
{
    for(RenderingCommandGroup& commandGroup : _commandGroups) {
        for(RenderingCommand& command : commandGroup.commands()) {
            if(LinePrimitive* primitive = dynamic_cast<LinePrimitive*>(command.primitive())) {
                // Make the line 1 device-independent pixel wide.
                if(primitive->lineWidth() <= 0) {
                    primitive->setLineWidth(devicePixelRatio());
                }
                if(primitive->pickingLineWidth() <= 0) {
                    primitive->setPickingLineWidth(defaultLinePickingWidth());
                }
            }
        }
    }
}

/******************************************************************************
 * Returns the line rendering width to use in object picking mode.
 ******************************************************************************/
FloatType FrameGraph::defaultLinePickingWidth() const { return FloatType(6) * devicePixelRatio(); }

}  // namespace Ovito
