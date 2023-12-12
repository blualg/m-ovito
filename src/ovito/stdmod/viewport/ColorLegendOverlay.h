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

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdmod/modifiers/ColorCodingModifier.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>
#include <ovito/core/rendering/FrameBuffer.h>

namespace Ovito {

/**
 * \brief A viewport overlay that displays the color legend of a ColorCodingModifier.
 */
class OVITO_STDMOD_EXPORT ColorLegendOverlay : public ViewportOverlay
{
    OVITO_CLASS(ColorLegendOverlay)
    Q_CLASSINFO("DisplayName", "Color legend");

public:

    /// \brief Constructor.
    Q_INVOKABLE ColorLegendOverlay(ObjectInitializationFlags flags);

    /// \brief This virtual method gets called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport) override;

    /// Lets the overlay paint its contents into the framebuffer.
    virtual void render(SceneRenderer* renderer, const QRect& logicalViewportRect, const QRect& physicalViewportRect) override;

    /// Moves the position of the overlay in the viewport by the given amount,
    /// which is specified as a fraction of the viewport render size.
    virtual void moveLayerInViewport(const Vector2& delta) override {
        auto roundPercent = [](FloatType f) { return std::round(f * 1e4) / 1e4; };
        setOffsetX(roundPercent(offsetX() + delta.x()));
        setOffsetY(roundPercent(offsetY() + delta.y()));
    }

    /// Returns a short piece information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene) const override;

Q_SIGNALS:

    /// Signal is emited whenever the automatic label texts got recalculated during rendering.
    void autoLabelsUpdated();

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

public:

    Q_PROPERTY(Ovito::ColorCodingModifier* modifier READ modifier WRITE setModifier)

private:

    /// Draws the color legend for a Color Coding modifier.
    void drawContinuousColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const PseudoColorMapping& mapping);

	/// Draws the color legend for a typed property.
	void drawDiscreteColorMap(SceneRenderer* renderer, const QRectF& colorBarRect, FloatType legendSize, const Property* property);

    // Determine the starting value and the tick spacing for a given color bar length and character size
    [[nodiscard]] std::tuple<FloatType, FloatType> getAutomaticTickPositions(FloatType lowerLimit, FloatType upperLimit,
                                                                             const FloatType lenColorbar,
                                                                             const QFontMetricsF& fontMetrics,
                                                                             const QByteArray& labelFormat,
                                                                             const int maxIter = 50) const;

    // Determine the starting value for a given tick spacing.
    [[nodiscard]] static FloatType getUserDefinedTickPositions(FloatType lowerLimit, FloatType upperLimit, const FloatType inter);

    /// The corner of the viewport where the color legend is displayed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// The orientation (horizontal/vertical) of the color legend.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, orientation, setOrientation, PROPERTY_FIELD_MEMORIZE);

    /// Controls the overall size of the color legend.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, legendSize, setLegendSize, PROPERTY_FIELD_MEMORIZE);

    /// Controls the aspect ration of the color bar.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, aspectRatio, setAspectRatio, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of legend position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, offsetX, setOffsetX)

    /// Controls the vertical offset of legend position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, offsetY, setOffsetY);

    /// Controls the title and label font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QFont, font, setFont, PROPERTY_FIELD_MEMORIZE);

    /// Controls the title font size.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, fontSize, setFontSize, PROPERTY_FIELD_MEMORIZE);

    /// Controls the relative (as fraction of the title font size) label font size.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, relLabelFontSize, setRelLabelFontSize, PROPERTY_FIELD_MEMORIZE);

    /// The title label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);

    /// User-defined text for the first numeric label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, label1, setLabel1);

    /// User-defined text for the second numeric label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, label2, setLabel2);

    /// The ColorCodingModifier for which to display the legend.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ColorCodingModifier>, modifier, setModifier,
                                             PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM |
                                                 PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// The selected PropertyColorMapping for which to display the legend.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<PropertyColorMapping>, colorMapping, setColorMapping,
                                             PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM |
                                                 PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// Controls the formatting of the value labels in the color legend.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, valueFormatString, setValueFormatString);

    /// Controls the text color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, textColor, setTextColor, PROPERTY_FIELD_MEMORIZE);

    /// The text outline color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, outlineColor, setOutlineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the outlining of the font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, outlineEnabled, setOutlineEnabled, PROPERTY_FIELD_MEMORIZE);

    /// The typed property whose element types are shown by the legend.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyDataObjectReference, sourceProperty, setSourceProperty);

    /// Controls the drawing of a border around the color map.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, borderEnabled, setBorderEnabled, PROPERTY_FIELD_MEMORIZE);

    /// The border color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, borderColor, setBorderColor, PROPERTY_FIELD_MEMORIZE);

    // Toggle tick marks.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, ticksEnabled, setTicksEnabled, PROPERTY_FIELD_MEMORIZE);

    // Controls the manual tick spacing.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, tickSpacing, setTickSpacing);

    // Controls the rotation of the title.
    // For vertical colorbars the title can either be vertical (enabled=true) or horizontal (enabled=false).
    // This flag has no effect for horizontal colorbars.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, titleRotationEnabled, setTitleRotationEnabled, PROPERTY_FIELD_MEMORIZE);

    // Toggle background.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, backgroundEnabled, setBackgroundEnabled, PROPERTY_FIELD_MEMORIZE);

    // Background color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, backgroundColor, setBackgroundColor, PROPERTY_FIELD_MEMORIZE);

    /// The automatically chosen texts of the title and numeric labels. These strings are determined during rendering of the overlay and will
    /// subsequently be displayed in the GUI panel as placeholder texts in the input fields.
    QString _autoTitleText, _autoLabel1Text, _autoLabel2Text;

    friend class ColorLegendOverlayEditor;
};

}   // End of namespace
