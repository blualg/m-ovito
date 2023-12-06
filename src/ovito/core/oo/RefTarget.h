////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/oo/NativePropertyFieldDescriptor.h>
#include <ovito/core/oo/PropertyField.h>
#include "RefMaker.h"

namespace Ovito {

/**
 * \brief Base class for objects that are referenced by RefMaker objects.
 */
class OVITO_CORE_EXPORT RefTarget : public RefMaker
{
    OVITO_CLASS(RefTarget)

protected:

    /// \brief Constructor.
    RefTarget(ObjectInitializationFlags flags);

#ifdef OVITO_DEBUG
    /// \brief Destructor.
    virtual ~RefTarget();
#endif

    //////////////////////////////// from OvitoObject //////////////////////////////////////

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being deleted.
    virtual void aboutToBeDeleted() override;

    //////////////////////////// Reference event handling ////////////////////////////////

    /// \brief Sends an event to all dependents of this RefTarget.
    /// \param event The notification event to be sent to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept;

    /// \brief Is called when the value of a reference field of this RefMaker changes.
    /// \param field Specifies the reference field of this RefMaker that has been changed.
    /// \param oldTarget The old target that was referenced by the ReferenceField. This can be \c NULL.
    /// \param newTarget The new target that is now referenced by the ReferenceField. This can be \c NULL.
    ///
    /// This method can by overridden by derived classes that want to be informed when
    /// any of their reference fields are changed.
    ///
    /// \note When this method is overridden in sub-classes then the base implementation of this method
    ///       should always be called from the new implementation to allow the base classes to handle
    ///       messages for their specific reference fields.
    ///
    /// The RefTarget implementation of this virtual method generates a ReferenceEvent::ReferenceChanged notification event
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override {
        notifyDependentsImpl(ReferenceFieldEvent(ReferenceEvent::ReferenceChanged, this, field, oldTarget, newTarget, listIndex));
        RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
    }

    /// \brief Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
    /// \param field Specifies the reference field of this RefMaker to which a new entry has been added.
    ///              This is always a VectorReferenceField.
    /// \param newTarget The new target added to the list of referenced objects.
    /// \param listIndex The index into the VectorReferenceField at which the new entry has been inserted.
    ///
    /// This method can by overridden by derived classes that want to be informed when
    /// a reference has been added to one of its vector reference fields.
    ///
    /// \note When this method is overridden in sub-classes then the base implementation of this method
    ///       should always be called from the new implementation to allow the base classes to handle
    ///       messages for their specific reference fields.
    ///
    /// The RefTarget implementation of this virtual method generates a ReferenceEvent::ReferenceAdded notification event
    virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) override {
        notifyDependentsImpl(ReferenceFieldEvent(ReferenceEvent::ReferenceAdded, this, field, nullptr, newTarget, listIndex));
        RefMaker::referenceInserted(field, newTarget, listIndex);
    }

    /// \brief Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
    /// \param field Specifies the reference field of this RefMaker from which an entry has been removed.
    ///              This is always a VectorReferenceField.
    /// \param oldTarget The old target that was reference before it has been removed from the vector reference field.
    /// \param listIndex The index into the VectorReferenceField at which the old entry was stored.
    ///
    /// This method can by overridden by derived classes that want to be informed when
    /// a reference has been removed from one of its vector reference fields.
    ///
    /// \note When this method is overridden in sub-classes then the base implementation of this method
    ///       should always be called from the new implementation to allow the base classes to handle
    ///       messages for their specific reference fields.
    ///
    /// The RefTarget implementation of this virtual method generates a ReferenceEvent::ReferenceRemoved notification event
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override {
        notifyDependentsImpl(ReferenceFieldEvent(ReferenceEvent::ReferenceRemoved, this, field, oldTarget, nullptr, listIndex));
        RefMaker::referenceRemoved(field, oldTarget, listIndex);
    }

    /// \brief Handles a notification event from a RefTarget referenced by this object.
    /// \param source Specifies the RefTarget that delivered the event.
    /// \param event The notification event.
    /// \return If \c true then the message is passed on to all dependents of this object.
    virtual bool handleReferenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    //////////////////////////////// Object cloning //////////////////////////////////////

    /// \brief Creates a copy of this RefTarget object.
    /// \param deepCopy If \c true, then all objects referenced by this RefTarget should also be copied.
    ///                 If \c false, then the new object clone should just take over the references of the original object
    ///                 and no copying of sub-objects takes place.
    /// \param cloneHelper Copying of sub-objects should be done using the passed CloneHelper instance.
    ///                    It makes sure that only one copy per object is made through the whole object graph.
    ///
    /// The default implementation of this method takes care of instance creation. It creates
    /// a new instance of the original object class.
    ///
    /// Sub-classes should override this method and must always call the base class' version of this method
    /// to create the new instance. The returned smart pointer can safely be cast to the class type of the original
    /// object.
    ///
    /// Every sub-class that has reference fields or other internal data fields should override this
    /// method and copy or transfer the members to the new clone.
    ///
    /// \sa CloneHelper::cloneObject()
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const;

Q_SIGNALS:

