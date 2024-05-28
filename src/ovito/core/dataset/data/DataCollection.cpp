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
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataCollection);
DEFINE_VECTOR_REFERENCE_FIELD(DataCollection, objects);
SET_PROPERTY_FIELD_LABEL(DataCollection, objects, "Data objects");

/******************************************************************************
* Tests if the given object is part of this collection (only at the root-level).
******************************************************************************/
bool DataCollection::contains(const DataObject* obj) const
{
    return objects().contains(const_cast<DataObject*>(obj));
}

/******************************************************************************
* Adds an additional root-level data object to this collection.
* The object must not already be part of the collection.
******************************************************************************/
void DataCollection::addObject(const DataObject* obj)
{
    OVITO_CHECK_OBJECT_POINTER(obj);
    OVITO_ASSERT_MSG(!contains(obj), "DataCollection::addObject", "Cannot add the same data object more than once.");
    _objects.push_back(this, PROPERTY_FIELD(objects), obj);
}

/******************************************************************************
* Inserts an additional root-level data object into this collection.
* The object must not already be part of the collection.
******************************************************************************/
void DataCollection::insertObject(qsizetype index, DataOORef<const DataObject> obj)
{
    OVITO_CHECK_OBJECT_POINTER(obj);

    // Undo recording should never be active during pipeline evaluation.
    OVITO_ASSERT(!isUndoRecording());

    OVITO_ASSERT_MSG(!contains(obj), "DataCollection::insertObject", "Cannot insert the same data object more than once.");
    OVITO_ASSERT(index >= 0 && index <= objects().size());
    _objects.insert(this, PROPERTY_FIELD(objects), index, std::move(obj));
}

/******************************************************************************
* Removes a root-level data object from this collection.
******************************************************************************/
void DataCollection::removeObjectByIndex(qsizetype index)
{
    OVITO_ASSERT(index >= 0 && index < objects().size());
    _objects.remove(this, PROPERTY_FIELD(objects), index);
}

/******************************************************************************
* Replaces a root-level data object in this collection with a different one.
******************************************************************************/
bool DataCollection::replaceObject(const DataObject* oldObj, const DataObject* newObj)
{
    OVITO_CHECK_OBJECT_POINTER(oldObj);
    if(!contains(oldObj)) {
        OVITO_ASSERT_MSG(false, "DataCollection::replaceObject", "Old data object not found.");
        return false;
    }
    if(newObj && !contains(newObj))
        replaceReferencesTo(oldObj, newObj);
    else
        clearReferencesTo(oldObj);
    return true;
}

/******************************************************************************
* Finds the first root-level object of the given class type in this collection.
* Return nullptr if not found.
******************************************************************************/
const DataObject* DataCollection::getObject(const DataObject::OOMetaClass& objectClass) const
{
    for(const DataObject* obj : objects()) {
        if(objectClass.isMember(obj))
            return obj;
    }
    return nullptr;
}

/******************************************************************************
* Throws an exception if the input does not contain a data object of the given type.
******************************************************************************/
const DataObject* DataCollection::expectObject(const DataObject::OOMetaClass& objectClass) const
{
    if(const DataObject* obj = getObject(objectClass))
        return obj;
    else {
        if(ExecutionContext::isInteractive()) {
            throw Exception(tr("The dataset does not contain an object of type: %1").arg(objectClass.displayName()));
        }
        else {
            throw Exception(tr("The input data collection contains no %1 data object.").arg(objectClass.displayName()));
        }
    }
}

/******************************************************************************
* Throws an exception if the input does not contain any a data object of the
* given type and under the given hierarchy path.
******************************************************************************/
const DataObject* DataCollection::expectLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const
{
    const DataObject* obj = getLeafObject(objectClass, pathString);
    if(!obj) {
        if(ExecutionContext::isInteractive()) {
            if(pathString.isEmpty())
                throw Exception(tr("The dataset does not contain an object of type: %1").arg(objectClass.displayName()));
            else
                throw Exception(tr("The dataset does not contain an object named '%2' of type '%1'.").arg(objectClass.displayName()).arg(pathString));
        }
        else {
            if(pathString.isEmpty())
                throw Exception(tr("No '%1' object in data collection.").arg(objectClass.displayName()));
            else
                throw Exception(tr("No '%1' object named '%2' in data collection.").arg(objectClass.displayName()).arg(pathString));
        }
    }
    return obj;
}

