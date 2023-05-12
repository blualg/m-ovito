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


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/DataObjectAccess.h>
#include "PropertyContainer.h"
#include "PropertyObject.h"

namespace Ovito::StdObj {

/**
 * Utility class that provides efficient read-write access to the data of a
 * particular set of properties stored in a PropertyContainer.
 */
template<int... CachedPropertyTypes>
class PropertyContainerAccess
{
public:

    /// Constructor.
    PropertyContainerAccess(const PropertyContainer* container) :
        _container(container),
        _elementCount(container ? container->elementCount() : 0),
        _cachedPointers{{ getPropertyMemory(CachedPropertyTypes)... }}
    {
        _mutableCachedPointers.fill(nullptr);
    }

    /// Destructor.
    ~PropertyContainerAccess() {
        // Make sure we don't leave a modified container in an inconsistent state.
        reset();
    }

    /// Releases the current container from this accessor and loads a new one.
    void reset(const PropertyContainer* newContainer = nullptr) noexcept {
        OVITO_ASSERT(newContainer == nullptr || newContainer != container());
        if(container()) {
            // Write changed element count back to the old container before releasing it.
            if(container()->elementCount() != _elementCount)
                mutableContainer()->setElementCount(_elementCount);
        }
        _container.reset(newContainer);
        _elementCount = newContainer ? newContainer->elementCount() : 0;
        _cachedPointers = { getPropertyMemory(CachedPropertyTypes)... };
        _mutableCachedPointers.fill(nullptr);
        _allPropertiesMutable = false;
    }

    /// Releases the current property container and returns it to the caller.
    OORef<const PropertyContainer> take() {
        if(container()) {
            // Write changed element count back to the container before releasing it.
            if(container()->elementCount() != _elementCount)
                mutableContainer()->setElementCount(_elementCount);
            _cachedPointers.fill(nullptr);
            _mutableCachedPointers.fill(nullptr);
            _allPropertiesMutable = false;
        }
        return _container.take();
    }

    /// Returns the number of data elements in each of the property arrays.
    size_t elementCount() const { return _elementCount; }

    /// Returns one of the standard properties from the container.
    const PropertyObject* getProperty(int type) const {
        OVITO_ASSERT(container());
        return container()->getProperty(type);
    }

    /// Looks up a user-defined property in the container.
    const PropertyObject* getProperty(const QString& name) const {
        OVITO_ASSERT(container());
        return container()->getProperty(name);
    }

    /// Returns one of the standard properties from the container.
    PropertyObject* getMutableProperty(int type) {
        OVITO_ASSERT(container());
        PropertyObject* property = mutableContainer()->getMutableProperty(type);
        if(property)
            updateMutablePropertyPointer(property);
        return property;
    }

    /// Returns a user property from the container.
    PropertyObject* getMutableProperty(const QString& name) {
        OVITO_ASSERT(container());
        PropertyObject* property = mutableContainer()->getMutableProperty(name);
        if(property)
            updateMutablePropertyPointer(property);
        return property;
    }

    /// Creates a new standard property in the container.
    PropertyObject* createProperty(DataBuffer::BufferInitialization init, int ptype) {
        // Write current element count back to container before allocating a new property array.
        updateElementCount();

        // Create the property object in the wrapped container.
        PropertyObject* property = mutableContainer()->createProperty(init, ptype);

        // Update our array memory pointers corresponding to this property.
        updateMutablePropertyPointer(property);

        return property;
    }

    /// Creates a user-defined property in the container.
    PropertyObject* createProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) {
        // Write current element count back to container before allocating a new property array.
        updateElementCount();

        // Create the property object in the wrapped container.
        return mutableContainer()->createProperty(init, name, dataType, componentCount, std::move(componentNames));
    }

    /// Inserts an existing property object into the contaienr.
    void addProperty(const PropertyObject* property) {
        OVITO_ASSERT(property && property->size() == _elementCount);

        // Write current element count back to container before inserting the property array.
        updateElementCount();

        // Insert the property object into the wrapped container.
        const PropertyObject* insertedProperty = mutableContainer()->createProperty(property);

        // Update our array memory pointers corresponding to this property.
        auto pindex = cachedPropertyIndex(insertedProperty->type());
        if(pindex < CachedPropertyCount) {
            _cachedPointers[pindex] = insertedProperty->cbuffer();
            _mutableCachedPointers[pindex] = nullptr;
        }
        // We don't know if the newly inserted property is mutable.
        _allPropertiesMutable = false;
    }

