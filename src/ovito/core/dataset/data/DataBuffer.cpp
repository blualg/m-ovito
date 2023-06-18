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
#include <ovito/core/dataset/DataSet.h>
#include "DataBuffer.h"
#include "BufferAccess.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataBuffer);

#ifdef OVITO_USE_SYCL
/// Helper function allocating a new SYCL buffer object of the given size.
static cl::sycl::buffer<std::byte> allocateSyclBuffer(size_t nelements, size_t stride)
{
#if 1
    // Using make_async_buffer() to create a buffer with a destructor that does not block.
    return cl::sycl::make_async_buffer<std::byte, 1>(nelements * stride);
#else
    return cl::sycl::buffer<std::byte, 1>(nelements * stride);
#endif
}
#endif

/******************************************************************************
* Constructor allocating a buffer array with given size and data layout.
******************************************************************************/
DataBuffer::DataBuffer(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, QStringList componentNames) :
    DataObject(flags),
    _dataType(dataType),
    _dataTypeSize(QMetaType(dataType).sizeOf()),
    _componentCount(componentCount),
    _componentNames(std::move(componentNames))
{
    _stride = _dataTypeSize * _componentCount;
    OVITO_ASSERT(dataType == Int8 || dataType == Int32 || dataType == Int64 || dataType == Float32 || dataType == Float64);
    OVITO_ASSERT(_dataTypeSize > 0);
    OVITO_ASSERT(_componentCount > 0);
    OVITO_ASSERT(_componentNames.empty() || _componentCount == _componentNames.size());
    OVITO_ASSERT(_stride >= _dataTypeSize * _componentCount);
    OVITO_ASSERT((_stride % _dataTypeSize) == 0);
    if(componentCount > 1) {
        for(size_t i = _componentNames.size(); i < componentCount; i++)
            _componentNames << QString::number(i + 1);
    }
    resize(elementCount, init == Initialized);
}

/******************************************************************************
* Creates a copy of a buffer object.
******************************************************************************/
OORef<RefTarget> DataBuffer::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<DataBuffer> clone = static_object_cast<DataBuffer>(DataObject::clone(deepCopy, cloneHelper));

    // Copy internal data.
    ReadAccess readAccess(*this);
    clone->_dataType = _dataType;
    clone->_dataTypeSize = _dataTypeSize;
    clone->_numElements = _numElements;
    clone->_stride = _stride;
    clone->_componentCount = _componentCount;
    clone->_componentNames = _componentNames;
#ifdef OVITO_DEBUG
    clone->_isDataInitialized = _isDataInitialized;
#endif
#ifdef OVITO_USE_SYCL
    if(clone->_numElements != 0) {
        // Allocate new buffer.
        clone->_data = allocateSyclBuffer(_numElements, _stride);
        // Copy data on the SYCL device.
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
            _hasScheduledSyclReadOperations = true;
            cgh.copy(
                _data->get_access(cgh, cl::sycl::range(_numElements * _stride), cl::sycl::read_only),
                clone->_data->get_access(cgh, cl::sycl::write_only, cl::sycl::no_init)
            );
        });
    }
#else
    clone->_capacity = _numElements;
    clone->_data.reset(new std::byte[_numElements * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
    std::memcpy(clone->_data.get(), _data.get(), _numElements * _stride);
#endif

    return clone;
}

/******************************************************************************
* Resizes the storage.
******************************************************************************/
void DataBuffer::resize(size_t newSize, bool preserveData)
{
    OVITO_ASSERT(_isDataInitialized || !preserveData || newSize == 0 || size() == 0);
#ifdef OVITO_DEBUG
    WriteAccess writeAccess(*this);
    _isDataInitialized = preserveData;
#endif

    // Note: We do not reallocate the storage for performance reasons when the number of elements becomes smaller.
#ifdef OVITO_USE_SYCL
    if(newSize != 0 && (!_data || newSize * _stride > _data->get_range()[0])) {
        cl::sycl::buffer<std::byte> newBuffer = allocateSyclBuffer(newSize, _stride);
        if(preserveData && _numElements != 0) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
                _hasScheduledSyclReadOperations = true;
                cgh.copy(
                    this->_data->get_access(cgh, cl::sycl::range(_numElements * _stride), cl::sycl::read_only),
                    newBuffer.get_access(cgh, cl::sycl::range(_numElements * _stride), cl::sycl::write_only, cl::sycl::no_init)
                );
            });
        }
        _data = std::move(newBuffer);
    }
    // Initialize newly appended data elements to zero.
    if(newSize > _numElements && preserveData) {
        OVITO_ASSERT(_data);
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
            cgh.fill(this->_data->get_access(cgh,
                cl::sycl::range((newSize - _numElements) * _stride),
                cl::sycl::id(_numElements * _stride),
                cl::sycl::write_only,
                _numElements == 0 ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{}), (std::byte)0);
        });
    }
