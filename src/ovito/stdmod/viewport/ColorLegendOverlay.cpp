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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/rendering/ColorMapHelper.h>

#include <algorithm>
#include "ColorLegendOverlay.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorLegendOverlay);
OVITO_CLASSINFO(ColorLegendOverlay, "DisplayName", "Color legend");
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, alignment);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, orientation);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, legendSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, font);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, fontSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, relLabelFontSize);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, offsetX);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, offsetY);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, aspectRatio);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, textColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, outlineColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, outlineEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, title);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, label1);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, label2);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, valueFormatString);
DEFINE_REFERENCE_FIELD(ColorLegendOverlay, modifier);
DEFINE_REFERENCE_FIELD(ColorLegendOverlay, colorMapping);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, sourceProperty);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, borderEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, borderColor);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, ticksEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, tickSpacing);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, titleRotationEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, backgroundEnabled);
DEFINE_PROPERTY_FIELD(ColorLegendOverlay, backgroundColor);
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, orientation, "Orientation");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, legendSize, "Legend size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, font, "Font");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, fontSize, "Font size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, relLabelFontSize, "Label size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, offsetX, "Offset X");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, offsetY, "Offset Y");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, aspectRatio, "Aspect ratio");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, textColor, "Font color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, outlineColor, "Outline color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, outlineEnabled, "Text outline");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, title, "Title");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, label1, "Label 1");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, label2, "Label 2");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, valueFormatString, "Number format");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, sourceProperty, "Source property");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, borderEnabled, "Draw border");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, borderColor, "Border color");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, ticksEnabled, "Draw ticks");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, tickSpacing, "Spacing");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, titleRotationEnabled, "Rotate title");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, backgroundEnabled, "Background enabled");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, backgroundColor, "Background color");
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, legendSize, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, aspectRatio, FloatParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(ColorLegendOverlay, fontSize, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, relLabelFontSize, PercentParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, tickSpacing, FloatParameterUnit, 0);

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void ColorLegendOverlay::initializeOverlay(Viewport* viewport)
{
    if(this_task::isInteractive() && !pipeline()) {

        // Find a ColorCodingModifier in the scene that we can connect to.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                PipelineNode* node = sceneNode->pipeline()->head();
                for(;;) {
                    if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(node)) {
                        if(ColorCodingModifier* mod = dynamic_object_cast<ColorCodingModifier>(modNode->modifier())) {
                            setPipeline(sceneNode->pipeline());
                            setModifier(mod);
                            if(mod->isEnabled())
                                return false; // Stop search if modifier is enabled; otherwise, keep looking for an alternative.
                        }
                        node = modNode->input();
                    }
                    else break;
                }
                return true;
            });
        }

        // If there is no ColorCodingModifier in the scene, initialize the overlay to use
        // the first available typed property as color mapping source.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                const PipelineFlowState& state = sceneNode->pipeline()->getCachedPipelineOutput(viewport->currentTime());
                for(const ConstDataObjectPath& dataPath : state.getObjectsRecursive(Property::OOClass())) {
                    const Property* property = static_object_cast<Property>(dataPath.back());
                    // Check if the property is a typed property, i.e. it has one or more ElementType objects attached to it.
                    if(property->isTypedProperty() && dataPath.size() >= 2) {
                        setPipeline(sceneNode->pipeline());
                        setSourceProperty(dataPath);
                        return false; // Stop search.
                    }
                }
                return true;
            });
        }

        // If we still don't have a valid source, look for a visual element in the scene which uses pseudo-color mapping.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
                for(DataVis* vis : sceneNode->pipeline()->visElements()) {
                    if(vis->isEnabled()) {
                        for(const PropertyFieldDescriptor* field : vis->getOOMetaClass().propertyFields()) {
                            if(field->isReferenceField() && field->targetClass()->isDerivedFrom(PropertyColorMapping::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM) && !field->isVector()) {
                                if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(vis->getReferenceFieldTarget(field))) {
                                    if(mapping->sourceProperty()) {
                                        setPipeline(sceneNode->pipeline());
                                        setColorMapping(mapping);
                                        return false; // Stop search.
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                return true;
            });
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ColorLegendOverlay::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(alignment) && !isBeingLoaded() && !isBeingDeleted() && !isUndoingOrRedoing() && this_task::isInteractive()) {
        // Automatically reset offset to zero when user changes the alignment of the overlay in the viewport.
        setOffsetX(0);
        setOffsetY(0);
    }
    else if(field == PROPERTY_FIELD(ColorLegendOverlay::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the overlay's parameters affect the result of ColorLegendOverlay::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    ViewportOverlay::propertyChanged(field);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ColorLegendOverlay::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == modifier()) {
        // Changes of some the object's parameters affect the result of ColorLegendOverlay::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return ViewportOverlay::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ColorLegendOverlay::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if((field == PROPERTY_FIELD(modifier) || field == PROPERTY_FIELD(colorMapping)) && !isBeingLoaded()) {
        // Changes of some the object's parameters affect the result of ColorLegendOverlay::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    ViewportOverlay::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the modifier's title in the pipeline editor list.
******************************************************************************/
QVariant ColorLegendOverlay::getPipelineEditorShortInfo(Scene* scene) const
{
    if(modifier()) {
        return modifier()->sourceProperty().nameWithComponent();
    }
    else if(colorMapping()) {
        return colorMapping()->sourceProperty().nameWithComponent();
    }
    else if(sourceProperty()) {
        return sourceProperty().dataTitleOrPath();
    }
    return {};
}

/******************************************************************************
* Lets the overlay paint its contents into the framebuffer.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> ColorLegendOverlay::render(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene)
{
    // Reset auto-generated label texts. Will be newly set by rendering code.
    _autoTitleText.clear();
    _autoLabel1Text.clear();
    _autoLabel2Text.clear();

    // Check alignment parameter.
    if(!frameGraph.isInteractive())
        checkAlignmentParameterValue(alignment());

    // Calculate position and size of color legend rectangle.
    FloatType legendSize = this->legendSize() * physicalViewportRect.height();
    if(legendSize <= 0) return {};

    FloatType colorBarWidth = legendSize;
    FloatType colorBarHeight = colorBarWidth / std::max(FloatType(0.01), aspectRatio());
    bool vertical = (orientation() == Qt::Vertical);
    if(vertical)
        std::swap(colorBarWidth, colorBarHeight);

    QPointF origin(offsetX() * physicalViewportRect.width() + physicalViewportRect.left(), -offsetY() * physicalViewportRect.height() + physicalViewportRect.top());
    FloatType hmargin = FloatType(0.01) * physicalViewportRect.width();
    FloatType vmargin = FloatType(0.01) * physicalViewportRect.height();

    if(alignment() & Qt::AlignLeft) origin.rx() += hmargin;
    else if(alignment() & Qt::AlignRight) origin.rx() += physicalViewportRect.width() - hmargin - colorBarWidth;
    else if(alignment() & Qt::AlignHCenter) origin.rx() += FloatType(0.5) * physicalViewportRect.width() - FloatType(0.5) * colorBarWidth;

    if(alignment() & Qt::AlignTop) origin.ry() += vmargin;
    else if(alignment() & Qt::AlignBottom) origin.ry() += physicalViewportRect.height() - vmargin - colorBarHeight;
    else if(alignment() & Qt::AlignVCenter) origin.ry() += FloatType(0.5) * physicalViewportRect.height() - FloatType(0.5) * colorBarHeight;

    QRectF colorBarRect(origin, QSizeF(colorBarWidth, colorBarHeight));

    // Determine the source pipeline.
    Pipeline* sourcePipeline = this->pipeline();
    if(!sourcePipeline) {
        // If no source pipeline has been specified by the user, use the first pipeline found in the current scene as a fallback.
        scene->visitPipelines([&](SceneNode* sceneNode) {
            sourcePipeline = sceneNode->pipeline();
            return false;
        });
    }

    if(sourcePipeline) {
        if(modifier()) {
            // Get modifier's parameters.
            _autoTitleText = modifier()->sourceProperty().nameWithComponent();

            // If the auto-adjust option is enabled for the color coding modifier, we have to do some more work to figure out
            // the current value range of the color mapping. It requires a partial pipeline evaluation up to the color coding modifier.
            if(modifier()->autoAdjustRange() && (label1().isEmpty() || label2().isEmpty())) {
                // Figure out which of the modifier's modification nodes belongs to the pipeline associated with this viewport overlay.
                ModificationNode* modNode = nullptr;
                PipelineNode* node = sourcePipeline->head();
                for(;;) {
                    if((modNode = dynamic_object_cast<ModificationNode>(node))) {
                        if(modNode->modifier() == modifier())
                            break;
                        node = modNode->input();
                    }
                    else break;
                }
                if(!modNode)
                    throw Exception(tr("Selected color coding could not be found in the selected pipeline."));

                // Request modifier's output.
                PipelineEvaluationResult pipelineEvaluationResult = modNode->evaluate(PipelineEvaluationRequest(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive()));

                // Wait for the modifier results.
                return pipelineEvaluationResult.then(ObjectExecutor(this), [this, frameGraph=OORef<FrameGraph>(&frameGraph), &commandGroup, modNode, colorBarRect, legendSize](const PipelineFlowState& state) -> PipelineStatus {
                    FloatType startValue = std::numeric_limits<FloatType>::quiet_NaN();
                    FloatType endValue = std::numeric_limits<FloatType>::quiet_NaN();
                    QVariant minValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMin"));
                    QVariant maxValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMax"));
                    if(minValue.isValid() && maxValue.isValid()) {
                        startValue = minValue.value<FloatType>();
                        endValue = maxValue.value<FloatType>();
                    }
                    if(modifier() && modifier()->useDiscreteColorMap() && minValue.isValid() && maxValue.isValid()) {
                        // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                        drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                             getDiscreteColorMapLabels(modifier()->colorGradient(), startValue, endValue, orientation()));
                    }
                    else if(modifier()) {
                        drawContinuousColorMap(*frameGraph, commandGroup, colorBarRect, legendSize,
                                               PseudoColorMapping(startValue, endValue, modifier()->colorGradient()));
                    }
                    return {};
                });
            }
            else {
                if(modifier()->useDiscreteColorMap() && std::isfinite(modifier()->startValue()) && std::isfinite(modifier()->endValue())) {
                    // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                    drawDiscreteColorMap(frameGraph, commandGroup, colorBarRect, legendSize,
                                         getDiscreteColorMapLabels(modifier()->colorGradient(), modifier()->startValue(),
                                                                   modifier()->endValue(), orientation()));
                }
                else {
                    drawContinuousColorMap(
                        frameGraph, commandGroup, colorBarRect, legendSize,
                        PseudoColorMapping(modifier()->startValue(), modifier()->endValue(), modifier()->colorGradient()));
                }
                return {};
            }
        }
        else if(colorMapping()) {
            _autoTitleText = colorMapping()->sourceProperty().nameWithComponent();
            if(colorMapping()->useDiscreteColorMap() && std::isfinite(colorMapping()->startValue()) &&
               std::isfinite(colorMapping()->endValue())) {
                // Reverse discrete colormap if vertical orientation is used to match the continuous colormap's direction.
                drawDiscreteColorMap(frameGraph, commandGroup, colorBarRect, legendSize,
                                     getDiscreteColorMapLabels(colorMapping()->pseudoColorMapping().gradient(),
                                                               colorMapping()->startValue(), colorMapping()->endValue(), orientation()));
            }
            else {
                drawContinuousColorMap(frameGraph, commandGroup, colorBarRect, legendSize, colorMapping()->pseudoColorMapping());
            }
            return {};
        }
        else if(sourceProperty()) {
            // Evaluate pipeline.
            PipelineEvaluationResult pipelineEvaluationResult = sourcePipeline->evaluatePipeline(PipelineEvaluationRequest(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive()));

            // Wait for the pipeline results.
            return pipelineEvaluationResult.then(ObjectExecutor(this), [this, frameGraph=OORef<FrameGraph>(&frameGraph), &commandGroup, colorBarRect, legendSize](const PipelineFlowState& state) -> PipelineStatus {

                // Look up the typed property.
                DataOORef<const Property> typedProperty = state.getLeafObject(sourceProperty());

                // Verify that the typed property, which has been selected as the source of the color legend, is available.
                if(!typedProperty) {
                    // Escalate to an error state if in console mode.
                    if(!this_task::isInteractive())
                        throw Exception(tr("The property '%1' set as source of the color legend is not present in the data pipeline output.").arg(sourceProperty().dataTitleOrPath()));
                    else
                        return PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not available in the pipeline output.").arg(sourceProperty().dataTitleOrPath()));
                }
                else if(!typedProperty->isTypedProperty()) {
                    // Escalate to an error state if in console mode.
                    if(!this_task::isInteractive())
                        throw Exception(tr("The property '%1' set as source of the color legend is not a typed property, i.e., it has no ElementType(s) attached.").arg(sourceProperty().dataTitleOrPath()));
                    else
                        return PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not a typed property.").arg(sourceProperty().dataTitleOrPath()));
                }

                _autoTitleText = typedProperty->objectTitle();
                drawDiscreteColorMap(*frameGraph, commandGroup, colorBarRect, legendSize, getDiscreteColorMapLabels(typedProperty));
                return {};
            });
        }
    }

    // Escalate to an error state if in console mode.
    if(!this_task::isInteractive()) {
        if(!sourcePipeline)
            throw Exception(tr("You are rendering a viewport with an attached ColorLegendOverlay that has no "
                                "source pipeline set. Make sure you set the legend's 'pipeline' field."));
        else
            throw Exception(tr("You are rendering a viewport with an attached ColorLegendOverlay that has no "
                                "source color mapping set. Did you forget to specify a color mapping for the color legend? "
                                "Make sure you set the legend's 'modifier', 'property', or 'color_mapping_source' field. "));
    }
    else {
        // Set warning status to be displayed in the GUI.
        return PipelineStatus(PipelineStatus::Warning, tr("No source color mapping has been specified for the color legend."));
    }
}

namespace {
/******************************************************************************
 * Estimates the order of magnitude of a given value
 * estimate since this approach has no mathematical proof
 * might not behave well for all edge cases
 ******************************************************************************/
[[nodiscard]] inline int estimateOrderOfMagnitude(const FloatType value)
{
    FloatType result = std::abs(value);
    constexpr FloatType eps{1e-18};
    if(result < eps) {
        return 0;
    }
    result = std::floor(std::log10(result));
    return static_cast<int>(result);
}

/******************************************************************************
 * Estimates the nearest multiple of tickSpacing from start
 * returns static if static is an integer multiple of tickSpacing
 * estimate since this approach has no mathematical proof
 * might not behave well for all edge cases
 ******************************************************************************/
[[nodiscard]] inline FloatType getFirstTickValidValue(const FloatType start, const FloatType tickSpacing)
{
    return tickSpacing * std::ceil(start / tickSpacing);
}

/******************************************************************************
 * Returns the starting value and the tick spacing as function of a control parameter N. Increment (decrement) N to increase
 * (decrease) the tick spacing. Ideally N should start at 0.
 ******************************************************************************/
[[nodiscard]] inline std::tuple<FloatType, FloatType> getTickPositionsFromN(FloatType lowerLimit, FloatType upperLimit, const int N)
{
    constexpr std::array<FloatType, 4> steps{{2, 4, 5, 10}};
    constexpr int num_steps = std::size(steps);

    // a % b = (b + (a % b)) % b <- correct for negative values of a
    // Selects a valid multiple (step width) from the steps array (based on N)
    const int index{(N % num_steps + num_steps) % num_steps};
    // guarantees flooring division (even for negative numbers)
    // first scaling factor for step width
    // N==-5 gives index==1 and pow==-2 (if steps[][2,5,10}])
    const int pow = static_cast<int>(std::floor(N / static_cast<FloatType>(num_steps)));

    const int oom = estimateOrderOfMagnitude(upperLimit - lowerLimit);
    // inter * 10^pow * 10^(oom-1) = inter * 10^(pow+oom-1)
    // const FloatType inter{steps[index] * std::pow(10, pow) * std::pow(10, oom - 1)};
    const FloatType inter = steps[index] * std::pow(10, pow + oom - 1);
    return {getFirstTickValidValue(lowerLimit, inter), inter};
}

[[nodiscard]] inline int get_number_of_ticks(FloatType lowerLimit, FloatType upperLimit, FloatType inter)
{
    return static_cast<int>(std::round(std::abs(upperLimit - lowerLimit) / inter));
}
}  // namespace

/******************************************************************************
 * Determine the starting value and the tick spacing for a given color bar length and character size. Ticks should be estimate
 * from most to least dense resulting in generally more dense ticks.
 ******************************************************************************/
[[nodiscard]] std::tuple<FloatType, FloatType> ColorLegendOverlay::getAutomaticTickPositions(
    FloatType lowerLimit, FloatType upperLimit, const FloatType lenColorbar, const QFontMetricsF& fontMetrics,
    const QByteArray& labelFormat, const int maxIter) const
{
    // Sort upper and lower limit
    if(lowerLimit > upperLimit) std::swap(lowerLimit, upperLimit);

    // If the format string is empty (or format is %s) 4 ticks are shown as fallback
    if(labelFormat.isNull()) {
        return {(upperLimit - lowerLimit) / 4, (upperLimit - lowerLimit) / 4};
    }

    int scale{0};
    FloatType totalLabelSize;
    for(int i{0}; i < maxIter; i++) {
        const auto [start, inter]{getTickPositionsFromN(lowerLimit, upperLimit, scale)};
        int num_ticks{get_number_of_ticks(lowerLimit, upperLimit, inter)};
        if(num_ticks < 1) {
            scale--;
            continue;
        }
        if(orientation() == Qt::Horizontal) {
            // Sometimes start or start + inter might fall on a "shorter" string label. Two subsequent values need to be checked
            // to guarantee at least one "long" label (num_ticks + 1) to give some more space as usually ticks are not distributed
            // all the way to the color bar boundary
            totalLabelSize =
                (num_ticks + 1) *
                std::max(
                    fontMetrics.horizontalAdvance(QString::asprintf(labelFormat.constData(), start + inter)),
                    fontMetrics.horizontalAdvance(QString::asprintf(labelFormat.constData(), start + inter * (num_ticks - 1))));
        }
        else {  // Vertical
                // num_ticks+1 to account for the top and bottom label denoting the color bar limits
                // lineSpacing gives the character height + the line separation
            totalLabelSize = (num_ticks + 1) * fontMetrics.lineSpacing();
        }
        if(totalLabelSize < lenColorbar) {
            if(num_ticks > 1) return {start, inter};
            return {(upperLimit - lowerLimit) / 2, (upperLimit - lowerLimit)};
        }
        scale++;
    }
    // Fallback, if no good ticks can be found, a single tick is added in the center
    return {(upperLimit - lowerLimit) / 2, (upperLimit - lowerLimit)};
}

/******************************************************************************
 * Determine the starting value for a given tick spacing.
 ******************************************************************************/
[[nodiscard]] FloatType ColorLegendOverlay::getUserDefinedTickPositions(FloatType lowerLimit, FloatType upperLimit,
                                                                        const FloatType inter)
{
    // Sort upper and lower limit
    if(lowerLimit > upperLimit) std::swap(lowerLimit, upperLimit);
    return getFirstTickValidValue(lowerLimit, inter);
}

/******************************************************************************
* Draws the color legend for a Color Coding modifier.
******************************************************************************/
void ColorLegendOverlay::drawContinuousColorMap(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup,
                                                const QRectF& colorBarRect, FloatType legendSize, const PseudoColorMapping& mapping)
{
    const qreal devicePixelRatio = frameGraph.devicePixelRatio();

    // Controls the tick color: Currently the order is:
    // Border color -> text color
    const Color tickColor{(borderEnabled() ? borderColor() : textColor())};

    // Width of the ticks in pixel.
    const int tickWidth{(int)std::ceil(2.0 * devicePixelRatio)};

    // Relative height of the ticks (as fraction of gradient image size)
    constexpr FloatType innerTickHeight{0.4};
    constexpr FloatType outerTickHeight{0.2};

    // Enforces a minimum distance of ticks from the color bar limits.
    // This prevents duplication of the start and end values, especially for the horizontal color bar
    constexpr FloatType minTickDistanceFromEdge{0.005};

    // Allows the second to last and last tick of a vertical color bar to overlap slightly to get a more
    // pleasant look.
    constexpr FloatType tickOverlapFactor{0.8};

    if(!mapping.gradient())
        return;

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    int borderWidth = borderEnabled() ? tickWidth : 0;

    // Look up the image primitive for the color bar in the cache.
    const auto& [image, offset] = frameGraph.visCache().lookup<std::tuple<QImage, QPointF>>(
        RendererResourceKey<struct ColorBarImageCache, OORef<ColorCodingGradient>, FloatType, int, bool, Color, QSizeF>{
            mapping.gradient(), devicePixelRatio, orientation(), borderEnabled(), borderColor(), colorBarRect.size()},
        [&](QImage& image, QPointF& offset) {
            // Render the color bar into an image texture.
            // Allocate the image buffer.
            QSize gradientSize = colorBarRect.size().toSize();
            image = QImage(gradientSize.width() + 2 * borderWidth, gradientSize.height() + 2 * borderWidth,
                        frameGraph.preferredImageFormat());
            if(borderEnabled()) image.fill((QColor)borderColor());

            // Create the color gradient image.
            if(orientation() == Qt::Vertical) {
                for(int y = 0; y < gradientSize.height(); y++) {
                    FloatType t = (FloatType)y / (FloatType)std::max(1, gradientSize.height() - 1);
                    unsigned int color = QColor(mapping.gradient()->valueToColor(1.0 - t)).rgb();
                    for(int x = 0; x < gradientSize.width(); x++) {
                        image.setPixel(x + borderWidth, y + borderWidth, color);
                    }
                }
            }
            else {
                for(int x = 0; x < gradientSize.width(); x++) {
                    FloatType t = (FloatType)x / (FloatType)std::max(1, gradientSize.width() - 1);
                    unsigned int color = QColor(mapping.gradient()->valueToColor(t)).rgb();
                    for(int y = 0; y < gradientSize.height(); y++) {
                        image.setPixel(x + borderWidth, y + borderWidth, color);
                    }
                }
            }
            offset = QPointF(-borderWidth, -borderWidth);
        });

    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    std::unique_ptr<ImagePrimitive> imagePrimitive = std::make_unique<ImagePrimitive>();
    imagePrimitive->setRectWindow(QRect(alignedPos, image.size()));
    imagePrimitive->setImage(image);

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive->windowRect()};
    boundingBox |= colorBarImageRect;

    QByteArray format = valueFormatString().toUtf8();
    if(format.contains("%s"))
        format.clear();

    _autoLabel1Text = std::isfinite(mapping.maxValue()) ? QString::asprintf(format.constData(), mapping.maxValue()) : QStringLiteral("###");
    _autoLabel2Text = std::isfinite(mapping.minValue()) ? QString::asprintf(format.constData(), mapping.minValue()) : QStringLiteral("###");

    // Notify the UI that the automatic label texts were recalculated during rendering.
    notifyDependents(ColorLegendOverlay::AutoLabelsUpdated);

    QString titleLabel = title().isEmpty() ? _autoTitleText : title();
    QString topLabel = label1().isEmpty() ? _autoLabel1Text : label1();
    QString bottomLabel = label2().isEmpty() ? _autoLabel2Text : label2();

    // Determine effective font size.
    const qreal fontSize{legendSize * std::max(FloatType(0), this->fontSize())};
    const qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

    // Prepare limit labels.
    std::unique_ptr<TextPrimitive> label1Primitive = std::make_unique<TextPrimitive>();
    std::unique_ptr<TextPrimitive> label2Primitive = std::make_unique<TextPrimitive>();

    // Font size is always in logical units.
    FloatType labelFontSize{fontSize * relLabelFontSize() / devicePixelRatio};
    // Qt font size is always in logical units.
    QFont labelFont = this->font();
    labelFont.setPointSizeF(labelFontSize);
    label1Primitive->setFont(labelFont);
    label2Primitive->setFont(labelFont);

    int topFlags = 0;
    int bottomFlags = 0;
    QPointF topPos;
    QPointF bottomPos;

    if(orientation() == Qt::Horizontal) {
        bottomFlags = Qt::AlignRight | Qt::AlignVCenter;
        topFlags = Qt::AlignLeft | Qt::AlignVCenter;
        bottomPos = QPointF(colorBarImageRect.left() - textMargin, colorBarImageRect.top() + 0.5 * colorBarImageRect.height());
        topPos = QPointF(colorBarImageRect.right() + textMargin, colorBarImageRect.top() + 0.5 * colorBarImageRect.height());
    }
    else {  // Vertical
            // If ticks are drawn, the labels are top/bottom labels are drawn further out to align with the tick labels
        FloatType tickSpacing{static_cast<int>(ticksEnabled()) * outerTickHeight * colorBarImageRect.width()};
        if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
            bottomFlags = Qt::AlignLeft | Qt::AlignVCenter;
            topFlags = Qt::AlignLeft | Qt::AlignVCenter;
            bottomPos = QPointF(colorBarImageRect.right() + textMargin + tickSpacing, colorBarImageRect.bottom());
            topPos = QPointF(colorBarImageRect.right() + textMargin + tickSpacing, colorBarImageRect.top());
        }
        else if(alignment() & Qt::AlignRight) {
            bottomFlags = Qt::AlignRight | Qt::AlignVCenter;
            topFlags = Qt::AlignRight | Qt::AlignVCenter;
            bottomPos = QPointF(colorBarImageRect.left() - textMargin - tickSpacing, colorBarImageRect.bottom());
            topPos = QPointF(colorBarImageRect.left() - textMargin - tickSpacing, colorBarImageRect.top());
        }
    }

    label1Primitive->setText(topLabel);
    label1Primitive->setAlignment(topFlags);
    label1Primitive->setPositionWindow(topPos);
    label1Primitive->setColor(textColor());
    label1Primitive->setTextFormat(Qt::AutoText);
    if(outlineEnabled())
        label1Primitive->setOutlineColor(outlineColor());
    QRectF topLabelBoundingBox = label1Primitive->computeBounds(devicePixelRatio);
    boundingBox |= topLabelBoundingBox;

    label2Primitive->setText(bottomLabel);
    label2Primitive->setAlignment(bottomFlags);
    label2Primitive->setPositionWindow(bottomPos);
    label2Primitive->setColor(textColor());
    label2Primitive->setTextFormat(Qt::AutoText);
    if(outlineEnabled())
        label2Primitive->setOutlineColor(outlineColor());
    boundingBox |= label2Primitive->computeBounds(devicePixelRatio);

    // Place the title label at the correct location based on color bar direction and position.
    int titleFlags = Qt::AlignBottom;
    QPointF titlePos;
    if(orientation() == Qt::Horizontal) {
        titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
        titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
        titlePos.ry() = colorBarImageRect.top() - 0.5 * textMargin;
    }
    else { // bar orientation == Qt::Vertical
        if(!titleRotationEnabled()) { // title orientation == Qt::Horizontal
            titlePos.ry() = colorBarImageRect.top() - 0.5 * (textMargin + topLabelBoundingBox.height());
            if(alignment() & Qt::AlignLeft) {
                titleFlags = Qt::AlignLeft | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left();
            }
            else if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignRight | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.right();
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            }
        }
        else {
            titlePos.ry() = colorBarImageRect.top() + 0.5 * colorBarImageRect.height();
            if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignHCenter | Qt::AlignTop;
                titlePos.rx() = colorBarImageRect.right() + textMargin;
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() - textMargin;
            }
        }
    }

    // Prepare title label.
    std::unique_ptr<TextPrimitive> titlePrimitive = std::make_unique<TextPrimitive>();
    QFont titleFont = this->font();
    titleFont.setPointSizeF(fontSize / devicePixelRatio); // Qt font size is always in logical units.
    titlePrimitive->setFont(titleFont);
    titlePrimitive->setText(titleLabel);
    titlePrimitive->setColor(textColor());
    if(outlineEnabled())
        titlePrimitive->setOutlineColor(outlineColor());
    titlePrimitive->setAlignment(titleFlags);
    titlePrimitive->setPositionWindow(titlePos);
    titlePrimitive->setTextFormat(Qt::AutoText);
    if(titleRotationEnabled() && orientation() == Qt::Vertical)
        titlePrimitive->setRotation(qDegreesToRadians(270));
    boundingBox |= titlePrimitive->computeBounds(devicePixelRatio);

    std::vector<Box2> tickRects;
    std::vector<std::unique_ptr<TextPrimitive>> tickLabels;

    if(ticksEnabled() && std::isfinite(mapping.minValue()) && std::isfinite(mapping.maxValue())) {
        // The font metric needs to be calculated without device pixel ratio scaling of the font.
        // A devicePixelRatio of 3 leads to an intermediate 3x larger colorbarLength during supersampling.
        // However, the font metrics and labels need to be measured based on the original size of 1x to give
        // the correct label size after downsampling the image back to 1x.
        labelFont.setPointSizeF(labelFontSize * devicePixelRatio);
        const QFontMetricsF fontMetrics{labelFont};
        const FloatType colorbarLength = (orientation() == Qt::Horizontal) ? colorBarImageRect.width() : colorBarImageRect.height();

        // Look up tick configuration in the cache
        const auto& [tickStart, tickStep] = frameGraph.visCache().lookup<std::tuple<FloatType, FloatType>>(
            RendererResourceKey<struct TickSpacingCache, QByteArray, FloatType, FloatType, FloatType, FloatType, FloatType, int>{
                format, labelFontSize, mapping.maxValue(), mapping.minValue(), tickSpacing(), colorbarLength, orientation()},
            [&](FloatType& tickStart, FloatType& tickStep) {
                // Calculate new tick configuration if it not found in the cache
                // tickSpacing() == 0 activates the automatic calculation
                // tickSpacing() != 0 uses the user defined settings
                if(tickSpacing() == 0) {
                    const auto [start, step]{
                        getAutomaticTickPositions(mapping.minValue(), mapping.maxValue(), colorbarLength, fontMetrics, format)};
                    tickStart = start;
                    tickStep = step;
                }
                else {
                    tickStart = getUserDefinedTickPositions(mapping.minValue(), mapping.maxValue(), tickSpacing());
                    tickStep = tickSpacing();
                }
            });

        int numTicks = get_number_of_ticks(mapping.minValue(), mapping.maxValue(), tickStep);
        // Check against the hard coded limit for the number of ticks. Prevents crash in the case of too many ticks
        {
            constexpr int maxTicks = 100;
            if(numTicks > maxTicks) {
                // Set warning status to be displayed in the GUI.
                setStatus(PipelineStatus(PipelineStatus::Warning, tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                                                     "ticks may be generated. Please increase the tick spacing.")
                                                                      .arg(numTicks)
                                                                      .arg(maxTicks)));

                // Escalate to an error state if in scripting mode.
                if(!this_task::isInteractive())
                    throw Exception(tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                       "ticks may be generated. Please increase the tick spacing.")
                                        .arg(numTicks)
                                        .arg(maxTicks));
                numTicks = 0;
            }
        }

        // Prepare tick marks and labels.
        TextPrimitive labelPrimitive;
        labelPrimitive.setColor(textColor());
        labelPrimitive.setTextFormat(Qt::AutoText);
        labelPrimitive.setFont(label1Primitive->font());
        if(outlineEnabled())
            labelPrimitive.setOutlineColor(outlineColor());
        if(orientation() == Qt::Horizontal) {
            // label
            labelPrimitive.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
            Point2 label_pos;
            label_pos.y() = colorBarImageRect.bottom() + outerTickHeight * colorBarImageRect.height() + fontMetrics.ascent() / 2;
            // ticks
            Point2 tickMin;
            Point2 tickMax;
            tickMin.y() = colorBarImageRect.top() + (1 - innerTickHeight) * colorBarImageRect.height();
            tickMax.y() = colorBarImageRect.top() + (1 + outerTickHeight) * colorBarImageRect.height() + borderWidth;
            boundingBox |= QRectF(QPointF(colorBarImageRect.left(), tickMin.y()), QPointF(colorBarImageRect.right(), tickMax.y()));

            // If the first tick is in the position of the minValue or maxValue it will be hidden.
            // Therefore we need to increase the num_ticks by 1 to get all required ticks drawn correctly.
            numTicks += ((tickStart == mapping.minValue()) || (tickStart == mapping.maxValue()));
            for(int i{0}; i <= numTicks; i++) {
                FloatType tickValue = tickStart + i * tickStep;
                // Fix tick values close to 0 being formatted as 5.5e-17 instead of 0 with the
                // default format specifier "%g".
                tickValue = (std::abs(tickValue) < 1e-12) ? 0.0 : tickValue;

                const FloatType tickPosition = (tickValue - mapping.minValue()) / (mapping.maxValue() - mapping.minValue());
                // omit labels to outside the range or too close to the color bar limit
                if((tickPosition <= 0) || (tickPosition >= 1)) {
                    continue;
                }
                // Label
                labelPrimitive.setText(QString::asprintf(format.constData(), tickValue));
                label_pos.x() = colorBarImageRect.left() + colorBarImageRect.width() * tickPosition;
                labelPrimitive.setPositionWindow(label_pos);
                boundingBox |= labelPrimitive.computeBounds(devicePixelRatio);
                tickLabels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

                // Tick.
                tickMin.x() = colorBarImageRect.left() + tickPosition * colorBarImageRect.width() - (FloatType)tickWidth / 2.0;
                tickMax.x() = colorBarImageRect.left() + tickPosition * colorBarImageRect.width() + (FloatType)tickWidth / 2.0;
                tickRects.emplace_back(tickMin, tickMax);
            }
        }
        else { // orientation() == Qt::Vertical
            // labels
            Point2 labelPos;

            // ticks
            Point2 tickMin;
            Point2 tickMax;
            if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
                // labels
                labelPrimitive.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                labelPos.x() = colorBarImageRect.right() + textMargin + outerTickHeight * colorBarImageRect.width();

                // ticks
                tickMin.x() = colorBarImageRect.left() + (1 - innerTickHeight) * colorBarImageRect.width();
                tickMax.x() = colorBarImageRect.left() + (1 + outerTickHeight) * colorBarImageRect.width() + borderWidth;
            }
            else {
                // labels
                labelPrimitive.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                labelPos.x() = colorBarImageRect.left() - textMargin - outerTickHeight * colorBarImageRect.width();

                // ticks
                tickMin.x() = colorBarImageRect.right() - (1 + outerTickHeight) * colorBarImageRect.width();
                tickMax.x() = colorBarImageRect.right() - (1 - innerTickHeight) * colorBarImageRect.width();
            }
            for(int i{0}; i < numTicks; i++) {
                FloatType tickValue = tickStart + i * tickStep;
                // Fix tick values close to 0 being formatted as 5.5e-17 instead of 0 with the
                // default format specifier "%g".
                tickValue = (std::abs(tickValue) < 1e-12) ? 0.0 : tickValue;

                FloatType tickPosition = (tickValue - mapping.minValue()) / (mapping.maxValue() - mapping.minValue());
                // omit labels to outside the range or too close to the color bar limit
                if((tickPosition <= minTickDistanceFromEdge) || (tickPosition >= (1 - minTickDistanceFromEdge))) {
                    continue;
                }
                // labels
                labelPrimitive.setText(QString::asprintf(format.constData(), tickValue));
                labelPos.y() = colorBarImageRect.bottom() - colorBarImageRect.height() * tickPosition;

                // Hide the first and last tick mark and label if they overlap with the limit labels
                if(((i == 0) || (i == (numTicks - 1))) &&
                   ((labelPos.y() > (colorBarImageRect.bottom() - tickOverlapFactor * fontMetrics.height())) ||
                    (labelPos.y() < (colorBarImageRect.top() + tickOverlapFactor * fontMetrics.height()))))
                    continue;
                labelPrimitive.setPositionWindow(labelPos);
                boundingBox |= labelPrimitive.computeBounds(devicePixelRatio);
                tickLabels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

                // Tick
                tickMin.y() = colorBarImageRect.bottom() - tickPosition * colorBarImageRect.height() - (FloatType)tickWidth / 2.0;
                tickMax.y() = colorBarImageRect.bottom() - tickPosition * colorBarImageRect.height() + (FloatType)tickWidth / 2.0;
                tickRects.emplace_back(tickMin, tickMax);
                boundingBox |= QRectF(QPointF(tickMin.x(), tickMin.y()), QPointF(tickMax.x(), tickMax.y()));
            }

            // Manually add the tick marks at the ends of the color bar for the limit labels
            tickRects.emplace_back(tickMin.x(), colorBarImageRect.bottom() - tickWidth, tickMax.x(), colorBarImageRect.bottom());
            tickRects.emplace_back(tickMin.x(), colorBarImageRect.top(), tickMax.x(), colorBarImageRect.top() + tickWidth);
            boundingBox |= QRectF(QPointF(tickMin.x(), colorBarImageRect.bottom()), QPointF(tickMax.x(), colorBarImageRect.top()));
        }
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image in the cache
        const QImage& backgroundImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()},
            [&](QImage& backgroundImage) {
                // Generate image if not found in the cache
                // 1 x 1 px texture of the right color which will be stretched to the desired rectangle dimensions.
                backgroundImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                backgroundImage.fill(static_cast<QColor>(backgroundColor()));
            });

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(backgroundImage, boundingBox.toAlignedRect()));
    }

    // Render color bar.
    commandGroup.addPrimitivePreprojected(std::move(imagePrimitive));

    // Render title and limit labels.
    commandGroup.addPrimitivePreprojected(std::move(titlePrimitive));
    commandGroup.addPrimitivePreprojected(std::move(label1Primitive));
    commandGroup.addPrimitivePreprojected(std::move(label2Primitive));

    // Render ticks.
    if(!tickRects.empty()) {

        // Look up tick image in the cache.
        const QImage& tickImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarTickImageCache, Color>{tickColor},
            [&](QImage& tickImage) {
                // Generate tick image primitive if not found in the cache.
                // 1x1 pixel texture of the right color which will be stretched to the desired tick dimensions
                tickImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                tickImage.fill(static_cast<QColor>(tickColor));
            });

        // Render the series of tick images.
        for(const auto& rect : tickRects) {
            commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(tickImage, rect));
        }
    }

    // Render tick labels.
    for(auto& labelPrimitive : tickLabels) {
        commandGroup.addPrimitivePreprojected(std::move(labelPrimitive));
    }
}