/******************************************************************************
* Ensures that a DataObject from this collection is not shared with others and is safe to modify.
******************************************************************************/
DataObject* DataCollection::makeMutable(const DataObject* obj)
{
    OVITO_ASSERT(contains(obj));
    if(!isSafeToModifySubObject(obj)) {
        OORef<DataObject> clone = CloneHelper::cloneSingleObject(obj, false);
        DataObject* clonedObj = clone.get();
        if(replaceObject(obj, std::move(clone))) {
            OVITO_ASSERT(clonedObj->isSafeToModify());
            return clonedObj;
        }
    }
    return const_cast<DataObject*>(obj);
}

/******************************************************************************
* Ensures that a DataObject from this collection is not shared with others and is safe to modify.
******************************************************************************/
DataObjectPath DataCollection::makeMutable(const ConstDataObjectPath& path)
{
    DataObjectPath result;
    DataObject* parent = this;
    for(const DataObject* obj : path) {
        if(obj == this) {
            OVITO_ASSERT(path.front() == this);
            result.push_back(this);
        }
        else {
            result.push_back(parent->makeMutable(obj));
        }
        parent = result.back();
    }
    return result;
}

/******************************************************************************
* Ensures that a DataObject from this collection is not shared with others and is safe to modify.
******************************************************************************/
DataObjectPath DataCollection::makeMutable(const ConstDataObjectPath& path, CloneHelper& cloneHelper)
{
    DataObjectPath result;
    DataObject* parent = this;
    for(const DataObject* obj : path) {
        if(obj == this) {
            OVITO_ASSERT(path.front() == this);
            result.push_back(this);
        }
        else {
            result.push_back(parent->makeMutable(obj, cloneHelper));
        }
        parent = result.back();
    }
    return result;
}

/******************************************************************************
* Finds an object of the given type and with the given identifier in the list
* of data objects stored in this collection.
******************************************************************************/
const DataObject* DataCollection::getObjectBy(const DataObject::OOMetaClass& objectClass, const PipelineNode* createdByNode, const QString& identifier) const
{
    OVITO_ASSERT(!identifier.isEmpty());
    if(!createdByNode)
        return nullptr;

    // Look for the data object with the given ID, or with the given ID followed
    // an enumeration index that was appended by generateUniqueIdentifier().
    for(const DataObject* obj : objects()) {
        if(objectClass.isMember(obj) && obj->createdByNode().lock().get() == createdByNode) {
            if(obj->identifier() == identifier || obj->identifier().startsWith(identifier + QChar('.')))
                return obj;
        }
    }
    return nullptr;
}

/******************************************************************************
* Part of the implementation of containsObjectRecursive()
******************************************************************************/
bool DataCollection::containsObjectRecursiveImpl(const DataObject* dataObj, const DataObject::OOMetaClass& objectClass)
{
    if(objectClass.isMember(dataObj))
        return true;

    // Recursively visit the sub-objects of the object.
    return dataObj->visitSubObjects([&objectClass](const DataObject* subObject) {
        return containsObjectRecursiveImpl(subObject, objectClass);
    });
}

/******************************************************************************
* Part of the implementation of getObjectsRecursive()
******************************************************************************/
void DataCollection::getObjectsRecursiveImpl(ConstDataObjectPath& path, const DataObject::OOMetaClass& objectClass, std::vector<ConstDataObjectPath>& results)
{
    if(objectClass.isMember(path.back()))
        results.push_back(path);

    // Recursively visit the sub-objects of the object.
    path.back()->visitSubObjects([&](const DataObject* subObject) {
        path.push_back(subObject);
        getObjectsRecursiveImpl(path, objectClass, results);
        path.pop_back();
        return false;
    });
}

