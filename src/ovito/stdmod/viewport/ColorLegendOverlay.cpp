////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, orientation, "Orientation");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, legendSize, "Overall size");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, font, "Font");
SET_PROPERTY_FIELD_LABEL(ColorLegendOverlay, fontSize, "Font size");
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
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(ColorLegendOverlay, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, legendSize, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, aspectRatio, FloatParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ColorLegendOverlay, fontSize, FloatParameterUnit, 0);

/******************************************************************************
* Constructor.
******************************************************************************/
ColorLegendOverlay::ColorLegendOverlay(ObjectCreationParams params) : ViewportOverlay(params),
	_alignment(Qt::AlignHCenter | Qt::AlignBottom),
	_orientation(Qt::Horizontal),
	_legendSize(0.3),
	_offsetX(0),
	_offsetY(0),
	_fontSize(0.1),
	_valueFormatString("%g"),
	_aspectRatio(8.0),
	_textColor(0,0,0),
	_outlineColor(1,1,1),
	_outlineEnabled(false),
	_borderEnabled(false),
	_borderColor(0,0,0)
{
}

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport. 
******************************************************************************/
void ColorLegendOverlay::initializeOverlay(Viewport* viewport)
{
	if(ExecutionContext::isInteractive()) {
		
		// Find a ColorCodingModifier in the scene that we can connect to.
		if(!modifier() && !sourceProperty() && viewport->scene()) {
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
		if(!modifier() && !sourceProperty() && viewport->scene()) {
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
void ColorLegendOverlay::render(SceneRenderer* renderer, const QRect& logicalViewportRect, const QRect& physicalViewportRect, MainThreadOperation& operation)
{
	DataOORef<const PropertyObject> typedProperty;

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

			// Evaulate pipeline and obtain output data collection.
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
		if(operation.isCanceled())
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
		setStatus(PipelineStatus(PipelineStatus::Warning, tr("No source Color Coding modifier has been selected for this color legend.")));

		// Escalate to an error state if in terminal mode.
		if(Application::instance()->consoleMode()) {
			throw Exception(tr("You are trying to render a Viewport with a ColorLegendOverlay whose 'modifier' property has "
							  "not been set to any ColorCodingModifier. Did you forget to assign a source for the color legend?"));
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

		drawContinuousColorMap(renderer, colorBarRect, legendSize, PseudoColorMapping(startValue, endValue, modifier()->colorGradient()), modifier()->sourceProperty().nameWithComponent());
	}
	else if(colorMapping()) {
		drawContinuousColorMap(renderer, colorBarRect, legendSize, colorMapping()->pseudoColorMapping(), colorMapping()->sourceProperty().nameWithComponent());
	}
	else if(typedProperty) {
		drawDiscreteColorMap(renderer, colorBarRect, legendSize, typedProperty);
	}
}

/******************************************************************************
* Draws the color legend for a Color Coding modifier.
******************************************************************************/
void ColorLegendOverlay::drawContinuousColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const PseudoColorMapping& mapping, const QString& propertyName)
{
	if(!mapping.gradient())
		return;

	// Look up the image primitive for the color bar in the cache.
	auto& [imagePrimitive, offset] = renderer->visCache().get<std::tuple<ImagePrimitive, QPointF>>(
		RendererResourceKey<struct ColorBarImageCache, OORef<ColorCodingGradient>, FloatType, int, bool, Color, QSizeF>{ 
			mapping.gradient(),
			renderer->devicePixelRatio(),
			orientation(),
			borderEnabled(),
			borderColor(),
			colorBarRect.size()
		});

	// Render the color bar into an image texture.
	if(imagePrimitive.image().isNull()) {

		// Allocate the image buffer.
		QSize gradientSize = colorBarRect.size().toSize();
		int borderWidth = borderEnabled() ? (int)std::ceil(2.0 * renderer->devicePixelRatio()) : 0;		
		QImage textureImage(gradientSize.width() + 2*borderWidth, gradientSize.height() + 2*borderWidth, QImage::Format_ARGB32_Premultiplied);
		if(borderEnabled())
			textureImage.fill((QColor)borderColor());

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
		offset = QPointF(-borderWidth,-borderWidth);
	}
	QPoint alignedPos = (colorBarRect.topLeft() + offset).toPoint();
	imagePrimitive.setRectWindow(QRect(alignedPos, imagePrimitive.image().size()));
	renderer->renderImage(imagePrimitive);

	qreal fontSize = legendSize * std::max(FloatType(0), this->fontSize());
	if(fontSize == 0) return;
	QFont font = this->font();

	QByteArray format = valueFormatString().toUtf8();
	if(format.contains("%s")) format.clear();

	QString titleLabel, topLabel, bottomLabel;
	if(label1().isEmpty())
		topLabel = std::isfinite(mapping.maxValue()) ? QString::asprintf(format.constData(), mapping.maxValue()) : QStringLiteral("###");
	else
		topLabel = label1();
	if(label2().isEmpty())
		bottomLabel = std::isfinite(mapping.minValue()) ? QString::asprintf(format.constData(), mapping.minValue()) : QStringLiteral("###");
	else
		bottomLabel = label2();
	if(title().isEmpty())
		titleLabel = propertyName;
	else
		titleLabel = title();

	font.setPointSizeF(fontSize / renderer->devicePixelRatio()); // Font size if always in logical units.

	qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

	// Move the text path to the correct location based on color bar direction and position
	int titleFlags = Qt::AlignBottom;
	QPointF titlePos;
	if(orientation() != Qt::Vertical || (alignment() & Qt::AlignHCenter)) {
		titleFlags |= Qt::AlignHCenter;
		titlePos.rx() = colorBarRect.left() + 0.5 * colorBarRect.width();
		titlePos.ry() = colorBarRect.top() - 0.5 * textMargin;
	}
	else {
		if(alignment() & Qt::AlignLeft) {
			titleFlags |= Qt::AlignLeft;
			titlePos.rx() = colorBarRect.left();
		}
		else if(alignment() & Qt::AlignRight) {
			titleFlags |= Qt::AlignRight;
			titlePos.rx() = colorBarRect.right();
		}
		else {
			titleFlags |= Qt::AlignHCenter;
			titlePos.rx() = colorBarRect.left() + 0.5 * colorBarRect.width();
		}
		titlePos.ry() = colorBarRect.top() - textMargin;
	}

	// Render title string.
	TextPrimitive textPrimitive;
	textPrimitive.setFont(font);
	textPrimitive.setText(titleLabel);
	textPrimitive.setColor(textColor());
	if(outlineEnabled()) textPrimitive.setOutlineColor(outlineColor());
	textPrimitive.setAlignment(titleFlags);
	textPrimitive.setPositionWindow(titlePos);
	textPrimitive.setTextFormat(Qt::AutoText);
	renderer->renderText(textPrimitive);

	// Render limit labels.
	font.setPointSizeF(fontSize * 0.8 / renderer->devicePixelRatio()); // Font size if always in logical units.
	textPrimitive.setFont(font);

	int topFlags = 0;
	int bottomFlags = 0;
	QPointF topPos;
	QPointF bottomPos;

	if(orientation() != Qt::Vertical) {
		bottomFlags = Qt::AlignRight | Qt::AlignVCenter;
		topFlags = Qt::AlignLeft | Qt::AlignVCenter;
		bottomPos = QPointF(colorBarRect.left() - textMargin, colorBarRect.top() + 0.5 * colorBarRect.height());
		topPos = QPointF(colorBarRect.right() + textMargin, colorBarRect.top() + 0.5 * colorBarRect.height());
	}
	else {
		if((alignment() & Qt::AlignLeft) || (alignment() & Qt::AlignHCenter)) {
			bottomFlags = Qt::AlignLeft | Qt::AlignBottom;
			topFlags = Qt::AlignLeft | Qt::AlignTop;
			bottomPos = QPointF(colorBarRect.right() + textMargin, colorBarRect.bottom());
			topPos = QPointF(colorBarRect.right() + textMargin, colorBarRect.top());
		}
		else if(alignment() & Qt::AlignRight) {
			bottomFlags = Qt::AlignRight | Qt::AlignBottom;
			topFlags = Qt::AlignRight | Qt::AlignTop;
			bottomPos = QPointF(colorBarRect.left() - textMargin, colorBarRect.bottom());
			topPos = QPointF(colorBarRect.left() - textMargin, colorBarRect.top());
		}
	}

	textPrimitive.setUseTightBox(true);
	textPrimitive.setText(topLabel);
	textPrimitive.setAlignment(topFlags);
	textPrimitive.setPositionWindow(topPos);
	renderer->renderText(textPrimitive);

	textPrimitive.setText(bottomLabel);
	textPrimitive.setAlignment(bottomFlags);
	textPrimitive.setPositionWindow(bottomPos);
	renderer->renderText(textPrimitive);
}

/******************************************************************************
* Draws the color legend for a typed property.
******************************************************************************/
void ColorLegendOverlay::drawDiscreteColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const PropertyObject* property)
{
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
			renderer->devicePixelRatio(),
			orientation(),
			borderEnabled(),
			borderColor(),
			colorBarRect.size()
		});

	// Render the color fields into an image texture.
	if(imagePrimitive.image().isNull()) {

		// Allocate the image buffer.
		QSize gradientSize = colorBarRect.size().toSize();
		int borderWidth = borderEnabled() ? (int)std::ceil(2.0 * renderer->devicePixelRatio()) : 0;		
		QImage textureImage(gradientSize.width() + 2*borderWidth, gradientSize.height() + 2*borderWidth, QImage::Format_ARGB32_Premultiplied);
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
	renderer->renderImage(imagePrimitive);

	// Count the number of element types that are enabled.
	int numTypes = typeColors.size();

	qreal fontSize = legendSize * std::max(FloatType(0), this->fontSize());
	if(fontSize == 0) return;
	QFont font = this->font();
	font.setPointSizeF(fontSize / renderer->devicePixelRatio()); // Font size if always in logical units.

	TextPrimitive textPrimitive;
	if(title().isEmpty())
		textPrimitive.setText(property->objectTitle());
	else
		textPrimitive.setText(title());
	textPrimitive.setFont(font);

	qreal textMargin = 0.2 * legendSize / std::max(FloatType(0.01), aspectRatio());

	// Move the text path to the correct location based on color bar direction and position
	int titleFlags = 0;
	QPointF titlePos;
	if(orientation() == Qt::Vertical) {
		if(alignment() & Qt::AlignLeft) {
			titleFlags = Qt::AlignLeft | Qt::AlignBottom;
			titlePos.rx() = colorBarRect.left();
			titlePos.ry() = colorBarRect.top() - textMargin;
		}
		else if(alignment() & Qt::AlignRight) {
			titleFlags = Qt::AlignRight | Qt::AlignBottom;
			titlePos.rx() = colorBarRect.right();
			titlePos.ry() = colorBarRect.top() - textMargin;
		}
		else {
			titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
			titlePos.rx() = colorBarRect.left() + 0.5 * colorBarRect.width();
			titlePos.ry() = colorBarRect.top() - textMargin;
		}
	}
	else {
		if((alignment() & Qt::AlignTop) || (alignment() & Qt::AlignVCenter)) {
			titleFlags = Qt::AlignHCenter | Qt::AlignBottom;
			titlePos.rx() = colorBarRect.left() + 0.5 * colorBarRect.width();
			titlePos.ry() = colorBarRect.top() - textMargin;
		}
		else {
			titleFlags = Qt::AlignHCenter | Qt::AlignTop;
			titlePos.rx() = colorBarRect.left() + 0.5 * colorBarRect.width();
			titlePos.ry() = colorBarRect.bottom() + 0.5 * textMargin;
		}
	}

	textPrimitive.setColor(textColor());
	if(outlineEnabled()) textPrimitive.setOutlineColor(outlineColor());
	textPrimitive.setAlignment(titleFlags);
	textPrimitive.setPositionWindow(titlePos);
	textPrimitive.setTextFormat(Qt::AutoText);
	renderer->renderText(textPrimitive);

	// Draw type name labels.
	if(numTypes == 0)
		return;

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

	for(const ElementType* type : property->elementTypes()) {
		if(!type || !type->enabled()) 
			continue;

		textPrimitive.setText(type->objectTitle());
		textPrimitive.setAlignment(labelFlags);
		textPrimitive.setPositionWindow(labelPos);
		renderer->renderText(textPrimitive);

		if(orientation() == Qt::Vertical)
			labelPos.ry() += colorBarRect.height() / numTypes;
		else
			labelPos.rx() += colorBarRect.width() / numTypes;
	}
}

}	// End of namespace
