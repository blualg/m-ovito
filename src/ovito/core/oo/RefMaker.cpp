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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "RefMaker.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(RefMaker);

/******************************************************************************
* This method is called when the reference counter of this OvitoObject
* has reached zero.
******************************************************************************/
void RefMaker::aboutToBeDeleted()
{
    OVITO_CHECK_OBJECT_POINTER(this);

    // Make sure undo recording is not active while deleting a RefTarget.
    OVITO_ASSERT_MSG(!RefTarget::OOClass().isMember(this) || isUndoRecording() == false, "RefMaker::aboutToBeDeleted()", "Cannot delete object from memory while undo recording is active.");

    // Clear all references this object has to other objects.
    clearAllReferences();

    OvitoObject::aboutToBeDeleted();
}

/******************************************************************************
* Returns the value stored in a non-animatable property field of this RefMaker object.
******************************************************************************/
QVariant RefMaker::getPropertyFieldValue(const PropertyFieldDescriptor* field) const
{
    OVITO_ASSERT_MSG(!field->isReferenceField(), "RefMaker::getPropertyFieldValue", "This function may be used only to access property fields and not reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::getPropertyFieldValue", "The property field has not been defined in this class or its base classes.");
    OVITO_ASSERT_MSG(field->_propertyStorageReadFunc != nullptr, "RefMaker::getPropertyFieldValue", "The property field is a runtime property field, which doesn't allow conversion to a QVariant value.");
    return field->_propertyStorageReadFunc(const_cast<RefMaker*>(this));
}

/******************************************************************************
* Sets the value stored in a non-animatable property field of this RefMaker object.
******************************************************************************/
void RefMaker::setPropertyFieldValue(const PropertyFieldDescriptor* field, const QVariant& newValue)
{
    OVITO_ASSERT_MSG(!field->isReferenceField(), "RefMaker::setPropertyFieldValue", "This function may be used only to access property fields and not reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::setPropertyFieldValue", "The property field has not been defined in this class or its base classes.");
    OVITO_ASSERT_MSG(field->_propertyStorageWriteFunc != nullptr, "RefMaker::getPropertyFieldValue", "The property field is a runtime property field, which doesn't allow assignment of a QVariant value.");
    field->_propertyStorageWriteFunc(this, newValue);
}

/******************************************************************************
* Copies the value stored in a non-animatable property field of from another
* RefMaker instance to this RefMaker object.
******************************************************************************/
void RefMaker::copyPropertyFieldValue(const PropertyFieldDescriptor* field, const RefMaker& other)
{
    OVITO_ASSERT_MSG(!field->isReferenceField(), "RefMaker::copyPropertyFieldValue", "This function may be used only to access property fields and not reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::copyPropertyFieldValue", "The property field has not been defined in this class or its base classes.");
    OVITO_ASSERT_MSG(other.getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::copyPropertyFieldValue", "The property field has not been defined in the source's class or its base classes.");
    OVITO_ASSERT(field->_propertyStorageCopyFunc != nullptr);
    field->_propertyStorageCopyFunc(this, &other);
}

/******************************************************************************
* Returns the target object a reference field of this RefMaker is pointing to.
******************************************************************************/
RefTarget* RefMaker::getReferenceFieldTarget(const PropertyFieldDescriptor* field) const
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::getReferenceFieldTarget()", "This function may not be used to retrieve property fields.");
    OVITO_ASSERT_MSG(field->isVector() == false, "RefMaker::getReferenceFieldTarget()", "This function may not be used to retrieve vector reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::getReferenceFieldTarget()", "The reference field has not been defined in this class or its base classes.");
    OVITO_ASSERT(field->_singleReferenceReadFunc != nullptr);
    return field->_singleReferenceReadFunc(this);
}

/******************************************************************************
* Returns the i-th target object from a vector reference field of this RefMaker.
******************************************************************************/
int RefMaker::getVectorReferenceFieldSize(const PropertyFieldDescriptor* field) const
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::getVectorReferenceFieldSize", "This function may not be used to retrieve property fields.");
    OVITO_ASSERT_MSG(field->isVector() == true, "RefMaker::getVectorReferenceFieldSize", "This function may not be used to retrieve single reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::getVectorReferenceFieldSize", "The reference field has not been defined in this class or its base classes.");
    OVITO_ASSERT(field->_vectorReferenceCountFunc != nullptr);
    return field->_vectorReferenceCountFunc(this);
}

