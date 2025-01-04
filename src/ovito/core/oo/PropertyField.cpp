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
#include <ovito/core/oo/PropertyFieldDescriptor.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/DataObject.h>
#include "PropertyField.h"

namespace Ovito {

#ifdef OVITO_DEBUG
/******************************************************************************
* Verifies that owner is of the right type, i.e., defines the given property field.
******************************************************************************/
bool PropertyFieldBase::ownerTypeCheck(RefMaker* owner, const PropertyFieldDescriptor* descriptor)
{
    OVITO_CHECK_OBJECT_POINTER(owner);
    return descriptor->definingClass()->isMember(owner);
}
#endif

/******************************************************************************
* Generates a notification event to inform the dependents of the field's owner
* that it has changed.
******************************************************************************/
void PropertyFieldBase::generateTargetChangedEvent(RefMaker* owner, const PropertyFieldDescriptor* descriptor, int eventType)
{
    // Make sure we are not trying to generate a change message for objects that are not RefTargets.
    OVITO_ASSERT_MSG(!descriptor->shouldGenerateChangeEvent() || descriptor->definingClass()->isDerivedFrom(RefTarget::OOClass()),
            "PropertyFieldBase::generateTargetChangedEvent()",
            qPrintable(QString("Flag PROPERTY_FIELD_NO_CHANGE_MESSAGE has not been set for property field '%1' of class '%2' even though '%2' is not derived from RefTarget.")
                    .arg(descriptor->identifier()).arg(descriptor->definingClass()->name())));

    // Suppress all change messages while the owner object is being initialized or destroyed.
    if(owner->isBeingInitializedOrDeleted())
        return;

    if(descriptor->definingClass()->isDerivedFrom(DataObject::OOClass())) {
        // Change events only need to be sent by a DataObject if the object
        // is not shared by multiple owners and if we are in the main thread.
        // This is a performance optimization to avoid sending change events unnecessarily
        // in situations where they certainly don't matter.
        if(this_task::isMainThread() == false)
            return;
        if(static_object_cast<DataObject>(owner)->dataReferenceCount() > 1) // Note: Using dataReferenceCount() instead of isSafeToModify() here is a performance optimization.
            return;
    }

    // Send notification message to dependents of owner object.
    if(eventType != ReferenceEvent::TargetChanged) {
        OVITO_ASSERT(RefTarget::OOClass().isMember(owner));
        static_object_cast<RefTarget>(owner)->notifyDependents(eventType);
    }
    else if(descriptor->shouldGenerateChangeEvent() && !owner->isBeingDeleted()) {
        OVITO_ASSERT(RefTarget::OOClass().isMember(owner));
        static_object_cast<RefTarget>(owner)->notifyTargetChanged(descriptor);
    }
}

/******************************************************************************
* Generates a notification event to inform the dependents of the field's owner
* that it has changed.
******************************************************************************/
void PropertyFieldBase::generatePropertyChangedEvent(RefMaker* owner, const PropertyFieldDescriptor* descriptor)
{
    owner->propertyChanged(descriptor);
}

/******************************************************************************
* Constructor.
******************************************************************************/
PropertyFieldBase::PropertyFieldOperation::PropertyFieldOperation(RefMaker* owner, const PropertyFieldDescriptor* descriptor) :
    _owner(!DataSet::OOClass().isMember(owner) ? owner : nullptr), _descriptor(descriptor)
{
}

/******************************************************************************
* Access to the object whose property was changed.
******************************************************************************/
RefMaker* PropertyFieldBase::PropertyFieldOperation::owner() const
{
    return static_cast<RefMaker*>(_owner.get());
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
template<typename T> SingleReferenceFieldBase<T>::~SingleReferenceFieldBase()
{
}
#endif

/******************************************************************************
* Replaces the current reference target with a new target. Handles undo recording.
******************************************************************************/
template<typename T> void SingleReferenceFieldBase<T>::set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, pointer newTarget)
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));

    if(_target == newTarget)
        return; // Nothing to change.

    // Check object type
    if(newTarget && !newTarget->getOOClass().isDerivedFrom(*descriptor->targetClass())) {
        OVITO_ASSERT_MSG(false, "SingleReferenceFieldBase::set()", "Tried to create a reference to an incompatible object for this reference field.");
        throw Exception(QStringLiteral("Cannot set a reference field of type %1 to an incompatible object of type %2.").arg(descriptor->targetClass()->name(), newTarget->getOOClass().name()));
    }

    // Make sure automatic undo is disabled for a reference field of a class that is not derived from RefTarget.
    OVITO_ASSERT_MSG(descriptor->automaticUndo() == false || RefTarget::OOClass().isMember(owner), "SingleReferenceFieldBase::set()",
            qPrintable(QStringLiteral("PROPERTY_FIELD_NO_UNDO flag has not been set for reference field '%1' of non-RefTarget derived class '%2'.")
                .arg(descriptor->identifier()).arg(descriptor->definingClass()->name())));

    class SetReferenceOperation final : public PropertyFieldOperation
    {
    private:

        /// The reference target that is currently not assigned to the reference field.
        /// This is stored here so that we can restore it on a call to undo().
        pointer _inactiveTarget;

        /// The reference field whose value has changed.
        SingleReferenceFieldBase& _reffield;

    public:

        SetReferenceOperation(RefMaker* owner, pointer oldTarget, SingleReferenceFieldBase& reffield, const PropertyFieldDescriptor* descriptor) :
            PropertyFieldOperation(owner, descriptor), _inactiveTarget(std::move(oldTarget)), _reffield(reffield) {}

        virtual void undo() override final {
            _reffield.swapReference(owner(), descriptor(), _inactiveTarget);
        }

        virtual QString displayName() const override final {
                return QStringLiteral("Setting reference field <%1> of %2 to point to %3")
                    .arg(descriptor()->identifier())
                    .arg(owner()->getOOClass().name())
                    .arg(_inactiveTarget ? _inactiveTarget->getOOClass().name() : "<null>");
        }
    };

    if(descriptor->automaticUndo() && !owner->isBeingInitializedOrDeleted() && CompoundOperation::isUndoRecording()) {
        auto op = std::make_unique<SetReferenceOperation>(owner, std::move(newTarget), *this, descriptor);
        op->redo();
        CompoundOperation::current()->addOperation(std::move(op));
    }
    else {
        swapReference(owner, descriptor, newTarget);
    }
}

