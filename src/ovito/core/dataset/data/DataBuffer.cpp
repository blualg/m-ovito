////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
    OVITO_ASSERT(dataType == Int || dataType == Int64 || dataType == Float);
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
    prepareReadAccess();
    clone->_dataType = _dataType;
    clone->_dataTypeSize = _dataTypeSize;
    clone->_numElements = _numElements;
    clone->_capacity = _numElements;
    clone->_stride = _stride;
    clone->_componentCount = _componentCount;
    clone->_componentNames = _componentNames;
    clone->_data.reset(new uint8_t[_numElements * _stride]);
    std::memcpy(clone->_data.get(), _data.get(), _numElements * _stride);
    finishReadAccess();

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
    prepareWriteAccess();
    if(newSize > _capacity || !_data) {
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[newSize * _stride]);
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
    finishWriteAccess();
}

/******************************************************************************
* Grows the number of data elements while preserving the exiting data.
* True if the memory buffer was reallocated, because the current capacity was insufficient
* to accommodate the new elements.
******************************************************************************/
bool DataBuffer::grow(size_t numAdditionalElements, bool callerAlreadyHasWriteAccess) 
{
    if(!callerAlreadyHasWriteAccess)
        prepareWriteAccess();
    size_t newSize = _numElements + numAdditionalElements;
    OVITO_ASSERT(newSize >= _numElements);
    bool needToGrow;
    if((needToGrow = (newSize > _capacity))) {
        // Grow the storage capacity of the data buffer.
        size_t newCapacity = (newSize < 1024)
            ? std::max(newSize * 2, (size_t)256)
            : (newSize * 3 / 2);
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[newCapacity * _stride]);
        std::memcpy(newBuffer.get(), _data.get(), _stride * _numElements);
        _data.swap(newBuffer);
        _capacity = newCapacity;
    }
    _numElements = newSize;
    if(!callerAlreadyHasWriteAccess)
        finishWriteAccess();
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
    prepareWriteAccess();
    _numElements -= numElementsToRemove;
    finishWriteAccess();
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void DataBuffer::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    DataObject::saveToStream(stream, excludeRecomputableData);

    prepareReadAccess();
    try {
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
    _data.reset(new uint8_t[_numElements * _stride]);
    stream.read(_data.get(), _stride * _numElements);
    stream.closeChunk();

    // Do floating-point precision conversion from single to double precision.
    if(_dataType == qMetaTypeId<float>() && DataBuffer::Float == qMetaTypeId<double>()) {
        OVITO_ASSERT(sizeof(FloatType) == sizeof(double));
        OVITO_ASSERT(_dataTypeSize == sizeof(float));
        _stride *= sizeof(double) / sizeof(float);
        _dataTypeSize = sizeof(double);
        _dataType = DataBuffer::Float;
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[_stride * _numElements]);
        double* dst = reinterpret_cast<double*>(newBuffer.get());
        const float* src = reinterpret_cast<const float*>(_data.get());
        for(size_t c = _numElements * _componentCount; c--; )
            *dst++ = (double)*src++;
        _data.swap(newBuffer);
    }

    // Do floating-point precision conversion from double to single precision.
    if(_dataType == qMetaTypeId<double>() && DataBuffer::Float == qMetaTypeId<float>()) {
        OVITO_ASSERT(sizeof(FloatType) == sizeof(float));
        OVITO_ASSERT(_dataTypeSize == sizeof(double));
        _stride /= sizeof(double) / sizeof(float);
        _dataTypeSize = sizeof(float);
        _dataType = DataBuffer::Float;
        std::unique_ptr<uint8_t[]> newBuffer(new uint8_t[_stride * _numElements]);
        float* dst = reinterpret_cast<float*>(newBuffer.get());
        const double* src = reinterpret_cast<const double*>(_data.get());
        for(size_t c = _numElements * _componentCount; c--; )
            *dst++ = (float)*src++;
        _data.swap(newBuffer);
    }
}

/******************************************************************************
* Extends the data array and replicates the old data N times.
******************************************************************************/
void DataBuffer::replicate(size_t n, bool replicateValues)
{
    OVITO_ASSERT(n >= 1);
    if(n <= 1) return;

    prepareWriteAccess();
    size_t oldSize = _numElements;
    std::unique_ptr<uint8_t[]> oldData = std::move(_data);

    _numElements *= n;
    _capacity = _numElements;
    _data = std::make_unique<uint8_t[]>(_capacity * _stride);
    if(replicateValues) {
        // Replicate data values N times.
        uint8_t* dest = _data.get();
        for(size_t i = 0; i < n; i++, dest += oldSize * stride()) {
            std::memcpy(dest, oldData.get(), oldSize * stride());
        }
    }
    else {
        // Copy just one replica of the data from the old memory buffer to the new one.
        std::memcpy(_data.get(), oldData.get(), oldSize * stride());
    }
    finishWriteAccess();
}

