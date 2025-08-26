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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/controller/LookAtController.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SceneNode);
DEFINE_REFERENCE_FIELD(SceneNode, transformationController);
DEFINE_REFERENCE_FIELD(SceneNode, lookatTargetNode);
DEFINE_REFERENCE_FIELD(SceneNode, pipeline);
DEFINE_VECTOR_REFERENCE_FIELD(SceneNode, children);
DEFINE_RUNTIME_PROPERTY_FIELD(SceneNode, hiddenInViewports);
DEFINE_PROPERTY_FIELD(SceneNode, sceneNodeName);
DEFINE_PROPERTY_FIELD(SceneNode, displayColor);
SET_PROPERTY_FIELD_LABEL(SceneNode, transformationController, "Transformation");
SET_PROPERTY_FIELD_LABEL(SceneNode, lookatTargetNode, "Target");
SET_PROPERTY_FIELD_LABEL(SceneNode, children, "Children");
SET_PROPERTY_FIELD_LABEL(SceneNode, sceneNodeName, "Name");
SET_PROPERTY_FIELD_LABEL(SceneNode, displayColor, "Display color");
SET_PROPERTY_FIELD_LABEL(SceneNode, pipeline, "Pipeline");
SET_PROPERTY_FIELD_CHANGE_EVENT(SceneNode, sceneNodeName, ReferenceEvent::TitleChanged);
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(SceneNode, sceneNodeName, "nodeName"); // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void SceneNode::initializeObject(ObjectInitializationFlags flags)
{
    RefTarget::initializeObject(flags);

    if(!flags.testFlag(DontInitializeObject)) {
        // Assign random color to scene node.
        if(this_task::isInteractive()) {
            static std::default_random_engine rng;
            setDisplayColor(Color::fromHSV(boost::random::uniform_real_distribution<FloatType>()(rng), 1, 1));
        }

        if(!isRootNode()) {
            // Create a transformation controller for the node (the root doesn't need one).
            setTransformationController(ControllerManager::createTransformationController());
        }
    }
}

/******************************************************************************
* Returns this node's world transformation matrix.
* This matrix contains the transformation of the parent node.
******************************************************************************/
const AffineTransformation& SceneNode::getWorldTransform(AnimationTime time, TimeInterval& validityInterval) const
{
    OVITO_ASSERT(this_task::isMainThread());

    if(!_worldTransformValidity.contains(time)) {
        _worldTransformValidity.setInfinite();
        _worldTransform.setIdentity();
        // Get parent node's tm.
        if(parentNode() && !parentNode()->isRootNode()) {
            _worldTransform = _worldTransform * parentNode()->getWorldTransform(time, _worldTransformValidity);
        }
        // Apply own tm.
        if(transformationController())
            transformationController()->applyTransformation(time, _worldTransform, _worldTransformValidity);
    }
    validityInterval.intersect(_worldTransformValidity);
    return _worldTransform;
}

/******************************************************************************
* Returns this node's local transformation matrix.
* This matrix  does not contain the ObjectTransform of this node and
* does not contain the transformation of the parent node.
******************************************************************************/
AffineTransformation SceneNode::getLocalTransform(AnimationTime time, TimeInterval& validityInterval) const
{
    OVITO_ASSERT(this_task::isMainThread());

    AffineTransformation result = AffineTransformation::Identity();
    if(transformationController())
        transformationController()->applyTransformation(time, result, validityInterval);
    return result;
}

/******************************************************************************
* This method marks the world transformation cache as invalid,
* so it will be rebuilt during the next call to GetWorldTransform().
******************************************************************************/
void SceneNode::invalidateWorldTransformation()
{
    OVITO_ASSERT(this_task::isMainThread());

    _worldTransformValidity.setEmpty();
    invalidateBoundingBox();
    for(SceneNode* child : children())
        child->invalidateWorldTransformation();
    notifyDependents(SceneNode::TransformationChanged);
}