    /// Removes a property from the container.
    void removeProperty(const PropertyObject* property) {
        // Reset the cached pointers to the property's memory.
        auto pindex = cachedPropertyIndex(property->type());
        if(pindex < CachedPropertyCount)
            _cachedPointers[pindex] = _mutableCachedPointers[pindex] = nullptr;
        // Remove the property from the parent container.
        mutableContainer()->removeProperty(property);
    }

    /// Grows the number of data elements in the properties while preserving the exiting data.
    /// Newly added data elements will *not* be initialized to zero.
    /// Returns the previous number of existing elements in the container.
    size_t growElements(size_t numAdditionalElements) {

        // Extend each property array.
        for(const PropertyObject* prop : mutableProperties()) {
            OVITO_ASSERT(prop->size() == _elementCount);
            if(const_cast<PropertyObject*>(prop)->grow(numAdditionalElements)) {
                // If the growing of the property array triggered a memory reallocation,
                // then we need to update our cached pointer corresponding to this standard property.
                updateMutablePropertyPointer(const_cast<PropertyObject*>(prop));
            }
        }

        // Increment our internal element counter.
        size_t oldCount = _elementCount;
        _elementCount += numAdditionalElements;
        return oldCount;
    }

    /// Deletes a number of elements from the end of each property array.
    void truncateElements(size_t numElementsToTruncate) {
        OVITO_ASSERT(numElementsToTruncate <= _elementCount);

        // Truncate each property array.
        for(const PropertyObject* prop : mutableProperties()) {
            OVITO_ASSERT(prop->size() == _elementCount);
            const_cast<PropertyObject*>(prop)->truncate(numElementsToTruncate);
        }

        // Decrement our internal element counter.
        _elementCount -= numElementsToTruncate;
    }

    /// Determines if a cached property is present in the container.
    template<int PropertyType>
    bool hasProperty() const {
        return _cachedPointers[cachedPropertyIndexExpect<PropertyType>()] != nullptr;
    }

    /// Reads the value of a cached property of one data element in the container.
    template<int PropertyType, typename T>
    const T& getPropertyValue(size_t index) const {
        OVITO_ASSERT(index < elementCount());
        const T* data = static_cast<const T*>(_cachedPointers[cachedPropertyIndexExpect<PropertyType>()]);
        OVITO_ASSERT(data != nullptr);
        OVITO_ASSERT(_container->getProperty(PropertyType)->size() == _elementCount);
        OVITO_ASSERT(_container->getProperty(PropertyType)->dataType() == DataBufferPrimitiveType<T>::value);
        OVITO_ASSERT(_container->getProperty(PropertyType)->stride() == sizeof(T));
        return data[index];
    }

    /// Writes the value of a cached property of one data element in the container.
    template<int PropertyType, typename T>
    void setPropertyValue(size_t index, const T& value) {
        OVITO_ASSERT(index < elementCount());
        T* data = static_cast<T*>(makeCachedPropertyMutable<PropertyType>());
        OVITO_ASSERT(data != nullptr);
        OVITO_ASSERT(_container->getProperty(PropertyType)->size() == _elementCount);
        OVITO_ASSERT(_container->getProperty(PropertyType)->dataType() == DataBufferPrimitiveType<T>::value);
        OVITO_ASSERT(_container->getProperty(PropertyType)->stride() == sizeof(T));
        data[index] = value;
    }

    /// Conditionally writes the value of a cached property of one data element in the container if that property exists.
    template<int PropertyType, typename T>
    void setOptionalPropertyValue(size_t index, const T& value) {
        OVITO_ASSERT(index < elementCount());
        if(T* data = static_cast<T*>(makeCachedPropertyMutable<PropertyType>())) {
            OVITO_ASSERT(_container->getProperty(PropertyType)->size() == _elementCount);
            OVITO_ASSERT(_container->getProperty(PropertyType)->dataType() == DataBufferPrimitiveType<T>::value);
            OVITO_ASSERT(_container->getProperty(PropertyType)->stride() == sizeof(T));
            data[index] = value;
        }
    }