ColorLegendOverlay::DiscreteColorMapLabels ColorLegendOverlay::getDiscreteColorMapLabels(const Property* property)
{
    OVITO_ASSERT(property->isTypedProperty());
    DiscreteColorMapLabels labels;
    for(const ElementType* type : property->elementTypes()) {
        if(type && type->enabled()) {
            labels.emplace_back(type->numericId(), type->objectTitle(), type->color());
        }
    }
    std::ranges::sort(labels, [](const auto& lhs, const auto& rhs) { return std::get<int>(lhs) < std::get<int>(rhs); });
    return labels;
}

ColorLegendOverlay::DiscreteColorMapLabels ColorLegendOverlay::getDiscreteColorMapLabels(const ColorCodingGradient* gradient,
                                                                                         FloatType startValue, FloatType endValue,
                                                                                         int orientation) const
{
    const int numDiscreteColors = DiscreteColorMap::binCount(startValue, endValue);
    const int offset = (int)std::round(std::min(startValue, endValue));
    DiscreteColorMapLabels labels;
    labels.reserve(numDiscreteColors);

    // Format the numbers matching continuous color maps.
    QByteArray format = valueFormatString().toUtf8();
    if(format.contains("%s")) {
        format.clear();
    }

    const bool reverseMapping = startValue > endValue;
    const bool reverseLabelsOrder = (orientation == Qt::Vertical) ^ reverseMapping;

    for(int i = 0; i < numDiscreteColors; ++i) {
        FloatType t = DiscreteColorMap::mapValue(static_cast<FloatType>(i) / (numDiscreteColors - 1), numDiscreteColors);
        labels.emplace_back(i, QString::asprintf(format.constData(), FloatType(offset + i)),
                            gradient->valueToColor(reverseMapping ? (1.0 - t) : t));
    }

    if(reverseLabelsOrder) {
        std::ranges::reverse(labels);
    }

    return labels;
}