#else
    if(newSize > _capacity) {
        std::unique_ptr<std::byte[]> newBuffer(new std::byte[newSize * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
        if(preserveData)
            std::memcpy(newBuffer.get(), _data.get(), _stride * std::min(_numElements, newSize));
        _data.swap(newBuffer);
        _capacity = newSize;
    }
    // Initialize newly appended data elements to zero.
    if(newSize > _numElements && preserveData) {
        OVITO_ASSERT(_data);
        std::memset(_data.get() + _numElements * _stride, 0, (newSize - _numElements) * _stride);
    }
#endif
    _numElements = newSize;
}

/******************************************************************************
* Resizes the buffer and copies the data element from an existing buffer.
******************************************************************************/
void DataBuffer::resizeCopyFrom(size_t newSize, const DataBuffer& original)
{
    OVITO_ASSERT(original._isDataInitialized || newSize == 0 || original.size() == 0);
    OVITO_ASSERT(original.dataType() == this->dataType() && original.stride() == this->stride());
    OVITO_ASSERT(newSize != this->size() || newSize == 0);
#ifdef OVITO_DEBUG
    WriteAccess writeAccess(*this);
    _isDataInitialized = true;
#endif

#ifdef OVITO_USE_SYCL
    if(newSize != 0 && (this != &original || !this->_data || newSize * _stride > this->_data->get_range()[0])) {
        cl::sycl::buffer<std::byte> newBuffer = allocateSyclBuffer(newSize, _stride);
        if(original._numElements != 0) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
                size_t nbytes = std::min(newSize, original._numElements) * original._stride;
                original._hasScheduledSyclReadOperations = true;
                cgh.copy(
                    original._data->get_access(cgh, cl::sycl::range(nbytes), cl::sycl::read_only),
                    newBuffer.get_access(cgh, cl::sycl::write_only, cl::sycl::no_init)
                );
            });
        }
        _data = std::move(newBuffer);
    }
    _numElements = std::min(newSize, original._numElements);
    // Initialize newly appended data elements to zero.
    if(newSize > original._numElements) {
        OVITO_ASSERT(_data);
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
            cgh.fill(this->_data->get_access(cgh,
                cl::sycl::range((newSize - _numElements) * _stride),
                cl::sycl::id(_numElements * _stride),
                cl::sycl::write_only,
                _numElements == 0 ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{}), (std::byte)0);
        });
    }
