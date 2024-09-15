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
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/PipelineCache.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

/**
 * \brief A visual scene node representing a data pipeline.
 */
class OVITO_CORE_EXPORT Pipeline : public SceneNode
{
    /// Give this class its own metaclass.
    class OVITO_CORE_EXPORT PipelineClass : public SceneNode::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using SceneNode::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes are of the deserialization of a serialized property field that has been removed from the class.
        /// This is needed for backward compatibility with OVITO 3.11.
        virtual SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const override;
    };
    OVITO_CLASS_META(Pipeline, PipelineClass)

public:

    /// Destructor.
    virtual ~Pipeline();

    /// Throws an exception if the pipeline stage cannot be evaluated at this time.
    /// This is called by the system to catch user mistakes that would lead to infinite recursion.
    void preEvaluationCheck(const PipelineEvaluationRequest& request) const;

    /// Performs an asynchronous evaluation of the data pipeline.
    [[nodiscard]] PipelineEvaluationResult evaluatePipeline(const PipelineEvaluationRequest& request) {
        return _pipelineCache.evaluatePipeline(request);
    }

    /// Returns the cached output of the data pipeline at the given time if available.
    /// This method will never throw an exception and doesn't require a valid execution context.
    PipelineFlowState getCachedPipelineOutput(AnimationTime time, bool interactiveMode = true) const {
        return _pipelineCache.getAt(time, interactiveMode);
    }

    /// Sets the data source of this pipeline, i.e., the object that provides the
    /// input data entering the pipeline.
    void setSource(PipelineNode* sourceObject);

    /// \brief Applies a modifier by appending a node for it to the pipeline.
    ModificationNode* applyModifier(AnimationTime time, bool interactiveMode, Modifier* modifier);

    /// \brief Returns the title of this object.
    virtual QString objectTitle() const override;

    /// \brief Deletes this node from the scene.
    virtual void deleteSceneNode() override;

    /// Rescales the times of all animation keys from the old animation interval to the new interval.
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

    /// \brief Replaces the given visual element in this pipeline's output with an independent copy.
    DataVis* makeVisElementIndependent(DataVis* visElement);

    /// Returns the internal replacement for the given data vis element.
    /// If there is no replacement, the original vis element is returned.
    DataVis* getReplacementVisElement(DataVis* vis) const;

    /// Gathers a list of data objects from the given pipeline flow state (which should have been produced by this pipeline)
    /// that are associated with the given vis element. This method takes into account replacement vis elements of this pipeline node.
    std::vector<ConstDataObjectPath> getDataObjectsForVisElement(const PipelineFlowState& state, DataVis* vis) const;

protected:

    /// This method is called when a referenced object has changed.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
    virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) override;

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// Computes the scene node's local bounding box.
    virtual Box3 localBoundingBoxInternal(AnimationTime time, TimeInterval& validity) const override;

private:

    /// Invalidates the contents of the data pipeline cache.
    void invalidatePipelineCache(TimeInterval keepInterval = TimeInterval::empty());

    /// Rebuilds the list of visual elements maintained by the scene node.
    void updateVisElementList(const PipelineFlowState& state);

    /// Determines the current source of the data pipeline and updates the internal weak reference field.
    void updatePipelineSourceReference();

    /// Helper function that recursively collects all visual elements attached to a
    /// data object and its children and stores them in an output vector.
    static void collectVisElements(const DataObject* dataObj, std::vector<DataVis*>& visElements);

    /// Helper function that recursively finds all data objects which the given vis element is associated with.
    void collectDataObjectsForVisElement(ConstDataObjectPath& path, DataVis* vis, std::vector<ConstDataObjectPath>& dataObjectPaths) const;

    /// Computes the bounding box of a data object and all its sub-objects.
    void getDataObjectBoundingBox(AnimationTime time, const DataObject* dataObj, const PipelineFlowState& state, TimeInterval& validity, Box3& bb, ConstDataObjectPath& dataObjectPath) const;

    /// The terminal node of the pipeline that generates the data to be rendered in the scene.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PipelineNode>, head, setHead);

    /// The transient list of visual elements that render the pipeline's output data objects in the viewports.
    /// This list is for internal caching purposes only and is rebuilt every time the pipeline is newly evaluated.
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<DataVis>, visElements, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// List of visual elements coming from the pipeline which shall be replaced with independent versions owned by this pipeline.
    DECLARE_RUNTIME_PROPERTY_FIELD(std::vector<OOWeakRef<DataVis>>{}, replacedVisElements, setReplacedVisElements);

    /// Visual elements owned by the pipeline itself, which replace the ones generated within the pipeline.
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<DataVis>, replacementVisElements, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Activates the precomputation of the pipeline results for all animation frames.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, pipelineTrajectoryCachingEnabled, setPipelineTrajectoryCachingEnabled, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// Reference to the pipeline's source node.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<PipelineNode>, source, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

    /// Enables or disables InteractiveStateAvailable signals from the pipeline in order to refresh the viewports each time partial computation results become available.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, preliminaryUpdatesEnabled, setPreliminaryUpdatesEnabled, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// The cached output of the data pipeline (without the effect of visualization elements).
    PipelineCache _pipelineCache{this};

    friend class PipelineCache;
};

}   // End of namespace