/******************************************************************************
* Draws the color legend for a typed property.
******************************************************************************/
void ColorLegendOverlay::drawDiscreteColorMap(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup,
                                              const QRectF& colorBarRect, FloatType legendSize,
                                              const DiscreteColorMapLabels& colorMapLabels)
{
    for(const auto& label : colorMapLabels) {
        qDebug() << std::get<int>(label) << std::get<QString>(label) << std::get<Color>(label);
    }

    const qreal devicePixelRatio = frameGraph.devicePixelRatio();

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    // Look up the image primitive for the color bar in the cache.
    const auto& [image, offset] = frameGraph.visCache().lookup<std::tuple<QImage, QPointF>>(
        RendererResourceKey<struct TypeColorsImageCache, DiscreteColorMapLabels, FloatType, int, bool, Color, QSizeF>{
            colorMapLabels,
            devicePixelRatio,
            orientation(),
            borderEnabled(),
            borderColor(),
            colorBarRect.size(),
        },
        [&](QImage& image, QPointF& offset) {
            // Render the color fields into an image texture.
            // Allocate the image buffer.
            QSize gradientSize = colorBarRect.size().toSize();
            int borderWidth = borderEnabled() ? (int)std::ceil(2.0 * devicePixelRatio) : 0;
            image = QImage(gradientSize.width() + 2*borderWidth, gradientSize.height() + 2*borderWidth, frameGraph.preferredImageFormat());
            if(borderEnabled())
                image.fill((QColor)borderColor());

            // Create the color gradient image.
            if(!colorMapLabels.empty()) {
                QPainter painter(&image);
                if(orientation() == Qt::Vertical) {
                    int effectiveSize = gradientSize.height() - borderWidth * (colorMapLabels.size() - 1);
                    for(size_t i = 0; i < colorMapLabels.size(); i++) {
                        QRect rect(borderWidth, borderWidth + (i * effectiveSize / colorMapLabels.size()) + i * borderWidth,
                                   gradientSize.width(), 0);
                        rect.setBottom(borderWidth + ((i + 1) * effectiveSize / colorMapLabels.size()) + i * borderWidth - 1);
                        painter.fillRect(rect, QColor(std::get<Color>(colorMapLabels[i])));
                    }
                }
                else {
                    int effectiveSize = gradientSize.width() - borderWidth * (colorMapLabels.size() - 1);
                    for(size_t i = 0; i < colorMapLabels.size(); i++) {
                        QRect rect(borderWidth + (i * effectiveSize / colorMapLabels.size()) + i * borderWidth, borderWidth, 0,
                                   gradientSize.height());
                        rect.setRight(borderWidth + ((i + 1) * effectiveSize / colorMapLabels.size()) + i * borderWidth - 1);
                        painter.fillRect(rect, QColor(std::get<Color>(colorMapLabels[i])));
                    }
                }
            }
            offset = QPointF(-borderWidth,-borderWidth);
        });

    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    std::unique_ptr<ImagePrimitive> imagePrimitive = std::make_unique<ImagePrimitive>();
    imagePrimitive->setRectWindow(QRect(alignedPos, image.size()));
    imagePrimitive->setImage(image);

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive->windowRect()};
    boundingBox |= colorBarImageRect;

    // Count the number of element types that are enabled.
    int numTypes = colorMapLabels.size();

    const qreal fontSize = legendSize * std::max(FloatType(0), this->fontSize());
    const qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

    // Move the text path to the correct location based on color bar direction and position.
    int titleFlags = 0;
    QPointF titlePos;
    if(orientation() == Qt::Horizontal) {
        if((alignment() & Qt::AlignTop) || (alignment() & Qt::AlignVCenter)) {
            titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
            titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            titlePos.ry() = colorBarImageRect.top() - 0.5 * textMargin;
        }
        else {
            titleFlags = Qt::AlignHCenter | Qt::AlignTop;
            titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            titlePos.ry() = colorBarImageRect.bottom() + 0.5 * textMargin;
        }
    }
    else { // bar orientation == Qt::Vertical
        if(!titleRotationEnabled()) { // title orientation == Qt::Horizontal
            titlePos.ry() = colorBarImageRect.top() - textMargin;
            if(alignment() & Qt::AlignLeft) {
                titleFlags = Qt::AlignLeft | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left();
            }
            else if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignRight | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.right();
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
            }
        }
        else {
            titlePos.ry() = colorBarImageRect.top() + 0.5 * colorBarImageRect.height();
            if(alignment() & Qt::AlignRight) {
                titleFlags = Qt::AlignHCenter | Qt::AlignTop;
                titlePos.rx() = colorBarImageRect.right() + textMargin;
            }
            else {
                titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
                titlePos.rx() = colorBarImageRect.left() - textMargin;
            }
        }
    }

    // Prepare title label.
    std::unique_ptr<TextPrimitive> titlePrimitive = std::make_unique<TextPrimitive>();
    QFont titleFont = this->font();
    titleFont.setPointSizeF(fontSize / devicePixelRatio); // Qt font size is always in logical units.
    titlePrimitive->setFont(titleFont);
    titlePrimitive->setText(title().isEmpty() ? _autoTitleText : title());
    titlePrimitive->setColor(textColor());
    if(outlineEnabled())
        titlePrimitive->setOutlineColor(outlineColor());
    titlePrimitive->setAlignment(titleFlags);
    titlePrimitive->setPositionWindow(titlePos);
    titlePrimitive->setTextFormat(Qt::AutoText);
    if(titleRotationEnabled() && orientation() == Qt::Vertical)
        titlePrimitive->setRotation(qDegreesToRadians(270));
    boundingBox |= titlePrimitive->computeBounds(devicePixelRatio);

    // Prepare type name labels.
    if(numTypes == 0)
        numTypes = 1; // Avoid division by 0 below.

    // Layout of the type labels.
    int labelFlags = 0;
    QPointF labelPos;
    if(orientation() == Qt::Vertical) {
        if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
            labelFlags |= Qt::AlignLeft | Qt::AlignVCenter;
            labelPos.setX(colorBarRect.right() + textMargin);
        }
        else {
            labelFlags |= Qt::AlignRight | Qt::AlignVCenter;
            labelPos.setX(colorBarRect.left() - textMargin);
        }
        labelPos.setY(colorBarRect.top() + 0.5 * colorBarRect.height() / numTypes);
    }
    else {
        if((alignment() & Qt::AlignTop) || (alignment() & Qt::AlignVCenter)) {
            labelFlags |= Qt::AlignHCenter | Qt::AlignTop;
            labelPos.setY(colorBarRect.bottom() + 0.5 * textMargin);
        }
        else {
            labelFlags |= Qt::AlignHCenter | Qt::AlignBottom;
            labelPos.setY(colorBarRect.top() - textMargin);
        }
        labelPos.setX(colorBarRect.left() + 0.5 * colorBarRect.width() / numTypes);
    }

    FloatType labelFontSize{fontSize * relLabelFontSize() / devicePixelRatio};
    TextPrimitive labelPrimitive;
    QFont labelFont = this->font();
    labelFont.setPointSizeF(labelFontSize);
    labelPrimitive.setFont(labelFont);
    labelPrimitive.setColor(textColor());
    if(outlineEnabled())
        labelPrimitive.setOutlineColor(outlineColor());
    labelPrimitive.setAlignment(labelFlags);
    labelPrimitive.setTextFormat(Qt::AutoText);

    std::vector<std::unique_ptr<TextPrimitive>> labels;
    for(const auto& colorLabel : colorMapLabels) {
        labelPrimitive.setText(std::get<QString>(colorLabel));
        labelPrimitive.setPositionWindow(labelPos);
        boundingBox |= labelPrimitive.computeBounds(devicePixelRatio);
        labels.push_back(std::make_unique<TextPrimitive>(labelPrimitive));

        if(orientation() == Qt::Vertical)
            labelPos.ry() += colorBarRect.height() / numTypes;
        else
            labelPos.rx() += colorBarRect.width() / numTypes;
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image in the cache
        const QImage& backgroundImage = frameGraph.visCache().lookup<QImage>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()},
            [&](QImage& backgroundImage) {
                // Generate image if not found in the cache
                // 1x1 pixel texture of the right color which will be stretched to the desired rectangle dimensions.
                backgroundImage = QImage{QSize(1, 1), frameGraph.preferredImageFormat()};
                backgroundImage.fill(static_cast<QColor>(backgroundColor()));
            });

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(backgroundImage, boundingBox.toAlignedRect()));
    }

    // Render title.
    commandGroup.addPrimitivePreprojected(std::move(titlePrimitive));

    // Render color bar.
    commandGroup.addPrimitivePreprojected(std::move(imagePrimitive));

    // Render type labels.
    for(auto& labelPrimitive : labels) {
        commandGroup.addPrimitivePreprojected(std::move(labelPrimitive));
    }

    // Notify the UI that the automatic label texts were recalculated during rendering.
    notifyDependents(ColorLegendOverlay::AutoLabelsUpdated);
}

