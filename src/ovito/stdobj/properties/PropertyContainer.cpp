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
void PropertyContainer::initializeObject(ObjectInitializationFlags flags, const QString& title)
{
    DataObject::initializeObject(flags);

    if(!title.isEmpty()) {
        setTitle(title);
        freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(PropertyContainer::title)});
    }
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
const Property* PropertyContainer::expectProperty(const QStringView propertyName, int dataType, size_t componentCount) const
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
* Looks up the named property in the container and resolves the specified
* vector component (if any). If lookup fails, an error message is returned
* in the errorDescription parameter.
******************************************************************************/
std::pair<const Property*, int> PropertyContainer::findPropertyWithComponent(const QStringView nameWithComponent, QString& errorDescription, bool requireComponent) const
{
    // Split property name into base name and vector component.
    QList<QStringView> parts = nameWithComponent.split(QChar('.'));
    if(parts.length() > 2) {
        errorDescription = tr("The property name '%1' contains too many dots.").arg(nameWithComponent);
        return { nullptr, -1 };
    }
    else if(parts.length() == 0 || parts[0].isEmpty()) {
        errorDescription = tr("Property name is empty.");
        return { nullptr, -1 };
    }

    // Look up the property by name.
    const QStringView& name = parts[0];
    const Property* property = getProperty(name);
    if(!property) {

        // Property not found.
        // As a fallback, look for a property that includes the vector component suffix in its name.
        // Such technically illegal property names may be produced by the SpatialBinningModifier for example.
        property = getProperty(nameWithComponent);
        if(property && property->componentCount() == 1) {
            return { property, requireComponent ? 0 : -1 };
        }

        errorDescription = tr("The %1 property with the name '%2' does not exist or has not been computed by the pipeline.").arg(getOOMetaClass().elementDescriptionName()).arg(name);
        return { nullptr, -1 };
    }

    int resolvedVectorComponent = -1;
    if(parts.size() == 2 && !parts[1].isEmpty()) {
        const QStringView& vectorComponentName = parts[1];
        if(!property->componentNames().empty()) {
            resolvedVectorComponent = property->componentNames().indexOf(vectorComponentName);
            if(resolvedVectorComponent < 0) {
                errorDescription = tr("The selected vector property component '%1' is invalid. Property '%2' has the following named components: %3")
                    .arg(vectorComponentName)
                    .arg(property->name())
                    .arg(property->componentNames().join(QStringLiteral(", ")));
                return { nullptr, -1 };
            }
        }
        else {
            bool ok;
            resolvedVectorComponent = vectorComponentName.toInt(&ok) - 1;
            if(ok) {
                if(resolvedVectorComponent < 0 || resolvedVectorComponent >= property->componentCount()) {
                    errorDescription = tr("The selected vector property component '%1' is out of range. Property '%2' has %3 component(s).")
                        .arg(vectorComponentName)
                        .arg(property->name())
                        .arg(property->componentCount());
                    return { nullptr, -1 };
                }
            }
            else {
                errorDescription = Property::tr("The selected vector property component '%1' cannot be resolved, because property '%2' does not have named components.")
                    .arg(vectorComponentName)
                    .arg(property->name());
                return { nullptr, -1 };
            }
        }
    }

    if(resolvedVectorComponent >= (int)property->componentCount()) {
        errorDescription = tr("The selected vector property component is out of range. The property '%1' has only %2 values per data element.")
                                        .arg(property->name())
                                        .arg(resolvedVectorComponent);
        return { nullptr, -1 };
    }

    if(requireComponent && resolvedVectorComponent < 0)
        resolvedVectorComponent = 0;

    return { property, resolvedVectorComponent };
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
    if(property && ((property->isBeingAccessedExternally() && !ignorePythonAccess) || !isSafeToModifySubObject(property))) {
        PropertyPtr clone;
        if(cloneMode == DataBuffer::Initialized) {
            clone = CloneHelper::cloneSingleObject(property, false);
        }
        else {
            // Custom clone implementation, which copies only the metadata but not the contents of the original property.
            clone = property->cloneWithoutData(property->size());
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
    if(property->isBeingAccessedExternally() || !isSafeToModifySubObject(property)) {
        // Custom clone implementation, which copies only the metadata but not the contents of the original property.
        PropertyPtr clone = property->cloneWithoutData(0);
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

    if(elementCount() == 0)
        return 0;

    // Determine number of selected elements in the selection array if it wasn't provided by the caller.
    if(selectionCount == std::numeric_limits<size_t>::max())
        selectionCount = selection->nonzeroCount();
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
Property* PropertyContainer::createProperty(DataBuffer::BufferInitialization init, const QStringView name, int dataType, size_t componentCount, QStringList componentNames)
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
const Property* PropertyContainer::createProperty(ConstPropertyPtr property)
{
    OVITO_CHECK_POINTER(property);
    OVITO_ASSERT(isSafeToModify());
    OVITO_ASSERT(!property->isStandardProperty() || getOOMetaClass().isValidStandardPropertyId(property->typeId()));

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
    if(property->isStandardProperty()) {
        existingProperty = getProperty(property->typeId());
    }
    else {
        existingProperty = nullptr;
        for(const Property* p : properties()) {
            if(!p->isStandardProperty() && p->name() == property->name()) {
                existingProperty = p;
                break;
            }
        }
    }

    const Property* ptr = property.get();
    if(existingProperty) {
        replaceReferencesTo(existingProperty, property);
    }
    else {
        OVITO_ASSERT(properties().contains(const_cast<Property*>(ptr)) == false);
        addProperty(std::move(property));
    }
    return ptr;
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
* Duplicates all data elements by extending the property arrays and
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
* Performs a numeric data type conversion of a property (unless the property already has the requested type).
******************************************************************************/
const Property* PropertyContainer::convertPropertyToDataType(const ConstPropertyPtr& property, int dataType)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(properties().contains(property.get()));
    OVITO_ASSERT(!property->isStandardProperty() || dataType == getOOMetaClass().standardPropertyDataType(property->typeId()));

    if(property->dataType() != dataType) {
        PropertyPtr convertedProperty = property->cloneWithoutData(property->size(), dataType);
        convertedProperty->copyFromAndConvert(*property);
        replaceReferencesTo(property, convertedProperty);
        return convertedProperty.get();
    }
    else {
        return property.get();
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
    boost::algorithm::iota(permutation, (size_t)0);
    boost::sort(permutation, [&](size_t a, size_t b) { return ids[a] < ids[b]; });
    std::vector<size_t> invertedPermutation(ids.size());
    bool isAlreadySorted = true;
    for(size_t i = 0; i < permutation.size(); i++) {
        invertedPermutation[permutation[i]] = i;
        if(permutation[i] != i)
            isAlreadySorted = false;
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
* Copies one or more property arrays from another container to this container if possible.
* Takes care of remapping the property values to the new container's element order.
* Non-existing elements in the source container are filled with default values.
* Copying may not be possible if the property containers have different element counts without unique identifiers.
******************************************************************************/
void PropertyContainer::tryToAdoptProperties(const PropertyContainer* sourceContainer, const std::vector<const Property*>& sourceProperties, const ConstDataObjectPath& containerPath)
{
    OVITO_ASSERT(this != sourceContainer);
    OVITO_ASSERT(this->getOOClass() == sourceContainer->getOOClass());
    OVITO_ASSERT(containerPath.empty() || containerPath.back() == this);

    // Are there any properties to adopt? If not, bail out early.
    if(std::all_of(sourceProperties.begin(), sourceProperties.end(), [](const Property* p) { return p == nullptr; }))
        return;

    if(this->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty)) {
        const Property* sourceIdentifiers = sourceContainer->getProperty(Property::GenericIdentifierProperty);
        const Property* destIdentifiers = this->getProperty(Property::GenericIdentifierProperty);
        if(sourceIdentifiers && destIdentifiers) {

            // First, check if the mapping is non-trivial, i.e., the IDs are not the same in both containers.
            if(sourceIdentifiers != destIdentifiers || sourceIdentifiers->size() != destIdentifiers->size() || sourceIdentifiers->checksum() != destIdentifiers->checksum()) {

                // Build id-to-index map for source container.
                std::unordered_map<IdentifierIntType, size_t> idMap;
                idMap.reserve(sourceIdentifiers->size());
                size_t index = 0;
                for(auto id : BufferReadAccess<IdentifierIntType>{sourceIdentifiers})
                    idMap.insert(std::make_pair(id, index++));

                for(const Property* sourceProperty : sourceProperties) {
                    if(!sourceProperty)
                        continue;
                    OVITO_ASSERT(sourceIdentifiers->size() == sourceProperty->size());

                    // Create destination property array. Initialize it with default values.
                    Property* destProperty;
                    if(sourceProperty->isStandardProperty())
                        destProperty = this->createProperty(DataBuffer::Initialized, sourceProperty->typeId(), containerPath);
                    else
                        destProperty = this->createProperty(DataBuffer::Initialized, sourceProperty->name(), sourceProperty->dataType(), sourceProperty->componentCount(), sourceProperty->componentNames());
                    destProperty->setVisElements(sourceProperty->visElements());

                    // Perform mapped copy of property values to destination container.
                    RawBufferAccess<access_mode::write> destAcc(destProperty);
                    RawBufferReadAccess sourceAcc(sourceProperty);
                    const auto stride = destProperty->stride();
                    auto* __restrict dst = destAcc.data();
                    for(auto id : BufferReadAccess<IdentifierIntType>{destIdentifiers}) {
                        if(auto it = idMap.find(id); it != idMap.end()) {
                            OVITO_ASSERT(it->second < sourceProperty->size());
                            const auto* __restrict src = sourceAcc.cdata() + it->second * stride;
                            std::memcpy(dst, src, stride);
                        }
                        dst += stride;
                    }
                }

                return;
            }
        }
        else if(sourceIdentifiers || destIdentifiers) {
            return;
        }
    }

    // In case of no unique identifiers, we can only copy properties if the element counts match exactly.
    if(this->elementCount() == sourceContainer->elementCount()) {
        for(const Property* sourceProperty : sourceProperties) {
            if(sourceProperty) {
                // Copying is trivial in this case: simply adopt the existing Property object into the new container.
                this->createProperty(sourceProperty);
            }
        }
    };
}

/******************************************************************************
* Makes sure that all property arrays in this container have a consistent length.
* If this is not the case, the method throws an exception.
******************************************************************************/
void PropertyContainer::verifyIntegrity() const
{
    size_t c = elementCount();
    for(const Property* property : properties()) {
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
                if(!type->ownerProperty()) {
                    const_cast<ElementType*>(type)->_ownerProperty.set(const_cast<ElementType*>(type), PROPERTY_FIELD(ElementType::ownerProperty), OwnerPropertyRef(&OOClass(), property));
                }
                if(ElementType* proxyType = dynamic_object_cast<ElementType>(type->editableProxy())) {
                    if(!proxyType->ownerProperty())
                        proxyType->_ownerProperty.set(proxyType, PROPERTY_FIELD(ElementType::ownerProperty), type->ownerProperty());
                }
            }
        }
    }

    // For backward compatibility with older OVITO versions that only knew FloatType, i.e., not both single and double precision floating-point.
    // Perform data type conversion for standard properties if necessary.
    if(stream.formatVersion() < 30010) {
        for(const Property* property : properties()) {
            if(property->isStandardProperty()) {
                int expectedDataType = getOOMetaClass().standardPropertyDataType(property->typeId());
                convertPropertyToDataType(property, expectedDataType);
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
        if(property->typeId() == Property::GenericSelectionProperty) continue;
        if(property->typeId() == Property::GenericColorProperty) continue;
        if(!str.isEmpty()) str += QStringLiteral("<sep>");
        str += QStringLiteral("<key>");
        str += property->name().toHtmlEscaped();
        str += QStringLiteral(":</key> <val>");
        property->forAnyType([&](auto _) {
            using T = decltype(_);
            BufferReadAccess<T*> data(property);
            for(size_t component = 0; component < property->componentCount(); component++) {
                if(component != 0) str += QStringLiteral(", ");
                str += QString::number(data.get(elementIndex, component));
                if constexpr(std::is_same_v<T, int32_t>) {
                    if(property->elementTypes().empty() == false) {
                        if(const ElementType* ptype = property->elementType(data.get(elementIndex, component))) {
                            if(!ptype->name().isEmpty())
                                str += QStringLiteral(" (%1)").arg(ptype->name().toHtmlEscaped());
                        }
                    }
                }
            }
        });
        str += QStringLiteral("</val>");
    }
    return str;
}

}   // End of namespace
