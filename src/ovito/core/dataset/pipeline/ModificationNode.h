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
    OVITO_CLASSINFO("ClassNameAlias", "ModifierApplication");  // For backward compatibility with OVITO 3.9.2
    OVITO_CLASSINFO("ClassNameAlias", "AsynchronousModifierApplication");  // For backward compatibility with OVITO 3.9.2
    OVITO_CLASSINFO("ClassNameAlias", "AsynchronousModificationNode");  // For backward compatibility with OVITO 3.10.2

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

    /// \brief Constructor.
    explicit ModificationNode(ObjectInitializationFlags flags) : PipelineNode(flags) {}

    /// \brief Determines the time interval over which a computed pipeline state will remain valid.
    virtual TimeInterval validityInterval(const PipelineEvaluationRequest& request) const override;

    /// \brief Throws an exception if the pipeline stage cannot be evaluated at this time. This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck() const override;

    /// \brief Asks the object for the result of the upstream data pipeline.
    SharedFuture<PipelineFlowState> evaluateInput(const PipelineEvaluationRequest& request) const;

    /// \brief Asks the object for the result of the upstream data pipeline at several animation times.
    Future<std::vector<PipelineFlowState>> evaluateInputMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times) const;

    /// \brief Requests the preliminary computation results from the upstream data pipeline.
    PipelineFlowState evaluateInputSynchronous(const PipelineEvaluationRequest& request) const { return input() ? input()->evaluateSynchronous(request) : PipelineFlowState(); }

    /// \brief Asks the object for the result of the data pipeline.
    virtual SharedFuture<PipelineFlowState> evaluate(const PipelineEvaluationRequest& request) override;

    /// \brief Asks the pipeline stage to compute the preliminary results in a synchronous fashion.
    virtual PipelineFlowState evaluateSynchronous(const PipelineEvaluationRequest& request) override;

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

    /// Returns the sequence of compute engines from a recent successfully completed modifier evaluation which are still valid.
    const std::vector<ModifierEnginePtr>& validStages() const { return _validStages; }

    /// Stores the sequence of compute engines from a recent successfully completed modifier evaluation.
    void setValidStages(std::vector<ModifierEnginePtr> validStages) { _validStages = std::move(validStages); }

    /// Returns a compute engine containing the results of a fully completed algorithm, which may be outdated.
    const ModifierEnginePtr& completedEngine() const { return _completedEngine; }

    /// Stores the compute engine containing the results of a fully completed algorithm.
    void setCompletedEngine(ModifierEnginePtr eng) { _completedEngine = std::move(eng); }

protected:

    /// \brief Asks the object for the result of the data pipeline.
    virtual Future<PipelineFlowState> evaluateInternal(const PipelineEvaluationRequest& request) override;

    /// \brief Lets the pipeline stage compute a preliminary result in a synchronous fashion.
    virtual PipelineFlowState evaluateInternalSynchronous(const PipelineEvaluationRequest& request) override;

    /// \brief Decides whether a preliminary viewport update is performed after this pipeline object has been
    ///        evaluated but before the rest of the pipeline is complete.
    virtual bool performPreliminaryUpdateAfterEvaluation() override;

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

    /// \brief Is called when a RefTarget referenced by this object generated an event.
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

    /// The sequence of compute engines from a recent successfully completed modifier evaluation which are still valid.
    std::vector<ModifierEnginePtr> _validStages;

    /// A compute engine containing the results of a fully completed algorithm, which may be outdated.
    ModifierEnginePtr _completedEngine;
};

/// This macro registers some ModificationNode-derived class as the pipeline node type of some Modifier-derived class.
#define SET_MODIFICATION_NODE_TYPE(ModifierClass, ModificationNodeClass) \
    static const int __modnodeSetter##ModifierClass = (Ovito::ModificationNode::registry().registerModificationNodeType(&ModifierClass::OOClass(), &ModificationNodeClass::OOClass()), 0);

}   // End of namespace

#include "ModifierEvaluationRequest.h"
#include "Modifier.h"