/******************************************************************************
* Returns the i-th target object from a vector reference field of this RefMaker.
******************************************************************************/
RefTarget* RefMaker::getVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index) const
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::getVectorReferenceFieldTarget", "This function may not be used to retrieve property fields.");
    OVITO_ASSERT_MSG(field->isVector() == true, "RefMaker::getVectorReferenceFieldTarget", "This function may not be used to retrieve single reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::getVectorReferenceFieldTarget", "The reference field has not been defined in this class or its base classes.");
    OVITO_ASSERT(field->_vectorReferenceGetFunc != nullptr);
    return field->_vectorReferenceGetFunc(this, index);
}

/******************************************************************************
* Replaces the i-th object from a vector reference field of this RefMaker with a different target.
******************************************************************************/
void RefMaker::setVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index, const RefTarget* target)
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::setVectorReferenceFieldTarget", "This function may not be used to retrieve property fields.");
    OVITO_ASSERT_MSG(field->isVector() == true, "RefMaker::setVectorReferenceFieldTarget", "This function may not be used to retrieve single reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::setVectorReferenceFieldTarget", "The reference field has not been defined in this class or its base classes.");
    OVITO_ASSERT(field->_vectorReferenceSetFunc != nullptr);
    field->_vectorReferenceSetFunc(this, index, target);
}

/******************************************************************************
* Removes the i-th target object from a vector reference field of this RefMaker.
******************************************************************************/
void RefMaker::removeVectorReferenceFieldTarget(const PropertyFieldDescriptor* field, int index)
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::removeVectorReferenceFieldTarget", "This function may not be used to retrieve property fields.");
    OVITO_ASSERT_MSG(field->isVector() == true, "RefMaker::removeVectorReferenceFieldTarget", "This function may not be used to retrieve single reference fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::removeVectorReferenceFieldTarget", "The reference field has not been defined in this class or its base classes.");
    OVITO_ASSERT(field->_vectorReferenceRemoveFunc != nullptr);
    field->_vectorReferenceRemoveFunc(this, index);
}

/******************************************************************************
* Determines if an object is among the targets of a vector reference field of this RefMaker.
******************************************************************************/
bool RefMaker::vectorReferenceFieldContains(const PropertyFieldDescriptor* field, const RefTarget* target) const
{
    int count = getVectorReferenceFieldSize(field);
    for(int i = 0; i < count; i++)
        if(getVectorReferenceFieldTarget(field, i) == target)
            return true;
    return false;
}

/******************************************************************************
* Handles a notification event from a RefTarget referenced by this object.
******************************************************************************/
bool RefMaker::handleReferenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    OVITO_CHECK_OBJECT_POINTER(this);

    // Handle delete signals.
    if(event.type() ==  ReferenceEvent::TargetDeleted) {
        OVITO_ASSERT(source == event.sender());
        referenceEvent(source, event);
        OVITO_CHECK_OBJECT_POINTER(this);
        clearReferencesTo(event.sender());
        return false;
    }

    // Handle CheckIsReferencedBy signals.
    if(event.type() ==  ReferenceEvent::CheckIsReferencedBy) {
        const CheckIsReferencedByEvent& queryEvent = static_cast<const CheckIsReferencedByEvent&>(event);
        if(queryEvent.onlyStrongReferences()) {
            // Determine if this RefMaker has any strong reference(s) to the event source.
            if(!hasStrongReferenceTo(source))
                return false;
        }
        if(queryEvent.dependent() == this) {
            queryEvent.setIsReferenced();
            return false;
        }
        return true;
    }

    // Let the RefMaker-derived class process the message.
    return referenceEvent(source, event);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool RefMaker::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.shouldPropagate()) {
        // Check if message is comming from a reference field for which message propagation is explicitly disabled.
        // Note that a target object may be referenced from multiple reference fields, some of which having
        // message propagation enabled and some not.
        bool isSupressedField = false;
        for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
            if(!field->isReferenceField()) continue;
            if(!field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES)) continue;
            if(!field->isVector()) {
                if(field->_singleReferenceReadFunc(this) == source) {
                    isSupressedField = true;
                    break;
                }
            }
            else {
                if(vectorReferenceFieldContains(field, source)) {
                    isSupressedField = true;
                    break;
                }
            }
        }
        if(!isSupressedField)
            return true;
        // Perform counter check and determine if message is comming from a reference field for which message propagation
        // is NOT explicitly disabled.
        for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
            if(!field->isReferenceField()) continue;
            if(!field->isVector()) {
                if(field->_singleReferenceReadFunc(this) == source) {
                    if(!field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES))
                        return true;
                }
            }
            else {
                if(vectorReferenceFieldContains(field, source)) {
                    if(!field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES))
                        return true;
                }
            }
        }
        // Do not propagate message.
        return false;
    }
    return false;
}

