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

#pragma once


#include <ovito/core/Core.h>
#include "DataBuffer.h"

#include <boost/range/adaptor/strided.hpp>

namespace Ovito {

namespace detail {

/// General base class storing a reference to the underlying DataBuffer
/// and to the raw memory address where the buffer's data is currently stored.
template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode>
class BufferAccessBase
{
protected:

    static_assert(std::is_const_v<BufferType> == (accessmode == access_mode::read));

    using size_type = size_t;

    /// (Smart-)pointer to the DataBuffer whose data is being accessed.
    std::conditional_t<StrongReference, DataOORef<BufferType>, BufferType*> _buffer{};

#ifdef OVITO_USE_SYCL
    /// The type of SYCL buffer used by the DataBuffer class.
    using sycl_buffer_type = sycl::buffer<DataBuffer::Byte>;

    /// The type of SYCL accessor used to access the buffer's contents on the host.
    using sycl_accessor_type = sycl::host_accessor<DataBuffer::Byte, 1,
        (accessmode == access_mode::read) ? sycl::access_mode::read : (
            (accessmode == access_mode::write || accessmode == access_mode::discard_write) ? sycl::access_mode::write : sycl::access_mode::read_write)>;

    /// SYCL accessor providing direct memory access to the underlying SYCL buffer.
    sycl_accessor_type _syclAccessor;
#endif

    /// Raw pointer to the buffer's underlying memory. Needs to be updated whenever a reallocation occurs.
    std::conditional_t<accessmode == access_mode::read, const DataBuffer::Byte*, DataBuffer::Byte*> _data = nullptr;

    /// Helper function that obtains the buffer's internal storage address for data elements.
    auto dataStorageAddress() const {
#ifdef OVITO_USE_SYCL
        if constexpr(accessmode == access_mode::read)
            return !_syclAccessor.empty() ? static_cast<const DataBuffer::Byte*>(_syclAccessor.get_pointer()) : nullptr;
        else
            return !_syclAccessor.empty() ? _syclAccessor.get_pointer() : nullptr;
#else
        if constexpr(accessmode == access_mode::read)
            return _buffer->cdata();
        else
            return _buffer->data();
#endif
    }

#ifdef OVITO_USE_SYCL
    static inline sycl_accessor_type create_accessor(const DataBuffer* buffer, bool no_init) {
        return (buffer && buffer->_data)
            ? sycl_accessor_type{const_cast<sycl_buffer_type&>(*buffer->_data),
                no_init
                    ? sycl::property_list{sycl::no_init}
                    : sycl::property_list{}}
            : sycl_accessor_type{};
    }
#endif

    struct PrivateConstructorTag {};

    /// Constructor that associates the access object with a buffer object.
    BufferAccessBase(decltype(BufferAccessBase::_buffer) buffer, bool no_init, PrivateConstructorTag) :
        _buffer(std::move(buffer)),
#ifdef OVITO_USE_SYCL
        _syclAccessor(create_accessor(_buffer, accessmode == access_mode::discard_write || accessmode == access_mode::discard_read_write || no_init)),
#endif
        _data(_buffer ? dataStorageAddress() : nullptr) {

        // When modifying the contents of the buffer, invalidate the cached number of non-zero elements.
        if constexpr(accessmode != access_mode::read) {
            if(this->_buffer)
                this->_buffer->invalidateCachedInfo();
        }

#ifdef OVITO_DEBUG
        if(this->_buffer) {
            if constexpr(accessmode != access_mode::read)
                this->_buffer->prepareWriteAccess();
            else
                this->_buffer->prepareReadAccess();
            if constexpr(accessmode != access_mode::discard_write && accessmode != access_mode::discard_read_write) {
                OVITO_ASSERT(this->_buffer->size() == 0 || this->_buffer->_isDataInitialized || no_init);
            }
            if constexpr(accessmode != access_mode::read) {
                if(this->_buffer->size() != 0)
                    this->_buffer->_isDataInitialized = true;
            }
        }
#endif
    }

public:

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    BufferAccessBase() noexcept = default;

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    BufferAccessBase(std::nullptr_t) noexcept {}

