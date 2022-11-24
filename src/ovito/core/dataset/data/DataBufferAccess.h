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

#pragma once


#include <ovito/core/Core.h>
#include "DataBuffer.h"

#include <boost/range/adaptor/strided.hpp>

namespace Ovito { 

namespace detail {

// Base class that stores a pointer to an underlying DataBuffer.
template<class PointerType, bool Writable>
class DataBufferAccessBase
{
protected:

	/// (Smart-)pointer to the DataBuffer whose data is being accessed.
	PointerType _buffer{};

	/// Constructor that creates an invalid access object not associated with any buffer object.
	DataBufferAccessBase() noexcept = default;

	/// Constructor that associates the access object with a buffer object (may be null).
	DataBufferAccessBase(PointerType buffer) noexcept : _buffer(std::move(buffer)) {
#ifdef OVITO_DEBUG
		if(this->_buffer) {
			if(Writable) this->_buffer->prepareWriteAccess();
			else this->_buffer->prepareReadAccess();
		}
#endif
	}

	/// Copy construction (only available for read-only accessors).
	DataBufferAccessBase(const DataBufferAccessBase& other) : _buffer(other._buffer) {
#ifdef OVITO_DEBUG
		if(this->_buffer) {
			if(Writable) this->_buffer->prepareWriteAccess();
			else this->_buffer->prepareReadAccess();
		}
#endif
	}

	/// Copy assignment.
	DataBufferAccessBase& operator=(const DataBufferAccessBase& other) {
		this->_buffer = other._buffer;
#ifdef OVITO_DEBUG
		if(this->_buffer) {
			if(Writable) this->_buffer->prepareWriteAccess();
			else this->_buffer->prepareReadAccess();
		}
#endif
		return *this;
	}

	/// Move construction.
	DataBufferAccessBase(DataBufferAccessBase&& other) noexcept : _buffer(std::exchange(other._buffer, nullptr)) {}

	/// Move assignment.
	DataBufferAccessBase& operator=(DataBufferAccessBase&& other) noexcept {
		this->_buffer = std::exchange(other._buffer, nullptr);
		return *this;
	}

#ifdef OVITO_DEBUG
	/// Destructor sets the internal storage pointer to null to easier detect invalid memory access.
	~DataBufferAccessBase() { reset(); }
#endif

public:

	/// Returns the number of elements in the data array.
	size_t size() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->size(); 
	}

	/// Returns the number of vector components per element.
	size_t componentCount() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->componentCount(); 
	}

	/// Returns the number of bytes per element.
	size_t stride() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->stride(); 
	}

	/// Returns the number of bytes per vector component.
	size_t dataTypeSize() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->dataTypeSize(); 
	}

	/// Returns the data type of the property.
	int dataType() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->dataType(); 
	}

	/// Returns whether this accessor object points to a valid DataBuffer. 
	explicit operator bool() const noexcept {
		return (bool)this->_buffer;
	}

	/// Returns the buffer object which is being accessed by this class.
	const PointerType& buffer() const {
		return this->_buffer;
	}

	/// Moves the internal buffer reference out of this accessor object.
	PointerType take() {
		return reset();
	}

	/// Detaches the accessor object from the underlying buffer object.
	PointerType reset() {
#ifdef OVITO_DEBUG
		if(this->_buffer) {
			if(Writable) this->_buffer->finishWriteAccess();
			else this->_buffer->finishReadAccess();
		}
#endif
		return std::exchange(this->_buffer, nullptr);
	}
};

// Base class that allows read access to the data elements of the underlying DataBuffer.
template<typename T, class PointerType, bool Writable = false>
class ReadOnlyDataBufferSubrangeAccessBase : public DataBufferAccessBase<PointerType, Writable>
{
public:

	using iterator = const T*;
	using const_iterator = const T*;

	/// Returns the number of elements in the data array.
	size_t size() const { 
		OVITO_ASSERT(this->_buffer);
		return this->_endIndex - this->_beginIndex; 
	}

	/// Returns the value of the i-th element from the array.
	const T& get(size_t i) const {
		OVITO_ASSERT(i < this->size());
		return *(this->cbegin() + i);
	}

	/// Indexed access to the elements of the array.
	const T& operator[](size_t i) const {
		return this->get(i);
	}

