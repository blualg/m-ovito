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
#include <ovito/core/oo/RefMakerClass.h>
#include <ovito/core/oo/ReferenceEvent.h>
#include <ovito/core/oo/PropertyFieldFlags.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/**
 * \brief Exception that is thrown when trying to create a cyclic reference.
 *
 * This exception object is thrown by the RefMaker class when
 * a cyclic reference would be created by setting a reference field to
 * a new target.
 */
class OVITO_CORE_EXPORT CyclicReferenceError : public Exception
{
public:
    /// Constructor.
    CyclicReferenceError() : Exception(QStringLiteral("Cyclic reference error")) {}
};

/**
 * \brief Base class for all objects that hold references to other objects (reference targets).
 */
class OVITO_CORE_EXPORT RefMaker : public OvitoObject
{
    OVITO_CLASS_META(RefMaker, RefMakerClass)

protected:

    // Specifies the default flags for all reference fields of RefMaker-derived classes that are not a RefTarget.
    static constexpr auto PROPERTY_FIELD_STANDARD_FLAGS =
                PROPERTY_FIELD_NEVER_CLONE_TARGET |
                PROPERTY_FIELD_NO_UNDO |
                PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES |
                PROPERTY_FIELD_NO_CHANGE_MESSAGE |
                PROPERTY_FIELD_NO_SUB_ANIM;

    /// \brief Constructor.
    RefMaker() = default;

    /////////////////////////////// Reference field events ///////////////////////////////////

    /// \brief Is called when a RefTarget referenced by this object generated an event.
    /// \param source A direct reference target of this RefMaker specifying the source of the message.
    ///               Note that this may not be the sender or generator of the notification
    ///               event. The sender is returned by ReferenceEvent::sender().
    /// \param event The notification event received by this RefMaker.
    /// \return \c true if the event should be recursively passed on to dependents of this object;
    ///         \c false if the event should not be sent to other dependents.
    ///
    /// \note When this method is overridden in sub-classes then the base implementation of this method
    ///       should always be called to allow the base class to handle message from its specific
    ///       reference targets.
    ///
    /// The default implementation of this method does nothing and returns \c true if the event's
    /// shouldPropagate() method returns true.
    ///
    /// \sa RefTarget::notifyDependents()
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event);

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
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) {}

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
    /// \sa VectorReferenceField::push_back()
    /// \sa referenceRemoved()
    virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) {}

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
    /// \sa VectorReferenceField::remove()
    /// \sa referenceInserted()
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) {}

    /// \brief Processes a notification event from a RefTarget referenced by this RefMaker.
    /// \param source Specifies the RefTarget referenced by this RefMaker that delivered the event.
    /// \param event The notification event.
    /// \return If \c true then the event is passed on to the dependents of this object.
    virtual bool handleReferenceEvent(RefTarget* source, const ReferenceEvent& event);

    /// \brief Is called when the value of a property of this object has changed.
    /// \param field Specifies the property field of this RefMaker that has changed.
    ///              This is always a non-animatable PropertyField.
    ///
    /// This method can by overridden by derived classes that want to be informed when
    /// any of their property fields change.
    ///
    /// \note When this method is overridden in sub-classes then the base implementation of this method
    ///       should always be called from the new implementation to allow the base classes to handle
    ///       messages for their specific property fields.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) {}

    /// \brief Stops observing a RefTarget object.
    /// \param target All references hold by the RefMaker to the this target are cleared.
    ///
    /// All reference fields containing a pointer to \a target will be reset to \c NULL.
    /// If \a target is referenced in a VectorReferenceField then the item is
    /// removed from the vector field.
    ///
    /// \undoable
    void clearReferencesTo(const RefTarget* target);

    /// \brief Clears a reference field.
    /// \param field Specifies the reference field of this RefMaker to be cleared.
    ///
    /// If the reference field specified by \a field is a single reference field then it is set to the value \c NULL.
    /// If it is a VectorReferenceField then all references are removed.
    ///
    /// \undoable
    void clearReferenceField(const PropertyFieldDescriptor* field);

    /// \brief Saves the class' contents to an output stream.
    /// \param stream The destination stream.
    ///
    /// Derived classes can overwrite this virtual method to store their specific data
    /// in the output stream. The derived class \b must always call the base implementation of the saveToStream() method
    /// before it writes its own data to the stream.
    ///
    /// The RefMaker implementation of this virtual method saves all
    /// references and referenced RefTarget object of this RefMaker to the output stream.
    ///
    /// \sa loadFromStream()
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// \brief Loads the class' contents from an input stream.
    /// \param stream The source data stream.
    /// \throw Exception when a parsing error has occurred.
    ///
    /// Derived classes can overwrite this virtual method to read their specific data
    /// from the input stream. The derived class \b must always call the loadFromStream() method
    /// of the base class before it reads its own data from the stream.
    ///
    /// The RefMaker implementation of this method restores all
    /// reference fields and loads the referenced objects that had been serialized to the data stream.
    ///
    /// \sa saveToStream()
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// \brief Allows the object to parse the serialized contents of a property field in a custom way.
    /// \param stream The source data stream.
    /// \param serializedField The property field to be parsed.
    /// \throw Exception when a parsing error has occurred.
    /// \return \c true if the function has parsed the field; \c false if the standard parsing routine should be invoked.
    ///
    /// Overriding this method is useful if a property field has been replaced by another. To maintain file
    /// compatibility, the object can parse the value of the old property field from the file and store it in the new field.
    ///
    /// The default implementation returns \c false.
    virtual bool loadPropertyFieldFromStream(ObjectLoadStream& stream, const RefMakerClass::SerializedClassInfo::PropertyFieldInfo& serializedField) { return false; }

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being deleted.
    virtual void aboutToBeDeleted() override;