/******************************************************************************
* Replaces the target stored in the reference field.
******************************************************************************/
template<typename T> void SingleReferenceFieldBase<T>::swapReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, pointer& inactiveTarget)
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));
    OVITO_ASSERT(!descriptor->isVector());
    OVITO_ASSERT(inactiveTarget != _target);

    // Check for cyclic references.
    if(inactiveTarget && owner->isReferencedBy(inactiveTarget, true))
        throw CyclicReferenceError();

    // Move the old pointer value into a local temporary.
    pointer oldTarget = std::exchange(_target, nullptr);
    OVITO_ASSERT(!_target);

    // Disconnect from the old target, but only if the dependent has no other references to the old target.
    if(oldTarget && !owner->hasReferenceTo(oldTarget)) {
        oldTarget->unregisterDependent(owner);
    }

    // Exchange pointer values.
    _target = std::move(inactiveTarget);
    inactiveTarget = std::move(oldTarget);

    // Register the dependent with the newly referenced object (only if it isn't already registered).
    if(_target)
        _target->registerDependent(owner);

    // Inform owner object about the changed reference value.
    owner->referenceReplaced(descriptor,
        const_cast<RefTarget*>(static_cast<const RefTarget*>(inactiveTarget.get())),
        const_cast<RefTarget*>(static_cast<const RefTarget*>(_target.get())),
        -1);

    // Emit object-changed signal.
    generateTargetChangedEvent(owner, descriptor);

    // Emit additional signal if SET_PROPERTY_FIELD_CHANGE_EVENT macro was used for this property field.
    if(descriptor->extraChangeEventType() != 0)
        generateTargetChangedEvent(owner, descriptor, descriptor->extraChangeEventType());
}

// Instantiate base class template for the fancy pointer base types needed.
#if defined(Q_CC_MSVC) || defined(Q_CC_CLANG) || defined(OVITO_BUILD_MONOLITHIC)
    template class OVITO_CORE_EXPORT SingleReferenceFieldBase<OORef<RefTarget>>;
    template class OVITO_CORE_EXPORT SingleReferenceFieldBase<DataOORef<const DataObject>>;
#endif

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
template<typename T> VectorReferenceFieldBase<T>::~VectorReferenceFieldBase()
{
}
#endif

