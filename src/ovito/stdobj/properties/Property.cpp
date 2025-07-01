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
#include <ovito/core/dataset/DataSet.h>
#include "Property.h"
#include "PropertyContainerClass.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(Property);
OVITO_CLASSINFO(Property, "DisplayName", "Property");
OVITO_CLASSINFO(Property, "ClassNameAlias", "PropertyObject");  // For backward compatibility with OVITO 3.9.2
DEFINE_VECTOR_REFERENCE_FIELD(Property, elementTypes);
DEFINE_PROPERTY_FIELD(Property, title);
SET_PROPERTY_FIELD_LABEL(Property, elementTypes, "Element types");
SET_PROPERTY_FIELD_LABEL(Property, title, "Title");
SET_PROPERTY_FIELD_CHANGE_EVENT(Property, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor creating an empty property array.
******************************************************************************/
void Property::initializeObject(ObjectInitializationFlags flags)
{
    DataBuffer::initializeObject(flags);
}

/******************************************************************************
* Constructor allocating a property array with given size and data layout.
******************************************************************************/
void Property::initializeObject(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, const QStringView name, int typeId, QStringList componentNames)
{
    DataBuffer::initializeObject(flags, init, elementCount, dataType, componentCount, std::move(componentNames));

    _typeId = typeId;
    setName(name.toString());
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
Property::~Property()
{
}
#endif

/******************************************************************************
* Creates a copy of a property object.
******************************************************************************/
OORef<RefTarget> Property::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<Property> clone = static_object_cast<Property>(DataBuffer::clone(deepCopy, cloneHelper));

    // Copy internal data.
    prepareReadAccess();
    clone->_typeId = _typeId;
    finishReadAccess();

#ifdef OVITO_USE_SYCL
    if(isBeingAccessedExternally()) {
        // Force flush SYCL queue to complete the memcpy to the cloned data buffer now.
        // That's needed because Python code may be performing subsequent writes to the old memory buffer that will go unnoticed by
        // SYCL. We need to make sure these happen after the memcpy is completed, because a direct write access to the array should never
        // affect the cloned property.
        RawBufferReadAccess{clone};
    }
#endif

    return clone;
}

/******************************************************************************
* Returns the display title of this property object in the user interface.
******************************************************************************/
QString Property::objectTitle() const
{
    return title().isEmpty() ? name() : title();
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void Property::propertyChanged(const PropertyFieldDescriptor* field)
{
    DataBuffer::propertyChanged(field);

    if(field == PROPERTY_FIELD(DataObject::identifier) && title().isEmpty()) {
        // Since the identifier is the property's name, the property's title potentially changes too.
        notifyDependents(ReferenceEvent::TitleChanged);
    }
}

/******************************************************************************
* Generates a human-readable string representation of the data object reference.
******************************************************************************/
QString Property::OOMetaClass::formatDataObjectPath(const ConstDataObjectPath& path) const
{
    QString str;
    for(auto obj = path.begin(); obj != path.end(); ++obj) {
        if(obj != path.begin())
            str += QStringLiteral(u" \u2192 ");  // Unicode arrow
        if(obj != path.end() - 1)
            str += (*obj)->objectTitle();
        else
            str += static_object_cast<Property>(*obj)->name();
    }
    return str;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void Property::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    DataBuffer::saveToStream(stream, excludeRecomputableData);

    prepareReadAccess();
    try {
        stream.beginChunk(0x100);
        stream << name();
        stream << _typeId;
        stream.endChunk();
        finishReadAccess();
    }
    catch(...) {
        finishReadAccess();
        throw;
    }
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void Property::loadFromStream(ObjectLoadStream& stream)
{
    QString name;
    if(stream.formatVersion() >= 30007) {
        DataBuffer::loadFromStream(stream);

        // Current file format:
        stream.expectChunk(0x100);
        stream >> name;
        stream >> _typeId;
        stream.closeChunk();
    }
    else {
        DataObject::loadFromStream(stream);

        // Legacy file format:
        // For backward compatibility with OVITO 3.3.5.
        stream.expectChunk(0x01);
        stream.expectChunk(0x02);
        stream >> name;
        stream >> _typeId;
        DataBuffer::loadFromStream(stream);
        stream.closeChunk();
    }

    setIdentifier(name);
}

/******************************************************************************
* Checks if this property storage and its contents exactly match those of
* another property storage.
******************************************************************************/
bool Property::equals(const Property& other) const
{
    prepareReadAccess();
    other.prepareReadAccess();

    bool result = [&]() {
        if(this->typeId() != other.typeId())
            return false;
        if(this->typeId() == GenericUserProperty && this->name() != other.name())
            return false;
        return true;
    }();

    other.finishReadAccess();
    finishReadAccess();

    if(!result)
        return false;

    return DataBuffer::equals(other);
}

/******************************************************************************
* Creates an empty copy of this property object - without copying the stored
* array data but cloning the metadata and list of element types.
******************************************************************************/
PropertyPtr Property::cloneWithoutData(size_t newSize, int overrideDataType) const
{
    UndoSuspender noUndo;

    PropertyPtr clone = PropertyPtr::create(
            ObjectInitializationFlag::DontInitializeObject, DataBuffer::Uninitialized, newSize, overrideDataType != 0 ? overrideDataType : this->dataType(),
            this->componentCount(), this->name(), this->typeId(), this->componentNames());

    clone->setVisElements(this->visElements());
    clone->setElementTypes(this->elementTypes());
    clone->setTitle(this->title());
    clone->setCreatedByNode(this->createdByNode());
    clone->setEditableProxy(this->editableProxy());

    return clone;
}

/******************************************************************************
* Helper method that remaps the existing type IDs to a contiguous range starting at the given
* base ID. This method is mainly used for file output, because some file formats
* work with numeric particle types only, which must form a contiguous range.
* The method returns the mapping of output type IDs to original type IDs
* and a copy of the property array in which the original type ID values have
* been remapped to the output IDs.
******************************************************************************/
std::tuple<std::map<int,int>, ConstPropertyPtr> Property::generateContiguousTypeIdMapping(int baseId) const
{
    OVITO_ASSERT(dataType() == Property::Int32 && componentCount() == 1);

    // Generate sorted list of existing type IDs.
    std::set<int32_t> typeIds;
    for(const ElementType* t : elementTypes())
        typeIds.insert(t->numericId());

    // Add ID values that occur in the property array but which have not been defined as a type.
    for(auto t : BufferReadAccess<int32_t>(this))
        typeIds.insert(t);

    // Build the mappings between old and new IDs.
    std::map<int32_t,int32_t> oldToNewMap;
    std::map<int32_t,int32_t> newToOldMap;
    bool remappingRequired = false;
    for(int32_t id : typeIds) {
        if(id != baseId) remappingRequired = true;
        oldToNewMap.emplace(id, baseId);
        newToOldMap.emplace(baseId++, id);
    }

    // Create a copy of the per-element type array in which old IDs have been replaced with new ones.
    ConstPropertyPtr remappedArray;
    if(remappingRequired) {
        // Make a copy of this property, which can be modified.
        PropertyPtr copy = CloneHelper::cloneSingleObject(this, false);
        for(auto& id : BufferWriteAccess<int32_t, access_mode::discard_write>(copy))
            id = oldToNewMap[id];
        remappedArray = std::move(copy);
    }
    else {
        // No data copied needed if ordering hasn't changed.
        remappedArray = this;
    }

    return std::make_tuple(std::move(newToOldMap), std::move(remappedArray));
}

/******************************************************************************
* Sorts the types w.r.t. their name.
* This method is used by file parsers that create element types on the
* go while the read the data. In such a case, the ordering of types
* depends on the storage order of data elements in the file, which is not desirable.
******************************************************************************/
void Property::sortElementTypesByName()
{
    OVITO_ASSERT(dataType() == DataBuffer::Int32 && componentCount() == 1);

    // Check if type IDs form a consecutive sequence starting at 1.
    // If not, we leave the type order as it is.
    int id = 1;
    for(const ElementType* type : elementTypes()) {
        if(type->numericId() != id++)
            return;
    }

    // Check if types are already in the correct order.
    if(std::is_sorted(elementTypes().begin(), elementTypes().end(),
            [](const ElementType* a, const ElementType* b) { return a->name().compare(b->name(), Qt::CaseInsensitive) < 0; }))
        return;

    // Reorder types by name.
    DataRefVector<ElementType> types = elementTypes();
    std::sort(types.begin(), types.end(),
        [](const ElementType* a, const ElementType* b) { return a->name().compare(b->name(), Qt::CaseInsensitive) < 0; });
    setElementTypes(std::move(types));

#if 0
    // NOTE: No longer reassigning numeric IDs to the types here, because the new requirement is
    // that the numeric ID of an existing ElementType never changes once the type has been created.
    // Otherwise, the editable proxy objects would become out of sync.

    // Build map of IDs.
    std::vector<int> mapping(elementTypes().size() + 1);
    for(int index = 0; index < elementTypes().size(); index++) {
        int id = elementTypes()[index]->numericId();
        mapping[id] = index + 1;
        if(id != index + 1)
            makeMutable(elementTypes()[index])->setNumericId(index + 1);
    }

    // Remap type IDs.
    for(int& t : BufferAccess<int32_t>(this)) {
        OVITO_ASSERT(t >= 1 && t < mapping.size());
        t = mapping[t];
    }
#endif
}

/******************************************************************************
* Sorts the element types with respect to the numeric identifier.
******************************************************************************/
void Property::sortElementTypesById()
{
    DataRefVector<ElementType> types = elementTypes();
    std::sort(types.begin(), types.end(),
        [](const auto& a, const auto& b) { return a->numericId() < b->numericId(); });
    setElementTypes(std::move(types));
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void Property::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath, bool forceProxyReplacement) const
{
    DataBuffer::updateEditableProxies(state, dataPath, forceProxyReplacement);

    // Note: 'this' may no longer exist at this point, because the base method implementation may
    // have already replaced it with a mutable copy.
    const Property* self = static_object_cast<Property>(dataPath.back());

    if(self->editableProxy() && !forceProxyReplacement) {
        Property* proxy = static_object_cast<Property>(self->editableProxy());

        // Synchronize the actual data object with the editable proxy object.
        OVITO_ASSERT(proxy->typeId() == self->typeId());
        OVITO_ASSERT(proxy->dataType() == self->dataType());
        OVITO_ASSERT(proxy->title() == self->title());

        // Add the proxies of newly created element types to the proxy property object.
        for(const ElementType* type : self->elementTypes()) {
            ElementType* proxyType = static_object_cast<ElementType>(type->editableProxy());
            OVITO_ASSERT(proxyType != nullptr);
            if(!proxy->elementTypes().contains(proxyType))
                proxy->addElementType(proxyType);
        }
    }
    else if(!self->elementTypes().empty()) {
        // Create and initialize a new proxy property object.
        // Note: We avoid copying the property data here by constructing the proxy Property from scratch instead of cloning the original data object.
        OORef<Property> newProxy = OORef<Property>::create(ObjectInitializationFlag::DontCreateVisElement, DataBuffer::Uninitialized, 0, self->dataType(), self->componentCount(), self->name(), self->typeId(), self->componentNames());
        newProxy->setTitle(self->title());

        // Adopt the proxy objects corresponding to the element types, which have already been created by
        // the recursive method.
        for(const ElementType* type : self->elementTypes()) {
            OVITO_ASSERT(type->editableProxy() != nullptr);
            newProxy->addElementType(static_object_cast<ElementType>(type->editableProxy()));
        }

        // Make this data object mutable and attach the proxy object to it.
        state.makeMutableInplace(dataPath)->setEditableProxy(std::move(newProxy));
    }
}

/******************************************************************************
* Creates and returns a new numeric element type with the given numeric ID and,
* optionally, a human-readable name. If an element type with the given numeric ID
* already exists in this property's element type list, it will be returned instead.
******************************************************************************/
const ElementType* Property::addNumericType(const PropertyContainerClass& containerClass, int id, const QString& name, OvitoClassPtr elementTypeClass)
{
    if(const ElementType* existingType = elementType(id))
        return existingType;

    // If the caller did not specify an element type class, let the PropertyConatiner class
    // determine the right element type class for the given property.
    if(elementTypeClass == nullptr) {
        elementTypeClass = containerClass.typedPropertyElementClass(typeId());
        if(elementTypeClass == nullptr)
            elementTypeClass = &ElementType::OOClass();
    }
    OVITO_ASSERT(elementTypeClass->isDerivedFrom(ElementType::OOClass()));

    // First initialization phase.
    DataOORef<ElementType> elementType = static_object_cast<ElementType>(elementTypeClass->createInstance());
    // Second initialization phase for element types, which takes into account the assigned ID and name and the property type.
    elementType->setNumericId(id);
    elementType->setName(name);
    elementType->initializeType(OwnerPropertyRef(&containerClass, this));

    // Log in type name assigned by the caller as default value for the element type.
    // This is needed for the Python code generator to detect manual changes subsequently made by the user.
    elementType->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ElementType::name)});

    // Add the new element type to the type list managed by this property.
    return addElementType(std::move(elementType));
}

