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

#include <ovito/core/Core.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/dataset/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include "RefTarget.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(RefTarget);

/******************************************************************************
* Constructor.
******************************************************************************/
RefTarget::RefTarget(ObjectCreationParams params) 
{
	// A Qt application object must exist.
	OVITO_ASSERT_MSG(QCoreApplication::instance() != nullptr, "RefTarget::RefTarget()", "Creating an instance of a RefTarget-derived class is only allowed while a Qt application object exists.");

	// Ovito objects always live in the main thread.
	moveToThread(QCoreApplication::instance()->thread());
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
RefTarget::~RefTarget() 
{
	// Make sure there are no more dependents left.
	static const QMetaMethod objectEventSignal = QMetaMethod::fromSignal(&RefTarget::objectEvent);
	OVITO_ASSERT_MSG(!isSignalConnected(objectEventSignal), "RefTarget destructor", "RefTarget object has not been correctly deleted. It still has dependents left.");
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
	OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "RefTarget::aboutToBeDeleted()", "This function may only be called from the main thread.");

	// Make sure undo recording is not active while deleting the object from memory.
	UndoSuspender noUndo;

	// This will remove all references to this target object.
	notifyDependents(ReferenceEvent::TargetDeleted);

	// Delete object from memory.
	RefMaker::aboutToBeDeleted();
}

/******************************************************************************
* Asks this object to delete itself.
******************************************************************************/
void RefTarget::deleteReferenceObject()
{
	OVITO_CHECK_OBJECT_POINTER(this);

	// This will remove all references to this target object.
	notifyDependents(ReferenceEvent::TargetDeleted);

	// At this point, the object might have been deleted from memory if its
	// reference counter has reached zero. If undo recording was enabled, however,
	// the undo record still holds a reference to this object and it will still be alive.
}

/******************************************************************************
* Notifies all registered dependents by sending out a message.
******************************************************************************/
void RefTarget::notifyDependentsImpl(const ReferenceEvent& event)
{
	OVITO_CHECK_OBJECT_POINTER(this);

	// If reference count is zero, then there cannot be any dependents.
	if(objectReferenceCount() == 0) {
#ifdef OVITO_DEBUG
		// Verify there are no dependents.
		static const QMetaMethod objectEventSignal = QMetaMethod::fromSignal(&RefTarget::objectEvent);
		OVITO_ASSERT(!isSignalConnected(objectEventSignal));
#endif
		return;
	}

	// Prevent this object from being deleted while emitting the event signal.
	OORef<RefTarget> this_(this);

#ifdef OVITO_QML_GUI
	// Emit Qt signals for certain events.
	if(event.type() == ReferenceEvent::TitleChanged)
		Q_EMIT objectTitleChanged();
#endif

	// Send the signal to the registered dependents.
	Q_EMIT objectEvent(this, event);

	OVITO_CHECK_OBJECT_POINTER(this);
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
	// Create a new instance of the object's class.
	// Note: Calling low-level method createInstanceImpl() instead of createInstanceImpl() here to avoid initialization of
	// object parameters to default values. Default initialization is not needed when cloning an object.
	OORef<RefTarget> clone = static_object_cast<RefTarget>(getOOClass().createInstanceImpl(ObjectCreationParams(ObjectCreationParams::DontCreateSubObjects)));
	OVITO_ASSERT(clone);
	OVITO_ASSERT(clone->getOOClass().isDerivedFrom(getOOClass()));
	if(!clone || !clone->getOOClass().isDerivedFrom(getOOClass()))
		throw Exception(tr("Failed to create clone instance of class %1.").arg(getOOClass().name()));

	// Clone properties and referenced objects.
	for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
		if(field->isReferenceField()) {
			if(!field->isVector()) {
				OVITO_ASSERT(field->_singleReferenceReadFunc != nullptr);
				OVITO_ASSERT(field->_singleReferenceWriteFuncRef != nullptr);
				const RefTarget* originalTarget = field->_singleReferenceReadFunc(this);
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
				field->_singleReferenceWriteFuncRef(clone, std::move(clonedReference));
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
					field->_vectorReferenceInsertFunc(clone, i, std::move(clonedReference));
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
* Flags this object when it is opened in an editor.
******************************************************************************/
void RefTarget::setObjectEditingFlag()
{
	// Increment counter.
	QVariant oldValue = property("OVITO_OBJECT_EDIT_COUNTER");
	setProperty("OVITO_OBJECT_EDIT_COUNTER", oldValue.toInt() + 1);
}

/******************************************************************************
* Unflags this object when it is no longer opened in an editor.
******************************************************************************/
void RefTarget::unsetObjectEditingFlag()
{
	// Decrement counter.
	QVariant oldValue = property("OVITO_OBJECT_EDIT_COUNTER");
	OVITO_ASSERT(oldValue.toInt() > 0);
	if(oldValue.toInt() == 1)
		setProperty("OVITO_OBJECT_EDIT_COUNTER", QVariant());
	else
		setProperty("OVITO_OBJECT_EDIT_COUNTER", oldValue.toInt() - 1);
}

/******************************************************************************
* Determines if this object's properties are currently being edited in an editor.
******************************************************************************/
bool RefTarget::isObjectBeingEdited() const
{
	return (property("OVITO_OBJECT_EDIT_COUNTER").toInt() != 0);
}

/******************************************************************************
* Returns an executor object to be used with Future<>::then(), which executes work
* in the context (and the thread) of this object.
******************************************************************************/
RefTargetExecutor RefTarget::executor(bool requireDeferredExecution) const
{
	return RefTargetExecutor(this, requireDeferredExecution);
}

}	// End of namespace
