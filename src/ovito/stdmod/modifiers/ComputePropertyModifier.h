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

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

class ComputePropertyModifier;         // defined below
class ComputePropertyModificationNode; // defined below

/**
 * \brief Base class for modifier delegates used by the ComputePropertyModifier class.
 */
class OVITO_STDMOD_EXPORT ComputePropertyModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(ComputePropertyModifierDelegate)

public:

    /// Returns the type of input property container that this delegate can process.
    PropertyContainerClassPtr inputContainerClass() const {
        return static_class_cast<PropertyContainer>(&getOOMetaClass().getApplicableObjectClass());
    }

    /// Returns the reference to the selected input property container for this delegate.
    PropertyContainerReference inputContainerRef() const {
        return PropertyContainerReference(inputContainerClass(), inputDataObject().dataPath(), inputDataObject().dataTitle());
    }

    /// Returns the title string to be displayed in the UI for the group box containing the math expression(s).
    virtual QString expressionUITitle(int componentCount) const { return componentCount <= 1 ? tr("Expression") : tr("Expressions"); }

    /// Sets the number of vector components of the property to compute.
    virtual void setComponentCount(int componentCount) {}

    /// Checks if math expressions are time-dependent, i.e. whether they involve the animation frame number.
    virtual bool isExpressionTimeDependent(ComputePropertyModifier* modifier) const;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;

    /// Returns how the elements computed by this delegate are called. This label is shown in the UI.
    virtual QString elementLabel() const { return inputContainerClass()->elementDescriptionName(); }

protected:

    // Sets up the visual element(s) associated with the new property.
    virtual void setupVisualElements(Property* outputProperty, ComputePropertyModificationNode* modNode);

    /// Launches the actual computations.
    virtual Future<PipelineFlowState> performComputation(const ComputePropertyModifier* modifier, ComputePropertyModificationNode* modNode, PipelineFlowState state, const PipelineFlowState& originalState, PropertyPtr outputProperty, ConstPropertyPtr selectionProperty, int frame) const;

    /// Initializes an expression evaluator.
    virtual std::unique_ptr<PropertyExpressionEvaluator> initializeExpressionEvaluator(const ComputePropertyModifier* modifier, const PipelineFlowState& originalState, int frame) const;
};

/**
 * \brief Computes the values of a property from a user-defined math expression.
 */
class OVITO_STDMOD_EXPORT ComputePropertyModifier : public DelegatingModifier
{
    /// Give this modifier class its own metaclass.
    class ComputePropertyModifierClass : public DelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using DelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return ComputePropertyModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(ComputePropertyModifier, ComputePropertyModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the current delegate of this ComputePropertyModifier.
    ComputePropertyModifierDelegate* delegate() const { return static_object_cast<ComputePropertyModifierDelegate>(DelegatingModifier::delegate()); }

    /// \brief Sets the math expression that is used to calculate the values of one of the new property's components.
    /// \param index The property component for which the expression should be set.
    /// \param expression The math formula.
    void setExpression(const QString& expression, int index = 0) {
        if(index < 0 || index >= expressions().size())
            throw Exception("Property component index is out of range.");
        QStringList copy = _expressions;
        copy[index] = expression;
        setExpressions(std::move(copy));
    }

    /// \brief Returns the math expression that is used to calculate the values of one of the new property's components.
    /// \param index The property component for which the expression should be returned.
    /// \return The math formula used to calculates the channel's values.
    const QString& expression(int index = 0) const {
        if(index < 0 || index >= expressions().size())
            throw Exception("Property component index is out of range.");
        return expressions()[index];
    }

    /// Returns the number of vector components of the property to create.
    int propertyComponentCount() const { return expressions().size(); }

    /// Sets the number of vector components of the property to create.
    void setPropertyComponentCount(int newComponentCount, const QStringList& componentNames);

    /// Returns the names of the vector components of the output property. This list is shown in the UI.
    QStringList effectiveComponentNames() const;

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return outputProperty().nameWithComponent(); }

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

protected:

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

private:

    /// The math expressions for calculating the property values. One for every vector component.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QStringList{"0"}, expressions, setExpressions);

    /// The names of the property's vector components (when computing a user-defined vector property).
    /// Note: In case of predefined standard properties, the component names are prescribed by the container class.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QStringList{}, componentNames, setComponentNames);

    /// Specifies the output property that will receive the computed per-particles values.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, outputProperty, setOutputProperty);

    /// Controls whether the math expression is evaluated and output only for selected elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedElements, setOnlySelectedElements);

    /// Controls whether multi-line input fields are shown in the UI for the expressions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useMultilineFields, setUseMultilineFields);
};

/**
 * Used by the ComputePropertyModifier to store working data.
 */
class OVITO_STDMOD_EXPORT ComputePropertyModificationNode : public ModificationNode
{
    OVITO_CLASS(ComputePropertyModificationNode)

public:

    /// Replaces all references to the given visual element in the pipeline with new compatible objects.
    virtual void replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement) override;

private:

    /// The cached visualization elements that are attached to the output property.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<DataVis>, cachedVisElements, setCachedVisElements, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM);

    /// The list of input variables during the last evaluation.
    DECLARE_RUNTIME_PROPERTY_FIELD_FLAGS(QStringList{}, inputVariableNames, setInputVariableNames, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO);

    /// The list of input variables for the expressions managed by the delegate during the last evaluation.
    DECLARE_RUNTIME_PROPERTY_FIELD_FLAGS(QStringList{}, delegateInputVariableNames, setDelegateInputVariableNames, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO);

    /// Human-readable text listing the input variables during the last evaluation.
    DECLARE_RUNTIME_PROPERTY_FIELD_FLAGS(QString{}, inputVariableTable, setInputVariableTable, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO);
};

}   // End of namespace