    /// Constructor that associates the access object with a read-only buffer object (reference may be null).
    template<typename V = BufferType>
    BufferAccessBase(std::enable_if_t<std::is_const_v<V>, const BufferType*> buffer) : BufferAccessBase(buffer, false, PrivateConstructorTag{}) {}

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<bool U = StrongReference>
    BufferAccessBase(std::enable_if_t<!U, std::remove_const_t<BufferType>*> buffer, DataBuffer::BufferInitialization initMode = DataBuffer::BufferInitialization::Initialized)
        : BufferAccessBase(buffer, (initMode == DataBuffer::BufferInitialization::Uninitialized) ? true : false, PrivateConstructorTag{}) {}

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename Derived, typename U = std::enable_if_t<!StrongReference && std::is_base_of_v<BufferType, Derived>>>
    BufferAccessBase(const DataOORef<Derived>& buffer) : BufferAccessBase(buffer.get(), false, PrivateConstructorTag{}) {}

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename Derived, typename U = std::enable_if_t<StrongReference && std::is_const_v<BufferType> && std::is_base_of_v<BufferType, Derived>>>
    BufferAccessBase(DataOORef<const Derived> buffer) : BufferAccessBase(std::move(buffer), false, PrivateConstructorTag{}) {}

    /// Constructor that associates the access object with a buffer object (reference may be null).
    template<typename Derived, typename U = std::enable_if_t<StrongReference && std::is_base_of_v<BufferType, Derived>>>
    BufferAccessBase(DataOORef<Derived> buffer, DataBuffer::BufferInitialization initMode = DataBuffer::BufferInitialization::Initialized)
        : BufferAccessBase(std::move(buffer), (initMode == DataBuffer::BufferInitialization::Uninitialized) ? true : false, PrivateConstructorTag{}) {}

#ifdef OVITO_DEBUG
    /// Copy construction (only enabled for read-only accessors).
    BufferAccessBase(const BufferAccessBase& other) : BufferAccessBase(other._buffer, false, PrivateConstructorTag{}) {
        OVITO_ASSERT(accessmode == access_mode::read);
    }
#endif

#ifdef OVITO_DEBUG
    /// Copy assignment (only enabled for read-only accessors).
    BufferAccessBase& operator=(const BufferAccessBase& other) {
        this->_buffer = other._buffer;
#ifdef OVITO_USE_SYCL
        this->_syclAccessor = create_accessor(this->_buffer, accessmode == access_mode::discard_write || accessmode == access_mode::discard_read_write);
        this->_data = this->_buffer ? dataStorageAddress() : nullptr;
#else
        this->_data = other._data;
#endif
#ifdef OVITO_DEBUG
        OVITO_ASSERT(accessmode == access_mode::read);
        if(this->_buffer) {
            this->_buffer->prepareReadAccess();
            OVITO_ASSERT(this->_buffer->size() == 0 || this->_buffer->_isDataInitialized);
        }
#endif
        return *this;
    }
#endif

#ifdef OVITO_DEBUG
    /// Move construction.
    BufferAccessBase(BufferAccessBase&& other) noexcept :
        _buffer(std::exchange(other._buffer, nullptr)),
#ifdef OVITO_USE_SYCL
        _syclAccessor(std::exchange(other._syclAccessor, sycl_accessor_type{})),
#endif
        _data(std::exchange(other._data, nullptr)) {}
#endif

#ifdef OVITO_DEBUG
    /// Move assignment.
    BufferAccessBase& operator=(BufferAccessBase&& other) noexcept {
        this->_buffer = std::exchange(other._buffer, nullptr);
#ifdef OVITO_USE_SYCL
        this->_syclAccessor = std::exchange(other._syclAccessor, sycl_accessor_type{});
#endif
        this->_data = std::exchange(other._data, nullptr);
        return *this;
    }
#endif

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
    inline bool valid() const noexcept {
        return (bool)_buffer;
    }

#ifndef OVITO_USE_SYCL
    /// Returns whether this accessor points to a valid DataBuffer.
    inline explicit operator bool() const noexcept { return valid(); }
#else
    // Note: Workaround for conflict with SYCL's marray::operator!
    inline operator bool() const noexcept { return valid(); }
    inline bool operator!() const noexcept { return !valid(); }
#endif