/******************************************************************************
* Returns the display name of the property including the name of the given
* vector component.
******************************************************************************/
QString Property::nameWithComponent(int vectorComponent) const
{
    if(componentCount() <= 1 || vectorComponent < 0) {
        if(componentNames().size() == 1)
            return QStringLiteral("%1.%2").arg(name()).arg(componentNames()[0]);
        else
            return name();
    }
    else if(vectorComponent < componentNames().size())
        return QStringLiteral("%1.%2").arg(name()).arg(componentNames()[vectorComponent]);
    else
        return QStringLiteral("%1.%2").arg(name()).arg(vectorComponent + 1);
}

/******************************************************************************
* Throws an exception with an informative text if the given string is not a
* valid name for an OVITO property.
******************************************************************************/
void Property::throwIfInvalidPropertyName(const QStringView name)
{
    if(name.isEmpty())
        throw Exception(tr("Invalid empty property name. OVITO property names must have at least length 1."));
    if(name.contains(QChar('.')))
        throw Exception(tr("Invalid property name: '%1'. Dots are not allowed in OVITO property names.").arg(name));
    if(name.contains(QChar('/')))
        throw Exception(tr("Invalid property name: '%1'. '/' is not allowed in OVITO property names.").arg(name));
    if(name.contains(QChar(':')))
        throw Exception(tr("Invalid property name: '%1'. ':' is not allowed in OVITO property names.").arg(name));
    if(name.startsWith(QChar(' ')))
        throw Exception(tr("Invalid property name: '%1'. OVITO property names must not start with whitespace.").arg(name));
    if(name.endsWith(QChar(' ')))
        throw Exception(tr("Invalid property name: '%1'. OVITO property names must not end with whitespace.").arg(name));
    if(name.endsWith(QChar('_')))
        throw Exception(tr("Invalid property name: '%1'. OVITO property names must not end with an underscore.").arg(name));
}

