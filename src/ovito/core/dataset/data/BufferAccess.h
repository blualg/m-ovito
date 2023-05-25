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
#include "DataBuffer.h"

#include <boost/range/adaptor/strided.hpp>

namespace Ovito {

namespace detail {

/// General base class storing a reference to the underlying DataBuffer
/// and to the raw memory address where the buffer's data is currently stored.
template<class BufferReference>
class BufferAccessBase
{
protected:

    /// Indicates whether write access to the buffer is enabled. This depends on whether the type BufferReference is a const or non-const reference to a DataBuffer.
    constexpr static bool IsWritable = !std::is_const_v<typename std::pointer_traits<BufferReference>::element_type>;

    using size_type = size_t;

    /// (Smart-)pointer to the DataBuffer whose data is being accessed.
    BufferReference _buffer{};

    /// Raw pointer to the buffer's underlying memory. Needs to be updated whenever a reallocation occurs.
    std::conditional_t<IsWritable, std::byte*, const std::byte*> _data = nullptr;

    /// Helper function that obtains the buffer's internal storage address for data elements.
    auto dataStorageAddress() const {
        if constexpr(IsWritable)
            return _buffer->data();
        else
            return _buffer->cdata();
    }

public:

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    BufferAccessBase() noexcept = default;

    /// Constructor that associates the access object with a buffer object (reference may be null).
    BufferAccessBase(BufferReference buffer) : _buffer(std::move(buffer)), _data(_buffer ? dataStorageAddress() : nullptr) {
#ifdef OVITO_DEBUG
        if(this->_buffer) {
            if constexpr(IsWritable)
                this->_buffer->prepareWriteAccess();
            else
                this->_buffer->prepareReadAccess();
        }
#endif
    }

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename DataBufferOrDerived>
    BufferAccessBase(DataBufferOrDerived* buffer) : BufferAccessBase(BufferReference(std::move(buffer))) {}

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename DataBufferOrDerived>
    BufferAccessBase(DataOORef<DataBufferOrDerived> buffer) : BufferAccessBase(BufferReference(std::move(buffer))) {}

    /// Copy construction (only enabled for read-only accessors).
    BufferAccessBase(const BufferAccessBase& other) noexcept : _buffer(other._buffer), _data(other._data) {
#ifdef OVITO_DEBUG
        OVITO_ASSERT(!IsWritable);
        if(this->_buffer)
            this->_buffer->prepareReadAccess();
#endif
    }

    /// Copy assignment (only enabled for read-only accessors).
    BufferAccessBase& operator=(const BufferAccessBase& other) noexcept {
        this->_buffer = other._buffer;
        this->_data = other._data;
#ifdef OVITO_DEBUG
        OVITO_ASSERT(!IsWritable);
        if(this->_buffer)
            this->_buffer->prepareReadAccess();
#endif
        return *this;
    }

    /// Move construction.
    BufferAccessBase(BufferAccessBase&& other) noexcept :
        _buffer(std::exchange(other._buffer, nullptr)),
        _data(std::exchange(other._data, nullptr)) {}

    /// Move assignment.
    BufferAccessBase& operator=(BufferAccessBase&& other) noexcept {
        this->_buffer = std::exchange(other._buffer, nullptr);
        this->_data = std::exchange(other._data, nullptr);
        return *this;
    }

#ifdef OVITO_DEBUG
    /// Destructor resets the internal references (to make debugging easier).
    ~BufferAccessBase() { reset(); }
#endif

    /// Returns the number of elements in the data array.
    inline auto size() const noexcept {
        OVITO_ASSERT(_buffer);
        return _buffer->size();
    }

    /// Returns the number of vector components per element.
    inline auto componentCount() const noexcept {
        OVITO_ASSERT(_buffer);
        return _buffer->componentCount();
    }

    /// Returns the number of bytes per element.
    inline auto stride() const noexcept {
        OVITO_ASSERT(_buffer);
        return _buffer->stride();
    }

    /// Returns the number of bytes per vector component.
    inline auto dataTypeSize() const noexcept {
        OVITO_ASSERT(_buffer);
        return _buffer->dataTypeSize();
    }

    /// Returns the data type of the property.
    inline auto dataType() const noexcept {
        OVITO_ASSERT(_buffer);
        return _buffer->dataType();
    }

    /// Returns whether this accessor points to a valid DataBuffer.
    explicit operator bool() const noexcept {
        return (bool)_buffer;
    }

    /// Returns the buffer object which is being accessed.
    inline const BufferReference& buffer() const noexcept {
        return _buffer;
    }

    /// Moves the internal buffer reference out of this accessor object.
    BufferReference take() noexcept {
        return reset();
    }