#else
    if(newSize > _capacity) {
        std::unique_ptr<std::byte[]> newBuffer(new std::byte[newSize * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
        std::memcpy(newBuffer.get(), original._data.get(), _stride * std::min(original._numElements, newSize));
        _data.swap(newBuffer);
        _capacity = newSize;
    }
    // Initialize newly appended data elements to zero.
    if(newSize > original._numElements) {
        OVITO_ASSERT(_data);
        std::memset(_data.get() + original._numElements * _stride, 0, (newSize - original._numElements) * _stride);
    }
#endif
    _numElements = newSize;
}

/******************************************************************************
* Grows the number of data elements while preserving the exiting data.
* True if the memory buffer was reallocated, because the current capacity was insufficient
* to accommodate the new elements.
******************************************************************************/
bool DataBuffer::grow(size_t numAdditionalElements, bool callerAlreadyHasWriteAccess)
{
    OVITO_ASSERT(size() == 0 || _isDataInitialized);
    if(numAdditionalElements == 0)
        return false;
#ifdef OVITO_DEBUG
    std::optional<WriteAccess> writeAccess;
    if(!callerAlreadyHasWriteAccess)
        writeAccess.emplace(*this);
#endif

    size_t newSize = _numElements + numAdditionalElements;
    OVITO_ASSERT(newSize >= _numElements);
#ifdef OVITO_USE_SYCL
    bool needToGrow = (!_data || newSize * _stride > _data->get_range()[0]);
#else
    bool needToGrow = (newSize > _capacity);
#endif
    if(needToGrow) {
        // Grow the storage capacity of the data buffer.
        size_t newCapacity = (newSize < 1024)
            ? std::max(newSize * 2, (size_t)256)
            : (newSize * 3 / 2);
#ifdef OVITO_USE_SYCL
        if(newCapacity != 0) {
            cl::sycl::buffer<std::byte> newBuffer = allocateSyclBuffer(newCapacity, _stride);
            if(_numElements != 0) {
                cl::sycl::host_accessor writeAccessor(newBuffer, cl::sycl::write_only, cl::sycl::no_init);
                cl::sycl::host_accessor readAccessor(*_data, cl::sycl::read_only);
                std::memcpy(writeAccessor.get_pointer(), readAccessor.get_pointer(), _stride * _numElements);
            }
            _data = std::move(newBuffer);
        }
        else _data = {};
#else
        std::unique_ptr<std::byte[]> newBuffer(new std::byte[newCapacity * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
        std::memcpy(newBuffer.get(), _data.get(), _stride * _numElements);
        _data.swap(newBuffer);
        _capacity = newCapacity;
#endif
    }
    _numElements = newSize;
    return needToGrow;
}

/******************************************************************************
* Reduces the number of data elements while preserving the exiting data.
* Note: This method never reallocates the memory buffer. Thus, the capacity of the array remains unchanged and the
* memory of the truncated elements is not released by the method.
******************************************************************************/
void DataBuffer::truncate(size_t numElementsToRemove, bool callerAlreadyHasWriteAccess)
{
    OVITO_ASSERT(numElementsToRemove <= _numElements);

#ifdef OVITO_DEBUG
    std::optional<WriteAccess> writeAccess;
    if(!callerAlreadyHasWriteAccess)
        writeAccess.emplace(*this);
#endif
    _numElements -= numElementsToRemove;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void DataBuffer::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    DataObject::saveToStream(stream, excludeRecomputableData);

    ReadAccess readAccess(*this);
    stream.beginChunk(0x03);
    stream << QByteArray(QMetaType(_dataType).name());
    stream.writeSizeT(_dataTypeSize);
    stream.writeSizeT(_stride);
    stream.writeSizeT(_componentCount);
    stream << _componentNames;
    if(excludeRecomputableData) {
        stream.writeSizeT(0);
    }
    else {
        stream.writeSizeT(_numElements);
        OVITO_ASSERT(_isDataInitialized);
#ifdef OVITO_USE_SYCL
        if(_numElements != 0) {
            cl::sycl::host_accessor readAccessor(*_data, cl::sycl::read_only);
            stream.write(readAccessor.get_pointer(), _stride * _numElements);
        }
#else
        stream.write(_data.get(), _stride * _numElements);
#endif
    }
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void DataBuffer::loadFromStream(ObjectLoadStream& stream)
{
    // Current file format:
    if(stream.formatVersion() >= 30007) {
        DataObject::loadFromStream(stream);
        stream.expectChunk(0x03);
    }

    QByteArray dataTypeName;
    stream >> dataTypeName;
    _dataType = QMetaType::fromName(dataTypeName).id();
    OVITO_ASSERT_MSG(_dataType != 0, "DataBuffer::loadFromStream()", qPrintable(QString("The metadata type '%1' seems to be no longer defined.").arg(QString::fromLatin1(dataTypeName))));
    OVITO_ASSERT(dataTypeName == this->dataTypeName());
    stream.readSizeT(_dataTypeSize);
    stream.readSizeT(_stride);
    stream.readSizeT(_componentCount);
    stream >> _componentNames;
    stream.readSizeT(_numElements);
#ifdef OVITO_DEBUG
    if(_numElements != 0)
        _isDataInitialized = true;
#endif
#ifdef OVITO_USE_SYCL
    if(_numElements != 0) {
        _data = allocateSyclBuffer(_numElements, _stride);
        cl::sycl::host_accessor writeAccessor(*_data, cl::sycl::write_only, cl::sycl::no_init);
        stream.read(writeAccessor.get_pointer(), _stride * _numElements);
    }
#else
    _capacity = _numElements;
    _data.reset(new std::byte[_numElements * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
    stream.read(_data.get(), _stride * _numElements);
#endif
    stream.closeChunk();
}

/******************************************************************************
* Replicates existing data N times.
******************************************************************************/
void DataBuffer::replicateFrom(size_t n, const DataBuffer& original)
{
    OVITO_ASSERT(original._isDataInitialized || original.size() == 0);
    OVITO_ASSERT(n >= 1);
    OVITO_ASSERT(_numElements == n * original._numElements);
    OVITO_ASSERT(this != &original);
    OVITO_ASSERT(this->stride() == original.stride() && this->dataType() == original.dataType());
    if(size() == 0)
        return;
#ifdef OVITO_DEBUG
    _isDataInitialized = true;
#endif

    WriteAccess writeAccess(*this);
    ReadAccess readAccess(original);

#ifdef OVITO_USE_SYCL
    const size_t nbytes = original.size() * original.stride();
    // Replicate data values N times.
    for(size_t i = 0; i < n; i++) {
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
            original._hasScheduledSyclReadOperations = true;
            cgh.copy(
                original._data->get_access(cgh, cl::sycl::range(nbytes), cl::sycl::read_only),
                _data->get_access(cgh, cl::sycl::range(nbytes), cl::sycl::id(nbytes * i), cl::sycl::write_only, i == 0 ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{})
            );
        });
    }
#else
    std::byte* dest = _data.get();
    const std::byte* src = original._data.get();
    // Replicate data values N times.
    for(size_t i = 0; i < n; i++, dest += original.size() * stride()) {
        std::memcpy(dest, src, original.size() * stride());
    }
#endif
}

/******************************************************************************
* Reduces the size of the storage array, removing elements for which
* the corresponding bits in the bit array are set.
******************************************************************************/
void DataBuffer::filterResizeCopyFrom(size_t newSize, const boost::dynamic_bitset<>& mask, const DataBuffer& original)
{
    OVITO_ASSERT(original._isDataInitialized || original.size() == 0);
    OVITO_ASSERT(original.size() == mask.size());
    OVITO_ASSERT(original.dataType() == this->dataType() && original.stride() == this->stride());

    if(newSize == 0) {
#ifdef OVITO_DEBUG
        WriteAccess writeAccess(*this);
        _isDataInitialized = false;
#endif
#ifdef OVITO_USE_SYCL
        _data.reset();
#else
        _capacity = 0;
        _data.reset();
#endif
        _numElements = 0;
        return;
    }
    OVITO_ASSERT(original.size() != 0);
#ifdef OVITO_DEBUG
    _isDataInitialized = true;
#endif

#ifdef OVITO_USE_SYCL
    auto newBuffer = allocateSyclBuffer(newSize, _stride);
#else
    std::unique_ptr<std::byte[]> newBuffer(new std::byte[newSize * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
#endif
    const size_t s = mask.size();

    auto specializedFilter = [&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(this->stride() == sizeof(T));
        WriteAccess writeAccess(*this);
#ifdef OVITO_USE_SYCL
        cl::sycl::buffer<T> oldBufferTyped = original._data->reinterpret<T, 1>();
        cl::sycl::host_accessor readAccessor(oldBufferTyped, cl::sycl::range(original.size()), cl::sycl::read_only);
        cl::sycl::buffer<T> newBufferTyped = newBuffer.reinterpret<T, 1>();
        cl::sycl::host_accessor writeAccessor(newBufferTyped, cl::sycl::write_only, cl::sycl::no_init);
        const T* __restrict src = readAccessor.get_pointer();
        T* __restrict dst = writeAccessor.get_pointer();
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i))
                *dst++ = *src;
        }
        OVITO_ASSERT(dst == writeAccessor.get_pointer() + newSize);
        _data = std::move(newBuffer);
#else
        const T* __restrict src = reinterpret_cast<const T*>(original.cdata());
        T* __restrict dst = reinterpret_cast<T*>(newBuffer.get());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i))
                *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<T*>(newBuffer.get()) + newSize);
        _data.swap(newBuffer);
        _capacity = newSize;
#endif
         _numElements = newSize;
    };

    // Optimize filter operation for the most common data types.
    if(dataType() == DataBuffer::Float32) {
        if(componentCount() == 1 && stride() == sizeof(float)) {
            specializedFilter(float{});
            return;
        }
        else if(componentCount() == 3 && stride() == sizeof(Point_3<float>)) {
            specializedFilter(Point_3<float>{});
            return;
        }
    }
    else if(dataType() == DataBuffer::Float64) {
        if(componentCount() == 1 && stride() == sizeof(double)) {
            specializedFilter(double{});
            return;
        }
        else if(componentCount() == 3 && stride() == sizeof(Point_3<double>)) {
            specializedFilter(Point_3<double>{});
            return;
        }
    }
    else if(dataType() == DataBuffer::Int32) {
        if(componentCount() == 1 && stride() == sizeof(int32_t)) {
            specializedFilter(int32_t{});
            return;
        }
        else if(componentCount() == 3 && stride() == sizeof(Vector_3<int32_t>)) {
            specializedFilter(Vector_3<int32_t>{});
            return;
        }
    }
    else if(dataType() == DataBuffer::Int64 && stride() == sizeof(int64_t)) {
        specializedFilter(int64_t{});
        return;
    }
    else if(dataType() == DataBuffer::Int8 && stride() == sizeof(int8_t)) {
        specializedFilter(int8_t{});
        return;
    }

    // Generic case:
    WriteAccess writeAccess(*this);
#ifdef OVITO_USE_SYCL
    cl::sycl::host_accessor readAccessor(*original._data, cl::sycl::range(original.size() * original.stride()), cl::sycl::read_only);
    cl::sycl::host_accessor writeAccessor(newBuffer, cl::sycl::write_only, cl::sycl::no_init);
    const std::byte* __restrict src = readAccessor.get_pointer();
    std::byte* __restrict dst = writeAccessor.get_pointer();
    const auto stride = this->stride();
    for(size_t i = 0; i < s; i++, src += stride) {
        if(!mask.test(i)) {
            std::memcpy(dst, src, stride);
            dst += stride;
        }
    }
    OVITO_ASSERT(dst == writeAccessor.get_pointer() + newSize * stride);
    _data = std::move(newBuffer);
#else
    const std::byte* __restrict src = original.cdata();
    std::byte* __restrict dst = newBuffer.get();
    const auto stride = this->stride();
    for(size_t i = 0; i < s; i++, src += stride) {
        if(!mask.test(i)) {
            std::memcpy(dst, src, stride);
            dst += stride;
        }
    }
    OVITO_ASSERT(dst == newBuffer.get() + newSize * _stride);
    _data.swap(newBuffer);
    _capacity = newSize;
#endif
    _numElements = newSize;
}