public:

    /// \brief Clears all references held by this RefMarker.
    ///
    /// All single reference fields are set to \c NULL and all vector reference
    /// fields are cleared.
    ///
    /// \undoable
    void clearAllReferences();

    /////////////////////////// Property field access ///////////////////////////////

    /// \brief Returns the value stored in a non-animatable property field of this RefMaker object.
    /// \param field The descriptor of a property field defined by this RefMaker derived class.
    /// \return The current value of the property field.
    /// \sa OvitoClass::firstPropertyField()
    /// \sa OvitoClass::findPropertyField()
    QVariant getPropertyFieldValue(const PropertyFieldDescriptor* field) const;

    /// \brief Sets the value stored in a non-animatable property field of this RefMaker object.
    /// \param field The descriptor of a property field defined by this RefMaker derived class.
    /// \param newValue The value to be assigned to the property. The QVariant data type must match the property data type.
    /// \sa OvitoClass::firstPropertyField()
    /// \sa OvitoClass::findPropertyField()
    void setPropertyFieldValue(const PropertyFieldDescriptor* field, const QVariant& newValue);

    /// \brief Copies the value stored in a non-animatable property field of from another RefMaker instance to this RefMaker object.
    /// \param field The descriptor of a property field defined by this RefMaker derived class.
    /// \param other The source object. Must be of the same class type as this instance.
    void copyPropertyFieldValue(const PropertyFieldDescriptor* field, const RefMaker& other);

    /// \brief Returns the target object a reference field of this RefMaker is pointing to.
    /// \param field The descriptor of a reference property field defined by this RefMaker derived class.
    /// \return The current value of the reference field.
    RefTarget* getReferenceFieldTarget(const PropertyFieldDescriptor* field) const;

    /// \brief Sets a reference field of this RefMaker to reference a different target.
    /// \param field The descriptor of a reference field defined in this RefMaker derived class.
    /// \param target The new target to replace the old one with.
    void setReferenceFieldTarget(const PropertyFieldDescriptor* field, const RefTarget* target);

    /// \brief Returns the number of target objects in a vector reference field of this RefMaker.
    /// \param field The descriptor of a vector reference field defined in this RefMaker derived class.
    /// \return The length of the vector.
    int getVectorReferenceFieldSize(const PropertyFieldDescriptor* field) const;

    /// \brief Returns the i-th target object from a vector reference field of this RefMaker.
    /// \param field The descriptor of a vector reference field defined in this RefMaker derived class.
    /// \param index The index into the reference target list of the field.
    /// \return The referenced target object.
    RefTarget* getVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index) const;

    /// \brief Replaces the i-th object from a vector reference field of this RefMaker with a different target.
    /// \param field The descriptor of a vector reference field defined in this RefMaker derived class.
    /// \param index The index into the reference target list of the field.
    /// \param target The new target to replace the old one with.
    void setVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index, const RefTarget* target);

    /// \brief Removes the i-th target object from a vector reference field of this RefMaker.
    /// \param field The descriptor of a vector reference field defined in this RefMaker derived class.
    /// \param index The index into the reference target list of the field.
    void removeVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index);

    /// \brief Determines if an object is among the targets of a vector reference field of this RefMaker.
    /// \param field The descriptor of a vector reference field defined in this RefMaker derived class.
    /// \param target The object to look for in the reference field.
    /// \return True if the object is found in the list of referenced targets.
    bool vectorReferenceFieldContains(const PropertyFieldDescriptor* field, const RefTarget* target) const;

    /// \brief Initializes a new instance as part of two-phase object initialization.
    ///
    /// This method is automatically called right after creation of a new object instance by the OORef<>::create() function.
    /// It loads the initial values for property fields with user-defined default settings (those having the PROPERTY_FIELD_MEMORIZE flag set).
    void initializeParametersToUserDefaults();

    /// \brief Initializes a new instance and all its children as part of two-phase object initialization.
    void initializeParametersToUserDefaultsRecursive();

    /// Creates a snapshot of the object's parameter values that will serve as reference to detect parameter changes made by the user.
    void freezeInitialParameterValues(std::initializer_list<const PropertyFieldDescriptor*> propertyFields);

    /// Copies the stored reference values of this object's parameters over to the given object (which must be of the same type).
    void copyInitialParametersToObject(RefMaker* obj) const;

    ////////////////////////////// Dependencies //////////////////////////////////

    /// \brief Checks whether this RefMaker has any (direct) references to a RefTarget.
    /// \param target Specifies the reference target.
    /// \return \c true if this RefMaker object has at least one direct reference to the given
    ///         RefTarget \a target; \c false otherwise.
    ///
    /// \sa isReferencedBy()
    bool hasReferenceTo(const RefTarget* target) const;

    /// \brief Checks whether this object is directly or indirectly referenced by the given RefMaker.
    /// \param obj The RefMaker that might hold a reference to \c this object.
    /// \param onlyStrongReferences If true, ignores reference fields that have been marked as weak and don't propagate messages.
    ///
    /// The RefMaker base implementation always returns \a false since this class is not a RefTarget and can therefore
    /// not be referenced. RefTarget overrides this method with a more meaningful implementation.
    virtual bool isReferencedBy(const RefMaker* obj, bool onlyStrongReferences = true) const { return false; }

    /// \brief Returns a list of all targets this RefMaker depends on (directly as well as indirectly).
    /// \return A list of all RefTargets that are directly or indirectly referenced by this RefMaker.
    /// \note The returned list is gathered recursively.
    QSet<RefTarget*> getAllDependencies() const;

    /// \brief Replaces all references of this RefMaker to some RefTarget with new ones.
    /// \param oldTarget Specifies which references should be replaced.
    /// \param newTarget Specifies the new target that should replace the old one.
    /// \undoable
    void replaceReferencesTo(const RefTarget* oldTarget, const RefTarget* newTarget);

    ///////////////////////////// User interface context ///////////////////////////////

    /// Indicates whether the current action being performed should be recorded on the undo stack.
    inline static bool isUndoRecording() { return CompoundOperation::isUndoRecording(); }

    /// Indicates whether a previously recorded action on the undo stack is currently being undone or redone.
    inline static bool isUndoingOrRedoing() { return CompoundOperation::isUndoingOrRedoing(); }

    /// Pushes an operation onto the undo stack if the undo stack is currently recording.
    /// The undo record class specified as a template parameter is instantiated only if the undo stack is recording.
    template<class UndoableOperationClass, class... Args>
    void pushIfUndoRecording(Args&&... args) {
        if(!isBeingConstructed() && isUndoRecording())
            CompoundOperation::current()->addOperation(std::make_unique<UndoableOperationClass>(std::forward<Args>(args)...));
    }

private:

    /// Checks whether this RefMaker has any (direct) strong references to a RefTarget.
    bool hasStrongReferenceTo(const RefTarget* target) const;

    /// Recursive gathering function used by getAllDependencies().
    static void walkNode(QSet<RefTarget*>& nodes, const RefMaker* node);

    friend class RefTarget;
    friend class PropertyFieldBase;
    template<typename T> friend class SingleReferenceFieldBase;
    template<typename T> friend class VectorReferenceFieldBase;
};

}   // End of namespace

Q_DECLARE_METATYPE(const Ovito::RefMakerClass*);
