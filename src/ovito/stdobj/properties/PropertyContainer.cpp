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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/core/dataset/DataSet.h>
#include "PropertyContainer.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PropertyContainer);
DEFINE_VECTOR_REFERENCE_FIELD(PropertyContainer, properties);
DEFINE_PROPERTY_FIELD(PropertyContainer, elementCount);
DEFINE_PROPERTY_FIELD(PropertyContainer, title);
DEFINE_SHADOW_PROPERTY_FIELD(PropertyContainer, title);
SET_PROPERTY_FIELD_LABEL(PropertyContainer, properties, "Properties");
SET_PROPERTY_FIELD_LABEL(PropertyContainer, elementCount, "Element count");
SET_PROPERTY_FIELD_LABEL(PropertyContainer, title, "Title");
SET_PROPERTY_FIELD_CHANGE_EVENT(PropertyContainer, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
PropertyContainer::PropertyContainer(ObjectInitializationFlags flags, const QString& title) : DataObject(flags),
    _elementCount(0),
    _title(title)
{
    if(!title.isEmpty())
        freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(PropertyContainer::title)});
}

/******************************************************************************
* Returns the display title of this object.
******************************************************************************/
QString PropertyContainer::objectTitle() const
{
    if(!title().isEmpty())
        return title();
    else
        return DataObject::objectTitle();
}

/******************************************************************************
* Returns the given standard property. If it does not exist, an exception is thrown.
******************************************************************************/
const Property* PropertyContainer::expectProperty(int typeId) const
{
    if(!getOOMetaClass().isValidStandardPropertyId(typeId))
        throw Exception(tr("Selections are not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
    const Property* property = getProperty(typeId);
    if(!property) {
        if(typeId == Property::GenericSelectionProperty)
            throw Exception(tr("The operation requires an input %1 selection.").arg(getOOMetaClass().elementDescriptionName()));
        else
            throw Exception(tr("Required %2 property '%1' does not exist in the input dataset.").arg(getOOMetaClass().standardPropertyName(typeId), getOOMetaClass().elementDescriptionName()));
    }
    if(property->size() != elementCount())
        throw Exception(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent container.").arg(property->name()));
    return property;
}

/******************************************************************************
* Returns the property with the given name and data layout.
******************************************************************************/
const Property* PropertyContainer::expectProperty(const QString& propertyName, int dataType, size_t componentCount) const
{
    const Property* property = getProperty(propertyName);
    if(!property)
        throw Exception(tr("Required property '%1' does not exist in the input dataset.").arg(propertyName));
    if(property->dataType() != dataType)
        throw Exception(tr("Property '%1' does not have the required data type in the pipeline dataset.").arg(property->name()));
    if(property->componentCount() != componentCount)
        throw Exception(tr("Property '%1' does not have the required number of components in the pipeline dataset.").arg(property->name()));
    if(property->size() != elementCount())
        throw Exception(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent container.").arg(property->name()));
    return property;
}

/******************************************************************************
* Duplicates and replaces the given property with its copy if it not exclusively
* owned by this container or is being accessed from Python. If a copy is being
* made, the payload data of the Property is NOT copied over.
* This method offers a performance benefit in situations where the calling code is going to
* completely overwrite the data in the mutable property with new values.
******************************************************************************/
Property* PropertyContainer::makePropertyMutable(const Property* property, DataBuffer::BufferInitialization cloneMode, bool ignorePythonAccess)
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(!property || properties().contains(property));

    // Always clone property if its memory is currently being accessed from Python code.
    // That's required, because Python value holders are not strong DataOORefs protecting the property object from changes.
    if(property && ((property->isBeingAccessedFromPython() && !ignorePythonAccess) || !isSafeToModifySubObject(property))) {
        DataOORef<Property> clone;
        if(cloneMode == DataBuffer::Initialized) {
            clone = CloneHelper::cloneSingleObject(property, false);
        }
        else {
            // Custom clone implementation, which copies only the metadata but not the contents of the original property.
            clone = DataOORef<Property>::create(
                ObjectInitializationFlag::DontInitializeObject, DataBuffer::Uninitialized, property->size(), property->dataType(),
                property->componentCount(), property->name(), property->type(), property->componentNames());
            {
                UndoSuspender noUndo;
                clone->setVisElements(property->visElements());
                clone->setElementTypes(property->elementTypes());
                clone->setTitle(property->title());
                clone->setCreatedByNode(property->createdByNode());
            }
        }
        replaceReferencesTo(property, clone);
        OVITO_ASSERT(hasReferenceTo(clone));
        OVITO_ASSERT(!hasReferenceTo(property));
        property = clone;
    }

    return const_cast<Property*>(property);
}

/******************************************************************************
* Duplicates and replaces the given property with its copy if it not exclusively
* owned by this container or is being accessed from Python. This method is
* similar to the DataObject::makeMutable() method, but won't copy the contents
* of the Property nor allocate memory for the new array.
******************************************************************************/
Property* PropertyContainer::makePropertyMutableUnallocated(const Property* property)
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_CHECK_OBJECT_POINTER(property);
    OVITO_ASSERT(hasReferenceTo(property));

    // Always clone property if it is currently being accessed from Python code.
    // That's required, because Python code never holds a strong DataOORef to the property object.
    if(property->isBeingAccessedFromPython() || !isSafeToModifySubObject(property)) {
        // Custom clone implementation, which copies only the metadata but not the contents of the original property.
        DataOORef<Property> clone = DataOORef<Property>::create(
                ObjectInitializationFlag::DontInitializeObject, DataBuffer::Uninitialized, 0, property->dataType(),
                property->componentCount(), property->name(), property->type(), property->componentNames());
        {
            UndoSuspender noUndo;
            clone->setVisElements(property->visElements());
            clone->setElementTypes(property->elementTypes());
            clone->setTitle(property->title());
            clone->setCreatedByNode(property->createdByNode());
        }
        replaceReferencesTo(property, clone);
        OVITO_ASSERT(hasReferenceTo(clone));
        OVITO_ASSERT(!hasReferenceTo(property));
        property = clone;
    }

    return const_cast<Property*>(property);
}

/******************************************************************************
* Duplicates any property objects that are shared with other containers or being accessed from Python.
* After this method returns, all property objects are exclusively owned by the container and
* can be safely modified without unwanted side effects.
******************************************************************************/
void PropertyContainer::makePropertiesMutableInternal()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(isSafeToModify());

    for(const Property* property : properties()) {
        makePropertyMutable(property, DataBuffer::Initialized);
    }
}

/******************************************************************************
* Sets the current number of data elements stored in the container.
* The lengths of the property arrays will be adjusted accordingly.
******************************************************************************/
void PropertyContainer::setElementCount(size_t count)
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(isSafeToModify());

    if(count != elementCount()) {

        // Resize each property array in the container.
        for(OORef<const Property> property : properties()) {
            makePropertyMutableUnallocated(property)->resizeCopyFrom(count, *property);
        }

        // Update internal element counter.
        _elementCount.set(this, PROPERTY_FIELD(elementCount), count);
    }

#ifdef OVITO_DEBUG
    verifyIntegrity();
#endif
}