/******************************************************************************
* Copies the contents from the given source into this property storage using
* a mapping of indices.
******************************************************************************/
void DataBuffer::mappedCopyFrom(const DataBuffer& source, const std::vector<size_t>& mapping, bool discardOldContents)
{
    OVITO_ASSERT(source.size() == mapping.size());
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(&source != this); // Do not allow aliasing.

    if(this->size() == 0 || source.size() == 0)
        return;

#ifdef OVITO_DEBUG
    OVITO_ASSERT(source._isDataInitialized);
    _isDataInitialized = true;
#endif

    WriteAccess writeAccess(*this);
    ReadAccess readAccess(source);

    auto specializedCopy = [&](auto _) {
        using T = decltype(_);
#ifdef OVITO_USE_SYCL
        cl::sycl::buffer<T> typedSource = source._data->reinterpret<T, 1>();
        cl::sycl::buffer<T> typedDest = _data->reinterpret<T, 1>();
        cl::sycl::host_accessor readAccessor(typedSource, cl::sycl::read_only);
        cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only);
        const T* __restrict src = readAccessor.get_pointer();
        T* __restrict dst = writeAccessor.get_pointer();
#else
        const T* __restrict src = reinterpret_cast<const T*>(source.cdata());
        T* __restrict dst = reinterpret_cast<T*>(data());
#endif
        for(auto idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    };

    // Optimize operation for the most common data types.
    if(dataType() == DataBuffer::Float32) {
        if(componentCount() == 1 && stride() == sizeof(float)) {
            specializedCopy(float{});
            return;
        }
        else if(componentCount() == 3 && stride() == sizeof(Vector_3<float>)) {
            specializedCopy(Vector_3<float>{});
            return;
        }
    }
    else if(dataType() == DataBuffer::Float64) {
        if(componentCount() == 1 && stride() == sizeof(double)) {
            specializedCopy(double{});
            return;
        }
        else if(componentCount() == 3 && stride() == sizeof(Vector_3<double>)) {
            specializedCopy(Vector_3<double>{});
            return;
        }
    }
    else if(dataType() == DataBuffer::Int32 && componentCount() == 1 && stride() == sizeof(int32_t)) {
        specializedCopy(int32_t{});
        return;
    }
    else if(dataType() == DataBuffer::Int64 && componentCount() == 1 && stride() == sizeof(int64_t)) {
        specializedCopy(int64_t{});
        return;
    }
    else if(dataType() == DataBuffer::Int8 && componentCount() == 1 && stride() == sizeof(int8_t)) {
        specializedCopy(int8_t{});
        return;
    }

    // General case:
#ifdef OVITO_USE_SYCL
    cl::sycl::host_accessor readAccessor(*source._data, cl::sycl::read_only);
    cl::sycl::host_accessor writeAccessor(*_data, cl::sycl::write_only, discardOldContents ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{});
    const std::byte* __restrict src = readAccessor.get_pointer();
    std::byte* __restrict dst = writeAccessor.get_pointer();
#else
    const std::byte* __restrict src = source.cdata();
    std::byte* __restrict dst = data();
#endif
    const auto stride = this->stride();
    for(size_t i = 0; i < source.size(); i++, src += stride) {
        OVITO_ASSERT(mapping[i] < this->size());
        std::memcpy(dst + stride * mapping[i], src, stride);
    }
}