/******************************************************************************
* Checks if this RefMaker has any reference to the given RefTarget.
******************************************************************************/
bool RefMaker::hasReferenceTo(const RefTarget* target) const
{
    if(!target) return false;
    OVITO_CHECK_OBJECT_POINTER(target);

    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(!field->isReferenceField()) continue;
        if(!field->isVector()) {
            if(field->_singleReferenceReadFunc(this) == target)
                return true;
        }
        else {
            if(vectorReferenceFieldContains(field, target))
                return true;
        }
    }
    return false;
}

/******************************************************************************
* Checks if this RefMaker has any strong reference to the given RefTarget.
******************************************************************************/
bool RefMaker::hasStrongReferenceTo(const RefTarget* target) const
{
    if(!target) return false;
    OVITO_CHECK_OBJECT_POINTER(target);

    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(!field->isReferenceField()) continue;
        // Skip weak references for which event propagation is disabled.
        if(field->isWeakReference() && field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES)) continue;
        if(!field->isVector()) {
            if(field->_singleReferenceReadFunc(this) == target)
                return true;
        }
        else {
            if(vectorReferenceFieldContains(field, target))
                return true;
        }
    }
    return false;
}

/******************************************************************************
* Replaces all references of this RefMaker to the old RefTarget with
* the new RefTarget.
******************************************************************************/
void RefMaker::replaceReferencesTo(const RefTarget* oldTarget, const RefTarget* newTarget)
{
    if(!oldTarget) return;
    OVITO_CHECK_OBJECT_POINTER(oldTarget);

    // Iterate over all reference fields in the class hierarchy.
#ifdef OVITO_DEBUG
    bool hasBeenReplaced = false;
#endif
    const OvitoClass& oldTargetClass = oldTarget->getOOClass();
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(!field->isReferenceField()) continue;
        if(!oldTargetClass.isDerivedFrom(*field->targetClass())) continue;
        if(!field->isVector()) {
            if(field->_singleReferenceReadFunc(this) == oldTarget) {
                // Check for cyclic strong references.
                if(newTarget && (!field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES) || !field->isWeakReference()) && isReferencedBy(newTarget, true))
                    throw CyclicReferenceError();
                field->_singleReferenceWriteFunc(this, newTarget);
#ifdef OVITO_DEBUG
                hasBeenReplaced = true;
#endif
            }
        }
        else {
            int count = getVectorReferenceFieldSize(field);
            for(int i = count; i--;) {
                if(getVectorReferenceFieldTarget(field, i) == oldTarget) {
                    // Check for cyclic references.
                    if(newTarget && (!field->flags().testFlag(PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES) || !field->isWeakReference()) && isReferencedBy(newTarget, true))
                        throw CyclicReferenceError();
                    setVectorReferenceFieldTarget(field, i, newTarget);
#ifdef OVITO_DEBUG
                    hasBeenReplaced = true;
#endif
                }
            }
        }
    }
    OVITO_ASSERT_MSG(hasBeenReplaced, "RefMaker::replaceReferencesTo", "The target to be replaced was not referenced by this RefMaker.");
}

/******************************************************************************
* Stops observing a RefTarget object.
* All single reference fields containing the RefTarget will be reset to NULL.
* If the target is referenced in a vector reference field then the item is
* removed from the vector.
******************************************************************************/
void RefMaker::clearReferencesTo(const RefTarget* target)
{
    if(!target) return;
    OVITO_CHECK_OBJECT_POINTER(target);

    // Iterate over all reference fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(!field->isReferenceField()) continue;
        if(!field->isVector()) {
            if(field->_singleReferenceReadFunc(this) == target)
                field->_singleReferenceWriteFunc(this, nullptr);
        }
        else {
            for(int i = getVectorReferenceFieldSize(field); i--; ) {
                if(getVectorReferenceFieldTarget(field, i) == target)
                    removeVectorReferenceFieldTarget(field, i);
            }
        }
    }
}

