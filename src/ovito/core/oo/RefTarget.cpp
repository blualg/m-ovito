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

#include <ovito/core/Core.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "RefTarget.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(RefTarget);

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
RefTarget::~RefTarget()
{
    // Make sure there are no more dependents left.
    OVITO_ASSERT_MSG(_dependents.empty(), "RefTarget destructor", "RefTarget object has not been correctly deleted. It still has dependents left.");
}
#endif

/******************************************************************************
* This method is called when the reference counter of this OvitoObject
* has reached zero.
******************************************************************************/
void RefTarget::aboutToBeDeleted()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(this->__isObjectAlive());
    OVITO_ASSERT(isBeingDeleted());

    // No dependents should be left at this point.
#ifdef OVITO_DEBUG
    if(!_dependents.empty()) {
        qDebug() << "The" << this << "being deleted still has dependents:";
        _dependents.visit([&](RefMaker* dependent) {
            qDebug() << " -" << dependent;
        });
        OVITO_ASSERT(false);
    }
#endif

    // Make sure no undo recording occurs while deleting the object.
#ifdef OVITO_DEBUG
    int oldCount = CompoundOperation::current() ? CompoundOperation::current()->count() : 0;
#endif

    // Clears all references this RefMaker has to other objects.
    RefMaker::aboutToBeDeleted();

    // Verify that no undo records have been put on the stack during object destruction.
#ifdef OVITO_DEBUG
    OVITO_ASSERT(!CompoundOperation::current() || CompoundOperation::current()->count() == oldCount);
#endif
}

/******************************************************************************
* Asks this object to delete itself.
******************************************************************************/
void RefTarget::requestObjectDeletion()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT_MSG(this_task::isMainThread(), "RefTarget::requestObjectDeletion()", "This function may only be called from the main thread.");
    OVITO_ASSERT(!isBeingInitialized());
    OVITO_ASSERT(!isBeingDeleted());

    // This will remove all references other RefMakers have to this target object.
    notifyDependents(ReferenceEvent::TargetDeleted);

    // At this point, the object might have been deleted from memory if its
    // reference count has reached zero. If undo recording was enabled, however,
    // the undo record still holds a reference to this object to keep it alive.
}

/******************************************************************************
* Notifies all registered dependents by sending out a message.
******************************************************************************/
void RefTarget::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    OVITO_CHECK_OBJECT_POINTER(this);

    // Suppress notification events during object construction and destruction.
    if(isBeingInitializedOrDeleted())
        return;

    // Prevent this object from being deleted while sending the notification event.
    OORef<RefTarget> self(this);

    // Send the signal to the registered dependents.
    _dependents.visit([&](RefMaker* dependent) {
        dependent->handleReferenceEvent(this, event);
    });
}

/******************************************************************************
* Handles a change notification message from a RefTarget.
* This implementation calls the onRefTargetMessage method
* and passes the message on to dependents of this RefTarget.
******************************************************************************/
bool RefTarget::handleReferenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    // Let this object process the message.
    if(!RefMaker::handleReferenceEvent(source, event))
        return false;

    // Pass event on to dependents of this RefTarget if our handleReferenceEvent() method has requested it.
    notifyDependentsImpl(event);

    return true;
}

/******************************************************************************
* Checks if this object is directly or indirectly referenced by the given RefMaker.
******************************************************************************/
bool RefTarget::isReferencedBy(const RefMaker* obj, bool onlyStrongReferences) const
{
    if(this == obj)
        return true;
    if(isBeingInitialized())
        return false;
    CheckIsReferencedByEvent event(const_cast<RefTarget*>(this), obj, onlyStrongReferences);
    const_cast<RefTarget*>(this)->notifyDependentsImpl(event);
    return event.isReferenced();
}

