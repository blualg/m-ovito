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
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/oo/RefMaker.h>
#include "PropertyFieldDescriptor.h"

namespace Ovito {

/// Constructor for a property field that stores a non-animatable property.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags,
    void (*propertyStorageCopyFunc)(RefMaker*, const PropertyFieldDescriptor*, const RefMaker*),
    QVariant (*propertyStorageReadFunc)(const RefMaker*, const PropertyFieldDescriptor*),
    void (*propertyStorageWriteFunc)(RefMaker*, const PropertyFieldDescriptor*, const QVariant&),
    void (*propertyStorageSaveFunc)(const RefMaker*, const PropertyFieldDescriptor*, SaveStream&),
    void (*propertyStorageLoadFunc)(RefMaker*, const PropertyFieldDescriptor*, LoadStream&),
    void (*propertyStorageTakeSnapshotFunc)(RefMaker*, const PropertyFieldDescriptor*),
    void (*propertyStorageRestoreSnapshotFunc)(const RefMaker*, const PropertyFieldDescriptor*, RefMaker*))
    : _definingClassDescriptor(definingClass), _identifier(identifier), _flags(flags),
        _propertyStorageCopyFunc(propertyStorageCopyFunc),
        _propertyStorageReadFunc(propertyStorageReadFunc),
        _propertyStorageWriteFunc(propertyStorageWriteFunc),
        _propertyStorageSaveFunc(propertyStorageSaveFunc),
        _propertyStorageLoadFunc(propertyStorageLoadFunc),
        _propertyStorageTakeSnapshotFunc(propertyStorageTakeSnapshotFunc),
        _propertyStorageRestoreSnapshotFunc(propertyStorageRestoreSnapshotFunc)
{
    OVITO_ASSERT(_identifier != nullptr);
    OVITO_ASSERT(!_flags.testFlag(PROPERTY_FIELD_VECTOR));
    OVITO_ASSERT(definingClass != nullptr);
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    if(!_flags.testFlag(PROPERTY_FIELD_DONT_REGISTER_IN_CLASS)) {
        // Make sure that there is no other reference field with the same identifier in the defining class.
        OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
            qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
        this->_next = definingClass->_firstPropertyField;
        definingClass->_firstPropertyField = this;
    }
}

/// Constructor for a property field that stores a single reference to a RefTarget.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
    RefTarget* (*singleReferenceReadFunc)(const RefMaker*, const PropertyFieldDescriptor*),
    void (*singleReferenceWriteFuncRef)(RefMaker*, const PropertyFieldDescriptor*, OORef<const RefTarget>))
    : _definingClassDescriptor(definingClass), _targetClassDescriptor(targetClass), _identifier(identifier), _flags(flags),
        _singleReferenceReadFunc(singleReferenceReadFunc),
        _singleReferenceWriteFuncRef(singleReferenceWriteFuncRef)
{
    OVITO_ASSERT(_identifier != nullptr);
    OVITO_ASSERT(_singleReferenceReadFunc != nullptr && _singleReferenceWriteFuncRef != nullptr);
    OVITO_ASSERT(!_flags.testFlag(PROPERTY_FIELD_VECTOR));
    OVITO_ASSERT(definingClass != nullptr);
    OVITO_ASSERT(targetClass != nullptr);
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    if(!_flags.testFlag(PROPERTY_FIELD_DONT_REGISTER_IN_CLASS)) {
        // Make sure that there is no other reference field with the same identifier in the defining class.
        OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
            qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
        this->_next = definingClass->_firstPropertyField;
        definingClass->_firstPropertyField = this;
    }
}

/// Constructor for a property field that stores a vector of references to RefTarget objects.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags,
        int (*vectorReferenceCountFunc)(const RefMaker*, const PropertyFieldDescriptor*),
        RefTarget* (*vectorReferenceGetFunc)(const RefMaker*, const PropertyFieldDescriptor*, int),
        void (*vectorReferenceSetFunc)(RefMaker*, const PropertyFieldDescriptor*, int, const RefTarget*),
        void (*vectorReferenceRemoveFunc)(RefMaker*, const PropertyFieldDescriptor*, int),
        void (*vectorReferenceInsertFunc)(RefMaker*, const PropertyFieldDescriptor*, int, OORef<RefTarget>))
    : _definingClassDescriptor(definingClass), _targetClassDescriptor(targetClass), _identifier(identifier), _flags(flags),
        _vectorReferenceCountFunc(vectorReferenceCountFunc),
        _vectorReferenceGetFunc(vectorReferenceGetFunc),
        _vectorReferenceSetFunc(vectorReferenceSetFunc),
        _vectorReferenceRemoveFunc(vectorReferenceRemoveFunc),
        _vectorReferenceInsertFunc(vectorReferenceInsertFunc)
{
    OVITO_ASSERT(_identifier != nullptr);
    OVITO_ASSERT(_vectorReferenceCountFunc != nullptr && _vectorReferenceGetFunc != nullptr);
    OVITO_ASSERT(_flags.testFlag(PROPERTY_FIELD_VECTOR));
    OVITO_ASSERT(definingClass != nullptr);
    OVITO_ASSERT(targetClass != nullptr);
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    if(!_flags.testFlag(PROPERTY_FIELD_DONT_REGISTER_IN_CLASS)) {
        OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
            qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
        this->_next = definingClass->_firstPropertyField;
        definingClass->_firstPropertyField = this;
    }
}

/******************************************************************************
* Return the human readable and localized name of the parameter field.
* This information is parsed from the plugin manifest file.
******************************************************************************/
QString PropertyFieldDescriptor::displayName() const
{
    if(_displayName.isNull())
        return QString::fromUtf8(identifier());
    else
        return _displayName;
}

/******************************************************************************
* Saves the current value of a property field in the application's settings store.
******************************************************************************/
void PropertyFieldDescriptor::memorizeDefaultValue(RefMaker* object) const
{
    OVITO_CHECK_OBJECT_POINTER(object);
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    settings.beginGroup(object->getOOClass().plugin()->pluginId());
    settings.beginGroup(object->getOOClass().name());
    QVariant v = object->getPropertyFieldValue(this);
    // Workaround for bug in Qt 5.7.0: QVariants of type float do not get correctly stored
    // by QSettings (at least on macOS), because QVariant::Float is not an official type.
    if(v.typeId() == QMetaType::Float)
        v = QVariant::fromValue((double)v.toFloat());
    settings.setValue(identifier(), v);
#endif
}

/******************************************************************************
* Loads the default value of a property field from the application's settings store.
******************************************************************************/
bool PropertyFieldDescriptor::loadDefaultValue(RefMaker* object) const
{
    OVITO_CHECK_OBJECT_POINTER(object);
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    settings.beginGroup(object->getOOClass().plugin()->pluginId());
    settings.beginGroup(object->getOOClass().name());
    QVariant v = settings.value(identifier());
    if(!v.isNull()) {
        object->setPropertyFieldValue(this, v);
        return true;
    }
#endif
    return false;
}

}   // End of namespace