/******************************************************************************
* Asks this object to delete itself.
******************************************************************************/
void SceneNode::requestObjectDeletion()
{
    // Delete target scene node too.
    if(OORef<SceneNode> tn = lookatTargetNode()) {
        // Clear reference first to prevent infinite recursion.
        _lookatTargetNode.set(this, PROPERTY_FIELD(lookatTargetNode), nullptr);
        tn->requestObjectDeletion();
    }

    // Delete all child nodes recursively.
    deleteChildren();
    OVITO_ASSERT(children().empty());

    // Delete pipeline if there are no more scene nodes referencing the same pipeline.
    if(OORef<Pipeline> pl = pipeline()) {
        setPipeline(nullptr);
        if(!pl->someSceneNode())
            pl->requestObjectDeletion();
    }

    // Delete scene node itself.
    RefTarget::requestObjectDeletion();
}

/******************************************************************************
* Returns the title of this object.
******************************************************************************/
QString SceneNode::objectTitle() const
{
    // If a user-defined name has been assigned to this scene node, return it as the its display title.
    if(!sceneNodeName().isEmpty())
        return sceneNodeName();

    // Otherwise, use the title of the pipeline.
    if(pipeline())
        return pipeline()->objectTitle();

    // Fall back to default behavior.
    return RefTarget::objectTitle();
}

/******************************************************************************
* Is called whenever one of the child nodes in the tree has generated a AnimationFramesChanged event.
******************************************************************************/
void SceneNode::onAnimationFramesChanged()
{
    if(parentNode())
        parentNode()->onAnimationFramesChanged();
}

/******************************************************************************
* Binds this scene node to a target node and creates a look at controller
* that lets this scene node look at the target. The target will automatically
* be deleted if this scene node is deleted and vice versa.
* Returns the newly created LookAtController assigned as rotation controller for this node.
******************************************************************************/
LookAtController* SceneNode::setLookatTargetNode(AnimationTime time, SceneNode* targetNode)
{
    _lookatTargetNode.set(this, PROPERTY_FIELD(lookatTargetNode), targetNode);

    // Let this node look at the target.
    PRSTransformationController* prs = dynamic_object_cast<PRSTransformationController>(transformationController());
    if(prs) {
        if(targetNode) {
            OVITO_CHECK_OBJECT_POINTER(targetNode);

            // Create a look-at controller.
            OORef<LookAtController> lookAtCtrl = dynamic_object_cast<LookAtController>(prs->rotationController());
            if(!lookAtCtrl)
                lookAtCtrl = OORef<LookAtController>::create();
            lookAtCtrl->setTargetNode(targetNode);

            // Assign it as rotation sub-controller.
            prs->setRotationController(std::move(lookAtCtrl));

            return dynamic_object_cast<LookAtController>(prs->rotationController());
        }
        else {
            // Save old rotation.
            TimeInterval iv;
            Rotation rotation;
            prs->rotationController()->getRotationValue(time, rotation, iv);

            // Reset to default rotation controller.
            OORef<Controller> controller = ControllerManager::createRotationController();
            controller->setRotationValue(time, rotation, true);
            prs->setRotationController(std::move(controller));
        }
    }

    return nullptr;
}

