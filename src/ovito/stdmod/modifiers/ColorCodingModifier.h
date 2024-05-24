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
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {

/**
 * \brief Base class for ColorCodingModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT ColorCodingModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(ColorCodingModifierDelegate)

public:

    /// Abstract class constructor.
    using ModifierDelegate::ModifierDelegate;

    /// Returns the type of input property container that this delegate can process.
    PropertyContainerClassPtr inputContainerClass() const {
        return static_class_cast<PropertyContainer>(&getOOMetaClass().getApplicableObjectClass());
    }

    /// Returns the reference to the selected input property container for this delegate.
    PropertyContainerReference inputContainerRef() const {
        return PropertyContainerReference(inputContainerClass(), inputDataObject().dataPath(), inputDataObject().dataTitle());
    }

    /// Returns the ID of the standard property that will receive the computed colors.
    virtual int outputColorPropertyId() const { return Property::GenericColorProperty; }

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief Function for the ColorCodingModifier that operates on lines.
 */
class LinesColorCodingModifierDelegate : public ColorCodingModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ColorCodingModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ColorCodingModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Lines::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesColorCodingModifierDelegate, OOMetaClass)

public:

    /// Constructor.
    using ColorCodingModifierDelegate::ColorCodingModifierDelegate;
};

/**
 * \brief This modifier assigns a colors to data elements based on the value of a property.
 */
class OVITO_STDMOD_EXPORT ColorCodingModifier : public DelegatingModifier
{
public:

    /// Give this modifier class its own metaclass.
    class ColorCodingModifierClass : public DelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using DelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return ColorCodingModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(ColorCodingModifier, ColorCodingModifierClass)

public:

    /// Constructor.
    explicit ColorCodingModifier(ObjectInitializationFlags flags);

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns the range start value.
    FloatType startValue() const { return startValueController() ? startValueController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the range start value.
    void setStartValue(FloatType value) { if(startValueController()) startValueController()->setFloatValue(AnimationTime(0), value); }

    /// Returns the range end value.
    FloatType endValue() const { return endValueController() ? endValueController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the range end value.
    void setEndValue(FloatType value) { if(endValueController()) endValueController()->setFloatValue(AnimationTime(0), value); }

    /// Sets the start and end value to the minimum and maximum value of the selected input property
    /// determined over the entire animation sequence.
    void adjustRangeGlobal(int startFrame, int endFrame);

    /// Returns the current delegate of this modifier.
    ColorCodingModifierDelegate* delegate() const {
        return static_object_cast<ColorCodingModifierDelegate>(DelegatingModifier::delegate());
    }

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return sourceProperty().nameWithComponent(); }

    /// Sets the start and end value to the minimum and maximum value of the selected input property.
    /// Returns true if successful.
    bool adjustRange(AnimationTime time);

public Q_SLOTS:

    /// Swaps the minimum and maximum values to reverse the color scale.
    void reverseRange();

protected:

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Determines the range of values in the input data for the selected property.
    bool determinePropertyValueRange(const PipelineFlowState& state, FloatType& min, FloatType& max) const;

private:

    /// This controller stores the start value of the color scale.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, startValueController, setStartValueController);

    /// This controller stores the end value of the color scale.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, endValueController, setEndValueController);

    /// This object converts property values to colors.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<ColorCodingGradient>, colorGradient, setColorGradient);

    /// The input property that is used as data source for the coloring.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference, sourceProperty, setSourceProperty);

    /// Controls whether the modifier assigns a color only to selected elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, colorOnlySelected, setColorOnlySelected);

    /// Controls whether the input selection is preserved. If false, the selection is cleared by the modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, keepSelection, setKeepSelection);

    /// Controls whether the value range of the color map is automatically adjusted to the range of input values.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, autoAdjustRange, setAutoAdjustRange);

    friend class ColorCodingModifierDelegate;
};

}   // End of namespace