/******************************************************************************
* This method is called once for this object after they have been completely loaded from a stream.
******************************************************************************/
void ColorLegendOverlay::loadFromStreamComplete(ObjectLoadStream& stream)
{
    ViewportOverlay::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.10.6:
    if(!pipeline() && stream.datasetToBePopulated()) {
        // Automatically choose a scene pipeline for this overlay.
        if(Viewport* vp = stream.datasetToBePopulated()->viewportConfig()->activeViewport()) {
            if(Scene* scene = vp->scene()) {
                Pipeline* selectedPipeline = nullptr;
                scene->visitPipelines([&](SceneNode* sceneNode) {
                    Pipeline* pipeline = sceneNode->pipeline();
                    if(selectedPipeline == nullptr)
                        selectedPipeline = pipeline;
                    if(modifier()) {
                        ModificationNode* modNode = nullptr;
                        PipelineNode* node = pipeline->head();
                        for(;;) {
                            if((modNode = dynamic_object_cast<ModificationNode>(node))) {
                                if(modNode->modifier() == modifier()) {
                                    selectedPipeline = pipeline;
                                    return false;
                                }
                                node = modNode->input();
                            }
                            else break;
                        }
                    }
                    else if(sourceProperty()) {
                        const PipelineFlowState& state = pipeline->getCachedPipelineOutput(scene->animationSettings()->currentTime());
                        if(state.getLeafObject(sourceProperty())) {
                            selectedPipeline = pipeline;
                            return false;
                        }
                    }
                    return true;
                });
                setPipeline(selectedPipeline);
            }
        }
    }
}

}   // End of namespace