/******************************************************************************
* Creates a copy of this RefTarget object.
* If deepCopy is true, then all objects referenced by this RefTarget should be copied too.
* This copying should be done via the passed CloneHelper instance.
* Classes that override this method MUST call the base class' version of this method
* to create an instance. The base implementation of RefTarget::clone() will create an
* instance of the derived class which can safely be cast.
******************************************************************************/
OORef<RefTarget> RefTarget::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Make sure CloneHelper has suspended undo recording.
    OVITO_ASSERT(CompoundOperation::isUndoRecording() == false);

    // Create a new instance of the object's class.
    // Note: Calling low-level method createInstanceImpl() instead of createInstance() here to avoid initialization of
    // object parameters to default values. Parameter initialization is not needed when cloning an object.
    OORef<RefTarget> clone = static_object_cast<RefTarget>(getOOClass().createInstanceImpl(ObjectInitializationFlag::DontInitializeObject));
    if(!clone)
        throw Exception(tr("Failed to create clone instance of class %1.").arg(getOOClass().name()));
    OVITO_ASSERT(clone->getOOClass().isDerivedFrom(getOOClass()));

    // Set being copied flag of the object.
    clone->setIsBeingCopied(true);

    // Clone properties and referenced objects.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(field->isReferenceField()) {
            if(!field->isVector()) {
                OVITO_ASSERT(field->_singleReferenceReadFunc != nullptr);
                OVITO_ASSERT(field->_singleReferenceWriteFuncRef != nullptr);
                const RefTarget* originalTarget = getReferenceFieldTarget(field);
                // Clone reference target.
                OORef<RefTarget> clonedReference;
                if(field->flags().testFlag(PROPERTY_FIELD_NEVER_CLONE_TARGET))
                    clonedReference = originalTarget;
                else if(field->flags().testFlag(PROPERTY_FIELD_ALWAYS_CLONE))
                    clonedReference = cloneHelper.cloneObject(originalTarget, deepCopy);
                else if(field->flags().testFlag(PROPERTY_FIELD_ALWAYS_DEEP_COPY))
                    clonedReference = cloneHelper.cloneObject(originalTarget, true);
                else
                    clonedReference = cloneHelper.copyReference(originalTarget, deepCopy);
                // Store in reference field of destination object.
                field->_singleReferenceWriteFuncRef(clone, field, std::move(clonedReference));
            }
            else {
                // Remove any preexisting references from the field of the cloned object.
                clone->clearReferenceField(field);

                // Clone all reference targets in the source vector.
                int count = getVectorReferenceFieldSize(field);
                for(int i = 0; i < count; i++) {
                    const RefTarget* originalTarget = getVectorReferenceFieldTarget(field, i);
                    OORef<RefTarget> clonedReference;
                    // Clone reference target.
                    if(field->flags().testFlag(PROPERTY_FIELD_NEVER_CLONE_TARGET))
                        clonedReference = originalTarget;
                    else if(field->flags().testFlag(PROPERTY_FIELD_ALWAYS_CLONE))
                        clonedReference = cloneHelper.cloneObject(originalTarget, deepCopy);
                    else if(field->flags().testFlag(PROPERTY_FIELD_ALWAYS_DEEP_COPY))
                        clonedReference = cloneHelper.cloneObject(originalTarget, true);
                    else
                        clonedReference = cloneHelper.copyReference(originalTarget, deepCopy);
                    // Store in reference field of destination object.
                    field->_vectorReferenceInsertFunc(clone, field, i, std::move(clonedReference));
                }
            }
        }
        else {
            // Just copy stored value for property fields.
            clone->copyPropertyFieldValue(field, *this);
        }
    }

    return clone;
}

/******************************************************************************
* Returns the title of this object.
******************************************************************************/
QString RefTarget::objectTitle() const
{
    return getOOClass().displayName();
}

/******************************************************************************
* Returns whether this object is currently opened in a parameter editor in the UI.
******************************************************************************/
bool RefTarget::isBeingEdited() const
{
    // Look up the PropertiesEditor class, which is defined in the GUI plugin module.
    // That means it is not accessible at compile time here from the Core module.
    // Note: The GUI module and the PropertiesEditor class may not be present if OVITO is built without GUI support.
    static const OvitoClassPtr propertiesEditorClass = PluginManager::instance().findClass("Gui", "PropertiesEditor");

    bool result = false;
    if(propertiesEditorClass) {
        // Check if this object is currently referenced by a PropertiesEditor instance.
        visitDependents([&](const RefMaker* dependent) {
            if(propertiesEditorClass->isMember(dependent))
                result = true;
        });
    }
    return result;
}

}   // End of namespace