/******************************************************************************
* Replaces the i-th reference target with a new target. Handles undo recording.
******************************************************************************/
template<typename T> void VectorReferenceFieldBase<T>::set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, pointer newTarget)
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));

    OVITO_ASSERT(i >= 0 && i < size());
    if(_targets[i] == newTarget)
        return; // Nothing to change.

    // Check object type
    if(newTarget && !newTarget->getOOClass().isDerivedFrom(*descriptor->targetClass())) {
        OVITO_ASSERT_MSG(false, "VectorReferenceFieldBase::set()", "Tried to create a reference to an incompatible object for this reference field.");
        throw Exception(QString("Cannot set a reference field of type %1 to an incompatible object of type %2.").arg(descriptor->targetClass()->name(), newTarget->getOOClass().name()));
    }

    // Make sure automatic undo is disabled for a reference field of a class that is not derived from RefTarget.
    OVITO_ASSERT_MSG(descriptor->automaticUndo() == false || RefTarget::OOClass().isMember(owner), "VectorReferenceFieldBase::set()",
            qPrintable(QString("PROPERTY_FIELD_NO_UNDO flag has not been set for reference field '%1' of non-RefTarget derived class '%2'.")
                .arg(descriptor->identifier()).arg(descriptor->definingClass()->name())));

    class SetReferenceOperation final : public PropertyFieldOperation
    {
    private:

        /// The reference target that is currently not assigned to the reference field.
        /// This is stored here so that we can restore it on a call to undo().
        pointer _inactiveTarget;

        /// The vector field index being replaced.
        size_type _index;

        /// The reference field whose value has changed.
        VectorReferenceFieldBase& _reffield;

    public:

        SetReferenceOperation(RefMaker* owner, pointer oldTarget, size_type i, VectorReferenceFieldBase& reffield, const PropertyFieldDescriptor* descriptor) :
            PropertyFieldOperation(owner, descriptor), _inactiveTarget(std::move(oldTarget)), _index(i), _reffield(reffield) {}

        virtual void undo() override final {
            _reffield.swapReference(owner(), descriptor(), _index, _inactiveTarget);
        }

        virtual QString displayName() const override final {
                return QStringLiteral("Setting entry %1 of vector reference field <%2> of %3 to point to %4")
                    .arg(_index)
                    .arg(descriptor()->identifier())
                    .arg(owner()->getOOClass().name())
                    .arg(_inactiveTarget ? _inactiveTarget->getOOClass().name() : "<null>");
        }
    };

    if(descriptor->automaticUndo() && !owner->isBeingInitializedOrDeleted() && CompoundOperation::isUndoRecording()) {
        auto op = std::make_unique<SetReferenceOperation>(owner, std::move(newTarget), i, *this, descriptor);
        op->redo();
        CompoundOperation::current()->addOperation(std::move(op));
    }
    else {
        swapReference(owner, descriptor, i, newTarget);
    }
}

/******************************************************************************
* Inserts or add a reference target to the internal list.
******************************************************************************/
template<typename T> auto VectorReferenceFieldBase<T>::insert(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, pointer newTarget) -> size_type
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));

    // Check object type
    if(newTarget && !newTarget->getOOClass().isDerivedFrom(*descriptor->targetClass())) {
        OVITO_ASSERT_MSG(false, "VectorReferenceFieldBase::insert()", "Cannot add incompatible object to this vector reference field.");
        throw Exception(QString("Cannot add an object to a reference field of type %1 that has the incompatible type %2.").arg(descriptor->targetClass()->name(), newTarget->getOOClass().name()));
    }

    // Make sure automatic undo is disabled for a reference field of a class that is not derived from RefTarget.
    OVITO_ASSERT_MSG(descriptor->automaticUndo() == false || RefTarget::OOClass().isMember(owner), "VectorReferenceFieldBase::insert()",
            qPrintable(QString("PROPERTY_FIELD_NO_UNDO flag has not been set for reference field '%1' of non-RefTarget derived class '%2'.")
                    .arg(descriptor->identifier()).arg(descriptor->definingClass()->name())));

    class InsertReferenceOperation final : public PropertyFieldOperation
    {
    private:

        /// The target that has been added into the vector reference field.
        pointer _target;

        /// The position at which the target has been inserted into the vector reference field.
        size_type _index;

        /// The vector reference field to which the reference has been added.
        VectorReferenceFieldBase& _reffield;

    public:

        InsertReferenceOperation(RefMaker* owner, pointer target, size_type index, VectorReferenceFieldBase& reffield, const PropertyFieldDescriptor* descriptor) :
            PropertyFieldOperation(owner, descriptor), _target(std::move(target)), _reffield(reffield), _index(index) {}

        virtual void undo() override final {
            OVITO_ASSERT(!_target);
            _reffield.removeReference(owner(), descriptor(), _index, _target);
        }

        virtual void redo() override final {
            _index = _reffield.addReference(owner(), descriptor(), _index, _target);
            OVITO_ASSERT(!_target);
        }

        size_type insertionIndex() const { return _index; }

        virtual QString displayName() const override final {
            return QStringLiteral("Insert reference to %1 into vector field <%2> of %3")
                .arg(_target ? _target->getOOClass().name() : "<null>")
                .arg(descriptor()->identifier())
                .arg(owner()->getOOClass().name());
        }
    };

    if(descriptor->automaticUndo() && !owner->isBeingInitializedOrDeleted() && CompoundOperation::isUndoRecording()) {
        auto op = std::make_unique<InsertReferenceOperation>(owner, std::move(newTarget), i, *this, descriptor);
        op->redo();
        int index = op->insertionIndex();
        CompoundOperation::current()->addOperation(std::move(op));
        return index;
    }
    else {
        return addReference(owner, descriptor, i, newTarget);
    }
}

