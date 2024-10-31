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
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A text string to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT TextPrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Sets the text to be rendered.
    void setText(const QString& text) { _text = text; }

    /// Returns the number of vertices stored in the buffer.
    const QString& text() const { return _text; }

    /// Sets the text color.
    void setColor(const ColorA& color) { _color = color; }

    /// Returns the text color.
    const ColorA& color() const { return _color; }

    /// Sets the text outline color.
    void setOutlineColor(const ColorA& color) { _outlineColor = color; }

    /// Returns the text outline color.
    const ColorA& outlineColor() const { return _outlineColor; }

    /// Sets the width of the text outline.
    void setOutlineWidth(FloatType width) { _outlineWidth = std::max(width, FloatType(0)); }

    /// Returns the width of the text outline.
    FloatType outlineWidth() const { return _outlineWidth; }

    /// Returns the width of the text outline multiplied with the device pixel ratio - or 0 if no outline color has been set.
    qreal effectiveOutlineWidth(qreal devicePixelRatio = 1) const { return outlineColor().a() > 0.0 ? (qreal)outlineWidth() * devicePixelRatio : 0.0; }

    /// Sets the text font.
    void setFont(const QFont& font) { _font = font; }

    /// Returns the text font.
    const QFont& font() const { return _font; }

    /// Returns the alignment of the text.
    int alignment() const { return _alignment; }

    /// Sets the alignment of the text.
    void setAlignment(int alignment) { _alignment = alignment; }

    /// Sets the text position in window coordinates.
    void setPositionWindow(const Point2& pos) { _position = pos; }

    /// Sets the text position in window coordinates.
    void setPositionWindow(const QPointF& pos) { _position = Point2(pos.x(), pos.y()); }

    /// Returns the text position in window coordinates.
    const Point2& position() const { return _position; }

    /// Returns whether the tight bounding box of the text is used for alignment.
    bool useTightBox() const { return _tightBox; }

    /// Sets whether the tight bounding box of the text is used for alignment.
    void setUseTightBox(bool use) { _tightBox = use; }

    /// Returns the type of text string (plain or rich text).
    Qt::TextFormat textFormat() const { return _textFormat; }

    /// Determines whether the text primitive uses rich text formatting or not.
    Qt::TextFormat resolvedTextFormat() const;

    /// Sets the type of text string (plain or rich text).
    void setTextFormat(Qt::TextFormat format) { _textFormat = format; }

    // Sets the rotation of the text (angle in radian).
    void setRotation(FloatType angle) { _rotation = angle; }

    // Returns the current rotation angle (in radian).
    FloatType rotation() const { return _rotation; }

    /// Computes the bounds of the text in local coordinates, i.e., in a
    /// coordinate system that is aligned with the text. The bounds are computed as if
    /// the text was drawn at (0,0).
    /// Does NOT take into account text alignment, offset position, rotation, or outline width.
    QRectF queryLocalBounds(qreal devicePixelRatio, Qt::TextFormat textFormatHint = Qt::AutoText) const;

    /// Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
    /// This method takes into account text alignment, offset position, rotation, and outline width.
    /// This overload uses the pre-computed size of the text in the local coordinate system.
    QRectF computeBounds(const QSizeF textSize, qreal devicePixelRatio) const;

    /// Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
    /// This method takes into account text alignment, offset position, rotation, and outline width.
    QRectF computeBounds(qreal devicePixelRatio) const {
        return computeBounds(queryLocalBounds(devicePixelRatio).size(), devicePixelRatio);
    }

    /// Draws the text (and the optional outline) using a QPainter.
    void draw(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const;

private:

    /// Draws the unformatted text (and optional outline) using a QPainter.
    void drawPlainText(QPainter& painter) const;

    /// Draws the formatted text (and optional outline) using a QPainter.
    void drawRichText(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const;

    /// The text to be rendered.
    QString _text;

    /// The text color.
    ColorA _color{1,1,1,1};

    /// The text outline color (no outline is rendered if alpha=0).
    ColorA _outlineColor{0,0,0,0};

    /// The width of the text outline.
    FloatType _outlineWidth{2.0};

    /// The text font.
    QFont _font;

    /// The rendering location in window coordinates.
    Point2 _position = Point2::Origin();

    /// The alignment of the text.
    int _alignment = Qt::AlignLeft | Qt::AlignTop;

    /// Use the tight bounding box of the text for alignment.
    bool _tightBox = false;

    /// The type of text string (plain or rich text).
    Qt::TextFormat _textFormat = Qt::PlainText;

    // Rotation angle (rad units).
    FloatType _rotation{0.0};
};

}   // End of namespace