    /// Returns an read-only iterator range over all values of a cached property.
    template<int PropertyType, typename T>
    auto propertyRange() const {
        const T* data = static_cast<const T*>(_cachedPointers[cachedPropertyIndexExpect<PropertyType>()]);
        OVITO_ASSERT(data != nullptr);
        OVITO_ASSERT(_container->getProperty(PropertyType)->dataType() == DataBufferPrimitiveType<T>::value);
        OVITO_ASSERT(_container->getProperty(PropertyType)->stride() == sizeof(T));
        return boost::make_iterator_range_n(data, _elementCount);
    }

    /// Returns a read-write iterator range over all values of a cached property.
    template<int PropertyType, typename T>
    auto mutablePropertyRange() {
        T* data = static_cast<T*>(makeCachedPropertyMutable<PropertyType>());
        OVITO_ASSERT(data != nullptr);
        OVITO_ASSERT(_container->getProperty(PropertyType)->size() == _elementCount);
        OVITO_ASSERT(_container->getProperty(PropertyType)->dataType() == DataBufferPrimitiveType<T>::value);
        OVITO_ASSERT(_container->getProperty(PropertyType)->stride() == sizeof(T));
        return boost::make_iterator_range_n(data, _elementCount);
    }

    /// Moves the property values of one data element in the property arrays from one index to another.
    void moveElement(size_t fromIndex, size_t toIndex) {
        copyElement(fromIndex, toIndex);
    }

    /// Copies the property values of one data element in the property arrays from one index to another.
    void copyElement(size_t fromIndex, size_t toIndex) {
        OVITO_ASSERT(fromIndex < elementCount());
        OVITO_ASSERT(toIndex < elementCount());
        for(const PropertyObject* property : mutableProperties()) {
            OVITO_ASSERT(property->size() == elementCount());
            std::memcpy(const_cast<PropertyObject*>(property)->buffer() + toIndex * property->stride(), property->cbuffer() + fromIndex * property->stride(), property->stride());
        }
    }

    /// Reduces the size of all property arrays, removing elements for which
    /// the corresponding bits in the bit array are set.
    void filterResize(const boost::dynamic_bitset<>& mask) {
        OVITO_ASSERT(mask.size() == elementCount());
        size_t deleteCount = std::numeric_limits<size_t>::max();
        for(const PropertyObject* property : mutableProperties()) {
            OVITO_ASSERT(property->size() == elementCount());
            const_cast<PropertyObject*>(property)->filterResize(mask);
            deleteCount = elementCount() - property->size();

#ifdef OVITO_DEBUG
            // Note: filterResize() should never reallocate memory.
            auto pindex = cachedPropertyIndex(property->type());
            if(pindex < CachedPropertyCount)
                OVITO_ASSERT(_cachedPointers[pindex] == _mutableCachedPointers[pindex] && _cachedPointers[pindex] == property->cbuffer());
#endif
        }
        if(deleteCount == std::numeric_limits<size_t>::max())
            deleteCount = mask.count();
        _elementCount -= deleteCount;
    }

    /// Exchanges the contents of this data structure with another structure.
    void swap(PropertyContainerAccess& other) {
        _container.swap(other._container);
        std::swap(_elementCount, other._elementCount);
        std::swap(_allPropertiesMutable, other._allPropertiesMutable);
        std::swap(_cachedPointers, other._cachedPointers);
        std::swap(_mutableCachedPointers, other._mutableCachedPointers);
    }

private:

    /// Returns the property container managed by this class.
    const PropertyContainer* container() const { return _container; }

    /// Makes sure the property container managed by this class is safe to modify.
    /// Automatically creates a copy of the property container if necessary.
    PropertyContainer* mutableContainer() { return _container.makeMutable(); }

    /// Prepares all property objects in the container for write access.
    const auto& mutableProperties() {
        OVITO_ASSERT(container());
        if(!_allPropertiesMutable) {
            // Note: Our manipulations made to the PropertyContainer should never get recorded on the undo stack.
            // The PropertyContainerAccess class must not be used to perform user editing actions.
            OVITO_ASSERT(QThread::currentThread() != container()->thread() || !container()->isUndoRecording());

            // Make the container and its property array mutable.
            mutableContainer()->makePropertiesMutable();

            // Update pointers to mutable property memory.
            _mutableCachedPointers = { getMutablePropertyMemory(CachedPropertyTypes)... };
            // Also update pointers to immutable property memory.
            std::copy(std::begin(_mutableCachedPointers), std::end(_mutableCachedPointers), std::begin(_cachedPointers));

            _allPropertiesMutable = true;
        }
#ifdef OVITO_DEBUG
        else {
            // Verify that all properties are indeed mutable.
            for(const PropertyObject* property : container()->properties())
                OVITO_ASSERT(property->isSafeToModify());
        }
#endif
        return container()->properties();
    }