    /// Detaches the accessor object from the underlying buffer object.
    BufferReference reset() noexcept {
#ifdef OVITO_DEBUG
        if(_buffer) {
            if constexpr(IsWritable)
                _buffer->finishWriteAccess();
            else
                _buffer->finishReadAccess();
        }
#endif
        _data = nullptr;
        return std::exchange(_buffer, nullptr);
    }

    /// Updates the internal data pointer (e.g. after the data buffer's memory has been reallocated).
    template<bool IsWritable2 = IsWritable>
    inline std::enable_if_t<IsWritable2, void> updateDataStorageAddress() noexcept {
        OVITO_ASSERT(_buffer);
        _data = _buffer->data();
    }
};

/// Base class that provides access to the individual data elements stored in a DataBuffer.
template<typename T, class BufferReference>
class BufferAccessTyped : public BufferAccessBase<BufferReference>
{
public:

    constexpr static bool ComponentWise = std::is_pointer_v<T>;
    using BufferAccessBase<BufferReference>::IsWritable;

    using element_type = std::remove_pointer_t<T>;
    using iterator = element_type*;
    using const_iterator = element_type*;
    using typename BufferAccessBase<BufferReference>::size_type;

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    BufferAccessTyped() noexcept = default;

    /// Copy constructor.
    BufferAccessTyped(const BufferAccessTyped& other) noexcept = default;

    /// Move constructor.
    BufferAccessTyped(BufferAccessTyped&& other) noexcept = default;

    /// Constructor that associates the access object with a buffer object (reference may be null).
    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename DataBufferOrDerived>
    BufferAccessTyped(DataBufferOrDerived&& buffer) : BufferAccessBase<BufferReference>(std::forward<DataBufferOrDerived>(buffer)) {
        OVITO_ASSERT(!this->_buffer || this->stride() == sizeof(element_type) * (ComponentWise ? this->componentCount() : 1));
        OVITO_ASSERT(!this->_buffer || this->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!this->_buffer || this->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : this->componentCount()));
    }

    /// Copy assignement.
    BufferAccessTyped& operator=(const BufferAccessTyped& other) noexcept = default;

    /// Move assignment.
    BufferAccessTyped& operator=(BufferAccessTyped&& other) noexcept = default;

    /// Returns a pointer to the first element of the data array.
    inline element_type* begin() const noexcept {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(this->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(this->stride() == sizeof(element_type) * (ComponentWise ? this->componentCount() : 1));
        OVITO_ASSERT(IsWritable == !std::is_const_v<element_type>);
        return reinterpret_cast<element_type*>(this->_data);
    }

    /// Returns a pointer to the end of the data array.
    inline element_type* end() const noexcept {
        if constexpr(!ComponentWise)
            return begin() + this->size();
        else
            return begin() + (this->size() * this->componentCount());
    }

    /// Returns a const pointer to the first element of the data array.
    inline const element_type* cbegin() const noexcept { return begin(); }

    /// Returns a pointer to the end of the data array.
    inline const element_type* cend() const noexcept { return end(); }

    /// Returns the value of the i-th element from the array.
    template<bool ComponentWise2 = ComponentWise>
    inline std::enable_if_t<!ComponentWise2, const element_type&> get(size_type i) const noexcept {
        OVITO_ASSERT(i < this->size());
        return *(this->cbegin() + i);
    }

    /// Returns the value of the i-th element's j-th component from the array.
    template<bool ComponentWise2 = ComponentWise>
    inline std::enable_if_t<ComponentWise2, const element_type&> get(size_type i, size_type j) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        return *(cbegin() + (i * this->componentCount()) + j);
    }

    /// Sets the value of the i-th element in the array.
    template<bool ComponentWise2 = ComponentWise, bool IsWritable2 = IsWritable>
    inline void set(size_type i, std::enable_if_t<!ComponentWise2 && IsWritable2, const element_type&> v) const noexcept {
        OVITO_ASSERT(i < this->size());
        *(this->begin() + i) = v;
    }

    /// Sets the value of the i-th element's j-th component in the array.
    template<bool ComponentWise2 = ComponentWise, bool IsWritable2 = IsWritable>
    inline void set(size_type i, size_type j, std::enable_if_t<ComponentWise2 && IsWritable2, const element_type&> v) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        *(begin() + (i * this->componentCount()) + j) = v;
    }