/******************************************************************************
* Finds an object of the given type and under the hierarchy path in this collection.
******************************************************************************/
ConstDataObjectPath DataCollection::getObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const
{
    ConstDataObjectPath result;
    if(!pathString.isEmpty()) {
        // Perform a recursive path lookup of the requested object.
        for(const DataObject* obj : objects()) {
            result.push_back(obj);
            if(getObjectImpl(objectClass, pathString, result))
                break;
            result.pop_back();
        }
    }
    else {
        // Without any path, perform a recursive search for the first object of the given type.
        std::vector<ConstDataObjectPath> paths = getObjectsRecursive(objectClass);
        if(!paths.empty())
            result = paths.front();
    }
    return result;
}

/******************************************************************************
* Throws an exception if the input does not contain any a data object of the
* given type and under the given hierarchy path.
******************************************************************************/
ConstDataObjectPath DataCollection::expectObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const
{
    ConstDataObjectPath path = getObject(objectClass, pathString);
    if(path.empty()) {
        if(ExecutionContext::isInteractive()) {
            if(pathString.isEmpty())
                throw Exception(tr("The dataset does not contain an object of type: %1").arg(objectClass.displayName()));
            else
                throw Exception(tr("The dataset does not contain an object named '%2' of type '%1'.").arg(objectClass.displayName()).arg(pathString));
        }
        else {
            if(pathString.isEmpty())
                throw Exception(tr("No '%1' data object in data collection.").arg(objectClass.displayName()));
            else
                throw Exception(tr("No '%1' data object named '%2' in data collection.").arg(objectClass.displayName()).arg(pathString));
        }
    }
    return path;
}

/******************************************************************************
* Throws an exception if the input does not contain any a data object of the
* given type and under the given hierarchy path.
******************************************************************************/
DataObjectPath DataCollection::expectMutableObject(const DataObject::OOMetaClass& objectClass, const QString& pathString)
{
    DataObjectPath path = getMutableObject(objectClass, pathString);
    if(path.empty()) {
        if(ExecutionContext::isInteractive()) {
            if(pathString.isEmpty())
                throw Exception(tr("The dataset does not contain an object of type: %1").arg(objectClass.displayName()));
            else
                throw Exception(tr("The dataset does not contain an object named '%2' of type '%1'.").arg(objectClass.displayName()).arg(pathString));
        }
        else {
            if(pathString.isEmpty())
                throw Exception(tr("No '%1' data object in data collection.").arg(objectClass.displayName()));
            else
                throw Exception(tr("No '%1' data object named '%2' in data collection.").arg(objectClass.displayName()).arg(pathString));
        }
    }
    return path;
}

/******************************************************************************
* Throws an exception if the input does not contain any a data object of the
* given type and under the given hierarchy path.
******************************************************************************/
DataObject* DataCollection::expectMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString)
{
    DataObjectPath path = expectMutableObject(objectClass, pathString);
    OVITO_ASSERT(!path.empty());
    return path.back();
}

/******************************************************************************
* Implementation detail of getObject().
******************************************************************************/
bool DataCollection::getObjectImpl(const DataObject::OOMetaClass& objectClass, QStringView pathString, ConstDataObjectPath& path)
{
    const DataObject* object = path.back();
    if(pathString.isEmpty()) {
        if(objectClass.isMember(object)) return true;
        if(!object->identifier().isEmpty()) return false;
        return object->visitSubObjects([&](const DataObject* subObject) {
            path.push_back(subObject);
            if(getObjectImpl(objectClass, pathString, path))
                return true;
            path.pop_back();
            return false;
        });
    }
    else {
        qsizetype separatorPos = pathString.indexOf(QChar('/'));
        if(separatorPos == -1) {
            if(object->identifier() != pathString) return false;
            if(objectClass.isMember(object)) return true;
            return object->visitSubObjects([&](const DataObject* subObject) {
                path.push_back(subObject);
                if(getObjectImpl(objectClass, {}, path))
                    return true;
                path.pop_back();
                return false;
            });
        }
        else if(object->identifier() == pathString.left(separatorPos)) {
            QStringView subPath = pathString.mid(separatorPos + 1);
            return object->visitSubObjects([&](const DataObject* subObject) {
                path.push_back(subObject);
                if(getObjectImpl(objectClass, subPath, path))
                    return true;
                path.pop_back();
                return false;
            });
        }
        else return false;
    }
}