/******************************************************************************
* Throws an exception with an informative text if the given string is not a
* valid name for an OVITO property vector component.
******************************************************************************/
void Property::throwIfInvalidPropertyComponentName(const QStringView name)
{
    if(name.isEmpty())
        throw Exception(tr("Invalid empty component name. OVITO vector component names must have at least length 1."));
    if(name.contains(QChar('.')))
        throw Exception(tr("Invalid component name: '%1'. Dots are not allowed in OVITO vector component names.").arg(name));
    if(name.contains(QChar('/')))
        throw Exception(tr("Invalid component name: '%1'. '/' is not allowed in OVITO vector component names.").arg(name));
    if(name.contains(QChar(':')))
        throw Exception(tr("Invalid component name: '%1'. ':' is not allowed in OVITO vector component names.").arg(name));
    if(name.contains(QChar(' ')))
        throw Exception(tr("Invalid component name: '%1'. OVITO vector component names must not contain whitespace.").arg(name));
    if(name.endsWith(QChar('_')))
        throw Exception(tr("Invalid component name: '%1'. OVITO vector component names must not end with an underscore.").arg(name));
}

/******************************************************************************
* Performs name mangling if necessary to turn the given name into a valid property name.
******************************************************************************/
QString Property::makePropertyNameValid(const QString& name)
{
    QString mangledName = name.trimmed();
    mangledName.replace(QChar('.'), QChar('_'));
    mangledName.replace(QChar('/'), QChar('_'));
    mangledName.replace(QChar(':'), QChar('_'));
    // Remove all underscores from the end of the name, because they interfere with
    // the "underscore notation" in OVITO's Python API (PropertyContainer dict lookup).
    while(mangledName.endsWith(QChar('_')))
        mangledName.chop(1);
    return mangledName;
}

/******************************************************************************
* Performs name mangling if necessary to turn the given name into a valid vector property component name.
******************************************************************************/
QString Property::makeComponentNameValid(const QString& name)
{
    QString mangledName = name.trimmed();
    mangledName.replace(QChar('.'), QChar('_'));
    mangledName.replace(QChar('/'), QChar('_'));
    mangledName.replace(QChar(':'), QChar('_'));
    mangledName.replace(QChar(' '), QChar('_'));
    // Remove all underscores from the end of the name, because they interfere with
    // the "underscore notation" in OVITO's Python API (PropertyContainer dict lookup).
    while(mangledName.endsWith(QChar('_')))
        mangledName.chop(1);
    return mangledName;
}

}   // End of namespace