/******************************************************************************
* Copies the elements from this storage array into the given destination array
* using an index mapping.
******************************************************************************/
void DataBuffer::mappedCopyTo(DataBuffer& destination, const std::vector<size_t>& mapping) const
{
    OVITO_ASSERT(destination.size() == mapping.size());
    OVITO_ASSERT(this->stride() == destination.stride());
    OVITO_ASSERT(&destination != this); // Do not allow aliasing.

    if(this->size() == 0 || destination.size() == 0)
        return;

#ifdef OVITO_DEBUG
    OVITO_ASSERT(_isDataInitialized);
    destination._isDataInitialized = true;
#endif

    ReadAccess readAccess(*this);
    WriteAccess writeAccess(destination);

#ifndef OVITO_USE_SYCL
    // Optimize copying operation for the most common property types.
    if(stride() == sizeof(FloatType)) {
        // Single float
        const FloatType* __restrict src = reinterpret_cast<const FloatType*>(cdata());
        FloatType* __restrict dst = reinterpret_cast<FloatType*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int32_t)) {
        // Single integer
        const int32_t* __restrict src = reinterpret_cast<const int32_t*>(cdata());
        int32_t* __restrict dst = reinterpret_cast<int32_t*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int64_t)) {
        // Single 64-bit integer
        const int64_t* __restrict src = reinterpret_cast<const int64_t*>(cdata());
        int64_t* __restrict dst = reinterpret_cast<int64_t*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int8_t)) {
        const int8_t* __restrict src = reinterpret_cast<const int8_t*>(cdata());
        int8_t* __restrict dst = reinterpret_cast<int8_t*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        const Point3* __restrict src = reinterpret_cast<const Point3*>(cdata());
        Point3* __restrict dst = reinterpret_cast<Point3*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Color)) {
        // Triple float
        const Color* __restrict src = reinterpret_cast<const Color*>(cdata());
        Color* __restrict dst = reinterpret_cast<Color*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3I)) {
        // Triple int
        const Point3I* __restrict src = reinterpret_cast<const Point3I*>(cdata());
        Point3I* __restrict dst = reinterpret_cast<Point3I*>(destination.data());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else {
        // General case:
        const std::byte* __restrict src = cdata();
        std::byte* __restrict dst = destination.data();
#else
        cl::sycl::host_accessor readAccessor(*_data, cl::sycl::read_only);
        cl::sycl::host_accessor writeAccessor(*destination._data, cl::sycl::write_only, cl::sycl::no_init);
        const std::byte* __restrict src = readAccessor.get_pointer();
        std::byte* __restrict dst = writeAccessor.get_pointer();
#endif
        size_t stride = this->stride();
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            std::memcpy(dst, src + stride * idx, stride);
            dst += stride;
        }
#ifndef OVITO_USE_SYCL
    }