/******************************************************************************
* From RefMaker.
******************************************************************************/
bool SceneNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == transformationController()) {
            // TM has changed -> rebuild world tm cache.
            invalidateWorldTransformation();
        }
        else {
            // The bounding box might have changed if the object has changed.
            invalidateBoundingBox();
        }
    }
    else if(event.type() == ReferenceEvent::TargetDeleted) {
        if(source == pipeline() || source == lookatTargetNode()) {
            // Pipeline or look-at target have been deleted -> delete this node too.
            if(!isUndoingOrRedoing())
                requestObjectDeletion();
        }
    }
    else if(event.type() == ReferenceEvent::AnimationFramesChanged) {
        if(source == pipeline() || children().contains(static_cast<SceneNode*>(source)))
            onAnimationFramesChanged();
    }
    else if(source == pipeline() && event.type() == Pipeline::BoundingBoxChanged) {
        // Mark the cached bounding box of this scene node as invalid.
        invalidateBoundingBox();
    }
    if(source == this->pipeline() && event.type() == ReferenceEvent::TitleChanged && sceneNodeName().isEmpty()) {
        // Forward this event to dependents.
        return true;
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* From RefMaker.
******************************************************************************/
void SceneNode::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(transformationController)) {
        // TM controller has changed -> rebuild world tm cache.
        invalidateWorldTransformation();
    }
    else if(field == PROPERTY_FIELD(children)) {
        // A child node has been replaced.
        SceneNode* oldChild = static_object_cast<SceneNode>(oldTarget);
        OVITO_ASSERT(oldChild->parentNode() == this);
        oldChild->_parentNode = nullptr;

        SceneNode* newChild = static_object_cast<SceneNode>(newTarget);
        OVITO_CHECK_OBJECT_POINTER(newChild);
        OVITO_ASSERT(newChild->parentNode() == nullptr);
        newChild->_parentNode = this;

        // Invalidate cached world bounding box of this parent node.
        invalidateBoundingBox();

        // The animation length might have changed when an object has been removed from the scene.
        onAnimationFramesChanged();
    }
    else if(field == PROPERTY_FIELD(pipeline)) {
        // When the pipeline of the scene node is being replaced, the scene node's title changes.
        if(sceneNodeName().isEmpty())
            notifyDependents(ReferenceEvent::TitleChanged);
    }
    RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* From RefMaker.
******************************************************************************/
void SceneNode::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(children)) {
        // A new child node has been added.
        SceneNode* child = static_object_cast<SceneNode>(newTarget);
        OVITO_CHECK_OBJECT_POINTER(child);
        OVITO_ASSERT(child->parentNode() == nullptr);
        child->_parentNode = this;

        // Invalidate cached world bounding box of this parent node.
        invalidateBoundingBox();

        // The animation length might have changed when an object has been removed from the scene.
        if(!isBeingLoaded())
            onAnimationFramesChanged();
    }
    RefTarget::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* From RefMaker.
******************************************************************************/
void SceneNode::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(children)) {
        // A child node has been removed.
        SceneNode* child = static_object_cast<SceneNode>(oldTarget);
        OVITO_ASSERT(child->parentNode() == this);
        child->_parentNode = nullptr;

        if(!isBeingDeleted()) {
            // Invalidate cached world bounding box of this parent node.
            invalidateBoundingBox();

            // The animation length might have changed when an object has been removed from the scene.
            onAnimationFramesChanged();
        }
    }
    RefTarget::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* This method marks the cached bounding box as invalid,
* so it will be rebuilt during the next call to worldBoundingBox().
******************************************************************************/
void SceneNode::invalidateBoundingBox()
{
    _boundingBoxValidity.setEmpty();
    if(parentNode())
        parentNode()->invalidateBoundingBox();
}

/******************************************************************************
* Adds a child scene node to this node.
******************************************************************************/
void SceneNode::insertChildNode(qsizetype index, OORef<SceneNode> newChild)
{
    OVITO_CHECK_OBJECT_POINTER(newChild);
    OVITO_ASSERT(this_task::isMainThread());

    // Check whether it is already a child of this parent.
    if(newChild->parentNode() == this) {
        OVITO_ASSERT(children().contains(newChild));
        return;
    }

    // Remove new child from old parent node first.
    if(newChild->parentNode()) {
        auto oldIndex = newChild->parentNode()->children().indexOf(newChild);
        newChild->parentNode()->removeChildNode(oldIndex);
    }
    OVITO_ASSERT(newChild->parentNode() == nullptr);

    // Insert into children array of this parent.
    _children.insert(this, PROPERTY_FIELD(children), index, newChild);
    // This node should have been automatically set as the child's parent by referenceInserted().
    OVITO_ASSERT(newChild->parentNode() == this);

    // Adjust transformation to preserve world position.
    TimeInterval iv;
    AnimationTime time = this_task::ui()->datasetContainer().currentAnimationTime();
    const AffineTransformation& newParentTM = getWorldTransform(time, iv);
    if(newParentTM != AffineTransformation::Identity())
        newChild->transformationController()->changeParent(time, AffineTransformation::Identity(), newParentTM, newChild);
    newChild->invalidateWorldTransformation();
}