/******************************************************************************
* Clones all properties in the container and newly allocates memory for all property arrays, possibly with a
* different element count than before. It's the callers responsibility to initialize the new property arrays.
******************************************************************************/
std::vector<std::pair<ConstPropertyPtr, Property*>> PropertyContainer::reallocateProperties(size_t numElements)
{
    std::vector<std::pair<ConstPropertyPtr, Property*>> result;

    // Note: Using strong-ref ConstPropertyPtr here to force makePropertyMutableUnallocated() into cloning the property.
    for(ConstPropertyPtr property : properties()) {
        Property* newProperty = makePropertyMutableUnallocated(property);
        OVITO_ASSERT(newProperty != property);
        newProperty->resize(numElements, false);
        result.emplace_back(std::move(property), newProperty);
    }

    // Update internal element counter.
    _elementCount.set(this, PROPERTY_FIELD(elementCount), numElements);

#ifdef OVITO_DEBUG
    verifyIntegrity();
#endif

    return result;
}

/******************************************************************************
* Deletes those data elements having a non-zero value in the given selection array.
* Returns the number of deleted elements. The original order of the remaining elements is preserved.
******************************************************************************/
size_t PropertyContainer::deleteElements(ConstDataBufferPtr selection, size_t selectionCount)
{
    OVITO_ASSERT(selection);
    OVITO_ASSERT(selection->size() == elementCount());
    OVITO_ASSERT(selection->dataType() == DataBuffer::IntSelection);
    OVITO_ASSERT(selection->componentCount() == 1);
    OVITO_ASSERT(!hasReferenceTo(selection));
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(isSafeToModify());

    // Determine number of selected elements in the selection array if it wasn't provided by the caller.
    if(selectionCount == std::numeric_limits<size_t>::max()) {
        selectionCount = 0;
        for(auto s : BufferReadAccess<SelectionIntType>(selection)) {
            if(s)
                selectionCount++;
        }
    }
    if(selectionCount == 0)
        return 0;   // Nothing to delete.

    OVITO_ASSERT(selectionCount <= elementCount());
    const size_t newElementCount = elementCount() - selectionCount;

    // Filter the property arrays and reduce their lengths.
    for(OORef<const Property> property : properties()) {
        makePropertyMutableUnallocated(property)->filterResizeCopyFrom(newElementCount, *selection, *property);
    }

    // Update internal element counter.
    _elementCount.set(this, PROPERTY_FIELD(elementCount), newElementCount);

#ifdef OVITO_DEBUG
    verifyIntegrity();
#endif

    return selectionCount;
}

