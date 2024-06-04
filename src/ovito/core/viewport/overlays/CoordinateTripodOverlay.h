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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "ViewportOverlay.h"

namespace Ovito {

/**
 * \brief A viewport overlay that displays the coordinate system orientation.
 */
class OVITO_CORE_EXPORT CoordinateTripodOverlay : public ViewportOverlay
{
    OVITO_CLASS(CoordinateTripodOverlay)

public:

    /// The supported rendering styles for the axis tripod.
    enum TripodStyle {
        FlatArrows,
        SolidArrows
    };
    Q_ENUM(TripodStyle);

public:

    /// Lets the overlay paint its contents into the framebuffer.
    virtual void render(FrameGraph& frameGraph, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene) override;

    /// Moves the position of the overlay in the viewport by the given amount,
    /// which is specified as a fraction of the viewport render size.
    virtual void moveLayerInViewport(const Vector2& delta) override {
        auto roundPercent = [](FloatType f) { return std::round(f * 1e4) / 1e4; };
        setOffsetX(roundPercent(offsetX() + delta.x()));
        setOffsetY(roundPercent(offsetY() + delta.y()));
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Paints a single arrow in flat style.
    FloatType paintFlatArrow(QPainter& painter, const Vector2& dir2d, FloatType arrowSize, FloatType lineWidth, FloatType tripodSize, QPointF origin);

    /// Paints a single arrow in solid style.
    FloatType paintSolidArrow(QPainter& painter, const Vector2& dir2d, const Vector3& dir3d, FloatType arrowSize, FloatType lineWidth, FloatType tripodSize, QPointF origin);

    /// Paints the tripod's joint in solid style.
    void paintSolidJoint(QPainter& painter, QPointF origin, const AffineTransformation& viewTM, FloatType lineWidth);

    /// The corner of the viewport where the tripod is shown in.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{Qt::AlignLeft | Qt::AlignBottom}, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// Controls the size of the tripod.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.075}, tripodSize, setTripodSize, PROPERTY_FIELD_MEMORIZE);

    /// Controls the line width.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.06}, lineWidth, setLineWidth, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of tripod position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetX, setOffsetX, PROPERTY_FIELD_MEMORIZE);

    /// Controls the vertical offset of tripod position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetY, setOffsetY, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QFont{}, font, setFont, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font size.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.4}, fontSize, setFontSize, PROPERTY_FIELD_MEMORIZE);

    /// Controls the display of the first axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, axis1Enabled, setAxis1Enabled);

    /// Controls the display of the second axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, axis2Enabled, setAxis2Enabled);

    /// Controls the display of the third axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, axis3Enabled, setAxis3Enabled);

    /// Controls the display of the fourth axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, axis4Enabled, setAxis4Enabled);

    /// The label of the first axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{"x"}, axis1Label, setAxis1Label);

    /// The label of the second axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{"y"}, axis2Label, setAxis2Label);

    /// The label of the third axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{"z"}, axis3Label, setAxis3Label);

    /// The label of the fourth axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{"w"}, axis4Label, setAxis4Label);

    /// The direction of the first axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{1,0,0}), axis1Dir, setAxis1Dir);

    /// The direction of the second axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{0,1,0}), axis2Dir, setAxis2Dir);

    /// The direction of the third axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{0,0,1}), axis3Dir, setAxis3Dir);

    /// The direction of the fourth axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{sqrt(0.5),sqrt(0.5),0}), axis4Dir, setAxis4Dir);

    /// The display color of the first axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,0,0}), axis1Color, setAxis1Color, PROPERTY_FIELD_MEMORIZE);

    /// The display color of the second axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0,0.8,0}), axis2Color, setAxis2Color, PROPERTY_FIELD_MEMORIZE);

    /// The display color of the third axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.2,0.2,1}), axis3Color, setAxis3Color, PROPERTY_FIELD_MEMORIZE);

    /// The display color of the fourth axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,0,1}), axis4Color, setAxis4Color, PROPERTY_FIELD_MEMORIZE);

    /// The rendering style of the tripod.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(TripodStyle{FlatArrows}, tripodStyle, setTripodStyle, PROPERTY_FIELD_MEMORIZE);

    /// The outline color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,1,1}), outlineColor, setOutlineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the outlining of the text and axis arrows.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outlineEnabled, setOutlineEnabled, PROPERTY_FIELD_MEMORIZE);

    /// Switches on perspective distortion of the tripod (if placed in a perspective viewport).
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, perspectiveDistortion, setPerspectiveDistortion, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