/******************************************************************************
* Removes a child node from this parent node.
******************************************************************************/
void SceneNode::removeChildNode(qsizetype index)
{
    OVITO_ASSERT(index >= 0 && index < children().size());
    OVITO_ASSERT(this_task::isMainThread());

    OORef<SceneNode> child = children()[index];
    OVITO_ASSERT_MSG(child->parentNode() == this, "SceneNode::removeChildNode()", "The node to be removed is not a child of this parent node.");

    // Remove child node from array.
    _children.remove(this, PROPERTY_FIELD(children), index);
    OVITO_ASSERT(children().contains(child) == false);
    OVITO_ASSERT(child->parentNode() == nullptr);

    // Update child node.
    TimeInterval iv;
    AnimationTime time = this_task::ui()->datasetContainer().currentAnimationTime();
    AffineTransformation oldParentTM = getWorldTransform(time, iv);
    if(oldParentTM != AffineTransformation::Identity())
        child->transformationController()->changeParent(time, oldParentTM, AffineTransformation::Identity(), child);
    child->invalidateWorldTransformation();
}

/******************************************************************************
* Returns true if this node is currently selected.
******************************************************************************/
Scene* SceneNode::scene() const
{
    SceneNode* n = const_cast<SceneNode*>(this);
    do {
        if(n->isRootNode())
            break;
        n = n->parentNode();
    }
    while(n != nullptr);
    return static_object_cast<Scene>(n);
}

/******************************************************************************
* Returns true if this node is currently selected.
******************************************************************************/
bool SceneNode::isSelected() const
{
    if(Scene* sc = scene()) {
        if(sc->selection())
            return sc->selection()->nodes().contains(const_cast<SceneNode*>(this));
    }
    return false;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void SceneNode::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    RefTarget::saveToStream(stream, excludeRecomputableData);

    stream.beginChunk(0x03);
    // Save list of weak references to viewports in which the node is hidden.
    stream.writeSizeT(hiddenInViewports().size());
    for(const auto& vpWeakRef : hiddenInViewports()) {
        stream.saveObject(vpWeakRef.lock().get(), excludeRecomputableData);
    }
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void SceneNode::loadFromStream(ObjectLoadStream& stream)
{
    RefTarget::loadFromStream(stream);

    int version = stream.expectChunkRange(0x01, 2);
    if(version >= 2) {
        // Load list of weak references to viewports in which the node is hidden.
        size_t numHiddenInViewports = stream.readSizeT();
        std::vector<OOWeakRef<Viewport>> viewports;
        for(size_t i = 0; i < numHiddenInViewports; i++) {
            if(OORef<Viewport> vp = stream.loadObject<Viewport>())
                viewports.push_back(std::move(vp));
        }
        setHiddenInViewports(std::move(viewports));
    }
    stream.closeChunk();

    // Restore parent/child hierarchy.
    for(SceneNode* child : children())
        child->_parentNode = this;
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed or changed in a newer version of OVITO.
* This is needed for file backward compatibility with OVITO 3.11.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr SceneNode::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // For backward compatibility with OVITO 3.11:
    if(field.definingClass == &SceneNode::OOClass() && stream.formatVersion() < 30013) {
        // The 'hiddenInViewports' list used to be a vector reference field in previous OVITO versions.
        // Now it is a simple property field holding a vector of weak references to viewports.
        if(field.identifier == "hiddenInViewports") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numHiddenInViewports;
                stream >> numHiddenInViewports;
                std::vector<OOWeakRef<Viewport>> viewports;
                for(qint32 i = 0; i < numHiddenInViewports; i++) {
                    viewports.push_back(stream.loadObject<Viewport>());
                }
                static_object_cast<SceneNode>(&owner)->setHiddenInViewports(std::move(viewports));
                stream.closeChunk();
            };
        }
        // The Pipeline class has been split from the SceneNode base class in OVITO 3.12. This means we have to handle
        // the deserialization of the children field here, which used to be a list of Pipeline objects (now a list of SceneNode instances).
        else if(field.identifier == "children") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numChildren;
                stream >> numChildren;
                for(qint32 i = 0; i < numChildren; i++) {
                    static_object_cast<SceneNode>(&owner)->_children.insert(&owner, PROPERTY_FIELD(SceneNode::children), i, stream.loadObject<Pipeline>()->deserializationSceneNode());
                }
                stream.closeChunk();
            };
        }
    }
    return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);
}