    /// Returns a modifiable reference to the j-th component of the i-th element of the array.
    template<bool ComponentWise2 = ComponentWise, bool IsWritable2 = IsWritable>
    inline std::enable_if_t<ComponentWise2 && IsWritable2, element_type&> value(size_type i, size_type j) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        return *(begin() + i * this->componentCount() + j);
    }

    /// Indexed access to the elements of the array.
    template<bool ComponentWise2 = ComponentWise>
    inline std::enable_if_t<!ComponentWise2, element_type&> operator[](size_type i) const noexcept {
        return *(begin() + i);
    }

    /// Returns a range of iterators over the elements stored in this array.
    inline boost::iterator_range<element_type*> range() const noexcept {
        return boost::make_iterator_range(begin(), end());
    }

    /// Returns a range of iterators over the elements stored in this array.
    inline boost::iterator_range<const element_type*> crange() const noexcept {
        return boost::make_iterator_range(cbegin(), cend());
    }

    /// Returns a range of iterators over the i-th vector component of all elements stored in this array.
    template<bool ComponentWise2 = ComponentWise>
    inline auto componentRange(std::enable_if_t<ComponentWise2, size_type> componentIndex) const noexcept {
        OVITO_ASSERT(componentIndex >= 0 && componentIndex < this->componentCount());
        auto begin = this->begin() + componentIndex;
        return boost::adaptors::stride(boost::make_iterator_range(begin, begin + (this->size() * this->componentCount())), this->componentCount());
    }

    /// Turns this array accessor into an accessor for a subrange of elements.
    auto subrange(size_type beginIndex, size_type endIndex) && noexcept {
        class SubRange
        {
        public:
            using element_type = typename BufferAccessTyped<T, BufferReference>::element_type;
            using size_type = typename BufferAccessTyped<T, BufferReference>::size_type;
            using iterator = typename BufferAccessTyped<T, BufferReference>::iterator;
            using const_iterator = typename BufferAccessTyped<T, BufferReference>::const_iterator;
            SubRange(BufferAccessTyped<T, BufferReference>&& accessor, size_type beginIndex, size_type endIndex) noexcept : _accessor(std::move(accessor)), _beginIndex(beginIndex), _endIndex(endIndex) {}
            auto begin() const noexcept { return _accessor.begin() + _beginIndex; }
            auto end() const noexcept { return _accessor.begin() + _endIndex; }
        private:
            const BufferAccessTyped<T, BufferReference> _accessor;
            const size_type _beginIndex;
            const size_type _endIndex;
        };
        OVITO_ASSERT(beginIndex <= endIndex);
        OVITO_ASSERT(endIndex <= this->size());
        return SubRange(std::move(*this), beginIndex, endIndex);
    }

    /// Turns this array accessor into an accessor for a subrange of elements.
    auto subrange(size_type beginIndex) && noexcept {
        return std::move(*this).subrange(beginIndex, this->size());
    }

    /// Appends a new element to the end of the data array.
    template<bool ComponentWise2 = ComponentWise, bool IsWritable2 = IsWritable>
    inline void push_back(std::enable_if_t<!ComponentWise2 && IsWritable2, const element_type&> v) {
        size_t oldCount = this->size();
        if(this->buffer()->grow(1, true))
            this->updateDataStorageAddress();
        set(oldCount, v);
    }
};

/// Base class that provides generic read/write access data elements stored in a DataBuffer.
template<class BufferReference>
class BufferAccessReadUntyped : public BufferAccessBase<BufferReference>
{
public:

    using typename BufferAccessBase<BufferReference>::size_type;

    // Inherit constructors from base class.
    using BufferAccessBase<BufferReference>::BufferAccessBase;

    /// Reads the j-th component of the i-th element from the array.
    template<typename U>
    inline U get(size_type i, size_type j) const {
        OVITO_ASSERT(i < this->size());
        auto addr = this->cdata(j) + i * this->stride();
        switch(this->dataType()) {
        case DataBuffer::Float32:
            return static_cast<U>(*reinterpret_cast<const float*>(addr));
        case DataBuffer::Float64:
            return static_cast<U>(*reinterpret_cast<const double*>(addr));
        case DataBuffer::Int8:
            return static_cast<U>(*reinterpret_cast<const int8_t*>(addr));
        case DataBuffer::Int32:
            return static_cast<U>(*reinterpret_cast<const int32_t*>(addr));
        case DataBuffer::Int64:
            return static_cast<U>(*reinterpret_cast<const int64_t*>(addr));
        default:
            OVITO_ASSERT(false);
            throw Exception(QStringLiteral("Data access failed. Data buffer has a non-standard data type."));
        }
    }

    /// Returns a pointer to the raw data of the data array.
    inline const std::byte* cdata(size_type component = 0) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (component * this->dataTypeSize());
    }

    /// Returns a pointer to the raw data of the data array.
    inline const std::byte* cdata(size_type index, size_type component) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(index < this->size());
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (index * this->stride()) + (component * this->dataTypeSize());
    }
};

