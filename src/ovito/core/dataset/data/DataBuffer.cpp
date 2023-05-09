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
#include "DataBufferAccess.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataBuffer);

/******************************************************************************
* Constructor allocating a buffer array with given size and data layout.
******************************************************************************/
DataBuffer::DataBuffer(ObjectCreationParams params, size_t elementCount, int dataType, size_t componentCount, InitializationFlags flags, QStringList componentNames) :
    DataObject(params),
    _dataType(dataType),
    _dataTypeSize(getQtTypeSizeFromId(dataType)),
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
    resize(elementCount, flags.testFlag(InitializeMemory));
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
    clone->_capacity = _numElements;
    clone->_stride = _stride;
    clone->_componentCount = _componentCount;
    clone->_componentNames = _componentNames;
    clone->_data.reset(new std::byte[_numElements * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
    std::memcpy(clone->_data.get(), _data.get(), _numElements * _stride);

    return clone;
}

/******************************************************************************
* Resizes the storage.
******************************************************************************/
void DataBuffer::resize(size_t newSize, bool preserveData)
{
    // Note: Do not reallocate the buffer when its size is reduced.
    // The filterResize() method relies on
    // the data buffer's memory pointer to remain the same when the buffer is shrinked.
    WriteAccess writeAccess(*this);
    if(newSize > _capacity || !_data) {
        std::unique_ptr<std::byte[]> newBuffer(new std::byte[newSize * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
        if(preserveData)
            std::memcpy(newBuffer.get(), _data.get(), _stride * std::min(_numElements, newSize));
        _data.swap(newBuffer);
        _capacity = newSize;
    }
    // Initialize new elements to zero.
    if(newSize > _numElements && preserveData) {
        std::memset(_data.get() + _numElements * _stride, 0, (newSize - _numElements) * _stride);
    }
    _numElements = newSize;
}

/******************************************************************************
* Grows the number of data elements while preserving the exiting data.
* True if the memory buffer was reallocated, because the current capacity was insufficient
* to accommodate the new elements.
******************************************************************************/
bool DataBuffer::grow(size_t numAdditionalElements, bool callerAlreadyHasWriteAccess)
{
    std::optional<WriteAccess> writeAccess;
    if(!callerAlreadyHasWriteAccess)
        writeAccess.emplace(*this);
    size_t newSize = _numElements + numAdditionalElements;
    OVITO_ASSERT(newSize >= _numElements);
    bool needToGrow;
    if((needToGrow = (newSize > _capacity))) {
        // Grow the storage capacity of the data buffer.
        size_t newCapacity = (newSize < 1024)
            ? std::max(newSize * 2, (size_t)256)
            : (newSize * 3 / 2);
        std::unique_ptr<std::byte[]> newBuffer(new std::byte[newCapacity * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
        std::memcpy(newBuffer.get(), _data.get(), _stride * _numElements);
        _data.swap(newBuffer);
        _capacity = newCapacity;
    }
    _numElements = newSize;
    return needToGrow;
}

/******************************************************************************
* Reduces the number of data elements while preserving the exiting data.
* Note: This method never reallocates the memory buffer. Thus, the capacity of the array remains unchanged and the
* memory of the truncated elements is not released by the method.
******************************************************************************/
void DataBuffer::truncate(size_t numElementsToRemove)
{
    OVITO_ASSERT(numElementsToRemove <= _numElements);

    WriteAccess writeAccess(*this);
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
    stream << QByteArray(getQtTypeNameFromId(_dataType));
    stream.writeSizeT(_dataTypeSize);
    stream.writeSizeT(_stride);
    stream.writeSizeT(_componentCount);
    stream << _componentNames;
    if(excludeRecomputableData) {
        stream.writeSizeT(0);
    }
    else {
        stream.writeSizeT(_numElements);
        stream.write(_data.get(), _stride * _numElements);
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
    _dataType = getQtTypeIdFromName(dataTypeName);
    OVITO_ASSERT_MSG(_dataType != 0, "DataBuffer::loadFromStream()", qPrintable(QString("The metadata type '%1' seems to be no longer defined.").arg(QString::fromLatin1(dataTypeName))));
    OVITO_ASSERT(dataTypeName == getQtTypeNameFromId(_dataType));
    stream.readSizeT(_dataTypeSize);
    stream.readSizeT(_stride);
    stream.readSizeT(_componentCount);
    stream >> _componentNames;
    stream.readSizeT(_numElements);
    _capacity = _numElements;
    _data.reset(new std::byte[_numElements * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
    stream.read(_data.get(), _stride * _numElements);
    stream.closeChunk();
}

/******************************************************************************
* Extends the data array and replicates the old data N times.
******************************************************************************/
void DataBuffer::replicate(size_t n, bool replicateValues)
{
    OVITO_ASSERT(n >= 1);
    if(n <= 1) return;

    WriteAccess writeAccess(*this);
    size_t oldSize = _numElements;
    std::unique_ptr<std::byte[]> oldData = std::move(_data);

    _numElements *= n;
    _capacity = _numElements;
    _data.reset(new std::byte[_capacity * _stride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.
    if(replicateValues) {
        // Replicate data values N times.
        std::byte* dest = _data.get();
        for(size_t i = 0; i < n; i++, dest += oldSize * stride()) {
            std::memcpy(dest, oldData.get(), oldSize * stride());
        }
    }
    else {
        // Copy just one replica of the data from the old memory buffer to the new one.
        std::memcpy(_data.get(), oldData.get(), oldSize * stride());
    }
}

/******************************************************************************
* Reduces the size of the storage array, removing elements for which
* the corresponding bits in the bit array are set.
******************************************************************************/
void DataBuffer::filterResize(const boost::dynamic_bitset<>& mask)
{
    OVITO_ASSERT(size() == mask.size());
    auto s = size();

    auto specializedFilter = [&](auto _) {
        using T = decltype(_);
        size_t newSize;
        {
            WriteAccess writeAccess(*this);
            auto src = reinterpret_cast<const T*>(cbuffer());
            auto dst = reinterpret_cast<T*>(buffer());
            for(size_t i = 0; i < s; ++i, ++src) {
                if(!mask.test(i))
                    *dst++ = *src;
            }
            newSize = dst - reinterpret_cast<T*>(buffer());
        }
        resize(newSize, true); // Note: This is a cheap operation - does no mem copy.
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
    size_t newSize;
    {
        WriteAccess writeAccess(*this);
        const std::byte* src = _data.get();
        std::byte* dst = _data.get();
        const auto stride = this->stride();
        for(size_t i = 0; i < s; i++, src += stride) {
            if(!mask.test(i)) {
                if(dst != src)
                    std::memcpy(dst, src, stride);
                dst += stride;
            }
        }
        newSize = (dst - _data.get()) / stride;
    }
    resize(newSize, true); // Note: This is a cheap operation - does not mem copy.
}

/******************************************************************************
* Creates a copy of the array, not containing those elements for which
* the corresponding bits in the given bit array were set.
******************************************************************************/
OORef<DataBuffer> DataBuffer::filterCopy(const boost::dynamic_bitset<>& mask) const
{
    auto copy = CloneHelper().cloneObject(this, false);

    ReadAccess readAccess(*this);
    OVITO_ASSERT(size() == mask.size());

    size_t s = size();
    size_t newSize = size() - mask.count();
    copy->resize(newSize, false);

    auto specializedFilter = [&](auto _) {
        using T = decltype(_);
        const T* __restrict src = reinterpret_cast<const T*>(cbuffer());
        T* __restrict dst = reinterpret_cast<T*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i))
                *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<T*>(copy->buffer()) + newSize);
    };

    // Optimize filter operation for the most common data types.
    if(dataType() == DataBuffer::Float32) {
        if(componentCount() == 1 && stride() == sizeof(float)) {
            specializedFilter(float{});
            return copy;
        }
        else if(componentCount() == 3 && stride() == sizeof(Point_3<float>)) {
            specializedFilter(Point_3<float>{});
            return copy;
        }
    }
    else if(dataType() == DataBuffer::Float64) {
        if(componentCount() == 1 && stride() == sizeof(double)) {
            specializedFilter(double{});
            return copy;
        }
        else if(componentCount() == 3 && stride() == sizeof(Point_3<double>)) {
            specializedFilter(Point_3<double>{});
            return copy;
        }
    }
    else if(dataType() == DataBuffer::Int32) {
        if(componentCount() == 1 && stride() == sizeof(int32_t)) {
            specializedFilter(int32_t{});
            return copy;
        }
        else if(componentCount() == 3 && stride() == sizeof(Vector_3<int32_t>)) {
            specializedFilter(Vector_3<int32_t>{});
            return copy;
        }
    }
    else if(dataType() == DataBuffer::Int64 && componentCount() == 1 && stride() == sizeof(int64_t)) {
        specializedFilter(int64_t{});
        return copy;
    }
    else if(dataType() == DataBuffer::Int8 && componentCount() == 1 && stride() == sizeof(int8_t)) {
        specializedFilter(int8_t{});
        return copy;
    }

    // Generic case:
    const std::byte* __restrict src = _data.get();
    std::byte* __restrict dst = copy->_data.get();
    const auto stride = this->stride();
    for(size_t i = 0; i < s; i++, src += stride) {
        if(!mask.test(i)) {
            std::memcpy(dst, src, stride);
            dst += stride;
        }
    }
    OVITO_ASSERT(dst == copy->_data.get() + newSize * stride);
    return copy;
}

/******************************************************************************
* Copies the contents from the given source into this property storage using
* a mapping of indices.
******************************************************************************/
void DataBuffer::mappedCopyFrom(const DataBuffer& source, const std::vector<size_t>& mapping)
{
    OVITO_ASSERT(source.size() == mapping.size());
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(&source != this); // Do not allow aliasing.

    WriteAccess writeAccess(*this);
    ReadAccess readAccess(source);

    auto specializedCopy = [&](auto _) {
        using T = decltype(_);
        const T* __restrict src = reinterpret_cast<const T*>(source.cbuffer());
        T* __restrict dst = reinterpret_cast<T*>(buffer());
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
    const std::byte* __restrict src = source.cbuffer();
    std::byte* __restrict dst = buffer();
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

    ReadAccess readAccess(*this);
    WriteAccess writeAccess(destination);

    // Optimize copying operation for the most common property types.
    if(stride() == sizeof(FloatType)) {
        // Single float
        const FloatType* __restrict src = reinterpret_cast<const FloatType*>(cbuffer());
        FloatType* __restrict dst = reinterpret_cast<FloatType*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int32_t)) {
        // Single integer
        const int32_t* __restrict src = reinterpret_cast<const int32_t*>(cbuffer());
        int32_t* __restrict dst = reinterpret_cast<int32_t*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int64_t)) {
        // Single 64-bit integer
        const int64_t* __restrict src = reinterpret_cast<const int64_t*>(cbuffer());
        int64_t* __restrict dst = reinterpret_cast<int64_t*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int8_t)) {
        const int8_t* __restrict src = reinterpret_cast<const int8_t*>(cbuffer());
        int8_t* __restrict dst = reinterpret_cast<int8_t*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        const Point3* __restrict src = reinterpret_cast<const Point3*>(cbuffer());
        Point3* __restrict dst = reinterpret_cast<Point3*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Color)) {
        // Triple float
        const Color* __restrict src = reinterpret_cast<const Color*>(cbuffer());
        Color* __restrict dst = reinterpret_cast<Color*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3I)) {
        // Triple int
        const Point3I* __restrict src = reinterpret_cast<const Point3I*>(cbuffer());
        Point3I* __restrict dst = reinterpret_cast<Point3I*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else {
        // General case:
        const std::byte* __restrict src = cbuffer();
        std::byte* __restrict dst = destination.buffer();
        size_t stride = this->stride();
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            std::memcpy(dst, src + stride * idx, stride);
            dst += stride;
        }
    }
}

/******************************************************************************
* Reorders the existing elements in this storage array using an index map.
******************************************************************************/
void DataBuffer::reorderElements(const std::vector<size_t>& mapping)
{
    auto copy = CloneHelper().cloneObject(this, false);
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
    if(&source != this) {
        WriteAccess writeAccess(*this);
        ReadAccess readAccess(source);
        std::memcpy(buffer(), source.cbuffer(), this->stride() * this->size());
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
    WriteAccess writeAccess(*this);
    ReadAccess readAccess(source);
    std::memcpy(
        buffer() + destIndex * this->stride(),
        source.cbuffer() + sourceIndex * source.stride(),
        this->stride() * count);
}

/******************************************************************************
* Checks if this buffer storage and its contents exactly match those of
* another buffer.
******************************************************************************/
bool DataBuffer::equals(const DataBuffer& other) const
{
    if(&other == this)
        return true;

    ReadAccess readAccess1(*this);
    ReadAccess readAccess2(other);

    if(this->dataType() != other.dataType()) return false;
    if(this->size() != other.size()) return false;
    if(this->componentCount() != other.componentCount()) return false;
    OVITO_ASSERT(this->stride() == other.stride());
    return std::equal(this->cbuffer(), this->cbuffer() + this->size() * this->stride(), other.cbuffer());
}

/******************************************************************************
* Changes the data type of the buffer in place and converts the stored values.
******************************************************************************/
void DataBuffer::convertDataType(int newDataType)
{
    OVITO_ASSERT(newDataType == Int8 || newDataType == Int32 || newDataType == Int64 || newDataType == Float32 || newDataType == Float64);

    if(dataType() == newDataType)
        return;

    size_t newDataTypeSize = getQtTypeSizeFromId(newDataType);
    size_t newStride = _componentCount * newDataTypeSize;
    std::unique_ptr<std::byte[]> newData(new std::byte[_numElements * newStride]); // TODO: Replace with std::make_unique_for_overwrite() in C++20.

    // Copy values from old buffer to new buffer and perform data type convertion.
    ConstDataBufferAccess<void, true> oldData(this);
    switch(newDataType) {
    case Int8:
        {
            int8_t* __restrict dest = reinterpret_cast<int8_t*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<int8_t>(i, j);
        }
        break;
    case Int32:
        {
            int32_t* __restrict dest = reinterpret_cast<int32_t*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<int32_t>(i, j);
        }
        break;
    case Int64:
        {
            int64_t* __restrict dest = reinterpret_cast<int64_t*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<int64_t>(i, j);
        }
        break;
    case Float32:
        {
            float* __restrict dest = reinterpret_cast<float*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<float>(i, j);
        }
        break;
    case Float64:
        {
            double* __restrict dest = reinterpret_cast<double*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<double>(i, j);
        }
        break;
    default:
        OVITO_ASSERT(false); // Unsupported data type
    }
    oldData.reset();

    WriteAccess writeAccess(*this);
    _dataType = newDataType;
    _dataTypeSize = newDataTypeSize;
    _stride = newStride;
    _capacity = _numElements;
    _data = std::move(newData);
}

}   // End of namespace
