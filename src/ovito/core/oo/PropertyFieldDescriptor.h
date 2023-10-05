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
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/oo/RefMakerClass.h>

namespace Ovito {

/// Bit-flags controlling the behavior of a property field.
enum PropertyFieldFlag
{
    /// Selects the default behavior.
    PROPERTY_FIELD_NO_FLAGS                     = 0,
    /// Indicates that a reference field is a vector of references.
    PROPERTY_FIELD_VECTOR                       = (1<<1),
    /// Do not create automatic undo records when the value of the property or reference field changes.
    PROPERTY_FIELD_NO_UNDO                      = (1<<2),
    /// Marks a reference to an object as a weak one that doesn't keep the target object alive.
    PROPERTY_FIELD_WEAK_REF                     = (1<<3),
    /// Controls whether or not a ReferenceField::TargetChanged event should
    /// be generated each time the property value changes.
    PROPERTY_FIELD_NO_CHANGE_MESSAGE            = (1<<4),
    /// The target of the reference field is never cloned when the owning object is cloned.
    PROPERTY_FIELD_NEVER_CLONE_TARGET           = (1<<5),
    /// The target of the reference field is shallow/deep copied depending on the mode when the owning object is cloned.
    PROPERTY_FIELD_ALWAYS_CLONE                 = (1<<6),
    /// The target of the reference field is always deep-copied completely when the owning object is cloned.
    PROPERTY_FIELD_ALWAYS_DEEP_COPY             = (1<<7),
    /// Save the last value of the property in the application's settings store and use it to initialize
    /// the property when a new object instance is created.
    PROPERTY_FIELD_MEMORIZE                     = (1<<8),
    /// Indicates that the reference field is NOT an animatable parameter owned by the RefMaker object.
    PROPERTY_FIELD_NO_SUB_ANIM                  = (1<<9),
    /// Indicates that the object(s) stored in the reference field should not save their recomputable data to a scene file.
    PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA  = (1<<10),
    /// Blocks propagating messages sent by the target.
    PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES      = (1<<11),
    /// Automatically opens a sub-editor for the given reference field.
    PROPERTY_FIELD_OPEN_SUBEDITOR               = (1<<12)
};
Q_DECLARE_FLAGS(PropertyFieldFlags, PropertyFieldFlag);

/**
 * \brief Provides meta information about a numerical parameter field of a class.
 */
struct NumericalParameterDescriptor
{
    /// The ParameterUnit-derived class which describes the units of the numerical parameter.
    const QMetaObject* unitType;

    /// The minimum value permitted for the parameter.
    FloatType minValue;

    /// The maximum value permitted for the parameter.
    FloatType maxValue;
};

/**
 * \brief This class describes one member field of a RefMaker that stores a property of the object.
 */
class OVITO_CORE_EXPORT PropertyFieldDescriptor
{
public:

    /// Constructor for a property field that stores a non-animatable property.
    PropertyFieldDescriptor(RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags,
            void (*propertyStorageCopyFunc)(RefMaker*, const RefMaker*),
            QVariant (*propertyStorageReadFunc)(const RefMaker*),
            void (*propertyStorageWriteFunc)(RefMaker*, const QVariant&),
            void (*propertyStorageSaveFunc)(const RefMaker*, SaveStream&),
            void (*propertyStorageLoadFunc)(RefMaker*, LoadStream&),
            void (*propertyStorageTakeSnapshotFunc)(RefMaker*) = nullptr,
            void (*propertyStorageRestoreSnapshotFunc)(const RefMaker*, RefMaker*) = nullptr);

    /// Constructor for a property field that stores a single reference to a RefTarget.
    PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
        RefTarget* (*singleReferenceReadFunc)(const RefMaker*),
        void (*singleReferenceWriteFunc)(RefMaker*, const RefTarget*),
        void (*singleReferenceWriteFuncRef)(RefMaker*, OORef<RefTarget>));