/// Base class that provides generic read/write access data elements stored in a DataBuffer.
template<class BufferReference>
class BufferAccessWriteUntyped : public BufferAccessReadUntyped<BufferReference>
{
public:

    using typename BufferAccessReadUntyped<BufferReference>::size_type;

    // Inherit constructors from base class.
    using BufferAccessReadUntyped<BufferReference>::BufferAccessReadUntyped;

    /// Sets the j-th component of the i-th element of the array to a new value.
    template<typename U>
    inline void set(size_type i, size_type j, const U& value) {
        OVITO_ASSERT(i < this->size());
        auto addr = this->data(j) + i * this->stride();
        switch(this->dataType()) {
        case DataBuffer::Float32:
            *reinterpret_cast<float*>(addr) = value;
            break;
        case DataBuffer::Float64:
            *reinterpret_cast<double*>(addr) = value;
            break;
        case DataBuffer::Int8:
            *reinterpret_cast<int8_t*>(addr) = value;
            break;
        case DataBuffer::Int32:
            *reinterpret_cast<int32_t*>(addr) = value;
            break;
        case DataBuffer::Int64:
            *reinterpret_cast<int64_t*>(addr) = value;
            break;
        default:
            OVITO_ASSERT(false);
            throw Exception(QStringLiteral("Data access failed. Data buffer has a non-standard data type."));
        }
    }

    /// Returns a pointer to the raw data of the data array.
    inline std::byte* data(size_type component = 0) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (component * this->dataTypeSize());
    }

    /// Returns a pointer to the raw data of the data array.
    inline std::byte* data(size_type index, size_type component) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(index < this->size());
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (index * this->stride()) + (component * this->dataTypeSize());
    }
};

template<typename DataType, bool StronglyReferenced>
using PickBufferAccessClass = BufferAccessTyped<
        DataType,
        std::conditional_t<StronglyReferenced,
            DataOORef<std::conditional_t<!std::is_const_v<std::remove_pointer_t<DataType>>, DataBuffer, const DataBuffer>>,
            std::conditional_t<!std::is_const_v<std::remove_pointer_t<DataType>>, DataBuffer*, const DataBuffer*>
        >
    >;

} // End of namespace detail.

using BufferReadAccess = detail::BufferAccessReadUntyped<const DataBuffer*>;
using BufferWriteAccess = detail::BufferAccessWriteUntyped<DataBuffer*>;

template<typename T>
using BufferAccess = detail::BufferAccessTyped<T, std::conditional_t<!std::is_const_v<std::remove_pointer_t<T>>, DataBuffer*, const DataBuffer*>>;
template<typename T>
using BufferAccessAndRef = detail::BufferAccessTyped<T, std::conditional_t<!std::is_const_v<std::remove_pointer_t<T>>, DataOORef<DataBuffer>, DataOORef<const DataBuffer>>>;

/**
 * Utility class that behaves like a BufferAccessAndRef but performs
 * a conversion operation if necessary (creating a temporary data copy) to
 * guarantee a specific data type for the (read-only) data access.
 *
 * Use this class for input data buffers that use a particular data type most of the time
 * but occasionally use a different data type (then incurring a costly conversion operation).
*/
template<typename T>
class BufferAccessConvertedTo : public BufferAccessAndRef<T>
{
public:

    using typename BufferAccessAndRef<T>::element_type;
    using typename BufferAccessAndRef<T>::iterator;
    using typename BufferAccessAndRef<T>::const_iterator;
    using typename BufferAccessAndRef<T>::size_type;

    /// Default constructor.
    BufferAccessConvertedTo() = default;

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename DataBufferOrDerived>
    BufferAccessConvertedTo(DataOORef<DataBufferOrDerived> buffer) : BufferAccessAndRef<T>(performDataTypeConversion(std::move(buffer))) {}

    /// Constructor that takes a raw pointer to a DataBuffer.
    BufferAccessConvertedTo(const DataBuffer* buffer) : BufferAccessConvertedTo(ConstDataBufferPtr(buffer)) {}

    /// Constructor that takes a raw pointer to a DataBuffer.
    BufferAccessConvertedTo(DataBuffer* buffer) : BufferAccessConvertedTo(DataBufferPtr(buffer)) {}

private:

    /// Helper function that checks the data type of the incoming data buffer and performs a copy-and-conversion
    /// operation only if necessary.
    template<typename DataBufferOrDerived>
    static DataOORef<DataBufferOrDerived> performDataTypeConversion(DataOORef<DataBufferOrDerived> buffer) {
        if(buffer && buffer->dataType() != DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value) {
            buffer.makeMutableInplace()->convertToDataType(DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        }
        return buffer;
    }
};

}   // End of namespace