    /// Returns the buffer object which is being accessed.
    inline const auto& buffer() const noexcept {
        return _buffer;
    }

    /// Moves the internal buffer reference out of this accessor object.
    auto take() noexcept {
        return reset();
    }

    /// Detaches the accessor object from the underlying buffer object.
    auto reset() {
#ifdef OVITO_DEBUG
        if(_buffer) {
            if constexpr(accessmode != access_mode::read)
                _buffer->finishWriteAccess();
            else
                _buffer->finishReadAccess();
        }
#endif
#ifdef OVITO_USE_SYCL
        _syclAccessor = {};
#endif
        _data = nullptr;
        return std::exchange(_buffer, nullptr);
    }

    /// Updates the internal data pointer (e.g. after the data buffer's memory has been reallocated).
    template<bool CanUpdateStorage = (accessmode != access_mode::read)>
    inline std::enable_if_t<CanUpdateStorage, void> updateDataStorageAddress(bool discardMode) {
        OVITO_ASSERT(_buffer);
#ifdef OVITO_USE_SYCL
        _syclAccessor = create_accessor(this->_buffer, accessmode == access_mode::discard_write || accessmode == access_mode::discard_read_write || discardMode);
#endif
        _data = dataStorageAddress();
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_buffer->size() == 0 || discardMode || accessmode == access_mode::discard_write || accessmode == access_mode::discard_read_write || _buffer->_isDataInitialized);
        if(_buffer->size() != 0)
            _buffer->_isDataInitialized = true;
#endif
    }
};

/// Base class that provides access to the individual data elements stored in a DataBuffer.
template<typename T, typename BufferType, bool StrongReference, Ovito::access_mode accessmode>
class BufferAccessTyped : public BufferAccessBase<BufferType, StrongReference, accessmode>
{
public:

    constexpr static bool ComponentWise = std::is_pointer_v<T>;

    using base_class = BufferAccessBase<BufferType, StrongReference, accessmode>;
    using element_type = std::remove_pointer_t<T>;
    using iterator = std::add_pointer_t<element_type>;
    using const_iterator = std::add_pointer_t<element_type>;
    using typename base_class::size_type;

    // Inherit constructors from base class.
    using BufferAccessBase<BufferType, StrongReference, accessmode>::BufferAccessBase;

#ifdef OVITO_DEBUG
    /// Copy constructor.
    BufferAccessTyped(const BufferAccessTyped& other) : base_class(other) {}

    /// Move constructor.
    BufferAccessTyped(BufferAccessTyped&& other) noexcept : base_class(std::move(other)) {}

    /// Copy assignment.
    BufferAccessTyped& operator=(const BufferAccessTyped& other) {
        base_class::operator=(static_cast<const base_class&>(other));
        return *this;
    }

    /// Move assignment.
    BufferAccessTyped& operator=(BufferAccessTyped&& other) noexcept {
        base_class::operator=(static_cast<base_class&&>(other));
        return *this;
    }
#endif