    /// Constructor for a property field that stores a vector of references to RefTarget objects.
    PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
        int (*vectorReferenceCountFunc)(const RefMaker*),
        RefTarget* (*vectorReferenceGetFunc)(const RefMaker*, int),
        void (*vectorReferenceSetFunc)(RefMaker*, int, const RefTarget*),
        void (*vectorReferenceRemoveFunc)(RefMaker*, int),
        void (*vectorReferenceInsertFunc)(RefMaker*, int, OORef<RefTarget>));

    /// Returns the unique identifier of the reference field.
    const char* identifier() const { return _identifier; }

    /// Returns the alias identifier of the reference field (used for backward compatibility) if defined.
    const char* identifierAlias() const { return _identifierAlias; }

    /// Returns the RefMaker derived class that owns the reference.
    const RefMakerClass* definingClass() const { return _definingClassDescriptor; }

    /// Returns the base type of the objects stored in this property field if it is a reference field; otherwise returns NULL.
    OvitoClassPtr targetClass() const { return _targetClassDescriptor; }

    /// Returns whether this is a reference field that stores a pointer to a RefTarget derived class.
    bool isReferenceField() const { return _targetClassDescriptor != nullptr; }

    /// Returns whether this reference field stores weak references.
    bool isWeakReference() const { return _flags.testFlag(PROPERTY_FIELD_WEAK_REF); }

    /// Returns true if this reference field stores a vector of objects.
    bool isVector() const { return _flags.testFlag(PROPERTY_FIELD_VECTOR); }

    /// Returns true if referenced objects should not save their recomputable data to a scene file.
    bool dontSaveRecomputableData() const { return _flags.testFlag(PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA); }

    /// Indicates that automatic undo-handling for this property field is enabled.
    /// This is the default.
    bool automaticUndo() const { return !_flags.testFlag(PROPERTY_FIELD_NO_UNDO); }

    /// Returns true if a TargetChanged event should be generated each time the property's value changes.
    bool shouldGenerateChangeEvent() const { return !_flags.testFlag(PROPERTY_FIELD_NO_CHANGE_MESSAGE); }

    /// Return the type of reference event to generate each time this property field's value changes
    /// (in addition to the TargetChanged event, which is generated by default).
    int extraChangeEventType() const { return _extraChangeEventType; }

    /// Returns the human readable and localized name of the property field.
    /// It will be used as label text in the user interface.
    QString displayName() const;

    /// Returns the next property field in the linked list (of the RefMaker derived class defining this property field).
    const PropertyFieldDescriptor* next() const { return _next; }

    /// Returns a descriptor structure that provides additional info about a numerical parameter. May be NULL.
    const NumericalParameterDescriptor* numericalParameterInfo() const { return _parameterInfo; }

    /// Returns the flags that control the behavior of the property field.
    PropertyFieldFlags flags() const { return _flags; }

    /// Saves the current value of a property field in the application's settings store.
    void memorizeDefaultValue(RefMaker* object) const;

    /// Loads the default value of a property field from the application's settings store.
    bool loadDefaultValue(RefMaker* object) const;

protected:

    /// The unique identifier of the reference field. This must be unique within
    /// a RefMaker derived class.
    const char* _identifier;

    /// The base type of the objects stored in this field if this is a reference field.
    OvitoClassPtr _targetClassDescriptor = nullptr;

    /// The RefMaker derived class that owns the property.
    const RefMakerClass* _definingClassDescriptor;

    /// The next property field in the linked list (of the RefMaker derived class defining this property field).
    const PropertyFieldDescriptor* _next;

    /// The flags that control the behavior of the property field.
    PropertyFieldFlags _flags;

    /// Stores a pointer to the function that copies the property field's value from one RefMaker instance to another.
    void (*_propertyStorageCopyFunc)(RefMaker*, const RefMaker*) = nullptr;

    /// Stores a pointer to the function that reads the property field's value for a RefMaker instance.
    QVariant (*_propertyStorageReadFunc)(const RefMaker*) = nullptr;

    /// Stores a pointer to the function that sets the property field's value for a RefMaker instance.
    void (*_propertyStorageWriteFunc)(RefMaker*, const QVariant&) = nullptr;

    /// Stores a pointer to the function that saves the property field's value to a stream.
    void (*_propertyStorageSaveFunc)(const RefMaker*, SaveStream&) = nullptr;

    /// Stores a pointer to the function that loads the property field's value from a stream.
    void (*_propertyStorageLoadFunc)(RefMaker*, LoadStream&) = nullptr;

    /// Pointer to a function that copies the current value of an object parameter to the shadow field.
    void (*_propertyStorageTakeSnapshotFunc)(RefMaker*) = nullptr;

    /// Pointer to a function that copies the stored reference value from the shadow field back into the property field of another instance.
    void (*_propertyStorageRestoreSnapshotFunc)(const RefMaker*, RefMaker*) = nullptr;

    /// Accessor function returning the referenced target object for a RefMaker instance.
    RefTarget* (*_singleReferenceReadFunc)(const RefMaker*) = nullptr;

    /// Accessor function setting the referenced target object for a RefMaker instance.
    void (*_singleReferenceWriteFunc)(RefMaker*, const RefTarget*) = nullptr;

    /// Accessor function setting the referenced target object for a RefMaker instance.
    void (*_singleReferenceWriteFuncRef)(RefMaker*, OORef<RefTarget>) = nullptr;

    /// Accessor function returning the number of referenced target objects in a vector reference field.
    int (*_vectorReferenceCountFunc)(const RefMaker*) = nullptr;

    /// Accessor function returning the i-th referenced target object for a vector reference field.
    RefTarget* (*_vectorReferenceGetFunc)(const RefMaker*, int) = nullptr;

    /// Accessor function replacing the i-th referenced target object from a vector reference field.
    void (*_vectorReferenceSetFunc)(RefMaker*, int, const RefTarget*) = nullptr;

    /// Accessor function erasing the i-th referenced target object from a vector reference field.
    void (*_vectorReferenceRemoveFunc)(RefMaker*, int) = nullptr;

    /// Accessor function insertings a target object into a vector reference field.
    void (*_vectorReferenceInsertFunc)(RefMaker*, int, OORef<RefTarget>) = nullptr;

    /// The human-readable name of this property field. It is used as label text in the user interface.
    QString _displayName;

    /// Provides further information about numerical parameters of objects.
    const NumericalParameterDescriptor* _parameterInfo;

    /// The type of reference event to generate each time this property field's value changes.
    int _extraChangeEventType = 0;

    /// The alias identifier of the reference field. This can be set for backward compatibility with older OVITO versions.
    const char* _identifierAlias = nullptr;

    friend class RefMaker;
    friend class RefTarget;
};

}   // End of namespace