	/// Returns a range of const iterators over the elements stored in this array.
	boost::iterator_range<const T*> crange() const {
		return boost::make_iterator_range(cbegin(), cend());
	}

	/// Returns a pointer to the first element of the data array.
	const T* begin() const {
		return cbegin();
	}

	/// Returns a pointer pointing to the end of the data array.
	const T* end() const {
		return cend();
	}

	/// Returns a pointer to the first element of the data array.
	const T* cbegin() const {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(this->_buffer->dataType() == DataBufferPrimitiveType<T>::value);
		OVITO_ASSERT(this->stride() == sizeof(T));
		return reinterpret_cast<const T*>(this->_buffer->cbuffer()) + this->_beginIndex;
	}

	/// Returns a pointer pointing to the end of the data array.
	const T* cend() const {
		return cbegin() + this->size();
	}

	/// Constructor that inherits the DataBuffer from another access object and takes an index sub-range.
	ReadOnlyDataBufferSubrangeAccessBase(DataBufferAccessBase<PointerType, Writable>&& other, size_t beginIndex, size_t endIndex) : DataBufferAccessBase<PointerType, Writable>(std::move(other)), 
		_beginIndex(beginIndex), _endIndex(endIndex) {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(this->_buffer->stride() == sizeof(T));
		OVITO_ASSERT(this->_buffer->dataType() == DataBufferPrimitiveType<T>::value);
		OVITO_ASSERT(beginIndex <= endIndex);
		OVITO_ASSERT(endIndex <= this->_buffer->size());
	}

protected:

	size_t _beginIndex;
	size_t _endIndex;
};

// Base class that allows read access to the data elements of the underlying DataBuffer.
template<typename T, class PointerType, bool Writable = false>
class ReadOnlyDataBufferAccessBase : public DataBufferAccessBase<PointerType, Writable>
{
public:

	using iterator = const T*;
	using const_iterator = const T*;

	/// Returns the value of the i-th element from the array.
	const T& get(size_t i) const {
		OVITO_ASSERT(i < this->size());
		return *(this->cbegin() + i);
	}

	/// Indexed access to the elements of the array.
	const T& operator[](size_t i) const {
		return this->get(i);
	}

	/// Returns a range of const iterators over the elements stored in this array.
	boost::iterator_range<const T*> crange() const {
		return boost::make_iterator_range(cbegin(), cend());
	}

	/// Turns this array accessor into an accessor for a subrange of elements.
	ReadOnlyDataBufferSubrangeAccessBase<T, PointerType, Writable> csubrange(size_t beginIndex, size_t endIndex) && {
		return ReadOnlyDataBufferSubrangeAccessBase<T, PointerType, Writable>(std::move(*this), beginIndex, endIndex);
	}

	/// Returns a pointer to the first element of the data array.
	const T* begin() const {
		return cbegin();
	}

	/// Returns a pointer pointing to the end of the data array.
	const T* end() const {
		return cend();
	}

	/// Returns a pointer to the first element of the data array.
	const T* cbegin() const {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(this->_buffer->dataType() == DataBufferPrimitiveType<T>::value);
		OVITO_ASSERT(this->stride() == sizeof(T));
		return reinterpret_cast<const T*>(this->_buffer->cbuffer());
	}

	/// Returns a pointer pointing to the end of the data array.
	const T* cend() const {
		return cbegin() + this->size();
	}

protected:

	/// Constructor that creates an invalid access object not associated with any DataBuffer.
	ReadOnlyDataBufferAccessBase() {}

	/// Constructor that associates the access object with a DataBuffer (may be null).
	ReadOnlyDataBufferAccessBase(PointerType buffer) : DataBufferAccessBase<PointerType, Writable>(std::move(buffer)) {
		OVITO_ASSERT(!this->_buffer || this->_buffer->stride() == sizeof(T));
		OVITO_ASSERT(!this->_buffer || this->_buffer->dataType() == DataBufferPrimitiveType<T>::value);
	}
};

// Base class that allows read access to the individual components of vector elements of the underlying DataBuffer.
template<typename T, class PointerType, bool Writable = false>
class ReadOnlyDataBufferAccessBaseTable : public DataBufferAccessBase<PointerType, Writable>
{
public:

	using iterator = const T*;
	using const_iterator = const T*;

