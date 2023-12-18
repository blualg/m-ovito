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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "ViewportOverlay.h"

namespace Ovito {

/**
 * \brief A viewport overlay that displays a user-defined text label.
 */
class OVITO_CORE_EXPORT TextLabelOverlay : public ViewportOverlay
{
    OVITO_CLASS(TextLabelOverlay)
    OVITO_CLASSINFO("DisplayName", "Text label")

public:

    /// Constructor.
    explicit TextLabelOverlay(ObjectInitializationFlags flags);

    /// Is called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport) override;

    /// Informs the overlay that a new scene node has been inserted into the scene.
    virtual void sceneNodeAdded(SceneNode* node) override;

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

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// This method paints the overlay contents onto the given canvas.
    void renderImplementation(SceneRenderer* renderer, const QRect& targetRect, const PipelineFlowState& flowState);

    /// The corner of the viewport where the label is shown in.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of label position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, offsetX, setOffsetX, PROPERTY_FIELD_MEMORIZE);

    /// Controls the vertical offset of label position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, offsetY, setOffsetY, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QFont, font, setFont, PROPERTY_FIELD_MEMORIZE);

    /// Controls the label font size.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, fontSize, setFontSize, PROPERTY_FIELD_MEMORIZE);

    /// The label's text.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, labelText, setLabelText);

    /// The display color of the label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, textColor, setTextColor, PROPERTY_FIELD_MEMORIZE);

    /// The text outline color.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, outlineColor, setOutlineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the outlining of the font.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, outlineEnabled, setOutlineEnabled, PROPERTY_FIELD_MEMORIZE);

    /// The pipeline providing global attributes that can be referenced in the text.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Pipeline*, pipeline, setPipeline, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_WEAK_REF | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// Controls the formatting of floating-point variable values referenced in the text string.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, valueFormatString, setValueFormatString);
};

}   // End of namespace