#endif
}

/******************************************************************************
* Reorders the existing elements in this storage array using an index map.
******************************************************************************/
void DataBuffer::reorderElements(const std::vector<size_t>& mapping)
{
    // TODO: Implement this in a more efficient way.
    auto copy = CloneHelper::cloneSingleObject(this, false);
    copy->mappedCopyTo(*this, mapping);
}

/******************************************************************************
* Copies the data elements from the given source array into this array.
* Array size, component count and data type of source and destination must match exactly.
******************************************************************************/
void DataBuffer::copyFrom(const DataBuffer& source)
{
    OVITO_ASSERT(&source != this); // Do not allow aliasing.
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(this->size() == source.size());
    OVITO_ASSERT(source._isDataInitialized);
#ifdef OVITO_DEBUG
    _isDataInitialized = true;
#endif
    if(&source != this && this->size() != 0) {
        WriteAccess writeAccess(*this);
        ReadAccess readAccess(source);
#ifdef OVITO_USE_SYCL
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
            source._hasScheduledSyclReadOperations = true;
            cgh.copy(
                source._data->get_access(cgh, cl::sycl::range(source.size() * source.stride()), cl::sycl::read_only),
                _data->get_access(cgh, cl::sycl::write_only, cl::sycl::no_init)
            );
        });
