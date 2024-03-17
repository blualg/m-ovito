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
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>

namespace Ovito {

/**
 * \brief Base class for CombineDatasetsModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT CombineDatasetsModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(CombineDatasetsModifierDelegate)

protected:

    /// Abstract class constructor.
    using ModifierDelegate::ModifierDelegate;

    /// Helper method that merges the set of element types defined for a property.
    void mergeElementTypes(Property* property1, const Property* property2, CloneHelper& cloneHelper);
};

/**
 * \brief Merges two separate datasets into one.
 */
class OVITO_STDMOD_EXPORT CombineDatasetsModifier : public MultiDelegatingModifier
{
    /// Give this modifier class its own metaclass.
    class CombineDatasetsModifierClass : public MultiDelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using MultiDelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return CombineDatasetsModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(CombineDatasetsModifier, CombineDatasetsModifierClass)

    OVITO_CLASSINFO("DisplayName", "Combine datasets");
    OVITO_CLASSINFO("Description", "Merge particles and bonds from two separate input files into one dataset.");
    OVITO_CLASSINFO("ModifierCategory", "Modification");

public:

    /// Constructor.
    explicit CombineDatasetsModifier(ObjectInitializationFlags flags);

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Returns the number of animation frames this modifier can provide.
    virtual int numberOfOutputFrames(ModificationNode* node) const override {
        int upstreamFrameCount = MultiDelegatingModifier::numberOfOutputFrames(node);
        return secondaryDataSource() ? std::max(secondaryDataSource()->numberOfSourceFrames(), upstreamFrameCount) : upstreamFrameCount;
    }

    /// Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels(QMap<int, QString> inputLabels) const override {
        if(secondaryDataSource())
            inputLabels.insert(secondaryDataSource()->animationFrameLabels());
        return std::move(inputLabels);
    }

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Implementation method, which performs the merging of two pipeline states.
    Future<PipelineFlowState> combineDatasets(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& secondaryState);

private:

    /// The source for particle data to be merged into the pipeline.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<PipelineNode>, secondaryDataSource, setSecondaryDataSource, PROPERTY_FIELD_NO_SUB_ANIM);
};

}   // End of namespace