/******************************************************************************
* Removes the element at index position i.
* Creates an undo record so the removal can be undone at a later time.
******************************************************************************/
template<typename T> T VectorReferenceFieldBase<T>::remove(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i)
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));
    OVITO_ASSERT(i >= 0 && i < size());

    // Make sure automatic undo is disabled for a reference field of a class that is not derived from RefTarget.
    OVITO_ASSERT_MSG(descriptor->automaticUndo() == false || RefTarget::OOClass().isMember(owner), "VectorReferenceFieldBase::remove()",
            qPrintable(QString("PROPERTY_FIELD_NO_UNDO flag has not been set for reference field '%1' of non-RefTarget derived class '%2'.")
                    .arg(descriptor->identifier()).arg(descriptor->definingClass()->name())));

    class RemoveReferenceOperation final : public PropertyFieldOperation
    {
    private:

        /// The target that has been removed from the vector reference field.
        pointer _target{};

        /// The position at which the target has been removed from the vector reference field.
        size_type _index;

        /// The vector reference field from which the reference has been removed.
        VectorReferenceFieldBase& _reffield;

    public:

        RemoveReferenceOperation(RefMaker* owner, size_type index, VectorReferenceFieldBase& reffield, const PropertyFieldDescriptor* descriptor) :
            PropertyFieldOperation(owner, descriptor), _reffield(reffield), _index(index) {}

        const pointer& storedTarget() const { return _target; }

        virtual void undo() override final {
            _index = _reffield.addReference(owner(), descriptor(), _index, _target);
            OVITO_ASSERT(!_target);
        }

        virtual void redo() override final {
            OVITO_ASSERT(!_target);
            _reffield.removeReference(owner(), descriptor(), _index, _target);
        }

        virtual QString displayName() const override final {
            return QStringLiteral("Remove reference to %1 from vector field <%2> of %3")
                .arg(_target ? _target->getOOClass().name() : "<null>")
                .arg(descriptor()->identifier())
                .arg(owner()->getOOClass().name());
        }
    };

    if(descriptor->automaticUndo() && !owner->isBeingInitializedOrDeleted() && CompoundOperation::isUndoRecording()) {
        auto op = std::make_unique<RemoveReferenceOperation>(owner, i, *this, descriptor);
        op->redo();
        pointer removedReference = op->storedTarget();
        CompoundOperation::current()->addOperation(std::move(op));
        return removedReference;
    }
    else {
        pointer removedReference;
        removeReference(owner, descriptor, i, removedReference);
        return removedReference;
    }
}

/******************************************************************************
* Clears all references and sets the vector size to zero.
******************************************************************************/
template<typename T> void VectorReferenceFieldBase<T>::clear(RefMaker* owner, const PropertyFieldDescriptor* descriptor)
{
    while(!_targets.empty())
        remove(owner, descriptor, _targets.size() - 1);
}

