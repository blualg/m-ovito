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
#include <ovito/core/dataset/pipeline/ActiveObject.h>
#include <ovito/core/utilities/concurrent/FutureCache.h>
#include "ModifierGroup.h"
#include "PipelineNode.h"

namespace Ovito {

/**
 * \brief Represents the application of a Modifier in a data pipeline.
 *
 * Modifiers can be shared by multiple data pipelines.
 * For every use of a Modifier instance in a pipeline, a ModificationNode is created.
 *
 * \sa Modifier
 */
class OVITO_CORE_EXPORT ModificationNode : public PipelineNode
{
    OVITO_CLASS(ModificationNode)

public:

    /// Registry for modification node types.
    class Registry : private std::map<OvitoClassPtr, OvitoClassPtr>
    {
    public:
        void registerModificationNodeType(OvitoClassPtr modifierClass, OvitoClassPtr modNodeClass) {
            insert(std::make_pair(modifierClass, modNodeClass));
        }
        OvitoClassPtr getModificationNodeType(OvitoClassPtr modifierClass) const {
            auto entry = find(modifierClass);
            if(entry != end()) return entry->second;
            else return nullptr;
        }
    };

    /// Returns the global class registry, which allows looking up the ModificationNode subclass for a Modifier subclass.
    static Registry& registry();

public:

    /// Constructor.
    using PipelineNode::PipelineNode;

    /// Throws an exception if the pipeline stage cannot be evaluated at this time. This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck() const override;

    /// Asks the object for the result of the upstream data pipeline.
    PipelineEvaluationResult evaluateInput(const PipelineEvaluationRequest& request) const;

    /// Asks the object for the result of the upstream data pipeline at several animation times.
    Future<std::vector<PipelineFlowState>> evaluateInputMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times) const;

    /// Is called by the pipeline system before a new evaluation begins to query the validity interval and evaluation result type of this pipeline stage.
    virtual void preevaluate(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) override;

    /// Asks the object for the result of the data pipeline.
    virtual PipelineEvaluationResult evaluate(const PipelineEvaluationRequest& request) override;

    /// Returns the cached output of this data pipeline stage at the given time if available.
    /// This method will never throw an exception and doesn't require a valid execution context.
    virtual PipelineFlowState getCachedPipelineNodeOutput(AnimationTime time, bool interactiveMode = true) const override {
        if(modifierAndGroupEnabled())
            return PipelineNode::getCachedPipelineNodeOutput(time, interactiveMode);
        else
            return getCachedPipelineNodeInput(time, interactiveMode);
    }

    /// \brief Returns the cached input of this modification node at the given time if available.
    /// This method will never throw an exception and doesn't require a valid execution context.
    PipelineFlowState getCachedPipelineNodeInput(AnimationTime time, bool interactiveMode = true) const {
        return input() ? input()->getCachedPipelineNodeOutput(time, interactiveMode) : PipelineFlowState();
    }

    /// \brief Returns the number of animation frames this pipeline object can provide.
    virtual int numberOfSourceFrames() const override;

    /// \brief Given an animation time, computes the source frame to show.
    virtual int animationTimeToSourceFrame(AnimationTime time) const override;

    /// \brief Given a source frame index, returns the animation time at which it is shown.
    virtual AnimationTime sourceFrameToAnimationTime(int frame) const override;

    /// \brief Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels() const override;

    /// \brief Returns a short piece information (typically a string or color) to be displayed next to the object's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene) const override;

    /// \brief Traverses the pipeline from this modifier application up to the source and
    /// returns the source object that generates the input data for the pipeline.
    PipelineNode* pipelineSource() const;

    /// \brief Returns the modification node that precedes this node in the pipeline.
    /// If this node is referenced by more than one modification node (=it is preceded by a pipeline branch),
    /// then nullptr is returned.
    ModificationNode* getPredecessorModNode() const;

    /// \brief Returns the title of this modification node.
    virtual QString objectTitle() const override;

    /// Returns whether the modifier AND the modifier group (if this node is part of one) are enabled.
    bool modifierAndGroupEnabled() const;

    /// Asks this object to delete itself.
    virtual void requestObjectDeletion() override;

    /// Returns the node's cache for partial modifier results.
    /// This cache can be used to enable fast interactive updates after parameter changes that do not invalidate the entire result.
    FutureCache<DataOORef<const DataCollection>>& partialResultsCache() { return _partialResultsCache; }

protected:

    /// Is called by the pipeline system before a new modifier evaluation begins to query the validity interval and evaluation result type of the pipeline stage.
    virtual void preevaluateInternal(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) override;

    /// Asks the object for the result of the data pipeline.
    virtual SharedFuture<PipelineFlowState> evaluateInternal(const PipelineEvaluationRequest& request) override;

    /// Decides whether a preliminary viewport update is performed after this pipeline object has been
    /// evaluated but before the rest of the pipeline is complete.
    virtual bool shouldRefreshViewportsAfterEvaluation() override;

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// Pipeline node providing the input data for the modifier.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<PipelineNode>, input, setInput, PROPERTY_FIELD_NEVER_CLONE_TARGET);

    /// The modifier.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Modifier>, modifier, setModifier, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_OPEN_SUBEDITOR);

    /// The logical group this modification node belongs to (only used in the GUI).
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ModifierGroup>, modifierGroup, setModifierGroup, PROPERTY_FIELD_ALWAYS_CLONE | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Cache for partial results computed by the modifier.
    /// This can be used by the modifier to enable fast interactive updates after parameter changes that do not invalidate the entire result.
    FutureCache<DataOORef<const DataCollection>> _partialResultsCache;
};

/// This macro registers some ModificationNode-derived class as the pipeline node type of some Modifier-derived class.
#define SET_MODIFICATION_NODE_TYPE(ModifierClass, ModificationNodeClass) \
    static const int __modnodeSetter##ModifierClass = (Ovito::ModificationNode::registry().registerModificationNodeType(&ModifierClass::OOClass(), &ModificationNodeClass::OOClass()), 0);

}   // End of namespace

#include "ModifierEvaluationRequest.h"
#include "Modifier.h"


