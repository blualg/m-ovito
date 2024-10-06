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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/Application.h>
#include "TextLabelOverlay.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TextLabelOverlay);
OVITO_CLASSINFO(TextLabelOverlay, "DisplayName", "Text label");
DEFINE_PROPERTY_FIELD(TextLabelOverlay, alignment);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, font);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, fontSize);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, labelText);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, offsetX);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, offsetY);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, textColor);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, outlineColor);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, outlineEnabled);
DEFINE_PROPERTY_FIELD(TextLabelOverlay, valueFormatString);
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, font, "Font");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, fontSize, "Font size");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, offsetX, "Offset X");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, offsetY, "Offset Y");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, textColor, "Text color");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, outlineColor, "Outline color");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, outlineEnabled, "Enable outline");
SET_PROPERTY_FIELD_LABEL(TextLabelOverlay, valueFormatString, "Number format");
SET_PROPERTY_FIELD_UNITS(TextLabelOverlay, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(TextLabelOverlay, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TextLabelOverlay, fontSize, FloatParameterUnit, 0, 1);

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void TextLabelOverlay::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(alignment) && !isBeingLoaded() && !isBeingDeleted() && !isUndoingOrRedoing() && this_task::isInteractive()) {
        // Automatically reset offset to zero when user changes the alignment of the overlay in the viewport.
        setOffsetX(0);
        setOffsetY(0);
    }
    else if(field == PROPERTY_FIELD(TextLabelOverlay::labelText) && !isBeingLoaded()) {
        // Changes of some the overlay's parameters affect the result of TextLabelOverlay::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    ViewportOverlay::propertyChanged(field);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the modifier's title in the pipeline editor list.
******************************************************************************/
QVariant TextLabelOverlay::getPipelineEditorShortInfo(Scene* scene) const
{
    return labelText();
}

/******************************************************************************
* Lets the overlay paint its contents into the framebuffer.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> TextLabelOverlay::render(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene)
{
    // Check alignment parameter.
    if(!frameGraph.isInteractive())
        checkAlignmentParameterValue(alignment());

    if(pipeline()) {
        PipelineEvaluationRequest request(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive());
        return pipeline()->evaluatePipeline(request).then(ObjectExecutor(this), [this, frameGraph=OORef<FrameGraph>(&frameGraph), &commandGroup, physicalViewportRect](const PipelineFlowState& state) {
            return renderImplementation(*frameGraph, commandGroup, physicalViewportRect, state);
        });
    }
    else {
        return renderImplementation(frameGraph, commandGroup, physicalViewportRect, {});
    }
}

/******************************************************************************
* This method paints the overlay contents onto the given canvas.
******************************************************************************/
PipelineStatus TextLabelOverlay::renderImplementation(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& viewportRect, const PipelineFlowState& flowState)
{
    // Resolve the label text.
    QString textString = labelText();

    // Resolve global attributes referenced by placeholders in the text string.
    if(flowState && textString.contains('[')) {
        const QVariantMap& attributes = flowState.buildAttributesMap();

        // Prepare the floating-point format string.
        QByteArray format = valueFormatString().toUtf8();
        if(format.isEmpty() || format.contains("%s")) format = QByteArrayLiteral("###");

        for(auto a = attributes.cbegin(); a != attributes.cend(); ++a) {
            QString valueString;
            if(a.value().typeId() == QMetaType::Double || a.value().typeId() == QMetaType::Float) {
                valueString = QString::asprintf(format.constData(), a.value().toDouble());
            }
            else {
                valueString = a.value().toString();
            }

            textString.replace(QStringLiteral("[") + a.key() + QStringLiteral("]"), valueString);
        }
    }
    if(textString.isEmpty())
        return {};

    // Prepare the text rendering primitive.
    std::unique_ptr<TextPrimitive> textPrimitive = std::make_unique<TextPrimitive>();
    textPrimitive->setColor(textColor());
    if(outlineEnabled())
        textPrimitive->setOutlineColor(outlineColor());
    textPrimitive->setAlignment(alignment());
    textPrimitive->setText(std::move(textString));
    textPrimitive->setTextFormat(Qt::AutoText);

    // Resolve the font used by the label.
    FloatType fontSize = this->fontSize() * viewportRect.height();
    if(fontSize <= 0)
        return {};
    QFont font = this->font();
    font.setPointSizeF(fontSize / frameGraph.devicePixelRatio()); // Font size if always in logical coordinates.
    textPrimitive->setFont(std::move(font));

    // Add an inset to the framebuffer rect.
    int margins = (int)fontSize;
    QRectF marginRect = viewportRect.marginsRemoved(QMargins(margins, margins, margins, margins));

    // Determine alignment of the text box in the framebuffer rect.
    Point2 pos;

    if(alignment() & Qt::AlignRight) pos.x() = marginRect.left() + marginRect.width();
    else if(alignment() & Qt::AlignHCenter) pos.x() = marginRect.left() + marginRect.width() / 2.0;
    else pos.x() = marginRect.left();

    if(alignment() & Qt::AlignBottom) pos.y() = marginRect.top() + marginRect.height();
    else if(alignment() & Qt::AlignVCenter) pos.y() = marginRect.top() + marginRect.height() / 2.0;
    else pos.y() = marginRect.top();

    // Compute final positions.
    textPrimitive->setPositionWindow(pos + Vector2(offsetX() * viewportRect.width(), -offsetY() * viewportRect.height()));

    // Add drawing command to frame graph.
    commandGroup.addPrimitivePreprojected(std::move(textPrimitive));

    return {};
}

}   // End of namespace