	/// Returns the value of the i-th element from the array.
	const T& get(size_t i, size_t j) const {
		OVITO_ASSERT(i < this->size());
		OVITO_ASSERT(j < this->componentCount());
		return *(this->cbegin() + (i * this->componentCount()) + j);
	}

	/// Returns a pointer to the beginning of the data array.
	const T* cbegin() const {
		return reinterpret_cast<const T*>(this->_buffer->cbuffer());
	}

	/// Returns a pointer to the end of the data array.
	const T* cend() const {
		return this->cbegin() + (this->size() * this->componentCount());
	}

	/// Returns a range of iterators over the i-th vector component of all elements stored in this array.
	auto componentRange(size_t componentIndex) const {
		OVITO_ASSERT(this->componentCount() > componentIndex);
		const T* begin = cbegin() + componentIndex;
		return boost::adaptors::stride(boost::make_iterator_range(begin, begin + (this->size() * this->componentCount())), this->componentCount());
	}

protected:

	/// Constructor that creates an invalid access object not associated with any DataBuffer.
	ReadOnlyDataBufferAccessBaseTable() {}

	/// Constructor that associates the access object with a DataBuffer (may be null).
	ReadOnlyDataBufferAccessBaseTable(PointerType buffer) : DataBufferAccessBase<PointerType, Writable>(std::move(buffer)) {
		OVITO_ASSERT(!this->_buffer || this->_buffer->stride() == sizeof(T) * this->_buffer->componentCount());
		OVITO_ASSERT(!this->_buffer || this->_buffer->dataType() == qMetaTypeId<T>());
		OVITO_ASSERT(!this->_buffer || this->_buffer->dataTypeSize() == sizeof(T));
	}
};

// Base class that allows read access to the raw data of the underlying DataBuffer.
template<class PointerType, bool Writable>
class ReadOnlyDataBufferAccessBaseTable<void, PointerType, Writable> : public DataBufferAccessBase<PointerType, Writable>
{
public:

	/// Returns the j-th component of the i-th element in the array.
	template<typename U>
	U get(size_t i, size_t j) const {
		switch(this->dataType()) {
		case DataBuffer::Float:
			return static_cast<U>(*reinterpret_cast<const FloatType*>(this->cdata(j) + i * this->stride()));
		case DataBuffer::Int:
			return static_cast<U>(*reinterpret_cast<const int*>(this->cdata(j) + i * this->stride()));
		case DataBuffer::Int64:
			return static_cast<U>(*reinterpret_cast<const qlonglong*>(this->cdata(j) + i * this->stride()));
		default:
			OVITO_ASSERT(false);
			throw Exception(QStringLiteral("Data access failed. Data buffer has a non-standard data type."));
		}
	}

	/// Returns a pointer to the raw data of the data array.
	const uint8_t* cdata(size_t component = 0) const {
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->cbuffer() + (component * this->dataTypeSize());
	}

	/// Returns a pointer to the raw data of the data array.
	const uint8_t* cdata(size_t index, size_t component) const {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(index < this->size());
		OVITO_ASSERT(component < this->componentCount());
		return this->_buffer->cbuffer() + (index * this->stride()) + (component * this->dataTypeSize());
	}

protected:

	// Inherit constructors from base class.
	using DataBufferAccessBase<PointerType, Writable>::DataBufferAccessBase;
};

// Base class that allows read/write access to the data elements of the underlying DataBuffer.
template<typename T, class PointerType>
class ReadWriteDataBufferSubrangeAccessBase : public ReadOnlyDataBufferSubrangeAccessBase<T, PointerType, true>
{
public:

	using iterator = T*;
	using const_iterator = T*;

	/// Sets the value of the i-th element in the array.
	void set(size_t i, const T& v) {
		OVITO_ASSERT(i < this->size());
		*(this->begin() + i) = v;
	}

	/// Indexed access to the elements of the array.
	T& operator[](size_t i) {
		OVITO_ASSERT(i < this->size());
		return *(this->begin() + i);
	}

	/// Indexed access to the elements of the array.
	const T& operator[](size_t i) const {
		OVITO_ASSERT(i < this->size());
		return *(this->cbegin() + i);
	}

	/// Returns a pointer to the first element of the data array.
	T* begin() const {
		OVITO_ASSERT(this->_buffer);
		return reinterpret_cast<T*>(this->_buffer->buffer()) + this->_beginIndex;
	}