    /// Returns a pointer to the first element of the data array.
    inline element_type* begin() const noexcept {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(this->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(this->stride() == sizeof(element_type) * (ComponentWise ? this->componentCount() : 1));
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
    template<bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, const element_type*> cbegin() const noexcept { return begin(); }

    /// Returns a pointer to the end of the data array.
    template<bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, const element_type*> cend() const noexcept { return end(); }

    /// Returns the value of the i-th element from the array.
    template<bool CanRead1D = (!ComponentWise && accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead1D, const element_type&> get(size_type i) const noexcept {
        OVITO_ASSERT(i < this->size());
        return *(this->cbegin() + i);
    }

    /// Returns the value of the i-th element's j-th component from the array.
    template<bool CanRead2D = (ComponentWise && accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead2D, const element_type&> get(size_type i, size_type j) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        return *(cbegin() + (i * this->componentCount()) + j);
    }

    /// Sets the value of the i-th element in the array.
    template<bool CanWrite1D = (!ComponentWise && accessmode != access_mode::read)>
    inline void set(size_type i, std::enable_if_t<CanWrite1D, const element_type&> v) const noexcept {
        OVITO_ASSERT(i < this->size());
        *(this->begin() + i) = v;
    }

    /// Sets the value of the i-th element's j-th component in the array.
    template<bool CanWrite2D = (ComponentWise && accessmode != access_mode::read)>
    inline void set(size_type i, size_type j, std::enable_if_t<CanWrite2D, const element_type&> v) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        *(begin() + (i * this->componentCount()) + j) = v;
    }

    /// Returns a modifiable reference to the j-th component of the i-th element of the array.
    template<bool CanReadWrite2D = (ComponentWise && (accessmode == access_mode::read_write || accessmode == access_mode::discard_read_write))>
    inline std::enable_if_t<CanReadWrite2D, element_type&> value(size_type i, size_type j) const noexcept {
        OVITO_ASSERT(i < this->size());
        OVITO_ASSERT(j < this->componentCount());
        return *(begin() + i * this->componentCount() + j);
    }

    /// Indexed access to the elements of the array.
    template<bool CanRef1D = !ComponentWise>
    inline std::enable_if_t<CanRef1D, element_type&> operator[](size_type i) const noexcept {
        return *(begin() + i);
    }

    /// Returns a range of iterators over the elements stored in this array.
    inline boost::iterator_range<element_type*> range() const noexcept {
        return boost::make_iterator_range(begin(), end());
    }

    /// Returns a range of iterators over the elements stored in this array.
    template<bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, boost::iterator_range<const element_type*>> crange() const noexcept {
        return boost::make_iterator_range(cbegin(), cend());
    }

    /// Returns a range of iterators over the i-th vector component of all elements stored in this array.
    template<bool CanRef2D = ComponentWise>
    inline auto componentRange(std::enable_if_t<CanRef2D, size_type> componentIndex) const noexcept {
        OVITO_ASSERT(componentIndex >= 0 && componentIndex < this->componentCount());
        auto begin = this->begin() + componentIndex;
        return boost::adaptors::stride(boost::make_iterator_range(begin, begin + (this->size() * this->componentCount())), this->componentCount());
    }

    /// Turns this array accessor into an accessor for a subrange of elements.
    auto subrange(size_type beginIndex, size_type endIndex) && noexcept {
        class SubRange
        {
        public:
            using element_type = typename BufferAccessTyped<T, BufferType, StrongReference, accessmode>::element_type;
            using size_type = typename BufferAccessTyped<T, BufferType, StrongReference, accessmode>::size_type;
            using iterator = typename BufferAccessTyped<T, BufferType, StrongReference, accessmode>::iterator;
            using const_iterator = typename BufferAccessTyped<T, BufferType, StrongReference, accessmode>::const_iterator;
            SubRange(BufferAccessTyped<T, BufferType, StrongReference, accessmode>&& accessor, size_type beginIndex, size_type endIndex) noexcept : _accessor(std::move(accessor)), _beginIndex(beginIndex), _endIndex(endIndex) {}
            auto begin() const noexcept { return _accessor.begin() + _beginIndex; }
            auto end() const noexcept { return _accessor.begin() + _endIndex; }
        private:
            const BufferAccessTyped<T, BufferType, StrongReference, accessmode> _accessor;
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
    template<bool CanPush = (!ComponentWise && accessmode != access_mode::read)>
    inline void push_back(std::enable_if_t<CanPush, const element_type&> v) {
        size_t oldCount = this->size();
        if(this->buffer()->grow(1, true))
            this->updateDataStorageAddress(oldCount == 0);
        set(oldCount, v);
    }

    /// Appends a new multi-component element to the end of the data array.
    template<typename Range, bool CanPush = (ComponentWise && accessmode != access_mode::read)>
    inline std::enable_if_t<CanPush, void> push_back(Range&& r) {
        size_t oldCount = this->size();
        if(this->buffer()->grow(1, true))
            this->updateDataStorageAddress(oldCount == 0);
        size_type c = 0;
        for(const auto& v : r)
            set(oldCount, c++, v);
        OVITO_ASSERT(c == this->componentCount());
    }
};

/// Base class that provides generic read/write access data elements stored in a DataBuffer.
template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode>
class BufferAccessUntyped : public BufferAccessBase<BufferType, StrongReference, accessmode>
{
public:

    using base_class = BufferAccessBase<BufferType, StrongReference, accessmode>;
    using typename base_class::size_type;

    // Inherit constructors from base class.
    using BufferAccessBase<BufferType, StrongReference, accessmode>::BufferAccessBase;

#ifdef OVITO_DEBUG
    /// Copy constructor.
    BufferAccessUntyped(const BufferAccessUntyped& other) : base_class(other) {}

    /// Move constructor.
    BufferAccessUntyped(BufferAccessUntyped&& other) noexcept : base_class(std::move(other)) {}

    /// Copy assignment.
    BufferAccessUntyped& operator=(const BufferAccessUntyped& other) {
        base_class::operator=(static_cast<const base_class&>(other));
        return *this;
    }

    /// Move assignment.
    BufferAccessUntyped& operator=(BufferAccessUntyped&& other) noexcept {
        base_class::operator=(static_cast<base_class&&>(other));
        return *this;
    }
#endif

    /// Reads the j-th component of the i-th element from the array.
    template<typename U, bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, U> get(size_type i, size_type j) const {
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
    template<bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, const DataBuffer::Byte*> cdata(size_type component = 0) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (component * this->dataTypeSize());
    }

    /// Returns a pointer to the raw data of the data array.
    template<bool CanRead = (accessmode != access_mode::write && accessmode != access_mode::discard_write)>
    inline std::enable_if_t<CanRead, const DataBuffer::Byte*> cdata(size_type index, size_type component) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(index < this->size());
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (index * this->stride()) + (component * this->dataTypeSize());
    }

    /// Sets the j-th component of the i-th element of the array to a new value.
    template<typename U, bool CanWrite = (accessmode != access_mode::read)>
    inline std::enable_if_t<CanWrite, void> set(size_type i, size_type j, const U& value) {
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
    template<bool CanWrite = (accessmode != access_mode::read)>
    inline std::enable_if_t<CanWrite, DataBuffer::Byte*> data(size_type component = 0) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (component * this->dataTypeSize());
    }

    /// Returns a pointer to the raw data of the data array.
    template<bool CanWrite = (accessmode != access_mode::read)>
    inline std::enable_if_t<CanWrite, DataBuffer::Byte*> data(size_type index, size_type component) const {
        OVITO_ASSERT(this->_buffer);
        OVITO_ASSERT(this->dataStorageAddress() == this->_data);
        OVITO_ASSERT(index < this->size());
        OVITO_ASSERT(component < this->componentCount());
        return this->_data + (index * this->stride()) + (component * this->dataTypeSize());
    }
};

} // End of namespace detail.

/**
 * Provides access to the raw bytes of a DataBuffer or to individual
 * 2d array elements with automatic type casting.
*/
template<access_mode accessmode>
using RawBufferAccess = detail::BufferAccessUntyped<std::conditional_t<accessmode == access_mode::read, const DataBuffer, DataBuffer>, false, accessmode>;

/**
 * Provides read-only access to the raw bytes of a DataBuffer or to individual
 * 2d array elements with automatic type casting.
*/
using RawBufferReadAccess = RawBufferAccess<access_mode::read>;

/**
 * Same as RawBufferReadAccess but additionally keeps a strong reference to the DataBuffer object
 * to keep it alive while the acessor object exists.
*/
using RawBufferReadAccessAndRef = detail::BufferAccessUntyped<const DataBuffer, true, access_mode::read>;;

/**
 * Provides read-only access to individual array elements of a DataBuffer.
*/
template<typename T>
using BufferReadAccess = detail::BufferAccessTyped<std::conditional_t<std::is_pointer_v<T>,
        std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>,
        std::add_const_t<T>>,
    const DataBuffer, false, access_mode::read>;

/**
 * Provides read and/or write access to individual array elements of a DataBuffer.
*/
template<typename T, access_mode accessmode>
using BufferWriteAccess = detail::BufferAccessTyped<T, DataBuffer, false, accessmode>;

/**
 * Same as BufferWriteAccess but additionally keeps a strong reference to the DataBuffer object
 * to keep it alive while the acessor object exists.
*/
template<typename T, access_mode accessmode>
using BufferWriteAccessAndRef = detail::BufferAccessTyped<T, DataBuffer, true, accessmode>;

/**
 * Same as BufferReadAccess but additionally keeps a strong reference to the DataBuffer object
 * to keep it alive while the acessor object exists.
*/
template<typename T>
using BufferReadAccessAndRef = detail::BufferAccessTyped<std::conditional_t<std::is_pointer_v<T>,
        std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>,
        std::add_const_t<T>>,
    const DataBuffer, true, access_mode::read>;

/**
 * Utility class that behaves like a BufferReadAccessAndRef but performs
 * a conversion operation if necessary (creating a temporary data copy) to
 * guarantee a specific data type for the (read-only) data access.
 *
 * Use this class for input data buffers that use a particular data type most of the time
 * but occasionally use a different data type (then incurring a costly conversion operation).
*/
template<typename T>
class BufferAccessConvertedTo : public BufferReadAccessAndRef<T>
{
    using base_type = BufferReadAccessAndRef<T>;

public:

    using typename base_type::element_type;
    using typename base_type::iterator;
    using typename base_type::const_iterator;
    using typename base_type::size_type;

    /// Default constructor.
    BufferAccessConvertedTo() = default;

    /// Constructor that associates the access object with a buffer object (reference may be null).
    BufferAccessConvertedTo(DataOORef<const DataBuffer> buffer) : base_type(performDataTypeConversion(std::move(buffer))) {}

    /// Constructor that takes a raw pointer to a DataBuffer.
    BufferAccessConvertedTo(const DataBuffer* buffer) : BufferAccessConvertedTo(ConstDataBufferPtr(buffer)) {}

    /// Constructor that takes a raw pointer to a DataBuffer.
    BufferAccessConvertedTo(DataBuffer* buffer) : BufferAccessConvertedTo(DataBufferPtr(buffer)) {}

private:

    /// Helper function that checks the data type of the incoming data buffer and performs a copy-and-conversion
    /// operation only if necessary.
    static DataOORef<const DataBuffer> performDataTypeConversion(DataOORef<const DataBuffer> buffer) {
        if(buffer && buffer->dataType() != DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value) {
            buffer.makeMutableInplace()->convertToDataType(DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        }
        return buffer;
    }
};

/**
 * Utility class that behaves like a BufferWriteAccessAndRef but additionally allocates a new DataBuffer upon construction.
*/
template<typename T>
class BufferFactory : public BufferWriteAccessAndRef<T, access_mode::discard_write>
{
    using base_class = BufferWriteAccessAndRef<T, access_mode::discard_write>;
    using base_class::ComponentWise;
    using typename base_class::element_type;

public:

    /// Null constructor.
    BufferFactory() noexcept : BufferWriteAccessAndRef<T, access_mode::discard_write>() {}

    /// Constructor allocating a new uninitialized DataBuffer of the given size.
    template<bool IsEnabled = !ComponentWise>
    BufferFactory(std::enable_if_t<IsEnabled, size_t> elementCount) :
        base_class(DataBufferPtr::create(
            DataBuffer::BufferInitialization::Uninitialized,
            elementCount,
            DataBufferPrimitiveType<element_type>::value,
            DataBufferPrimitiveComponentCount<element_type>::value)) {}

    /// Constructor allocating a new DataBuffer and initializing it with the values from the given iterator range.
    template<typename InputIterator>
    BufferFactory(InputIterator begin, InputIterator end) : BufferFactory(std::distance(begin, end)) {
        std::copy(std::move(begin), std::move(end), this->begin());
    }

    /// Constructor allocating a new uninitialized vector array of the given size and component count.
    template<bool IsEnabled = ComponentWise>
    BufferFactory(std::enable_if_t<IsEnabled, size_t> elementCount, size_t componentCount, QStringList componentNames = QStringList()) :
        base_class(DataBufferPtr::create(
            DataBuffer::BufferInitialization::Uninitialized,
            elementCount,
            DataBufferPrimitiveType<element_type>::value,
            componentCount,
            std::move(componentNames))) {
        static_assert(DataBufferPrimitiveComponentCount<element_type>::value == 1);
    }
};

}   // End of namespace
