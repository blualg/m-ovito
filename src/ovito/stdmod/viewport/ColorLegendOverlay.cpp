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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "ColorLegendOverlay.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ColorLegendOverlay);
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
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, fontSize, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, relLabelFontSize, PercentParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, tickSpacing, FloatParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
ColorLegendOverlay::ColorLegendOverlay(ObjectInitializationFlags flags)
    : ViewportOverlay(flags),
      _alignment(Qt::AlignHCenter | Qt::AlignBottom),
      _orientation(Qt::Horizontal),
      _legendSize(0.3),
      _offsetX(0),
      _offsetY(0),
      _fontSize(0.1),
      _relLabelFontSize(0.6),
      _valueFormatString("%g"),
      _aspectRatio(8.0),
      _textColor(0, 0, 0),
      _outlineColor(1, 1, 1),
      _outlineEnabled(false),
      _borderEnabled(false),
      _borderColor(0, 0, 0),
      _ticksEnabled(false),
      _tickSpacing(0),
      _titleRotationEnabled(false),
      _backgroundEnabled(false),
      _backgroundColor(1,1,1)
{
}

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void ColorLegendOverlay::initializeOverlay(Viewport* viewport)
{
    if(ExecutionContext::isInteractive()) {

        // Find a ColorCodingModifier in the scene that we can connect to.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
                PipelineObject* obj = pipeline->dataProvider();
                while(obj) {
                    if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj)) {
                        if(ColorCodingModifier* mod = dynamic_object_cast<ColorCodingModifier>(modApp->modifier())) {
                            setModifier(mod);
                            if(mod->isEnabled())
                                return false; // Stop search.
                        }
                        obj = modApp->input();
                    }
                    else break;
                }
                return true;
            });
        }

        // If there is no ColorCodingModifier in the scene, initialize the overlay to use
        // the first available typed property as color source.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
                const PipelineFlowState& state = pipeline->evaluatePipelineSynchronous(viewport->scene()->animationSettings()->currentTime(), false);
                for(const ConstDataObjectPath& dataPath : state.getObjectsRecursive(PropertyObject::OOClass())) {
                    const PropertyObject* property = static_object_cast<PropertyObject>(dataPath.back());
                    // Check if the property is a typed property, i.e. it has one or more ElementType objects attached to it.
                    if(property->isTypedProperty() && dataPath.size() >= 2) {
                        setSourceProperty(dataPath);
                        return false; // Stop search.
                    }
                }
                return true;
            });
        }

        // If we still don't have a valid source, look for a visual element in the scene which uses pseudo-color mapping.
        if(!modifier() && !sourceProperty() && !colorMapping() && viewport->scene()) {
            viewport->scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
                for(DataVis* vis : pipeline->visElements()) {
                    if(vis->isEnabled()) {
                        for(const PropertyFieldDescriptor* field : vis->getOOMetaClass().propertyFields()) {
                            if(field->isReferenceField() && !field->isWeakReference() && field->targetClass()->isDerivedFrom(PropertyColorMapping::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM) && !field->isVector()) {
                                if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(vis->getReferenceFieldTarget(field))) {
                                    if(mapping->sourceProperty()) {
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
    if(field == PROPERTY_FIELD(alignment) && !isBeingLoaded() && !isAboutToBeDeleted() && !isUndoingOrRedoing() && ExecutionContext::isInteractive()) {
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
* Is called when a RefTarget referenced by this object has generated an event.
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
* Returns a short piece information (typically a string or color) to be
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
        return sourceProperty().dataTitleOrString();
    }
    return {};
}

/******************************************************************************
* Lets the overlay paint its contents into the framebuffer.
******************************************************************************/
void ColorLegendOverlay::render(SceneRenderer* renderer, const QRect& logicalViewportRect, const QRect& physicalViewportRect)
{
    DataOORef<const PropertyObject> typedProperty;

    // Reset auto-generated label texts. Will be newly set by rendering code.
    _autoTitleText.clear();
    _autoLabel1Text.clear();
    _autoLabel2Text.clear();

    // Check alignment parameter.
    if(!renderer->isInteractive())
        checkAlignmentParameterValue(alignment());

    // Check whether a source has been set for this color legend:
    if(modifier() || colorMapping()) {
        // Reset status of overlay.
        setStatus(PipelineStatus::Success);
    }
    else if(sourceProperty()) {
        // Look up the typed property in one of the scene's pipeline outputs.
        renderer->scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {

            // Evaluate pipeline and obtain output data collection.
            if(!renderer->isInteractive()) {
                PipelineEvaluationFuture pipelineEvaluation = pipeline->evaluatePipeline(PipelineEvaluationRequest(renderer->time()));
                if(!pipelineEvaluation.waitForFinished())
                    return false;
                // Look up the typed property.
                typedProperty = pipelineEvaluation.result().getLeafObject(sourceProperty());
            }
            else {
                const PipelineFlowState& state = pipeline->evaluatePipelineSynchronous(renderer->time(), false);
                // Look up the typed property.
                typedProperty = state.getLeafObject(sourceProperty());
            }
            if(typedProperty)
                return false;

            return true;
        });
        if(Task::current()->isCanceled())
            return;

        // Verify that the typed property, which has been selected as the source of the color legend, is available.
        if(!typedProperty) {
            // Set warning status to be displayed in the GUI.
            setStatus(PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not available in the pipeline output.").arg(sourceProperty().dataTitleOrString())));

            // Escalate to an error state if in terminal mode.
            if(Application::instance()->consoleMode())
                throw Exception(tr("The property '%1' set as source of the color legend is not present in the data pipeline output.").arg(sourceProperty().dataTitleOrString()));
            else
                return;
        }
        else if(!typedProperty->isTypedProperty()) {
            // Set warning status to be displayed in the GUI.
            setStatus(PipelineStatus(PipelineStatus::Warning, tr("The property '%1' is not a typed property.").arg(sourceProperty().dataTitleOrString())));

            // Escalate to an error state if in terminal mode.
            if(Application::instance()->consoleMode())
                throw Exception(tr("The property '%1' set as source of the color legend is not a typed property, i.e., it has no ElementType(s) attached.").arg(sourceProperty().dataTitleOrString()));
            else
                return;
        }

        // Reset status of overlay.
        setStatus(PipelineStatus::Success);
    }
    else {
        // Set warning status to be displayed in the GUI.
        setStatus(PipelineStatus(PipelineStatus::Warning, tr("No data source has been specified for the color legend.")));

        // Escalate to an error state if in terminal mode.
        if(Application::instance()->consoleMode()) {
            throw Exception(tr("You are rendering a Viewport with a ColorLegendOverlay that is not associated with any "
                              "data source. Did you forget to specify a data source for the color legend?"));
        }
        else {
            // Ignore invalid configuration in GUI mode by not rendering the legend.
            return;
        }
    }

    // Calculate position and size of color legend rectangle.
    FloatType legendSize = this->legendSize() * physicalViewportRect.height();
    if(legendSize <= 0) return;

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

    if(modifier()) {

        // Get modifier's parameters.
        FloatType startValue = modifier()->startValue();
        FloatType endValue = modifier()->endValue();
        if(modifier()->autoAdjustRange() && (label1().isEmpty() || label2().isEmpty())) {
            // Get the automatically adjusted range of the color coding modifier.
            // This requires a partial pipeline evaluation up to the color coding modifier.
            startValue = std::numeric_limits<FloatType>::quiet_NaN();
            endValue = std::numeric_limits<FloatType>::quiet_NaN();
            if(ModifierApplication* modApp = modifier()->someModifierApplication()) {
                QVariant minValue, maxValue;
                PipelineEvaluationRequest request(renderer->time());
                if(!renderer->isInteractive()) {
                    SharedFuture<PipelineFlowState> stateFuture = modApp->evaluate(request);
                    if(!stateFuture.waitForFinished())
                        return;
                    const PipelineFlowState& state = stateFuture.result();
                    minValue = state.getAttributeValue(modApp, QStringLiteral("ColorCoding.RangeMin"));
                    maxValue = state.getAttributeValue(modApp, QStringLiteral("ColorCoding.RangeMax"));
                }
                else {
                    const PipelineFlowState& state = modApp->evaluateSynchronous(request);
                    minValue = state.getAttributeValue(modApp, QStringLiteral("ColorCoding.RangeMin"));
                    maxValue = state.getAttributeValue(modApp, QStringLiteral("ColorCoding.RangeMax"));
                }
                if(minValue.isValid() && maxValue.isValid()) {
                    startValue = minValue.value<FloatType>();
                    endValue = maxValue.value<FloatType>();
                }
            }
        }

        _autoTitleText = modifier()->sourceProperty().nameWithComponent();
        drawContinuousColorMap(renderer, colorBarRect, legendSize, PseudoColorMapping(startValue, endValue, modifier()->colorGradient()));
    }
    else if(colorMapping()) {
        _autoTitleText = colorMapping()->sourceProperty().nameWithComponent();
        drawContinuousColorMap(renderer, colorBarRect, legendSize, colorMapping()->pseudoColorMapping());
    }
    else if(typedProperty) {
        _autoTitleText = typedProperty->objectTitle();
        drawDiscreteColorMap(renderer, colorBarRect, legendSize, typedProperty);
    }

    // Notify the UI panel that the automatic label texts were recalculated during rendering.
    Q_EMIT autoLabelsUpdated();
}

/******************************************************************************
 * Estimates the order of magnitude of a given value
 * estimate since this approach has no mathematical proof
 * might not behave well for all edge cases
 ******************************************************************************/
[[nodiscard]] static int estimateOrderOfMagnitude(const FloatType value)
{
    FloatType result{std::abs(value)};
    const FloatType eps{1e-18};
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
[[nodiscard]] static FloatType getFirstTickValidValue(const FloatType start, const FloatType tickSpacing)
{
    return tickSpacing * std::ceil(start / tickSpacing);
}

/******************************************************************************
 * Returns the starting value and the tick spacing as function of a control parameter N. Increment (decrement) N to increase
 * (decrease) the tickspacing. Ideally N should start at 0.
 ******************************************************************************/
[[nodiscard]] static std::tuple<FloatType, FloatType> getTickPositionsFromN(FloatType lowerLimit, FloatType upperLimit,
                                                                            const int N)
{
    constexpr FloatType steps[]{2, 4, 5, 10};
    constexpr int num_steps{std::size(steps)};

    // a % b = (b + (a % b)) % b <- correct for negative values of a
    // Selects a valid multiple (step width) from the steps array (based on N)
    const int index{(N % num_steps + num_steps) % num_steps};
    // guarantees flooring division (even for negative numbers)
    // first scaling factor for step width
    // N==-5 gives index==1 and pow==-2 (if steps[][2,5,10}])
    const int pow{static_cast<int>(std::floor(N / static_cast<FloatType>(num_steps)))};

    const int oom{estimateOrderOfMagnitude(upperLimit - lowerLimit)};
    // inter * 10^pow * 10^(oom-1) = inter * 10^(pow+oom-1)
    // const FloatType inter{steps[index] * std::pow(10, pow) * std::pow(10, oom - 1)};
    const FloatType inter{steps[index] * std::pow(10, pow + oom - 1)};
    return {getFirstTickValidValue(lowerLimit, inter), inter};
}

[[nodiscard]] static int get_number_of_ticks(FloatType lowerLimit, FloatType upperLimit, FloatType inter)
{
    return static_cast<int>(std::round(std::abs(upperLimit - lowerLimit) / inter));
}

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
            // all the way to the colorbar boundary
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
void ColorLegendOverlay::drawContinuousColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const PseudoColorMapping& mapping)
{
    const qreal devicePixelRatio = renderer->devicePixelRatio();

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

    // Allows the second to last and last tick of a vertical colorbar to overlapp slightly to get a more
    // pleasant look.
    constexpr FloatType tickOverlappFactor{0.8};

    if(!mapping.gradient())
        return;

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    // Look up the image primitive for the color bar in the cache.
    auto& [imagePrimitive, offset] = renderer->visCache().get<std::tuple<ImagePrimitive, QPointF>>(
        RendererResourceKey<struct ColorBarImageCache, OORef<ColorCodingGradient>, FloatType, int, bool, Color, QSizeF>{
            mapping.gradient(), devicePixelRatio, orientation(), borderEnabled(), borderColor(),
            colorBarRect.size()});

    // Render the color bar into an image texture.
    int borderWidth = borderEnabled() ? tickWidth : 0;
    if(imagePrimitive.image().isNull()) {
        // Allocate the image buffer.
        QSize gradientSize = colorBarRect.size().toSize();
        QImage textureImage(gradientSize.width() + 2 * borderWidth, gradientSize.height() + 2 * borderWidth,
                            renderer->preferredImageFormat());
        if(borderEnabled()) textureImage.fill((QColor)borderColor());

        // Create the color gradient image.
        if(orientation() == Qt::Vertical) {
            for(int y = 0; y < gradientSize.height(); y++) {
                FloatType t = (FloatType)y / (FloatType)std::max(1, gradientSize.height() - 1);
                unsigned int color = QColor(mapping.gradient()->valueToColor(1.0 - t)).rgb();
                for(int x = 0; x < gradientSize.width(); x++) {
                    textureImage.setPixel(x + borderWidth, y + borderWidth, color);
                }
            }
        }
        else {
            for(int x = 0; x < gradientSize.width(); x++) {
                FloatType t = (FloatType)x / (FloatType)std::max(1, gradientSize.width() - 1);
                unsigned int color = QColor(mapping.gradient()->valueToColor(t)).rgb();
                for(int y = 0; y < gradientSize.height(); y++) {
                    textureImage.setPixel(x + borderWidth, y + borderWidth, color);
                }
            }
        }
        imagePrimitive.setImage(std::move(textureImage));
        offset = QPointF(-borderWidth, -borderWidth);
    }
    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    imagePrimitive.setRectWindow(QRect(alignedPos, imagePrimitive.image().size()));

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive.windowRect()};
    boundingBox |= colorBarImageRect;

    QByteArray format = valueFormatString().toUtf8();
    if(format.contains("%s"))
        format.clear();

    _autoLabel1Text = std::isfinite(mapping.maxValue()) ? QString::asprintf(format.constData(), mapping.maxValue()) : QStringLiteral("###");
    _autoLabel2Text = std::isfinite(mapping.minValue()) ? QString::asprintf(format.constData(), mapping.minValue()) : QStringLiteral("###");

    QString titleLabel = title().isEmpty() ? _autoTitleText : title();
    QString topLabel = label1().isEmpty() ? _autoLabel1Text : label1();
    QString bottomLabel = label2().isEmpty() ? _autoLabel2Text : label2();

    // Determine effective font size.
    const qreal fontSize{legendSize * std::max(FloatType(0), this->fontSize())};
    const qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

    // Prepare limit labels.
    TextPrimitive label1Primitive, label2Primitive;

    // Font size is always in logical units.
    FloatType labelFontSize{fontSize * relLabelFontSize() / devicePixelRatio};
    // Qt font size is always in logical units.
    QFont labelFont = this->font();
    labelFont.setPointSizeF(labelFontSize);
    label1Primitive.setFont(labelFont);
    label2Primitive.setFont(labelFont);

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
            // If ticks are drawn, the labels are top/bottom lables are drawn further out to align with the tick labels
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

    label1Primitive.setText(topLabel);
    label1Primitive.setAlignment(topFlags);
    label1Primitive.setPositionWindow(topPos);
    label1Primitive.setColor(textColor());
    label1Primitive.setTextFormat(Qt::AutoText);
    if(outlineEnabled())
        label1Primitive.setOutlineColor(outlineColor());
    QRectF topLabelBoundingBox = label1Primitive.computeBoundingBox(devicePixelRatio);
    boundingBox |= topLabelBoundingBox;

    label2Primitive.setText(bottomLabel);
    label2Primitive.setAlignment(bottomFlags);
    label2Primitive.setPositionWindow(bottomPos);
    label2Primitive.setColor(textColor());
    label2Primitive.setTextFormat(Qt::AutoText);
    if(outlineEnabled())
        label2Primitive.setOutlineColor(outlineColor());
    boundingBox |= label2Primitive.computeBoundingBox(devicePixelRatio);

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
    TextPrimitive titlePrimitive;
    QFont titleFont = this->font();
    titleFont.setPointSizeF(fontSize / devicePixelRatio); // Qt font size is always in logical units.
    titlePrimitive.setFont(titleFont);
    titlePrimitive.setText(titleLabel);
    titlePrimitive.setColor(textColor());
    if(outlineEnabled())
        titlePrimitive.setOutlineColor(outlineColor());
    titlePrimitive.setAlignment(titleFlags);
    titlePrimitive.setPositionWindow(titlePos);
    titlePrimitive.setTextFormat(Qt::AutoText);
    if(titleRotationEnabled() && orientation() == Qt::Vertical)
        titlePrimitive.setRotation(qDegreesToRadians(270));
    boundingBox |= titlePrimitive.computeBoundingBox(devicePixelRatio);

    std::vector<Box2> tickRects;
    std::vector<TextPrimitive> tickLabels;

    if(ticksEnabled() && std::isfinite(mapping.minValue()) && std::isfinite(mapping.maxValue())) {
        // The font metric needs to be caculated without device pixel ratio scaling of the font.
        // A devicePixelRatio of 3 leads to an intermediate 3x larger colorbarLength during supersampling.
        // However, the font metrics and labels need to be measured based on the original size of 1x to give
        // the correct label size after downsampling the image back to 1x.
        labelFont.setPointSizeF(labelFontSize * devicePixelRatio);
        const QFontMetricsF fontMetrics{labelFont};
        const FloatType colorbarLength{(orientation() == Qt::Horizontal) ? colorBarImageRect.width()
                                                                         : colorBarImageRect.height()};

        // Look up tick configuration in the cache
        auto& [tickStart, tickStep, tickSpacingCacheSet] = renderer->visCache().get<std::tuple<FloatType, FloatType, bool>>(
            RendererResourceKey<struct TickSpacingCache, QByteArray, FloatType, FloatType, FloatType, FloatType, FloatType, int>{
                format, labelFontSize, mapping.maxValue(), mapping.minValue(), tickSpacing(), colorbarLength, orientation()});
        // Calculate new tick configuration if it not found in the cache
        // tickSpacing() == 0 activates the automatic calculation
        // tickSpacing() != 0 uses the user defined settings
        if(!tickSpacingCacheSet && tickSpacing() == 0) {
            const auto [start, step]{
                getAutomaticTickPositions(mapping.minValue(), mapping.maxValue(), colorbarLength, fontMetrics, format)};
            tickStart = start;
            tickStep = step;
            tickSpacingCacheSet = true;
        }
        else if(!tickSpacingCacheSet && tickSpacing() != 0) {
            tickStart = getUserDefinedTickPositions(mapping.minValue(), mapping.maxValue(), tickSpacing());
            tickStep = tickSpacing();
            tickSpacingCacheSet = true;
        }
        int num_ticks{get_number_of_ticks(mapping.minValue(), mapping.maxValue(), tickStep)};
        // Check against the hard coded limit for the number of ticks. Prevents crash in the case of too many ticks
        {
            const int max_ticks{100};
            if(num_ticks > max_ticks) {
                // Set warning status to be displayed in the GUI.
                setStatus(
                    PipelineStatus(PipelineStatus::Warning, tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                                               "ticks may be generated. Please increase the tick spacing.")
                                                                .arg(num_ticks)
                                                                .arg(max_ticks)));

                // Escalate to an error state if in terminal mode.
                if(Application::instance()->consoleMode())
                    throw Exception(tr("Tried to generate %1 tick marks. Currently, no more than %2 "
                                       "ticks may be generated. Please increase the tick spacing.")
                                        .arg(num_ticks)
                                        .arg(max_ticks));
                num_ticks = 0;
            }
        }

        // Prepare tick marks and labels.
        TextPrimitive labelPrimitive;
        labelPrimitive.setColor(textColor());
        labelPrimitive.setTextFormat(Qt::AutoText);
        labelPrimitive.setFont(label1Primitive.font());
        if(outlineEnabled())
            labelPrimitive.setOutlineColor(outlineColor());
        if(orientation() == Qt::Horizontal) {
            // label
            labelPrimitive.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
            Point2 label_pos;
            label_pos.y() = colorBarImageRect.bottom() + outerTickHeight * colorBarImageRect.height() + fontMetrics.ascent() / 2;
            // ticks
            Point2 tick_min;
            Point2 tick_max;
            tick_min.y() = colorBarImageRect.top() + (1 - innerTickHeight) * colorBarImageRect.height();
            tick_max.y() = colorBarImageRect.top() + (1 + outerTickHeight) * colorBarImageRect.height() + borderWidth;
            boundingBox |= QRectF(QPointF(colorBarImageRect.left(), tick_min.y()), QPointF(colorBarImageRect.right(), tick_max.y()));

            // If the first tick is in the position of the minValue or maxValue it will be hidden.
            // Therefore we need to increase the num_ticks by 1 to get all required ticks drawn correctly.
            num_ticks += ((tickStart == mapping.minValue()) || (tickStart == mapping.maxValue()));
            for(int i{0}; i < num_ticks; i++) {
                FloatType tick_value{tickStart + i * tickStep};
                FloatType tick_position{(tick_value - mapping.minValue()) / (mapping.maxValue() - mapping.minValue())};
                // omit labels to outside the range or too close to the color bar limit
                if((tick_position <= 0) || (tick_position >= 1)) {
                    continue;
                }
                // Label
                labelPrimitive.setText(QString::asprintf(format.constData(), tick_value));
                label_pos.x() = colorBarImageRect.left() + colorBarImageRect.width() * tick_position;
                labelPrimitive.setPositionWindow(label_pos);
                boundingBox |= labelPrimitive.computeBoundingBox(devicePixelRatio);
                tickLabels.push_back(labelPrimitive);

                // Tick.
                tick_min.x() = colorBarImageRect.left() + tick_position * colorBarImageRect.width() - tickWidth / 2;
                tick_max.x() = colorBarImageRect.left() + tick_position * colorBarImageRect.width() + tickWidth / 2;
                tickRects.emplace_back(tick_min, tick_max);
            }
        }
        else { // orientation() == Qt::Vertical
            // labels
            Point2 label_pos;

            // ticks
            Point2 tick_min;
            Point2 tick_max;
            if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
                // labels
                labelPrimitive.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                label_pos.x() = colorBarImageRect.right() + textMargin + outerTickHeight * colorBarImageRect.width();

                // ticks
                tick_min.x() = colorBarImageRect.left() + (1 - innerTickHeight) * colorBarImageRect.width();
                tick_max.x() = colorBarImageRect.left() + (1 + outerTickHeight) * colorBarImageRect.width() + borderWidth;
            }
            else {
                // labels
                labelPrimitive.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                label_pos.x() = colorBarImageRect.left() - textMargin - outerTickHeight * colorBarImageRect.width();

                // ticks
                tick_min.x() = colorBarImageRect.right() - (1 + outerTickHeight) * colorBarImageRect.width();
                tick_max.x() = colorBarImageRect.right() - (1 - innerTickHeight) * colorBarImageRect.width();
            }
            for(int i{0}; i < num_ticks; i++) {
                FloatType tick_value{tickStart + i * tickStep};
                FloatType tick_position{(tick_value - mapping.minValue()) / (mapping.maxValue() - mapping.minValue())};
                // omit labels to outside the range or too close to the color bar limit
                if((tick_position <= minTickDistanceFromEdge) || (tick_position >= (1 - minTickDistanceFromEdge))) {
                    continue;
                }
                // labels
                labelPrimitive.setText(QString::asprintf(format.constData(), tick_value));
                label_pos.y() = colorBarImageRect.bottom() - colorBarImageRect.height() * tick_position;

                // Hide the first and last tick mark and label if they overlap with the limit labels
                if(((i == 0) || (i == (num_ticks - 1))) &&
                   ((label_pos.y() > (colorBarImageRect.bottom() - tickOverlappFactor * fontMetrics.height())) ||
                    (label_pos.y() < (colorBarImageRect.top() + tickOverlappFactor * fontMetrics.height()))))
                    continue;
                labelPrimitive.setPositionWindow(label_pos);
                boundingBox |= labelPrimitive.computeBoundingBox(devicePixelRatio);
                tickLabels.push_back(labelPrimitive);

                // Tick
                tick_min.y() = colorBarImageRect.bottom() - tick_position * colorBarImageRect.height() - tickWidth / 2;
                tick_max.y() = colorBarImageRect.bottom() - tick_position * colorBarImageRect.height() + tickWidth / 2;
                tickRects.emplace_back(tick_min, tick_max);
                boundingBox |= QRectF(QPointF(tick_min.x(), tick_min.y()), QPointF(tick_max.x(), tick_max.y()));
            }

            // Manually add the tick marks at the ends of the color bar for the limit labels
            tickRects.emplace_back(tick_min.x(), colorBarImageRect.bottom() - tickWidth, tick_max.x(), colorBarImageRect.bottom());
            tickRects.emplace_back(tick_min.x(), colorBarImageRect.top(), tick_max.x(), colorBarImageRect.top() + tickWidth);
            boundingBox |= QRectF(QPointF(tick_min.x(), colorBarImageRect.bottom()), QPointF(tick_max.x(), colorBarImageRect.top()));
        }
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image primitve in the cache
        auto& backgroundImagePrimitive = renderer->visCache().get<ImagePrimitive>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()});

        // Generate image primitive if not found in the cache
        if(backgroundImagePrimitive.image().isNull()) {
            // 1 x 1 px texture of the right color which will be streched to the desired rectangle dimensions.
            QImage backgroundTextureImage{QSize(1, 1), renderer->preferredImageFormat()};
            backgroundTextureImage.fill(static_cast<QColor>(backgroundColor()));
            backgroundImagePrimitive.setImage(std::move(backgroundTextureImage));
        }

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        backgroundImagePrimitive.setRectWindow(boundingBox.toAlignedRect());
        renderer->renderImage(backgroundImagePrimitive);
    }

    // Render color bar.
    renderer->renderImage(imagePrimitive);

    // Render title and limit labels.
    renderer->renderText(titlePrimitive);
    renderer->renderText(label1Primitive);
    renderer->renderText(label2Primitive);

    // Render ticks.
    if(!tickRects.empty()) {

        // Look up tick image primitve in the cache.
        auto& tickImagePrimitive = renderer->visCache().get<ImagePrimitive>(
            RendererResourceKey<struct ColorBarTickImageCache, Color>{tickColor});

        // Generate tick image primitive if not found in the cache.
        if(tickImagePrimitive.image().isNull()) {
            // 1 x 1 px texture of the right color which will be streched to the desired tick dimensions
            QImage tickTextureImage{QSize(1, 1), QImage::Format_ARGB32_Premultiplied};
            tickTextureImage.fill(static_cast<QColor>(tickColor));
            tickImagePrimitive.setImage(std::move(tickTextureImage));
        }

        // Render the series of tick images.
        for(const auto& rect : tickRects) {
            tickImagePrimitive.setRectWindow(rect);
            renderer->renderImage(tickImagePrimitive);
        }
    }

    // Render tick labels.
    for(const auto& labelPrimitive : tickLabels) {
        renderer->renderText(labelPrimitive);
    }
}

