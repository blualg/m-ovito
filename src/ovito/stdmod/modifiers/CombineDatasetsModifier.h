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

#pragma once


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>

namespace Ovito::StdMod {

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
    void mergeElementTypes(PropertyObject* property1, const PropertyObject* property2, CloneHelper& cloneHelper);
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

    Q_CLASSINFO("DisplayName", "Combine datasets");
    Q_CLASSINFO("Description", "Merge particles and bonds from two separate input files into one dataset.");
#ifndef OVITO_QML_GUI
    Q_CLASSINFO("ModifierCategory", "Modification");
#else
    Q_CLASSINFO("ModifierCategory", "-");
#endif

public:

    /// Constructor.
    Q_INVOKABLE CombineDatasetsModifier(ObjectCreationParams params);

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Returns the number of animation frames this modifier can provide.
    virtual int numberOfOutputFrames(ModifierApplication* modApp) const override {
        int upstreamFrameCount = MultiDelegatingModifier::numberOfOutputFrames(modApp);
        return secondaryDataSource() ? std::max(secondaryDataSource()->numberOfSourceFrames(), upstreamFrameCount) : upstreamFrameCount;
    }

    /// Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels(QMap<int, QString> inputLabels) const override {
        if(secondaryDataSource())
            inputLabels.insert(secondaryDataSource()->animationFrameLabels());
        return std::move(inputLabels);
    }

    /// Implementation method, which performs the merging of two pipeline states.
    void combineDatasets(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& secondaryState);

protected:

    /// \brief Is called when a RefTarget referenced by this object has generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// The source for particle data to be merged into the pipeline.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<PipelineObject>, secondaryDataSource, setSecondaryDataSource, PROPERTY_FIELD_NO_SUB_ANIM);
};

}   // End of namespace