/******************************************************************************
* Creates a copy of this object.
******************************************************************************/
OORef<RefTarget> SceneNode::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<SceneNode> clone = static_object_cast<SceneNode>(RefTarget::clone(deepCopy, cloneHelper));

    // Clone orientation target node too.
    if(clone->lookatTargetNode()) {
        OVITO_ASSERT(lookatTargetNode());

        // Insert the cloned target into the same scene as our target.
        if(lookatTargetNode()->parentNode() && !clone->lookatTargetNode()->parentNode()) {
            lookatTargetNode()->parentNode()->addChildNode(clone->lookatTargetNode());
        }

        // Set new target for look-at controller.
        clone->setLookatTargetNode(AnimationTime(0), clone->lookatTargetNode());
    }

    return clone;
}

/******************************************************************************
* Computes the bounding box of the scene node in local coordinates.
******************************************************************************/
Box3 SceneNode::localBoundingBox(AnimationTime time) const
{
    if(!_boundingBoxValidity.contains(time)) {
        _boundingBoxValidity.setInfinite();
        _localBoundingBox = localBoundingBoxInternal(time, _boundingBoxValidity);
    }
    return _localBoundingBox;
}

/******************************************************************************
* Returns the bounding box of the scene node in world coordinates.
*    time - The time at which the bounding box should be returned.
******************************************************************************/
Box3 SceneNode::worldBoundingBox(AnimationTime time, Viewport* vp) const
{
    OVITO_ASSERT(this_task::isMainThread());

    if(vp && isHiddenInViewport(vp, true))
        return Box3();
    TimeInterval iv;
    const AffineTransformation& tm = getWorldTransform(time, iv);
    Box3 worldBoundingBox = localBoundingBox(time).transformed(tm);
    for(SceneNode* child : children()) {
        worldBoundingBox.addBox(child->worldBoundingBox(time, vp));
    }
    return worldBoundingBox;
}

/******************************************************************************
* Shows/hides this node in the given viewport, i.e. turns rendering on or off.
******************************************************************************/
void SceneNode::setPerViewportVisibility(Viewport* vp, bool visible)
{
    OVITO_ASSERT(vp);
    OVITO_ASSERT(this_task::isMainThread());

    if(visible) {
        for(size_t i = 0; i < hiddenInViewports().size(); i++) {
            if(hiddenInViewports()[i] == vp) {
                auto newHiddenInViewports = hiddenInViewports();
                newHiddenInViewports.erase(newHiddenInViewports.begin() + i);
                // Remove expired weak references from the list.
                std::erase_if(newHiddenInViewports, std::mem_fn(&OOWeakRef<Viewport>::expired));
                setHiddenInViewports(std::move(newHiddenInViewports));
                break;
            }
        }
    }
    else {
        if(boost::find(hiddenInViewports(), vp) == hiddenInViewports().end()) {
            auto newHiddenInViewports = hiddenInViewports();
            newHiddenInViewports.push_back(vp);
            // Remove expired weak references from the list.
            std::erase_if(newHiddenInViewports, std::mem_fn(&OOWeakRef<Viewport>::expired));
            setHiddenInViewports(std::move(newHiddenInViewports));
        }
    }
}

/******************************************************************************
* Returns whether this scene node (or one of its parents in the node hierarchy) has been hidden
* specifically in the given viewport.
******************************************************************************/
bool SceneNode::isHiddenInViewport(const Viewport* vp, bool includeHierarchyParent) const
{
    OVITO_ASSERT(vp);
    OVITO_ASSERT(this_task::isMainThread());

    if(boost::find(hiddenInViewports(), vp) != hiddenInViewports().end())
        return true;
    if(includeHierarchyParent && parentNode())
        return parentNode()->isHiddenInViewport(vp, true);
    else
        return false;
}

}   // End of namespace