#else
        std::memcpy(data(), source.cdata(), this->stride() * this->size());
#endif
    }
}

/******************************************************************************
* Copies a range of data elements from the given source array into this array.
* Component count and data type of source and destination must be compatible.
******************************************************************************/
void DataBuffer::copyRangeFrom(const DataBuffer& source, size_t sourceIndex, size_t destIndex, size_t count)
{
    OVITO_ASSERT(&source != this); // Do not allow aliasing.
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(sourceIndex + count <= source.size());
    OVITO_ASSERT(destIndex + count <= this->size());
    if(this->size() == 0 || source.size() == 0 || count == 0)
        return;
#ifdef OVITO_DEBUG
    OVITO_ASSERT(source._isDataInitialized);
    _isDataInitialized = true;
#endif
    WriteAccess writeAccess(*this);
    ReadAccess readAccess(source);
#ifdef OVITO_USE_SYCL
    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
        source._hasScheduledSyclReadOperations = true;
        cgh.copy(
            source._data->get_access(cgh, cl::sycl::range(count * source.stride()), cl::sycl::id(sourceIndex * source.stride()), cl::sycl::read_only),
            _data->get_access(cgh, cl::sycl::range(count * this->stride()), cl::sycl::id(destIndex * this->stride()), cl::sycl::write_only,  (destIndex == 0 && count == this->size()) ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{})
        );
    });
#else
    std::memcpy(
        data() + destIndex * this->stride(),
        source.cdata() + sourceIndex * source.stride(),
        this->stride() * count);
#endif
}

/******************************************************************************
* Checks if this buffer storage and its contents exactly match those of
* another buffer.
******************************************************************************/
bool DataBuffer::equals(const DataBuffer& other) const
{
    OVITO_ASSERT(_isDataInitialized);
    OVITO_ASSERT(other._isDataInitialized);

    if(&other == this)
        return true;

    ReadAccess readAccess1(*this);
    ReadAccess readAccess2(other);

    if(this->dataType() != other.dataType()) return false;
    if(this->size() != other.size()) return false;
    if(this->componentCount() != other.componentCount()) return false;
    OVITO_ASSERT(this->stride() == other.stride());
    if(this->size() == 0) return true;
#ifdef OVITO_USE_SYCL
    cl::sycl::host_accessor readAccessor1(*other._data, cl::sycl::read_only);
    cl::sycl::host_accessor readAccessor2(*this->_data, cl::sycl::read_only);
    return std::equal(readAccessor1.get_pointer(), readAccessor1.get_pointer() + this->size() * this->stride(), readAccessor2.get_pointer());
#else
    return std::equal(this->cdata(), this->cdata() + this->size() * this->stride(), other.cdata());
#endif
}