/******************************************************************************
* Clears all references held by this RefMarker.
******************************************************************************/
void RefMaker::clearAllReferences()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT_MSG(getOOClass() != RefMaker::OOClass(), "RefMaker::clearAllReferences", "clearAllReferences() must not be called from the RefMaker destructor.");

    // Iterate over all reference fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(field->isReferenceField())
            clearReferenceField(field);
    }
}

/******************************************************************************
* Clears the given reference field.
* If this is a single reference field then it is set to NULL.
* If it is a list reference field the all references are removed.
******************************************************************************/
void RefMaker::clearReferenceField(const PropertyFieldDescriptor* field)
{
    OVITO_ASSERT_MSG(field->isReferenceField(), "RefMaker::clearReferenceField", "This function may not be used for property fields.");
    OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::clearReferenceField()", "The reference field has not been defined in this class or its base classes.");

    if(!field->isVector()) {
        field->_singleReferenceWriteFunc(this, nullptr);
    }
    else {
        while(int count = getVectorReferenceFieldSize(field))
            removeVectorReferenceFieldTarget(field, count - 1);
    }
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void RefMaker::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    OvitoObject::saveToStream(stream, excludeRecomputableData);

#if 0
    qDebug() << "Saving object" << this;
#endif

    // Iterate over all property fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {

        if(field->isReferenceField()) {
            // Write the object pointed to by the reference field to the stream.
            if(!field->dontSaveTarget()) {
                stream.beginChunk(0x02);
                try {
                    if(!field->isVector()) {
                        stream.saveObject(field->_singleReferenceReadFunc(this), excludeRecomputableData || field->dontSaveRecomputableData());
                    }
                    else {
                        qint32 count = getVectorReferenceFieldSize(field);
                        stream << count;
                        for(int i = 0; i < count; i++)
                            stream.saveObject(getVectorReferenceFieldTarget(field, i), excludeRecomputableData || field->dontSaveRecomputableData());
                    }
                }
                catch(Exception& ex) {
                    throw ex.prependGeneralMessage(tr("Failed to serialize contents of reference field %1 of class %2.").arg(field->identifier()).arg(field->definingClass()->name()));
                }
                stream.endChunk();
            }
            else {
                // Indicate that this target is not serizalized.
                stream.beginChunk(0x05);
                stream.endChunk();
            }
        }
        else {
            // Write the primitive value stored in the property field to the stream.
            if(field->_propertyStorageSaveFunc != nullptr) {
                stream.beginChunk(0x04);
                field->_propertyStorageSaveFunc(this, stream);
#if 0
                qDebug() << "  Property field" << field->identifier() << " contains" << field->propertyStorageReadFunc(this);
#endif
            }
            else {
                // Indicate that this property field is not serizaliable.
                stream.beginChunk(0x05);
            }
            stream.endChunk();
        }
    }
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void RefMaker::loadFromStream(ObjectLoadStream& stream)
{
    OvitoObject::loadFromStream(stream);
    OVITO_ASSERT(!isUndoRecording());

#if 0
    qDebug() << "Loading object" << this;
#endif

    // Look up the serialized metadata for this RefMaker-derived class,
    // which was loaded from the input stream.
    const RefMakerClass::SerializedClassInfo* classInfo = static_cast<const RefMakerClass::SerializedClassInfo*>(stream.getSerializedClassInfo());

    // Read property field values from the stream.
    for(const RefMakerClass::SerializedClassInfo::PropertyFieldInfo& fieldEntry : classInfo->propertyFields) {
        if(fieldEntry.customDeserializationFunction) {
            // The class has installed its own custom deserialization function for this property field.
            fieldEntry.customDeserializationFunction(fieldEntry, stream, *this);
        }
        else if(fieldEntry.isReferenceField) {
            OVITO_ASSERT(fieldEntry.targetClass != nullptr);

            // Parse target object(s).
            int chunkId = stream.openChunk();
            if(chunkId == 0x02) {

                // Parse object chunk describing the reference target.
                if(fieldEntry.field != nullptr) {
                    OVITO_CHECK_POINTER(fieldEntry.field);
                    OVITO_ASSERT(fieldEntry.field->isVector() == ((fieldEntry.field->flags() & PROPERTY_FIELD_VECTOR) != 0));
                    OVITO_ASSERT(fieldEntry.targetClass->isDerivedFrom(*fieldEntry.field->targetClass()));
                    if(!fieldEntry.field->isVector()) {
                        OORef<RefTarget> target = stream.loadObject<RefTarget>();
                        if(target && !target->getOOClass().isDerivedFrom(*fieldEntry.targetClass)) {
                            throw Exception(tr("Incompatible object stored in reference field %1 of class %2. Expected class %3 but found class %4 in file.")
                                .arg(QString(fieldEntry.identifier)).arg(fieldEntry.definingClass->name()).arg(fieldEntry.targetClass->name()).arg(target->getOOClass().name()));
                        }
#if 0
                        qDebug() << "  Reference field" << fieldEntry.identifier << " contains" << target;
#endif
                        if(!fieldEntry.field->isWeakReference())
                            fieldEntry.field->_singleReferenceWriteFuncRef(this, std::move(target));
                        else
                            fieldEntry.field->_singleReferenceWriteFunc(this, target.get());
                    }
                    else {
                        // Remove any prexisting targets from the reference field.
                        clearReferenceField(fieldEntry.field);

                        // Load each target object and store it in the list reference field.
                        qint32 numEntries;
                        stream >> numEntries;
                        OVITO_ASSERT(numEntries >= 0);
                        for(qint32 i = 0; i < numEntries; i++) {
                            OORef<RefTarget> target = stream.loadObject<RefTarget>();
                            if(target && !target->getOOClass().isDerivedFrom(*fieldEntry.targetClass)) {
                                throw Exception(tr("Incompatible object stored in reference field %1 of class %2. Expected class %3 but found class %4 in file.")
                                    .arg(QString(fieldEntry.identifier)).arg(fieldEntry.definingClass->name(), fieldEntry.targetClass->name(), target->getOOClass().name()));
                            }
#if 0
                            qDebug() << "  Vector reference field" << fieldEntry.identifier << " contains" << target;
#endif
                            fieldEntry.field->_vectorReferenceInsertFunc(this, i, std::move(target));
                        }
                    }
                }
                else {
#if 0
                    qDebug() << "  Reference field" << fieldEntry.identifier << " no longer exists.";
#endif
                    // The serialized reference field no longer exists in the current program version.
                    // Load object from stream and release it immediately.
                    if(fieldEntry.flags & PROPERTY_FIELD_VECTOR) {
                        qint32 numEntries;
                        stream >> numEntries;
                        for(qint32 i = 0; i < numEntries; i++)
                            stream.loadObject<RefTarget>();
                    }
                    else {
                        stream.loadObject<RefTarget>();
                    }
                }
            }
            else if(chunkId == 0x05) {
                // The target object has not been serialized (typically due to flag PROPERTY_FIELD_DONT_SAVE_TARGET).
            }
            else {
                throw Exception(tr("Expected reference field '%1' in object %2").arg(QString(fieldEntry.identifier)).arg(fieldEntry.definingClass->name()));
            }
            stream.closeChunk();
        }
        else {
            // Read the primitive value of the property field from the stream.
            OVITO_ASSERT(fieldEntry.targetClass == nullptr);
            int chunkId = stream.openChunk();
            if(chunkId == 0x04) {
                if(!loadPropertyFieldFromStream(stream, fieldEntry)) {
                    if(fieldEntry.field && fieldEntry.field->_propertyStorageLoadFunc != nullptr) {
                        fieldEntry.field->_propertyStorageLoadFunc(this, stream);
                    }
                    else {
                        // The property field no longer exists.
                        // Ignore chunk contents.
                    }
                }
            }
            else if(chunkId != 0x05) {
                throw Exception(tr("Expected non-serializable property field '%1' in object %2").arg(QString(fieldEntry.identifier)).arg(fieldEntry.definingClass->name()));
            }
            stream.closeChunk();
        }
    }

#if 0
    qDebug() << "Done loading automatic fields of " << this;
#endif
}