    /// Obtains a pointer to the read-write array memory of a given property.
    const void* getPropertyMemory(int propertyType) const {
        if(container()) {
            if(const PropertyObject* p = container()->getProperty(propertyType))
                return p->cbuffer();
        }
        return nullptr;
    }

    /// Obtains a pointer to the read-write array memory of a given property.
    void* getMutablePropertyMemory(int propertyType) const {
        OVITO_ASSERT(container());
        if(const PropertyObject* p = container()->getProperty(propertyType)) {
            OVITO_ASSERT(p && p->isSafeToModify());
            return const_cast<PropertyObject*>(p)->buffer();
        }
        return nullptr;
    }

    /// Updates the cached pointer to mutable memory of a single property.
    /// The property does't have to be a cached property.
    void updateMutablePropertyPointer(PropertyObject* property) {
        OVITO_ASSERT(property);
        OVITO_ASSERT(property->isSafeToModify());
        auto pindex = cachedPropertyIndex(property->type());
        if(pindex < CachedPropertyCount)
            _cachedPointers[pindex] = _mutableCachedPointers[pindex] = property->buffer();
    }

    /// Writes the local element count back to the wrapped container.
    void updateElementCount() {
        if(_elementCount != container()->elementCount()) {
            mutableContainer()->setElementCount(_elementCount);

            // Update internal pointers to mutable property memory.
            _mutableCachedPointers = { getMutablePropertyMemory(CachedPropertyTypes)... };
            // Also update pointers to immutable property memory.
            std::copy(std::begin(_mutableCachedPointers), std::end(_mutableCachedPointers), std::begin(_cachedPointers));

            // PropertyContainer::setElementCount() makes all properties mutable if the container's element count is being updated.
            _allPropertiesMutable = true;
        }
    }

    /// Prepares a single cached property array for modification.
    /// Returns a pointer to the internal memory buffer of the property array.
    template<int PropertyType>
    void* makeCachedPropertyMutable() {
        constexpr auto pindex = cachedPropertyIndex(PropertyType);
        static_assert(pindex < CachedPropertyCount, "This standard property is not being cached by this class template specialization.");
        if(!_mutableCachedPointers[pindex]) {
            if(PropertyObject* p = mutableContainer()->getMutableProperty(PropertyType)) {
                _cachedPointers[pindex] = _mutableCachedPointers[pindex] = p->buffer();
            }
        }
        return _mutableCachedPointers[pindex];
    }

    /// The number of properties that this class should cache.
    static constexpr auto CachedPropertyCount = sizeof...(CachedPropertyTypes);

    /// Returns the index of a property in the list of cached properties (at compile time).
    static constexpr std::size_t cachedPropertyIndex(int propertyType) {
        constexpr int s_arr[] = { CachedPropertyTypes... };
        for(std::size_t i = 0; i != CachedPropertyCount; ++i) {
            if(s_arr[i] == propertyType) return i;
        }
        return CachedPropertyCount;
    }

    /// Returns the index of a property in the list of cached properties (at compile time).
    template<int PropertyType>
    static constexpr std::size_t cachedPropertyIndexExpect() {
        static_assert(PropertyType != 0, "Cannot cache user-defined properties.");
        constexpr std::size_t idx = cachedPropertyIndex(PropertyType);
        assert(idx < CachedPropertyCount); // Property type not found among the properties cached by this PropertyContainerAccess class.
        return idx;
    }

private:

    /// The property container wrapped by this class.
    DataObjectAccess<OORef, PropertyContainer> _container;

    /// The number of data elements stored in each property arrray of the container.
    size_t _elementCount = 0;

    /// Indicates that all properties in the container have been made mutable.
    bool _allPropertiesMutable = false;

    /// Stores cached pointers to the read-only array memory of selected properties.
    std::array<const void*, CachedPropertyCount> _cachedPointers;

    /// Stores cached pointers to the mutable array memory of selected properties.
    std::array<void*, CachedPropertyCount> _mutableCachedPointers;
};

}   // End of namespace