/******************************************************************************
* Reduces the size of the storage array, removing elements for which
* the corresponding bits in the bit array are set.
******************************************************************************/
void DataBuffer::filterResize(const boost::dynamic_bitset<>& mask)
{
    OVITO_ASSERT(size() == mask.size());
    size_t s = size();

    // Optimize filter operation for the most common property types.
    if(dataType() == DataBuffer::Float && stride() == sizeof(FloatType)) {
        prepareWriteAccess();
        // Single float
        auto src = reinterpret_cast<const FloatType*>(cbuffer());
        auto dst = reinterpret_cast<FloatType*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<FloatType*>(buffer()), true);
    }
    else if(dataType() == DataBuffer::Int && stride() == sizeof(int)) {
        // Single integer
        prepareWriteAccess();
        auto src = reinterpret_cast<const int*>(cbuffer());
        auto dst = reinterpret_cast<int*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<int*>(buffer()), true);
    }
    else if(dataType() == DataBuffer::Int64 && stride() == sizeof(qlonglong)) {
        // Single 64-bit integer
        prepareWriteAccess();
        auto src = reinterpret_cast<const qlonglong*>(cbuffer());
        auto dst = reinterpret_cast<qlonglong*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<qlonglong*>(buffer()), true);
    }
    else if(dataType() == DataBuffer::Float && stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        prepareWriteAccess();
        auto src = reinterpret_cast<const Point3*>(cbuffer());
        auto dst = reinterpret_cast<Point3*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<Point3*>(buffer()), true);
    }
    else if(dataType() == DataBuffer::Float && stride() == sizeof(Color)) {
        // Triple float
        prepareWriteAccess();
        auto src = reinterpret_cast<const Color*>(cbuffer());
        auto dst = reinterpret_cast<Color*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<Color*>(buffer()), true);
    }
    else if(dataType() == DataBuffer::Int && stride() == sizeof(Point3I)) {
        // Triple int.
        prepareWriteAccess();
        auto src = reinterpret_cast<const Point3I*>(cbuffer());
        auto dst = reinterpret_cast<Point3I*>(buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        finishWriteAccess();
        resize(dst - reinterpret_cast<Point3I*>(buffer()), true);
    }
    else {
        // Generic case:
        prepareWriteAccess();
        const uint8_t* src = _data.get();
        uint8_t* dst = _data.get();
        size_t stride = this->stride();
        for(size_t i = 0; i < s; i++, src += stride) {
            if(!mask.test(i)) {
                std::memcpy(dst, src, stride);
                dst += stride;
            }
        }
        finishWriteAccess();
        resize((dst - _data.get()) / stride, true);
    }
}