/******************************************************************************
* Replaces the i-th target stored in the vector reference field.
******************************************************************************/
template<typename T> void VectorReferenceFieldBase<T>::swapReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& inactiveTarget)
{
    OVITO_CHECK_POINTER(this);
    OVITO_CHECK_OBJECT_POINTER(owner);
    OVITO_ASSERT(descriptor->isVector());

    // Check for cyclic strong references.
    if(inactiveTarget && owner->isReferencedBy(inactiveTarget, true))
        throw CyclicReferenceError();

    // Move the old pointer value into a local temporary.
    pointer oldTarget = std::exchange(_targets[index], nullptr);
    OVITO_ASSERT(!_targets[index]);

    // Disconnect from the old target, but only if the dependent has no other references to the old target.
    if(oldTarget && !owner->hasReferenceTo(oldTarget))
        oldTarget->unregisterDependent(owner);

    // Exchange pointer values.
    _targets[index] = std::move(inactiveTarget);
    inactiveTarget = std::move(oldTarget);

    // Register the dependent with the newly referenced object (only if it isn't already registered).
    if(_targets[index])
        _targets[index]->registerDependent(owner);

    // Inform owner object about the changed reference value.
    owner->referenceReplaced(descriptor,
        const_cast<RefTarget*>(static_cast<const RefTarget*>(inactiveTarget.get())),
        const_cast<RefTarget*>(static_cast<const RefTarget*>(_targets[index].get())),
        index);

    // Emit object-changed signal.
    generateTargetChangedEvent(owner, descriptor);

    // Emit additional signal if SET_PROPERTY_FIELD_CHANGE_EVENT macro was used for this property field.
    if(descriptor->extraChangeEventType() != 0)
        generateTargetChangedEvent(owner, descriptor, descriptor->extraChangeEventType());
}

/******************************************************************************
* Removes the i-th target from the vector reference field.
******************************************************************************/
template<typename T> void VectorReferenceFieldBase<T>::removeReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& inactiveTarget)
{
    OVITO_CHECK_POINTER(this);
    OVITO_CHECK_OBJECT_POINTER(owner);
    OVITO_ASSERT(descriptor->isVector());

    inactiveTarget = std::move(_targets[index]);
    _targets.remove(index);

    // Disconnect from the old target, but only if the dependent has no other references to the old target.
    if(inactiveTarget && !owner->hasReferenceTo(inactiveTarget))
        inactiveTarget->unregisterDependent(owner);

    // Inform owner object about the removed reference value.
    owner->referenceRemoved(descriptor,
        const_cast<RefTarget*>(static_cast<const RefTarget*>(inactiveTarget.get())),
        index);

    // Emit object-changed signal.
    generateTargetChangedEvent(owner, descriptor);

    // Emit additional signal if SET_PROPERTY_FIELD_CHANGE_EVENT macro was used for this property field.
    if(descriptor->extraChangeEventType() != 0)
        generateTargetChangedEvent(owner, descriptor, descriptor->extraChangeEventType());
}

/******************************************************************************
* Adds the target to the vector reference field.
******************************************************************************/
template<typename T> auto VectorReferenceFieldBase<T>::addReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& target) -> size_type
{
    OVITO_ASSERT(ownerTypeCheck(owner, descriptor));
    OVITO_ASSERT(descriptor->isVector());

    // Check for cyclic strong references.
    if(target && owner->isReferencedBy(target, true))
        throw CyclicReferenceError();

    // Add new reference to list.
    if(index == -1) {
        index = _targets.size();
        _targets.push_back(std::exchange(target, nullptr));
    }
    else {
        OVITO_ASSERT(index >= 0 && index <= _targets.size());
        _targets.insert(index, std::exchange(target, nullptr));
    }
    OVITO_ASSERT(!target);

    // Register the dependent with the newly referenced object (only if it isn't already registered).
    if(_targets[index])
         _targets[index]->registerDependent(owner);

    // Inform derived classes.
    owner->referenceInserted(descriptor, const_cast<RefTarget*>(static_cast<const RefTarget*>(_targets[index].get())), index);

    // Send auto change message.
    generateTargetChangedEvent(owner, descriptor);

    // An additional message can be requested by the user using the SET_PROPERTY_FIELD_CHANGE_EVENT macro.
    if(descriptor->extraChangeEventType() != 0)
        generateTargetChangedEvent(owner, descriptor, descriptor->extraChangeEventType());

    return index;
}

// Instantiate base class template for the fancy pointer base types needed.
#if defined(Q_CC_MSVC) || defined(Q_CC_CLANG) || defined(OVITO_BUILD_MONOLITHIC)
    template class OVITO_CORE_EXPORT VectorReferenceFieldBase<OORef<RefTarget>>;
    template class OVITO_CORE_EXPORT VectorReferenceFieldBase<DataOORef<const DataObject>>;
#endif

}   // End of namespace