/******************************************************************************
* Draws the color legend for a typed property.
******************************************************************************/
void ColorLegendOverlay::drawDiscreteColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const PropertyObject* property)
{
    qreal devicePixelRatio = renderer->devicePixelRatio();

    // Compute bounding box of the entire legend to draw the background rectangle.
    QRectF boundingBox;

    // Compile the list of type colors.
    std::vector<Color> typeColors;
    for(const ElementType* type : property->elementTypes()) {
        if(type && type->enabled())
            typeColors.push_back(type->color());
    }

    // Look up the image primitive for the color bar in the cache.
    auto& [imagePrimitive, offset] = renderer->visCache().get<std::tuple<ImagePrimitive, QPointF>>(
        RendererResourceKey<struct TypeColorsImageCache, std::vector<Color>, FloatType, int, bool, Color, QSizeF>{
            typeColors,
            devicePixelRatio,
            orientation(),
            borderEnabled(),
            borderColor(),
            colorBarRect.size()
        });

    // Render the color fields into an image texture.
    if(imagePrimitive.image().isNull()) {

        // Allocate the image buffer.
        QSize gradientSize = colorBarRect.size().toSize();
        int borderWidth = borderEnabled() ? (int)std::ceil(2.0 * devicePixelRatio) : 0;
        QImage textureImage(gradientSize.width() + 2*borderWidth, gradientSize.height() + 2*borderWidth, renderer->preferredImageFormat());
        if(borderEnabled())
            textureImage.fill((QColor)borderColor());

        // Create the color gradient image.
        if(!typeColors.empty()) {
            QPainter painter(&textureImage);
            if(orientation() == Qt::Vertical) {
                int effectiveSize = gradientSize.height() - borderWidth * (typeColors.size() - 1);
                for(size_t i = 0; i < typeColors.size(); i++) {
                    QRect rect(borderWidth, borderWidth + (i * effectiveSize / typeColors.size()) + i * borderWidth, gradientSize.width(), 0);
                    rect.setBottom(borderWidth + ((i+1) * effectiveSize / typeColors.size()) + i * borderWidth - 1);
                    painter.fillRect(rect, QColor(typeColors[i]));
                }
            }
            else {
                int effectiveSize = gradientSize.width() - borderWidth * (typeColors.size() - 1);
                for(size_t i = 0; i < typeColors.size(); i++) {
                    QRect rect(borderWidth + (i * effectiveSize / typeColors.size()) + i * borderWidth, borderWidth, 0, gradientSize.height());
                    rect.setRight(borderWidth + ((i+1) * effectiveSize / typeColors.size()) + i * borderWidth - 1);
                    painter.fillRect(rect, QColor(typeColors[i]));
                }
            }
        }
        imagePrimitive.setImage(std::move(textureImage));
        offset = QPointF(-borderWidth,-borderWidth);
    }
    QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
    imagePrimitive.setRectWindow(QRect(alignedPos, imagePrimitive.image().size()));

    // Actual bounding box of the rendered color bar including the border (if set).
    const QRectF colorBarImageRect{imagePrimitive.windowRect()};
    boundingBox |= colorBarImageRect;

    // Count the number of element types that are enabled.
    int numTypes = typeColors.size();

    const qreal fontSize = legendSize * std::max(FloatType(0), this->fontSize());
    const qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

    // Move the text path to the correct location based on color bar direction and position.
    int titleFlags = 0;
    QPointF titlePos;
    if(orientation() == Qt::Horizontal) {
        titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
        titlePos.rx() = colorBarImageRect.left() + 0.5 * colorBarImageRect.width();
        titlePos.ry() = colorBarImageRect.top() - 0.5 * textMargin;
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
    TextPrimitive titlePrimitive;
    QFont titleFont = this->font();
    titleFont.setPointSizeF(fontSize / devicePixelRatio); // Qt font size is always in logical units.
    titlePrimitive.setFont(titleFont);
    titlePrimitive.setText(title().isEmpty() ? _autoTitleText : title());
    titlePrimitive.setColor(textColor());
    if(outlineEnabled())
        titlePrimitive.setOutlineColor(outlineColor());
    titlePrimitive.setAlignment(titleFlags);
    titlePrimitive.setPositionWindow(titlePos);
    titlePrimitive.setTextFormat(Qt::AutoText);
    if(titleRotationEnabled() && orientation() == Qt::Vertical)
        titlePrimitive.setRotation(qDegreesToRadians(270));
    boundingBox |= titlePrimitive.computeBoundingBox(devicePixelRatio);

    // Prepare type name labels.
    if(numTypes == 0)
        numTypes = 1; // Avoid division by 0 below.

    // Layouting of the type labels.
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

    std::vector<TextPrimitive> labels;
    for(const ElementType* type : property->elementTypes()) {
        if(!type || !type->enabled())
            continue;

        labelPrimitive.setText(type->objectTitle());
        labelPrimitive.setPositionWindow(labelPos);
        boundingBox |= labelPrimitive.computeBoundingBox(devicePixelRatio);
        labels.push_back(labelPrimitive);

        if(orientation() == Qt::Vertical)
            labelPos.ry() += colorBarRect.height() / numTypes;
        else
            labelPos.rx() += colorBarRect.width() / numTypes;
    }

    // Render background rectangle.
    if(backgroundEnabled()) {
        // Look up tick image primitve in the cache
        auto& backgroundImagePrimitive = renderer->visCache().get<ImagePrimitive>(
            RendererResourceKey<struct ColorBarBackgroundImageCache, Color>{backgroundColor()});

        // Generate image primitive if not found in the cache
        if(backgroundImagePrimitive.image().isNull()) {
            // 1 x 1 px texture of the right color which will be streched to the desired rectangle dimensions.
            QImage backgroundTextureImage{QSize(1, 1), renderer->preferredImageFormat()};
            backgroundTextureImage.fill(static_cast<QColor>(backgroundColor()));
            backgroundImagePrimitive.setImage(std::move(backgroundTextureImage));
        }

        boundingBox.adjust(-textMargin, -textMargin, textMargin, textMargin);
        backgroundImagePrimitive.setRectWindow(boundingBox.toAlignedRect());
        renderer->renderImage(backgroundImagePrimitive);
    }

    // Render title.
    renderer->renderText(titlePrimitive);

    // Render color bar.
    renderer->renderImage(imagePrimitive);

    // Render type labels.
    for(const auto& labelPrimitive : labels) {
        renderer->renderText(labelPrimitive);
    }
}

}   // End of namespace