/******************************************************************************
* Creates a copy of the array, not containing those elements for which
* the corresponding bits in the given bit array were set.
******************************************************************************/
OORef<DataBuffer> DataBuffer::filterCopy(const boost::dynamic_bitset<>& mask) const
{
    auto copy = CloneHelper().cloneObject(this, false);

    prepareReadAccess();
    OVITO_ASSERT(size() == mask.size());

    size_t s = size();
    size_t newSize = size() - mask.count();
    copy->resize(newSize, false);

    // Optimize filter operation for the most common property types.
    if(dataType() == DataBuffer::Float && stride() == sizeof(FloatType)) {
        // Single float
        auto src = reinterpret_cast<const FloatType*>(cbuffer());
        auto dst = reinterpret_cast<FloatType*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<FloatType*>(copy->buffer()) + newSize);
    }
    else if(dataType() == DataBuffer::Int && stride() == sizeof(int)) {
        // Single integer
        auto src = reinterpret_cast<const int*>(cbuffer());
        auto dst = reinterpret_cast<int*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<int*>(copy->buffer()) + newSize);
    }
    else if(dataType() == DataBuffer::Int64 && stride() == sizeof(qlonglong)) {
        // Single 64-bit integer
        auto src = reinterpret_cast<const qlonglong*>(cbuffer());
        auto dst = reinterpret_cast<qlonglong*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<qlonglong*>(copy->buffer()) + newSize);
    }
    else if(dataType() == DataBuffer::Float && stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        auto src = reinterpret_cast<const Point3*>(cbuffer());
        auto dst = reinterpret_cast<Point3*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<Point3*>(copy->buffer()) + newSize);
    }
    else if(dataType() == DataBuffer::Float && stride() == sizeof(Color)) {
        // Triple float
        auto src = reinterpret_cast<const Color*>(cbuffer());
        auto dst = reinterpret_cast<Color*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<Color*>(copy->buffer()) + newSize);
    }
    else if(dataType() == DataBuffer::Int && stride() == sizeof(Point3I)) {
        // Triple int.
        auto src = reinterpret_cast<const Point3I*>(cbuffer());
        auto dst = reinterpret_cast<Point3I*>(copy->buffer());
        for(size_t i = 0; i < s; ++i, ++src) {
            if(!mask.test(i)) *dst++ = *src;
        }
        OVITO_ASSERT(dst == reinterpret_cast<Point3I*>(copy->buffer()) + newSize);
    }
    else {
        // Generic case:
        const uint8_t* src = _data.get();
        uint8_t* dst = copy->_data.get();
        size_t stride = this->stride();
        for(size_t i = 0; i < s; i++, src += stride) {
            if(!mask.test(i)) {
                std::memcpy(dst, src, stride);
                dst += stride;
            }
        }
        OVITO_ASSERT(dst == copy->_data.get() + newSize * stride);
    }
    finishReadAccess();
    return copy;
}