/******************************************************************************
* Finds an object of the given type and under the hierarchy path in this collection.
******************************************************************************/
const DataObject* DataCollection::getLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const
{
    if(!pathString.isEmpty()) {
        for(const DataObject* obj : objects()) {
            if(const DataObject* result = getLeafObjectImpl(objectClass, pathString, obj))
                return result;
        }
        return nullptr;
    }
    else {
        // Without any path, perform a recursive search for the first object of the given type.
        std::vector<ConstDataObjectPath> paths = getObjectsRecursive(objectClass);
        if(!paths.empty())
            return paths.front().back();
        else
            return {};
    }
}

/******************************************************************************
* Implementation detail of getLeafObject().
******************************************************************************/
const DataObject* DataCollection::getLeafObjectImpl(const DataObject::OOMetaClass& objectClass, QStringView pathString, const DataObject* parent)
{
    if(pathString.isEmpty()) {
        if(objectClass.isMember(parent)) return parent;
        if(!parent->identifier().isEmpty()) return nullptr;
        const DataObject* result = nullptr;
        parent->visitSubObjects([&](const DataObject* subObject) {
            result = getLeafObjectImpl(objectClass, pathString, subObject);
            return result != nullptr;
        });
        return result;
    }
    else {
        qsizetype separatorPos = pathString.indexOf(QChar('/'));
        if(separatorPos == -1) {
            if(objectClass.isMember(parent) && parent->identifier() == pathString)
                return parent;
        }
        else if(parent->identifier() == pathString.left(separatorPos)) {
            QStringView subPath = pathString.mid(separatorPos + 1);
            const DataObject* result = nullptr;
            parent->visitSubObjects([&](const DataObject* subObject) {
                result = getLeafObjectImpl(objectClass, subPath, subObject);
                return result != nullptr;
            });
            return result;
        }
        return nullptr;
    }
}

/******************************************************************************
* Finds an object of the given type and under the hierarchy path in this collection.
* Duplicates it, and all its parent objects, if needed so that it can be safely
* modified without unwanted side effects.
******************************************************************************/
DataObjectPath DataCollection::getMutableObject(const DataObject::OOMetaClass& objectClass, const QString& pathString)
{
    // First, determine the full path to the object we are searching for.
    const ConstDataObjectPath path = getObject(objectClass, pathString);
    DataObjectPath resultPath;

    // If found, clone the object as well as all parents up the hierarchy.
    if(!path.empty()) {
        resultPath.resize(path.size());
        resultPath.front() = makeMutable(path.front());
        for(int index = 1; index < path.size(); index++) {
            resultPath[index] = resultPath[index - 1]->makeMutable(path[index]);
        }
    }
    return resultPath;
}

/******************************************************************************
* Finds an object of the given type and under the hierarchy path in this flow state.
* Duplicates it, and all its parent objects, if needed so that it can be safely
* modified without unwanted side effects.
******************************************************************************/
DataObject* DataCollection::getMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString)
{
    DataObjectPath path = getMutableObject(objectClass, pathString);
    return path.empty() ? nullptr : path.back();
}