/******************************************************************************
* Changes the data type of the buffer in place and converts the stored values.
******************************************************************************/
void DataBuffer::convertToDataType(int newDataType)
{
    OVITO_ASSERT(newDataType == Int8 || newDataType == Int32 || newDataType == Int64 || newDataType == Float32 || newDataType == Float64);

    if(dataType() == newDataType)
        return;

    size_t newDataTypeSize = QMetaType(newDataType).sizeOf();
    size_t newStride = _componentCount * newDataTypeSize;
#ifdef OVITO_USE_SYCL
    std::optional<cl::sycl::buffer<std::byte>> newData;
    if(_numElements != 0)
        newData = allocateSyclBuffer(_numElements, newStride);
#else
    std::unique_ptr<std::byte[]> newData(new std::byte[_numElements * newStride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
#endif

    // Copy values from old buffer to new buffer and perform data type convertion.
    if(_numElements != 0) {
        OVITO_ASSERT(_isDataInitialized);
        RawBufferReadAccess oldData(this);
        switch(newDataType) {
        case Int8:
            {
#ifdef OVITO_USE_SYCL
                auto typedDest = newData->reinterpret<int8_t, 1>();
                cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only, cl::sycl::no_init);
                int8_t* __restrict dest = writeAccessor.get_pointer();
#else
                int8_t* __restrict dest = reinterpret_cast<int8_t*>(newData.get());
#endif
                for(size_t i = 0; i < _numElements; i++)
                    for(size_t j = 0; j < _componentCount; j++)
                        *dest++ = oldData.get<int8_t>(i, j);
            }
            break;
        case Int32:
            {
#ifdef OVITO_USE_SYCL
                auto typedDest = newData->reinterpret<int32_t, 1>();
                cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only, cl::sycl::no_init);
                int32_t* __restrict dest = writeAccessor.get_pointer();
#else
                int32_t* __restrict dest = reinterpret_cast<int32_t*>(newData.get());
#endif
                for(size_t i = 0; i < _numElements; i++)
                    for(size_t j = 0; j < _componentCount; j++)
                        *dest++ = oldData.get<int32_t>(i, j);
            }
            break;
        case Int64:
            {
#ifdef OVITO_USE_SYCL
                auto typedDest = newData->reinterpret<int64_t, 1>();
                cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only, cl::sycl::no_init);
                int64_t* __restrict dest = writeAccessor.get_pointer();
#else
                int64_t* __restrict dest = reinterpret_cast<int64_t*>(newData.get());
#endif
                for(size_t i = 0; i < _numElements; i++)
                    for(size_t j = 0; j < _componentCount; j++)
                        *dest++ = oldData.get<int64_t>(i, j);
            }
            break;
        case Float32:
            {
#ifdef OVITO_USE_SYCL
                auto typedDest = newData->reinterpret<float, 1>();
                cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only, cl::sycl::no_init);
                float* __restrict dest = writeAccessor.get_pointer();
#else
                float* __restrict dest = reinterpret_cast<float*>(newData.get());
#endif
                for(size_t i = 0; i < _numElements; i++)
                    for(size_t j = 0; j < _componentCount; j++)
                        *dest++ = oldData.get<float>(i, j);
            }
            break;
        case Float64:
            {
#ifdef OVITO_USE_SYCL
                auto typedDest = newData->reinterpret<double, 1>();
                cl::sycl::host_accessor writeAccessor(typedDest, cl::sycl::write_only, cl::sycl::no_init);
                double* __restrict dest = writeAccessor.get_pointer();
#else
                double* __restrict dest = reinterpret_cast<double*>(newData.get());
#endif
                for(size_t i = 0; i < _numElements; i++)
                    for(size_t j = 0; j < _componentCount; j++)
                        *dest++ = oldData.get<double>(i, j);
            }
            break;
        default:
            OVITO_ASSERT(false); // Unsupported data type
        }
    }

    WriteAccess writeAccess(*this);
    _dataType = newDataType;
    _dataTypeSize = newDataTypeSize;
    _stride = newStride;
    _data = std::move(newData);
#ifndef OVITO_USE_SYCL
    _capacity = _numElements;
#endif
}

/******************************************************************************
* Set all stored values to zeros.
******************************************************************************/
void DataBuffer::fillZero()
{
    if(size() == 0)
        return;
#ifdef OVITO_DEBUG
    _isDataInitialized = true;
#endif
#ifdef OVITO_USE_SYCL
    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
        cgh.fill(_data->get_access(cgh,
            cl::sycl::range(_numElements * _stride),
            cl::sycl::write_only,
            cl::sycl::no_init), (std::byte)0);
    });
#else
    RawBufferAccess<access_mode::discard_write> writeAccess(this);
    std::memset(writeAccess.data(), 0, this->size() * this->stride());
#endif
}

#ifdef OVITO_USE_SYCL
/******************************************************************************
* Blocks until all SYCL kernels in the queue that read from this buffer have finished running.
* Only then it is safe again to write into the buffer on the host. This function is used by the
* Python binding layer, which requires permanent write access to the buffer's underlying memory on the host.
******************************************************************************/
void DataBuffer::blockUntilSyclKernelsFinished()
{
    if(_hasScheduledSyclReadOperations) {
        RawBufferAccess<access_mode::write> accessor{this};
        _hasScheduledSyclReadOperations = false;
    }
}
#endif

}   // End of namespace