/******************************************************************************
* Copies the contents from the given source into this property storage using
* a mapping of indices.
******************************************************************************/
void DataBuffer::mappedCopyFrom(const DataBuffer& source, const std::vector<size_t>& mapping)
{
    OVITO_ASSERT(source.size() == mapping.size());
    OVITO_ASSERT(stride() == source.stride());
    OVITO_ASSERT(&source != this);
    prepareWriteAccess();
    source.prepareReadAccess();

    // Optimize copying operation for the most common property types.
    if(stride() == sizeof(FloatType)) {
        // Single float
        auto src = reinterpret_cast<const FloatType*>(source.cbuffer());
        auto dst = reinterpret_cast<FloatType*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else if(stride() == sizeof(int)) {
        // Single integer
        auto src = reinterpret_cast<const int*>(source.cbuffer());
        auto dst = reinterpret_cast<int*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else if(stride() == sizeof(qlonglong)) {
        // Single 64-bit integer
        auto src = reinterpret_cast<const qlonglong*>(source.cbuffer());
        auto dst = reinterpret_cast<qlonglong*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else if(stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        auto src = reinterpret_cast<const Point3*>(source.cbuffer());
        auto dst = reinterpret_cast<Point3*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else if(stride() == sizeof(Color)) {
        // Triple float
        auto src = reinterpret_cast<const Color*>(source.cbuffer());
        auto dst = reinterpret_cast<Color*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else if(stride() == sizeof(Point3I)) {
        // Triple int
        auto src = reinterpret_cast<const Point3I*>(source.cbuffer());
        auto dst = reinterpret_cast<Point3I*>(buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < this->size());
            dst[idx] = *src++;
        }
    }
    else {
        // General case:
        const uint8_t* src = source.cbuffer();
        uint8_t* dst = buffer();
        size_t stride = this->stride();
        for(size_t i = 0; i < source.size(); i++, src += stride) {
            OVITO_ASSERT(mapping[i] < this->size());
            std::memcpy(dst + stride * mapping[i], src, stride);
        }
    }

    source.finishReadAccess();
    finishWriteAccess();
}

/******************************************************************************
* Copies the elements from this storage array into the given destination array 
* using an index mapping.
******************************************************************************/
void DataBuffer::mappedCopyTo(DataBuffer& destination, const std::vector<size_t>& mapping) const
{
    OVITO_ASSERT(destination.size() == mapping.size());
    OVITO_ASSERT(this->stride() == destination.stride());
    OVITO_ASSERT(&destination != this);
    prepareReadAccess();
    destination.prepareWriteAccess();

    // Optimize copying operation for the most common property types.
    if(stride() == sizeof(FloatType)) {
        // Single float
        auto src = reinterpret_cast<const FloatType*>(cbuffer());
        auto dst = reinterpret_cast<FloatType*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(int)) {
        // Single integer
        auto src = reinterpret_cast<const int*>(cbuffer());
        auto dst = reinterpret_cast<int*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(qlonglong)) {
        // Single 64-bit integer
        auto src = reinterpret_cast<const qlonglong*>(cbuffer());
        auto dst = reinterpret_cast<qlonglong*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3)) {
        // Triple float (may actually be four floats when SSE instructions are enabled).
        auto src = reinterpret_cast<const Point3*>(cbuffer());
        auto dst = reinterpret_cast<Point3*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Color)) {
        // Triple float
        auto src = reinterpret_cast<const Color*>(cbuffer());
        auto dst = reinterpret_cast<Color*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else if(stride() == sizeof(Point3I)) {
        // Triple int
        auto src = reinterpret_cast<const Point3I*>(cbuffer());
        auto dst = reinterpret_cast<Point3I*>(destination.buffer());
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            *dst++ = src[idx];
        }
    }
    else {
        // General case:
        const uint8_t* src = cbuffer();
        uint8_t* dst = destination.buffer();
        size_t stride = this->stride();
        for(size_t idx : mapping) {
            OVITO_ASSERT(idx < size());
            std::memcpy(dst, src + stride * idx, stride);
            dst += stride;
        }
    }
    destination.finishWriteAccess();
    finishReadAccess();
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
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(this->size() == source.size());
    if(&source != this) {
        prepareWriteAccess();
        source.prepareReadAccess();
        std::memcpy(buffer(), source.cbuffer(), this->stride() * this->size());
        source.finishReadAccess();
        finishWriteAccess();
    }
}

/******************************************************************************
* Copies a range of data elements from the given source array into this array. 
* Component count and data type of source and destination must be compatible.
******************************************************************************/
void DataBuffer::copyRangeFrom(const DataBuffer& source, size_t sourceIndex, size_t destIndex, size_t count)
{
    OVITO_ASSERT(this->dataType() == source.dataType());
    OVITO_ASSERT(this->stride() == source.stride());
    OVITO_ASSERT(sourceIndex + count <= source.size());
    OVITO_ASSERT(destIndex + count <= this->size());
    prepareWriteAccess();
    source.prepareReadAccess();
    std::memcpy(buffer() + destIndex * this->stride(), source.cbuffer() + sourceIndex * source.stride(), this->stride() * count);
    source.finishReadAccess();
    finishWriteAccess();
}

/******************************************************************************
* Checks if this buffer storage and its contents exactly match those of 
* another buffer.
******************************************************************************/
bool DataBuffer::equals(const DataBuffer& other) const
{
    prepareReadAccess();
    other.prepareReadAccess();

    bool result = [&]() {
        if(this->dataType() != other.dataType()) return false;
        if(this->size() != other.size()) return false;
        if(this->componentCount() != other.componentCount()) return false;
        OVITO_ASSERT(this->stride() == other.stride());
        return std::equal(this->cbuffer(), this->cbuffer() + this->size() * this->stride(), other.cbuffer());
    }();

    other.finishReadAccess();
    finishReadAccess();

    return result;
}

/******************************************************************************
* Changes the data type of the buffer in place and converts the stored values.
******************************************************************************/
void DataBuffer::convertDataType(int newDataType)
{
    OVITO_ASSERT(newDataType == Int || newDataType == Int64 || newDataType == Float);

    if(dataType() == newDataType)
        return;

    size_t newDataTypeSize = getQtTypeSizeFromId(newDataType);
    size_t newStride = _componentCount * newDataTypeSize;
    std::unique_ptr<uint8_t[]> newData = std::make_unique<uint8_t[]>(_numElements * newStride);

    // Copy values from old buffer to new buffer and perform data type convertion.
    ConstDataBufferAccess<void, true> oldData(this);
    switch(newDataType) {
    case Int:
        {
            int* dest = reinterpret_cast<int*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<int>(i, j);
        }
        break;
    case Int64:
        {
            qlonglong* dest = reinterpret_cast<qlonglong*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<qlonglong>(i, j);
        }
        break;
    case Float:
        {
            FloatType* dest = reinterpret_cast<FloatType*>(newData.get());
            for(size_t i = 0; i < _numElements; i++)
                for(size_t j = 0; j < _componentCount; j++)
                    *dest++ = oldData.get<FloatType>(i, j);
        }
        break;
    default:
        OVITO_ASSERT(false); // Unsupported data type
    }
    oldData.reset();

    prepareWriteAccess();
    _dataType = newDataType;
    _dataTypeSize = newDataTypeSize;
    _stride = newStride;
    _capacity = _numElements;
    _data = std::move(newData);
    finishWriteAccess();
}

}   // End of namespace