/******************************************************************************
* Returns a list of all targets this RefMaker depends on (both
* directly and indirectly).
******************************************************************************/
QSet<RefTarget*> RefMaker::getAllDependencies() const
{
    QSet<RefTarget*> nodes;
    walkNode(nodes, this);
    return nodes;
}

/******************************************************************************
* Recursive gathering function.
******************************************************************************/
void RefMaker::walkNode(QSet<RefTarget*>& nodes, const RefMaker* node)
{
    OVITO_CHECK_OBJECT_POINTER(node);

    // Iterate over all reference fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : node->getOOMetaClass().propertyFields()) {
        if(!field->isReferenceField()) continue;
        if(!field->isVector()) {
            RefTarget* target = field->_singleReferenceReadFunc(node);
            if(target != nullptr && !nodes.contains(target)) {
                nodes.insert(target);
                walkNode(nodes, target);
            }
        }
        else {
            int count = node->getVectorReferenceFieldSize(field);
            for(int i = 0; i < count; i++) {
                RefTarget* target = node->getVectorReferenceFieldTarget(field, i);
                if(target != nullptr && !nodes.contains(target)) {
                    nodes.insert(target);
                    walkNode(nodes, target);
                }
            }
        }
    }
}

/******************************************************************************
* Initializes a new instance as part of two-phase object initialization.
* This method is automatically called right after creation of a new object instance
* by the OORef<>::create() function. It loads the initial values for property fields
* with user-defined default settings (those having the PROPERTY_FIELD_MEMORIZE flag set).
******************************************************************************/
void RefMaker::initializeParametersToUserDefaults()
{
    // Iterate over all property fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(field->flags().testFlag(PROPERTY_FIELD_MEMORIZE)) {
            if(!field->isReferenceField()) {
                // If it's a property field, load the user-defined default value.
                field->loadDefaultValue(this);
            }
            else if(!field->isVector()) {
#ifndef OVITO_DISABLE_QSETTINGS
                // If it's a controller type, load default controller value.
                if(Controller* ctrl = dynamic_object_cast<Controller>(field->_singleReferenceReadFunc(this))) {
                    QSettings settings;
                    settings.beginGroup(getOOClass().plugin()->pluginId());
                    settings.beginGroup(getOOClass().name());
                    QVariant v = settings.value(field->identifier());
                    if(!v.isNull()) {
                        if(ctrl->controllerType() == Controller::ControllerTypeFloat) {
                            ctrl->setFloatValue(AnimationTime(0), v.value<FloatType>());
                        }
                        else if(ctrl->controllerType() == Controller::ControllerTypeInt) {
                            ctrl->setIntValue(AnimationTime(0), v.value<int>());
                        }
                        else if(ctrl->controllerType() == Controller::ControllerTypeVector3) {
                            ctrl->setVector3Value(AnimationTime(0), v.value<Vector3>());
                        }
                    }
                }
#endif
            }
        }
    }
}

