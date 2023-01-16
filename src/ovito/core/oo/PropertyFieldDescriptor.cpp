////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/dataset/DataSet.h>
#include "PropertyFieldDescriptor.h"

namespace Ovito {

/// Constructor for a property field that stores a non-animatable property.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, const char* identifier, PropertyFieldFlags flags,
    void (*propertyStorageCopyFunc)(RefMaker*, const RefMaker*),
    QVariant (*propertyStorageReadFunc)(const RefMaker*),
    void (*propertyStorageWriteFunc)(RefMaker*, const QVariant&),
    void (*propertyStorageSaveFunc)(const RefMaker*, SaveStream&),
    void (*propertyStorageLoadFunc)(RefMaker*, LoadStream&),
    void (*propertyStorageTakeSnapshotFunc)(RefMaker*),
    void (*propertyStorageRestoreSnapshotFunc)(const RefMaker*, RefMaker*))
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
    // Make sure that there is no other reference field with the same identifier in the defining class.
    OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
        qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    this->_next = definingClass->_firstPropertyField;
    definingClass->_firstPropertyField = this;
//  if(qstrcmp(identifier, "identifier") == 0)
//      qDebug() << "PropertyFieldDescriptor:" << identifier << (void*)definingClass;
}

/// Constructor for a property field that stores a single reference to a RefTarget.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags, RefTarget* (*singleReferenceReadFunc)(const RefMaker*), void (*singleReferenceWriteFunc)(RefMaker*, const RefTarget*), void (*singleReferenceWriteFuncRef)(RefMaker*, OORef<RefTarget>))
    : _definingClassDescriptor(definingClass), _targetClassDescriptor(targetClass), _identifier(identifier), _flags(flags), _singleReferenceReadFunc(singleReferenceReadFunc), _singleReferenceWriteFunc(singleReferenceWriteFunc), _singleReferenceWriteFuncRef(singleReferenceWriteFuncRef)
{
    OVITO_ASSERT(_identifier != nullptr);
    OVITO_ASSERT(_singleReferenceReadFunc != nullptr && _singleReferenceWriteFunc != nullptr);
    OVITO_ASSERT(!_flags.testFlag(PROPERTY_FIELD_VECTOR));
    OVITO_ASSERT(definingClass != nullptr);
    OVITO_ASSERT(targetClass != nullptr);
    // Make sure that there is no other reference field with the same identifier in the defining class.
    OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
        qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    this->_next = definingClass->_firstPropertyField;
    definingClass->_firstPropertyField = this;
}

/// Constructor for a property field that stores a vector of references to RefTarget objects.
PropertyFieldDescriptor::PropertyFieldDescriptor(RefMakerClass* definingClass, OvitoClassPtr targetClass, const char* identifier, PropertyFieldFlags flags, 
        int (*vectorReferenceCountFunc)(const RefMaker*), RefTarget* (*vectorReferenceGetFunc)(const RefMaker*, int), void (*vectorReferenceSetFunc)(RefMaker*, int, const RefTarget*),
        void (*vectorReferenceRemoveFunc)(RefMaker*, int), void (*vectorReferenceInsertFunc)(RefMaker*, int, OORef<RefTarget>))
    : _definingClassDescriptor(definingClass), _targetClassDescriptor(targetClass), _identifier(identifier), _flags(flags), 
        _vectorReferenceCountFunc(vectorReferenceCountFunc), _vectorReferenceGetFunc(vectorReferenceGetFunc), _vectorReferenceSetFunc(vectorReferenceSetFunc),
        _vectorReferenceRemoveFunc(vectorReferenceRemoveFunc), _vectorReferenceInsertFunc(vectorReferenceInsertFunc)
{
    OVITO_ASSERT(_identifier != nullptr);
    OVITO_ASSERT(_vectorReferenceCountFunc != nullptr && _vectorReferenceGetFunc != nullptr);
    OVITO_ASSERT(_flags.testFlag(PROPERTY_FIELD_VECTOR));
    OVITO_ASSERT(definingClass != nullptr);
    OVITO_ASSERT(targetClass != nullptr);
    OVITO_ASSERT_MSG(definingClass->findPropertyField(identifier) == nullptr, "PropertyFieldDescriptor",
        qPrintable(QString("Property field identifier is not unique for class %2: %1").arg(identifier).arg(definingClass->name())));
    // Insert into linked list of reference fields stored in the defining class' descriptor.
    this->_next = definingClass->_firstPropertyField;
    definingClass->_firstPropertyField = this;
}

/******************************************************************************
* Return the human readable and localized name of the parameter field.
* This information is parsed from the plugin manifest file.
******************************************************************************/
QString PropertyFieldDescriptor::displayName() const
{
    if(_displayName.isEmpty())
        return identifier();
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
        //qDebug() << "Loading default value for parameter" << identifier() << "of class" << definingClass()->name() << ":" << v;
        object->setPropertyFieldValue(this, v);
        return true;
    }
#endif
    return false;
}

}   // End of namespace