/******************************************************************************
* Builds a list of the global attributes stored in this pipeline state.
******************************************************************************/
QVariantMap DataCollection::buildAttributesMap() const
{
    QVariantMap attributes;
    for(const DataObject* obj : objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(!attributes.contains(attribute->identifier())) {
                attributes.insert(attribute->identifier(), attribute->value());
            }
            else {
                for(int counter = 2; ; counter++) {
                    QString uniqueName = attribute->identifier() + QChar('.') + QString::number(counter);
                    if(!attributes.contains(uniqueName)) {
                        attributes.insert(uniqueName, attribute->value());
                        break;
                    }
                }
            }
        }
    }
    return attributes;
}

/******************************************************************************
* Looks up the value for the given global attribute.
* Returns a given default value if the attribute is not defined in this pipeline state.
******************************************************************************/
QVariant DataCollection::getAttributeValue(const QString& attrName, const QVariant& defaultValue) const
{
    for(const DataObject* obj : objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(attribute->identifier() == attrName)
                return attribute->value();
        }
    }
    return defaultValue;
}

/******************************************************************************
* Looks up the value for the global attribute with the given base name and creator.
* Returns a given default value if the attribute is not defined in this pipeline state.
******************************************************************************/
QVariant DataCollection::getAttributeValue(const PipelineNode* createdByNode, const QString& attrBaseName, const QVariant& defaultValue) const
{
    if(const AttributeDataObject* attribute = getObjectBy<AttributeDataObject>(createdByNode, attrBaseName))
        return attribute->value();
    else
        return defaultValue;
}

/******************************************************************************
* Inserts a new global attribute into the pipeline state.
******************************************************************************/
AttributeDataObject* DataCollection::addAttribute(const QString& key, QVariant value, OOWeakRef<const PipelineNode> createdByNode)
{
    return createObject<AttributeDataObject>(key, std::move(createdByNode), std::move(value));
}

/******************************************************************************
* Inserts a new global attribute into the pipeline state overwritting any
* existing attribute with the same name.
******************************************************************************/
AttributeDataObject* DataCollection::setAttribute(const QString& key, QVariant value, OOWeakRef<const PipelineNode> createdByNode)
{
    for(const DataObject* obj : objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(attribute->identifier() == key) {
                AttributeDataObject* newAttribute = makeMutable(attribute);
                newAttribute->setValue(std::move(value));
                newAttribute->setCreatedByNode(createdByNode);
                return newAttribute;
            }
        }
    }
    return createObject<AttributeDataObject>(key, std::move(createdByNode), std::move(value));
}

/******************************************************************************
* Copies all global attribute created by the given pipeline node over to this
* data collection.
******************************************************************************/
void DataCollection::adoptAttributesFrom(const DataCollection& other, const OOWeakRef<const PipelineNode>& createdByNode)
{
    for(const DataObject* obj : other.objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(attribute->createdByNode() == createdByNode) {
                addObject(attribute);
            }
        }
    }
}

/******************************************************************************
* Returns a new unique data object identifier that does not collide with the
* identifiers of any existing data object of the given type in the same data
* collection.
******************************************************************************/
QString DataCollection::generateUniqueIdentifier(const QString& baseName, const OvitoClass& dataObjectClass) const
{
    // This helper function checks if the given id is already used by another data object of the
    // given type in the this data collection.
    auto doesExist = [this,&dataObjectClass](const QString& id) {
        for(const DataObject* obj : objects()) {
            if(dataObjectClass.isMember(obj)) {
                if(obj->identifier() == id)
                    return true;
            }
        }
        return false;
    };

    if(!doesExist(baseName)) {
        return baseName;
    }
    else {
        // Append consecutive indices to the base ID name.
        for(int i = 2; ; i++) {
            QString uniqueId = baseName + QChar('.') + QString::number(i);
            if(!doesExist(uniqueId)) {
                return uniqueId;
            }
        }
    }
    OVITO_ASSERT(false);
}

/******************************************************************************
* Returns the source frame number associated with this pipeline state.
* If the data does not originate from a pipeline with a FileSource, returns -1.
******************************************************************************/
int DataCollection::sourceFrame() const
{
    return getAttributeValue(QStringLiteral("SourceFrame"), -1).toInt();
}

}   // End of namespace