	/// Returns a pointer pointing to the end of the data array.
	T* end() const {
		return this->begin() + this->size();
	}

	/// Returns a range of iterators over the elements stored in this array.
	boost::iterator_range<T*> range() {
		return boost::make_iterator_range(begin(), end());
	}

protected:

	// Inherit constructors from base class.
	using ReadOnlyDataBufferSubrangeAccessBase<T, PointerType, true>::ReadOnlyDataBufferSubrangeAccessBase;
};

// Base class that allows read/write access to the data elements of the underlying DataBuffer.
template<typename T, class PointerType>
class ReadWriteDataBufferAccessBase : public ReadOnlyDataBufferAccessBase<T, PointerType, true>
{
public:

	using iterator = T*;
	using const_iterator = T*;

	/// Sets the value of the i-th element in the array.
	void set(size_t i, const T& v) {
		OVITO_ASSERT(i < this->size());
		*(this->begin() + i) = v;
	}

	/// Indexed access to the elements of the array.
	T& operator[](size_t i) {
		OVITO_ASSERT(i < this->size());
		return *(this->begin() + i);
	}

	/// Indexed access to the elements of the array.
	const T& operator[](size_t i) const {
		OVITO_ASSERT(i < this->size());
		return *(this->cbegin() + i);
	}

	/// Returns a pointer to the first element of the data array.
	T* begin() const {
		OVITO_ASSERT(this->_buffer);
		return reinterpret_cast<T*>(this->_buffer->buffer());
	}

	/// Returns a pointer pointing to the end of the data array.
	T* end() const {
		return this->begin() + this->size();
	}

	/// Returns a range of iterators over the elements stored in this array.
	boost::iterator_range<T*> range() {
		return boost::make_iterator_range(begin(), end());
	}

	/// Returns a subrange of elements.
	ReadWriteDataBufferSubrangeAccessBase<T, PointerType> subrange(size_t beginIndex, size_t endIndex) && {
		return ReadWriteDataBufferSubrangeAccessBase<T, PointerType>(std::move(*this), beginIndex, endIndex);
	}

	/// Appends a new element to the end of the data array.
	void push_back(const T& v) {
		size_t oldCount = this->size();
		this->buffer()->grow(1, true);
		set(oldCount, v);
	}

protected:

	// Inherit constructors from base class.
	using ReadOnlyDataBufferAccessBase<T, PointerType, true>::ReadOnlyDataBufferAccessBase;
};

// Base class that allows read/write access to the individual components of the vector elements of the underlying DataBuffer.
template<typename T, class PointerType>
class ReadWriteDataBufferAccessBaseTable : public ReadOnlyDataBufferAccessBaseTable<T, PointerType, true>
{
public:

	using iterator = T*;
	using const_iterator = T*;

	/// Returns a pointer to the first element of the data array.
	T* begin() const {
		OVITO_ASSERT(this->_buffer);
		return reinterpret_cast<T*>(this->_buffer->buffer());
	}

	/// Returns a pointer pointing to the end of the data array.
	T* end() const {
		OVITO_ASSERT(this->stride() == sizeof(T) * this->componentCount());
		return this->begin() + (this->size() * this->componentCount());
	}

	/// Returns a range of iterators over the i-th vector component of all elements stored in this array.
	auto componentRange(size_t componentIndex) {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(this->_buffer->componentCount() > componentIndex);
		T* begin = this->begin() + componentIndex;
		return boost::adaptors::stride(boost::make_iterator_range(begin, begin + (this->size() * this->componentCount())), this->componentCount());
	}

	/// Returns a range of iterators over the elements stored in this array.
	boost::iterator_range<T*> range() {
		return boost::make_iterator_range(begin(), end());
	}

	/// Sets the j-th component of the i-th element of the array to a new value.
	void set(size_t i, size_t j, const T& value) {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(i < this->size());
		OVITO_ASSERT(j < this->componentCount());
		*(begin() + i * this->componentCount() + j) = value;
	}

	/// Returns a modifiable reference to the j-th component of the i-th element of the array.
	T& value(size_t i, size_t j) {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(i < this->size());
		OVITO_ASSERT(j < this->componentCount());
		return *(begin() + i * this->componentCount() + j);
	}

protected:

	// Inherit constructors from base class.
	using ReadOnlyDataBufferAccessBaseTable<T, PointerType, true>::ReadOnlyDataBufferAccessBaseTable;
};

// Base class that allows read/write access to the raw data of the underlying DataBuffer.
template<class PointerType>
class ReadWriteDataBufferAccessBaseTable<void, PointerType> : public ReadOnlyDataBufferAccessBaseTable<void, PointerType, true>
{
public:

	/// Sets the j-th component of the i-th element of the array to a new value.
	template<typename U>
	void set(size_t i, size_t j, const U& value) {
		OVITO_ASSERT(this->_buffer);
		switch(this->_buffer->dataType()) {
		case DataBuffer::Float:
			*reinterpret_cast<FloatType*>(this->data(j) + i * this->stride()) = value;
			break;
		case DataBuffer::Int:
			*reinterpret_cast<int*>(this->data(j) + i * this->stride()) = value;
			break;
		case DataBuffer::Int64:
			*reinterpret_cast<qlonglong*>(this->data(j) + i * this->stride()) = value;
			break;
		default:
			OVITO_ASSERT(false);
			throw Exception(QStringLiteral("Data access failed. Data buffer has a non-standard data type."));
		}
	}

	/// Returns a pointer to the raw data of the data array.
	uint8_t* data(size_t component = 0) {
		OVITO_ASSERT(this->_buffer);
		return this->_buffer->buffer() + (component * this->dataTypeSize());
	}

	/// Returns a pointer to the raw data of the data array.
	uint8_t* data(size_t index, size_t component) {
		OVITO_ASSERT(this->_buffer);
		OVITO_ASSERT(index < this->size());
		OVITO_ASSERT(component < this->componentCount());
		return this->_buffer->buffer() + (index * this->stride()) + (component * this->dataTypeSize());
	}

protected:

	// Inherit constructors from base class.
	using ReadOnlyDataBufferAccessBaseTable<void, PointerType, true>::ReadOnlyDataBufferAccessBaseTable;
};

} // End of namespace detail.

/**
 * Helper class that provides read access to the data elements of a DataBuffer.
 * 
 * The TableMode template parameter should be set to true if access to the individual components
 * of a vector data array is desired or if the number of vector components is unknown at compile time. 
 * If TableMode is set to false, the data elements can only be access as a whole and the number of components must
 * be a compile-time constant.
 */
template<typename T, bool TableMode = false, typename DataBufferClass = DataBuffer>
class ConstDataBufferAccess : public std::conditional_t<TableMode, Ovito::detail::ReadOnlyDataBufferAccessBaseTable<T, const DataBufferClass*>, Ovito::detail::ReadOnlyDataBufferAccessBase<T, const DataBufferClass*>>
{
	using ParentType = std::conditional_t<TableMode, Ovito::detail::ReadOnlyDataBufferAccessBaseTable<T, const DataBufferClass*>, Ovito::detail::ReadOnlyDataBufferAccessBase<T, const DataBufferClass*>>;

public:

	/// Constructs an accessor object not associated yet with any DataBuffer.
	ConstDataBufferAccess() = default;

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccess(const DataBufferClass* buffer) 
		: ParentType(buffer) {}

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccess(const DataOORef<const DataBufferClass>& buffer)
		: ParentType(buffer.get()) {}

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccess(const DataOORef<DataBufferClass>& buffer)
		: ParentType(buffer.get()) {}
};

/**
 * Helper class that provides read access to the data elements in a DataBuffer
 *        and which keeps a strong reference to the DataBuffer.
 */
template<typename T, bool TableMode = false, typename DataBufferClass = DataBuffer>
class ConstDataBufferAccessAndRef : public std::conditional_t<TableMode, Ovito::detail::ReadOnlyDataBufferAccessBaseTable<T, DataOORef<const DataBufferClass>>, Ovito::detail::ReadOnlyDataBufferAccessBase<T, DataOORef<const DataBufferClass>>>
{
	using ParentType = std::conditional_t<TableMode, Ovito::detail::ReadOnlyDataBufferAccessBaseTable<T, DataOORef<const DataBufferClass>>, Ovito::detail::ReadOnlyDataBufferAccessBase<T, DataOORef<const DataBufferClass>>>;

public:

