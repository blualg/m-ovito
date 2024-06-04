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


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

/**
 * \brief Base class for AssignColorModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT AssignColorModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(AssignColorModifierDelegate)

public:

    /// Applies the modifier operation to the data in a pipeline flow state.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Returns the type of input property container that this delegate can process.
    PropertyContainerClassPtr inputContainerClass() const {
        return static_class_cast<PropertyContainer>(&getOOMetaClass().getApplicableObjectClass());
    }

    /// Returns a reference to the property container being modified by this delegate.
    PropertyContainerReference inputContainerRef() const {
        return PropertyContainerReference(inputContainerClass(), inputDataObject().dataPath(), inputDataObject().dataTitle());
    }

protected:

    /// returns the ID of the standard property that will receive the assigned colors.
    virtual int outputColorPropertyId() const = 0;
};

/**
 * \brief This modifier assigns a uniform color to all selected elements.
 */
class OVITO_STDMOD_EXPORT AssignColorModifier : public DelegatingModifier
{
public:

    /// Give this modifier class its own metaclass.
    class AssignColorModifierClass : public DelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using DelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return AssignColorModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(AssignColorModifier, AssignColorModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns the color that is assigned to the selected elements.
    Color color() const { return colorController() ? colorController()->getColorValue(AnimationTime(0)) : Color(0,0,0); }

    /// Sets the color that is assigned to the selected elements.
    void setColor(const Color& color) { if(colorController()) colorController()->setColorValue(AnimationTime(0), color); }

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return QVariant::fromValue(static_cast<QColor>(color())); }

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// This controller stores the color to be assigned.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Controller>, colorController, setColorController, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the input selection is preserved.
    /// If false, the selection is cleared by the modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, keepSelection, setKeepSelection);
};

}   // End of namespace