/******************************************************************************
* Initializes a new instance and all its children as part of two-phase object initialization.
******************************************************************************/
void RefMaker::initializeParametersToUserDefaultsRecursive()
{
    initializeParametersToUserDefaults();

    // Iterate over all reference fields in the class hierarchy.
    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(field->isReferenceField()) {
            if(!field->isVector()) {
                if(RefTarget* target = field->_singleReferenceReadFunc(this))
                    target->initializeParametersToUserDefaultsRecursive();
            }
            else {
                int count = getVectorReferenceFieldSize(field);
                for(int i = 0; i < count; i++) {
                    if(RefTarget* target = getVectorReferenceFieldTarget(field, i))
                        target->initializeParametersToUserDefaultsRecursive();
                }
            }
        }
    }
}

/******************************************************************************
* Creates a snapshot of the object's parameter values that will serve as
* reference to detect parameter changes made by the user.
******************************************************************************/
void RefMaker::freezeInitialParameterValues(std::initializer_list<const PropertyFieldDescriptor*> propertyFields)
{
    // Copy current values of selected properties from the public property field to the shadow property field.
    for(const PropertyFieldDescriptor* field : propertyFields) {
        OVITO_ASSERT_MSG(!field->isReferenceField(), "RefMaker::freezeInitialParameterValues", "This function can only handle shadow property fields, not reference fields.");
        OVITO_ASSERT_MSG(getOOClass().isDerivedFrom(*field->definingClass()), "RefMaker::freezeInitialParameterValues", "The shadow property field has not been defined in this class or its base classes.");
        OVITO_ASSERT_MSG(field->_propertyStorageTakeSnapshotFunc != nullptr, "RefMaker::freezeInitialParameterValues", "The property field is not a shadow property field.");

        field->_propertyStorageTakeSnapshotFunc(this);
    }
}

/******************************************************************************
* Copies the stored reference values of this object's parameters over to the
* given object (which must be of the same type).
******************************************************************************/
void RefMaker::copyInitialParametersToObject(RefMaker* obj) const
{
    OVITO_ASSERT(obj);
    OVITO_ASSERT(getOOClass() == obj->getOOClass());

    for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
        if(field->_propertyStorageRestoreSnapshotFunc)
            field->_propertyStorageRestoreSnapshotFunc(this, obj);
    }
}

}   // End of namespace