/******************************************************************************
* Creates a property and adds it to the container.
* In case the property already exists, it is made sure that it's safe to modify it.
******************************************************************************/
Property* PropertyContainer::createProperty(DataBuffer::BufferInitialization init, int typeId, const ConstDataObjectPath& containerPath)
{
    OVITO_ASSERT(isSafeToModify());

    if(getOOMetaClass().isValidStandardPropertyId(typeId) == false) {
        if(typeId == Property::GenericSelectionProperty)
            throw Exception(tr("Creating selections is not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
        else if(typeId == Property::GenericColorProperty)
            throw Exception(tr("Assigning colors is not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
        else
            throw Exception(tr("%1 is not a standard property ID supported by the '%2' object class.").arg(typeId).arg(getOOMetaClass().propertyClassDisplayName()));
    }

    // Check if property already exists in the output.
    if(const Property* existingProperty = getProperty(typeId)) {
        OVITO_ASSERT(existingProperty->size() == elementCount());
        return makePropertyMutable(existingProperty, init);
    }
    else {
        // Create a new property object.
        PropertyPtr newProperty = getOOMetaClass().createStandardProperty(init, elementCount(), typeId, containerPath);
        addProperty(newProperty);
        return newProperty;
    }
}

/******************************************************************************
* Creates a user-defined property and adds it to the container.
* In case the property already exists, it is made sure that it's safe to modify it.
******************************************************************************/
Property* PropertyContainer::createProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount, QStringList componentNames)
{
    OVITO_ASSERT(isSafeToModify());

    // Check if property already exists in the output.
    const Property* existingProperty = getProperty(name);

    // Check if property already exists in the output.
    if(existingProperty) {
        OVITO_ASSERT(existingProperty->size() == elementCount());
        if(existingProperty->dataType() != dataType)
            throw Exception(tr("Existing property '%1' has a different data type.").arg(name));
        if(existingProperty->componentCount() != componentCount)
            throw Exception(tr("Existing property '%1' has a different number of components.").arg(name));
        return makePropertyMutable(existingProperty, init);
    }
    else {
        // Create a new property object.
        PropertyPtr newProperty = getOOMetaClass().createUserProperty(init, elementCount(), dataType, componentCount, name, 0, std::move(componentNames));
        addProperty(newProperty);
        return newProperty;
    }
}

/******************************************************************************
* Adds a property object to the container, replacing any preexisting property
* in the container with the same type.
******************************************************************************/
const Property* PropertyContainer::createProperty(const Property* property)
{
    OVITO_CHECK_POINTER(property);
    OVITO_ASSERT(isSafeToModify());
    OVITO_ASSERT(property->type() == 0 || getOOMetaClass().isValidStandardPropertyId(property->type()));

    // Length of first property array determines number of data elements in the container.
    if(properties().empty() && elementCount() == 0)
        _elementCount.set(this, PROPERTY_FIELD(elementCount), property->size());

    // Length of new property array must match the existing number of elements.
    if(property->size() != elementCount()) {
#ifdef OVITO_DEBUG
        qDebug() << "Property array size mismatch. Container has" << elementCount() << "existing elements. New property" << property->name() << "to be added has" << property->size() << "elements.";
#endif
        throw Exception(tr("Cannot add new %1 property '%2': Array length is not consistent with number of elements in the parent container.").arg(getOOMetaClass().propertyClassDisplayName()).arg(property->name()));
    }

    // Check if the same property already exists in the container.
    const Property* existingProperty;
    if(property->type() != 0) {
        existingProperty = getProperty(property->type());
    }
    else {
        existingProperty = nullptr;
        for(const Property* p : properties()) {
            if(p->type() == 0 && p->name() == property->name()) {
                existingProperty = p;
                break;
            }
        }
    }

    if(existingProperty) {
        replaceReferencesTo(existingProperty, property);
    }
    else {
        OVITO_ASSERT(properties().contains(const_cast<Property*>(property)) == false);
        addProperty(property);
    }
    return property;
}

/******************************************************************************
* Replaces the property arrays in this property container with a new set of
* properties. Existing element types of typed properties will be preserved by
* the method.
******************************************************************************/
void PropertyContainer::setContent(size_t newElementCount, const DataRefVector<Property>& newProperties)
{
    // Lengths of new property arrays must be consistent.
    for(const Property* property : newProperties) {
        OVITO_ASSERT(!properties().contains(property));
        if(property->size() != newElementCount) {
            OVITO_ASSERT(false);
            throw Exception(tr("Cannot add new %1 property '%2': Array length does not match number of elements in the parent container.").arg(getOOMetaClass().propertyClassDisplayName()).arg(property->name()));
        }
    }

    // Removal phase:
    _properties.clear(this, PROPERTY_FIELD(properties));

    // Update internal element counter.
    _elementCount.set(this, PROPERTY_FIELD(elementCount), newElementCount);

    // Insertion phase:
    _properties.setTargets(this, PROPERTY_FIELD(properties), std::move(newProperties));
}

/******************************************************************************
* Duplicates all data elements by extensing the property arrays and
* replicating the existing data N times.
******************************************************************************/
void PropertyContainer::replicate(size_t n)
{
    OVITO_ASSERT(n >= 1);
    if(n <= 1)
        return;

    size_t newCount = elementCount() * n;
    if(newCount / n != elementCount())
        throw Exception(tr("Replicate operation failed: Maximum number of elements exceeded."));

    for(auto [oldProperty, newProperty] : reallocateProperties(newCount)) {
        newProperty->replicateFrom(n, *oldProperty);
    }
}

/******************************************************************************
* Sorts the data elements with respect to their unique IDs.
* Does nothing if data elements do not have IDs.
******************************************************************************/
std::vector<size_t> PropertyContainer::sortById()
{
#ifdef OVITO_DEBUG
    verifyIntegrity();
#endif
    if(!getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        return {};
    BufferReadAccess<IdentifierIntType> ids = getProperty(Property::GenericIdentifierProperty);
    if(!ids)
        return {};

    // Determine new permutation of data elements which sorts them by ascending ID.
    std::vector<size_t> permutation(ids.size());
    std::iota(permutation.begin(), permutation.end(), (size_t)0);
    std::sort(permutation.begin(), permutation.end(), [&](size_t a, size_t b) { return ids[a] < ids[b]; });
    std::vector<size_t> invertedPermutation(ids.size());
    bool isAlreadySorted = true;
    for(size_t i = 0; i < permutation.size(); i++) {
        invertedPermutation[permutation[i]] = i;
        if(permutation[i] != i) isAlreadySorted = false;
    }
    ids.reset();
    if(isAlreadySorted)
        return {};

    // Re-order all values in the property arrays.
    makePropertiesMutableInternal();
    for(const Property* prop : properties()) {
        const_cast<Property*>(prop)->reorderElements(permutation);
    }

    OVITO_ASSERT(boost::range::is_sorted(BufferReadAccess<IdentifierIntType>(getProperty(Property::GenericIdentifierProperty)).range()));

    return invertedPermutation;
}

/******************************************************************************
* Makes sure that all property arrays in this container have a consistent length.
* If this is not the case, the method throws an exception.
******************************************************************************/
void PropertyContainer::verifyIntegrity() const
{
    size_t c = elementCount();
    for(const Property* property : properties()) {
//      OVITO_ASSERT_MSG(property->size() == c, "PropertyContainer::verifyIntegrity()", qPrintable(QString("Property array '%1' has wrong length. It does not match the number of elements in the parent %2 container.").arg(property->name()).arg(getOOMetaClass().propertyClassDisplayName())));
        if(property->size() != c) {
            throw Exception(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent %2 container.").arg(property->name()).arg(getOOMetaClass().propertyClassDisplayName()));
        }
    }
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void PropertyContainer::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    DataObject::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x01);
    stream << excludeRecomputableData;
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void PropertyContainer::loadFromStream(ObjectLoadStream& stream)
{
    DataObject::loadFromStream(stream);
    if(stream.formatVersion() >= 30004) {
        stream.expectChunk(0x01);
        bool excludeRecomputableData;
        stream >> excludeRecomputableData;
        if(excludeRecomputableData) {
            // Reset internal element counter.
            _elementCount.set(this, PROPERTY_FIELD(elementCount), 0);
        }
        stream.closeChunk();
    }
    // This is needed only for backward compatibility with early dev builds of OVITO 3.0:
    if(identifier().isEmpty())
        setIdentifier(getOOMetaClass().pythonName());
}

/******************************************************************************
* Is called once for this object after it has been completely loaded from a stream.
******************************************************************************/
void PropertyContainer::loadFromStreamComplete(ObjectLoadStream& stream)
{
    DataObject::loadFromStreamComplete(stream);

    // For backward compatibility with old OVITO versions.
    // Make sure sizes of deserialized property arrays are consistent.
    if(stream.formatVersion() < 30004) {
        for(const Property* property : properties()) {
            if(property->size() != elementCount()) {
                makeMutable(property)->resize(elementCount(), true);
            }
        }
    }

    // For backward compatibility with OVITO 3.3.5:
    // The ElementType::ownerProperty parameter field did not exist in older OVITO versions and does not have
    // a valid value when loaded from a state file. The following code initializes the parameter field to
    // a meaningful value.
    if(stream.formatVersion() < 30007) {
        for(const Property* property : properties()) {
            for(const ElementType* type : property->elementTypes()) {
                if(type->ownerProperty().isNull()) {
                    const_cast<ElementType*>(type)->_ownerProperty.set(const_cast<ElementType*>(type), PROPERTY_FIELD(ElementType::ownerProperty), PropertyReference(&OOClass(), property));
                }
                if(ElementType* proxyType = dynamic_object_cast<ElementType>(type->editableProxy())) {
                    if(proxyType->ownerProperty().isNull())
                        proxyType->_ownerProperty.set(proxyType, PROPERTY_FIELD(ElementType::ownerProperty), type->ownerProperty());
                }
            }
        }
    }

    // For backward compatibility with older OVITO versions that only knew FloatType.
    // Perform data type conversion if necessary.
    if(stream.formatVersion() < 30010) {
        for(const Property* property : properties()) {
            if(property->type() != 0) {
                int expectedDataType = getOOMetaClass().standardPropertyDataType(property->type());
                if(property->dataType() != expectedDataType)
                    makeMutable(property)->convertToDataType(expectedDataType);
            }
        }
    }
}

/******************************************************************************
* Generates the info string to be displayed in the OVITO status bar for an element from this container.
******************************************************************************/
QString PropertyContainer::elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path) const
{
    QString str;
    for(const Property* property : properties()) {
        if(property->size() <= elementIndex) continue;
        if(property->type() == Property::GenericSelectionProperty) continue;
        if(property->type() == Property::GenericColorProperty) continue;
        if(!str.isEmpty()) str += QStringLiteral("<sep>");
        str += QStringLiteral("<key>");
        str += property->name().toHtmlEscaped();
        str += QStringLiteral(":</key> <val>");
        if(property->dataType() == Property::Int32) {
            BufferReadAccess<int*> data(property);
            for(size_t component = 0; component < data.componentCount(); component++) {
                if(component != 0) str += QStringLiteral(", ");
                str += QString::number(data.get(elementIndex, component));
                if(property->elementTypes().empty() == false) {
                    if(const ElementType* ptype = property->elementType(data.get(elementIndex, component))) {
                        if(!ptype->name().isEmpty())
                            str += QString(" (%1)").arg(ptype->name().toHtmlEscaped());
                    }
                }
            }
        }
        else if(property->dataType() == Property::Int64) {
            BufferReadAccess<int64_t*> data(property);
            for(size_t component = 0; component < property->componentCount(); component++) {
                if(component != 0) str += QStringLiteral(", ");
                str += QString::number(data.get(elementIndex, component));
            }
        }
        else if(property->dataType() == Property::Float32) {
            BufferReadAccess<float*> data(property);
            for(size_t component = 0; component < property->componentCount(); component++) {
                if(component != 0) str += QStringLiteral(", ");
                str += QString::number(data.get(elementIndex, component));
            }
        }
        else if(property->dataType() == Property::Float64) {
            BufferReadAccess<double*> data(property);
            for(size_t component = 0; component < property->componentCount(); component++) {
                if(component != 0) str += QStringLiteral(", ");
                str += QString::number(data.get(elementIndex, component));
            }
        }
        else {
            str += QStringLiteral("<%1>").arg(property->dataTypeName());
        }
        str += QStringLiteral("</val>");
    }
    return str;
}

}   // End of namespace