    /// This Qt signal is used to communicate with the dependents of this RefTarget.
    void objectEvent(RefTarget* sender, const ReferenceEvent& event);

public:

    /// \brief Returns true if this object is an instance of a RefTarget derived class.
    virtual bool isRefTarget() const override { return true; }

    //////////////////////////////// Notification events ////////////////////////////////////

    /// \brief Sends an event to all dependents of this RefTarget.
    /// \param eventType The event type passed to the ReferenceEvent constructor.
    inline void notifyDependents(ReferenceEvent::Type eventType) const noexcept {
        OVITO_ASSERT(eventType != ReferenceEvent::TargetChanged);
        OVITO_ASSERT(eventType != ReferenceEvent::ReferenceChanged);
        OVITO_ASSERT(eventType != ReferenceEvent::ReferenceAdded);
        OVITO_ASSERT(eventType != ReferenceEvent::ReferenceRemoved);
        OVITO_ASSERT(eventType != ReferenceEvent::CheckIsReferencedBy);
        OVITO_ASSERT(eventType != ReferenceEvent::RequestGoToAnimationTime);
        const_cast<RefTarget*>(this)->notifyDependentsImpl(ReferenceEvent(eventType, const_cast<RefTarget*>(this)));
    }

    /// \brief Sends a ReferenceEvent::TargetChanged event to all dependents of this RefTarget.
    inline void notifyTargetChanged(const PropertyFieldDescriptor* field = nullptr) const noexcept {
        const_cast<RefTarget*>(this)->notifyDependentsImpl(TargetChangedEvent(const_cast<RefTarget*>(this), field));
    }

    /// \brief Notifies the dependents that this object's state has changed outside of the given animation time interval
    ///        but remained the same within the interval.
    inline void notifyTargetChangedOutsideInterval(const TimeInterval& interval) const noexcept {
        const_cast<RefTarget*>(this)->notifyDependentsImpl(TargetChangedEvent(const_cast<RefTarget*>(this), nullptr, interval));
    }

    ////////////////////////////////// Dependency graph ///////////////////////////////////////

    /// \brief Checks whether this object is directly or indirectly referenced by some other object.
    /// \param obj The object that might hold a reference to \c this object.
    /// \param onlyStrongReferences If true, ignores reference fields that have been marked as weak and don't propagate messages.
    /// \return \c true if \a obj has a direct or indirect reference to this object;
    ///         \c false if \a obj does not depend on this object.
    virtual bool isReferencedBy(const RefMaker* obj, bool onlyStrongReferences = true) const override;

    /// \brief Visits all immediate dependents that reference this target object
    ///        and invokes the given function for every dependent encountered.
    ///
    /// \note The visitor function may be called multiple times for a dependent if that dependent
    ///       has multiple references to this target.
    template<class Callable>
    void visitDependents(Callable&& fn) const {
        VisitDependentsEvent event(const_cast<RefTarget*>(this), std::move(fn));
        const_cast<RefTarget*>(this)->notifyDependentsImpl(event);
    }

    /// \brief Asks this object to delete itself.
    ///
    /// If undo recording is active, the object instance is kept alive such that
    /// the deletion can be undone.
    virtual void deleteReferenceObject();

    /// \brief Returns the title of this object.
    /// \return A string that is used as label or title for this object in the user interface.
    ///
    /// The default implementation returns OvitoClass::objectTitle().
    /// Sub-classes can override this method to return a title that depends on the internal state of the object.
    virtual QString objectTitle() const;

    /// \brief Flags this object when it is opened in an editor.
    void setObjectEditingFlag();

    /// \brief Unflags this object when it is no longer opened in an editor.
    void unsetObjectEditingFlag();

    /// \brief Determines if this object's properties are currently being edited in an editor.
    bool isObjectBeingEdited() const;

    /// \brief Rescales the times of all animation keys from the old animation interval to the new interval.
    /// \param oldAnimationInterval The old animation interval, which should be mapped to the new animation interval.
    /// \param newAnimationInterval The new animation interval.
    ///
    /// For keyed controllers this will rescale the key times of all keys from the
    /// old animation interval to the new interval using a linear mapping.
    ///
    /// Keys that lie outside of the old animation interval will also be scaled using linear extrapolation.
    ///
    /// The default implementation does nothing.
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) {}

private:

    friend class RefMaker;
    friend class CloneHelper;
};

}   // End of namespace