	/// Constructs an accessor object not associated yet with any DataBuffer.
	ConstDataBufferAccessAndRef() = default;

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccessAndRef(DataOORef<const DataBufferClass> buffer)
		: ParentType(std::move(buffer)) {}

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccessAndRef(DataOORef<DataBufferClass> buffer)
		: ParentType(std::move(buffer)) {}

	/// Constructs a read-only accessor for the data in a DataBuffer.
	ConstDataBufferAccessAndRef(const DataBufferClass* buffer)
		: ParentType(DataOORef<const DataBufferClass>(buffer)) {}
};

/**
 * Helper class that provides read/write access to the data elements in a DataBuffer.
 * 
 * The TableMode template parameter should be set to true if access to the individual components
 * of a vector data array is desired or if the number of vector components of the property is unknown at compile time. 
 * If TableMode is set to false, the data elements can only be access as a whole and the number of components must
 * be a compile-time constant.
 * 
 * If the DataBufferAccess object is initialized from a DataBuffer pointer, the buffer object's notifyTargetChanged()
 * method will be automatically called when the DataBufferAccess object goes out of scope to inform the system about
 * a modification of the stored property values.
 */
template<typename T, bool TableMode = false, typename DataBufferClass = DataBuffer>
class DataBufferAccess : public std::conditional_t<TableMode, Ovito::detail::ReadWriteDataBufferAccessBaseTable<T, DataBufferClass*>, Ovito::detail::ReadWriteDataBufferAccessBase<T, DataBufferClass*>>
{
	using ParentType = std::conditional_t<TableMode, Ovito::detail::ReadWriteDataBufferAccessBaseTable<T, DataBufferClass*>, Ovito::detail::ReadWriteDataBufferAccessBase<T, DataBufferClass*>>;

public:

	/// Constructs an accessor object not associated yet with any DataBuffer.
	DataBufferAccess() = default;

	/// Constructs a read/write accessor for the data in a DataBuffer.
	DataBufferAccess(DataBufferClass* buffer) 
		: ParentType(buffer) {}

	/// Constructs a read/write accessor for the data in a DataBuffer.
	DataBufferAccess(const DataOORef<DataBufferClass>& buffer) 
		: ParentType(buffer.get()) {}

	/// Forbid copy construction.
	DataBufferAccess(const DataBufferAccess& other) = delete;

	/// Allow move construction.
	DataBufferAccess(DataBufferAccess&& other) = default;

	/// Forbid copy assignment.
	DataBufferAccess& operator=(const DataBufferAccess& other) = delete;

	/// Allow move assignment.
	DataBufferAccess& operator=(DataBufferAccess&& other) = default;
};

/**
 * Helper class that provides read/write access to the data elements in a DataBuffer object
 *        and which keeps a strong reference to the DataBuffer.
 */
template<typename T, bool TableMode = false, typename DataBufferClass = DataBuffer>
class DataBufferAccessAndRef : public std::conditional_t<TableMode, Ovito::detail::ReadWriteDataBufferAccessBaseTable<T, DataOORef<DataBufferClass>>, Ovito::detail::ReadWriteDataBufferAccessBase<T, DataOORef<DataBufferClass>>>
{
	using ParentType = std::conditional_t<TableMode, Ovito::detail::ReadWriteDataBufferAccessBaseTable<T, DataOORef<DataBufferClass>>, Ovito::detail::ReadWriteDataBufferAccessBase<T, DataOORef<DataBufferClass>>>;

public:

	/// Constructs an accessor object not associated yet with any DataBuffer.
	DataBufferAccessAndRef() = default;

	/// Constructs a read/write accessor for the data in a DataBuffer.
	DataBufferAccessAndRef(DataOORef<DataBufferClass> buffer) 
		: ParentType(std::move(buffer)) {}

	/// Forbid copy construction.
	DataBufferAccessAndRef(const DataBufferAccessAndRef& other) = delete;

	/// Allow move construction.
	DataBufferAccessAndRef(DataBufferAccessAndRef&& other) = default;

	/// Forbid copy assignment.
	DataBufferAccessAndRef& operator=(const DataBufferAccessAndRef& other) = delete;

	/// Allow move assignment.
	DataBufferAccessAndRef& operator=(DataBufferAccessAndRef&& other) = default;
};

}	// End of namespace
