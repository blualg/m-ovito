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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief A one- or two-dimensional array of data elements.
 */
class OVITO_CORE_EXPORT DataBuffer : public DataObject
{
    OVITO_CLASS(DataBuffer);
    Q_CLASSINFO("DisplayName", "Data buffer");

public:

    /// \brief The most commonly used data types. Note that, at least in principle,
    ///        the class supports any data type registered with the Qt meta type system.
    enum StandardDataType {
#ifndef Q_CC_MSVC
        Int = qMetaTypeId<int>(),
        Int64 = qMetaTypeId<qlonglong>(),
        Float = qMetaTypeId<FloatType>()
#else // MSVC compiler doesn't treat qMetaTypeId() function as constexpr. Workaround:
        Int = QMetaType::Int,
        Int64 = QMetaType::LongLong,
#ifdef FLOATTYPE_FLOAT
        Float = QMetaType::Float
#else
        Float = QMetaType::Double
#endif
#endif
    };

    enum InitializationFlag {
        NoFlags = 0,
        InitializeMemory = (1<<0)
    };
    Q_DECLARE_FLAGS(InitializationFlags, InitializationFlag);
    Q_FLAG(InitializationFlags);

public:

    /// \brief Creates an empty buffer.
    Q_INVOKABLE DataBuffer(ObjectCreationParams params) : DataObject(params) {}

    /// \brief Constructor that creates and initializes a new buffer array.
    DataBuffer(ObjectCreationParams params, size_t elementCount, int dataType, size_t componentCount = 1, InitializationFlags flags = NoFlags, QStringList componentNames = QStringList());

    /// \brief Returns the number of elements stored in the buffer array.
    size_t size() const { return _numElements; }

    /// \brief Resizes the buffer.
    /// \param newSize The new number of elements.
    /// \param preserveData Controls whether the existing per-element data is preserved.
    ///                     This also determines whether newly allocated memory is initialized to zero.
    void resize(size_t newSize, bool preserveData);

    /// \brief Grows the number of data elements while preserving the exiting data.
    /// Newly added elements are *not* initialized to zero by this method.
    /// \return True if the memory buffer was reallocated, because the current capacity was insufficient
    /// to accommodate the new elements.
    bool grow(size_t numAdditionalElements, bool callerAlreadyHasWriteAccess = false);

    /// \brief Reduces the number of data elements while preserving the exiting data.
    /// Note: This method never reallocates the memory buffer. Thus, the capacity of the array remains unchanged and the
    /// memory of the truncated elements is not released by the method.
    void truncate(size_t numElementsToRemove);

    /// \brief Returns the data type of the property.
    /// \return The identifier of the data type used for the elements stored in
    ///         this property storage according to the Qt meta type system.
    int dataType() const { return _dataType; }

    /// \brief Returns the number of bytes per value.
    /// \return Number of bytes used to store a single value of the data type
    ///         specified by type().
    size_t dataTypeSize() const { return _dataTypeSize; }

    /// \brief Returns the number of bytes used per element.
    size_t stride() const { return _stride; }

    /// \brief Returns the number of vector components per element.
    /// \return The number of data values stored per element in this buffer object.
    size_t componentCount() const { return _componentCount; }

    /// \brief Returns the human-readable names for the vector components if this is a vector buffer.
    /// \return The names of the vector components if this buffer contains more than one value per element.
    ///         If this is only a scalar value buffer then an empty list is returned by this method.
    const QStringList& componentNames() const { return _componentNames; }

    /// \brief Sets the human-readable names for the vector components if this is a vector buffer.
    void setComponentNames(QStringList names) {
        OVITO_ASSERT(names.empty() || names.size() == componentCount());
        _componentNames = std::move(names);
    }

    /// Changes the data type of the buffer in place and converts the stored values.
    void convertDataType(int newDataType);

    /// \brief Returns a read-only pointer to the raw element data stored in this buffer.
    const uint8_t* cbuffer() const {
        return _data.get();
    }

    /// \brief Returns a read-write pointer to the raw element data stored in this buffer.
    uint8_t* buffer() {
        return _data.get();
    }

    /// \brief Sets all array elements to the given uniform value.
    template<typename T>
    void fill(const T value) {
        prepareWriteAccess();
        OVITO_ASSERT(stride() == sizeof(T));
        T* begin = reinterpret_cast<T*>(buffer());
        T* end = begin + this->size();
        std::fill(begin, end, value);
        finishWriteAccess();
    }

    /// \brief Sets all array elements for which the corresponding entries in the 
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T value, const DataBuffer& selectionProperty) {
        prepareWriteAccess();
        selectionProperty.prepareReadAccess();
        OVITO_ASSERT(selectionProperty.size() == this->size());
        OVITO_ASSERT(selectionProperty.dataType() == Int);
        OVITO_ASSERT(selectionProperty.componentCount() == 1);
        const int* selectionIter = reinterpret_cast<const int*>(selectionProperty.cbuffer());
        for(T* v = reinterpret_cast<T*>(buffer()), *end = v + this->size(); v != end; ++v)
            if(*selectionIter++) *v = value;
        selectionProperty.finishReadAccess();
        finishWriteAccess();
    }

    /// \brief Sets all array elements for which the corresponding entries in the 
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T& value, const DataBuffer* selectionProperty) {
        if(selectionProperty)
            fillSelected(value, *selectionProperty);
        else
            fill(value);
    }

    // Set all stored values to zeros.
    void fillZero() {
        prepareWriteAccess();
        std::memset(_data.get(), 0, this->size() * this->stride());
        finishWriteAccess();
    }

    /// Extends the data array and replicates the existing data N times.
    void replicate(size_t n, bool replicateValues = true);

    /// Reduces the size of the storage array, removing elements for which
    /// the corresponding bits in the bit array are set.
    void filterResize(const boost::dynamic_bitset<>& mask);

    /// Creates a copy of the array, not containing those elements for which
    /// the corresponding bits in the given bit array were set.
    OORef<DataBuffer> filterCopy(const boost::dynamic_bitset<>& mask) const;

    /// Copies the contents from the given source into this storage using a element mapping.
    void mappedCopyFrom(const DataBuffer& source, const std::vector<size_t>& mapping);

    /// Copies the elements from this storage array into the given destination array using an index mapping.
    void mappedCopyTo(DataBuffer& destination, const std::vector<size_t>& mapping) const;

    /// Reorders the existing elements in this storage array using an index map.
    void reorderElements(const std::vector<size_t>& mapping);

    /// Copies the data elements from the given source array into this array. 
    /// Array size, component count and data type of source and destination must match exactly.
    void copyFrom(const DataBuffer& source);

    /// Copies a range of data elements from the given source array into this array. 
    /// Component count and data type of source and destination must be compatible.
    void copyRangeFrom(const DataBuffer& source, size_t sourceIndex, size_t destIndex, size_t count);

    /// Copies the values stored this buffer to the given output iterator if it is compatible.
    /// Returns false if copying was not possible, because the data type of the array and the output iterator
    /// are not compatible.
    template<typename Iter>
    bool copyTo(Iter iter, size_t component = 0) const {
        size_t cmpntCount = componentCount();
        if(component >= cmpntCount) return false;
        if(size() == 0) return true;
        if(dataType() == DataBuffer::Int) {
            prepareReadAccess();
            for(auto v = reinterpret_cast<const int*>(cbuffer()) + component, v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
                *iter++ = *v;
            finishReadAccess();
            return true;
        }
        else if(dataType() == DataBuffer::Int64) {
            prepareReadAccess();
            for(auto v = reinterpret_cast<const qlonglong*>(cbuffer()) + component, v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
                *iter++ = *v;
            finishReadAccess();
            return true;
        }
        else if(dataType() == DataBuffer::Float) {
            prepareReadAccess();
            for(auto v = reinterpret_cast<const FloatType*>(cbuffer()) + component, v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
                *iter++ = *v;
            finishReadAccess();
            return true;
        }
        return false;
    }

    /// Calls a functor provided by the caller for every value of the given vector component.
    template<typename F>
    bool forEach(size_t component, F&& func) const {
        size_t cmpntCount = componentCount();
        if(component >= cmpntCount) 
            return false;
        size_t s = size();
        if(s == 0) 
            return true;
        if(dataType() == DataBuffer::Int) {
            prepareReadAccess();
            auto v = reinterpret_cast<const int*>(cbuffer()) + component;
            for(size_t i = 0; i < s; i++, v += cmpntCount)
                std::invoke(std::forward<F>(func), i, *v);
            finishReadAccess();
            return true;
        }
        else if(dataType() == DataBuffer::Int64) {
            prepareReadAccess();
            auto v = reinterpret_cast<const qlonglong*>(cbuffer()) + component;
            for(size_t i = 0; i < s; i++, v += cmpntCount)
                std::invoke(std::forward<F>(func), i, *v);
            finishReadAccess();
            return true;
        }
        else if(dataType() == DataBuffer::Float) {
            prepareReadAccess();
            auto v = reinterpret_cast<const FloatType*>(cbuffer()) + component;
            for(size_t i = 0; i < s; i++, v += cmpntCount)
                std::invoke(std::forward<F>(func), i, *v);
            finishReadAccess();
            return true;
        }
        return false;
    }

    /// Checks if this buffer|s metadata and the contents exactly match those of another buffer.
    bool equals(const DataBuffer& other) const;
    
    ////////////////////////////// Data access management //////////////////////////////

    /// Informs the buffer object that a read accessor is becoming active.
    inline void prepareReadAccess() const {
#ifdef OVITO_DEBUG
        if(_activeAccessors.fetchAndAddAcquire(1) == -1) {
            OVITO_ASSERT_MSG(false, "DataBuffer::prepareReadAccess()", "Property cannot be read from while it is locked for write access.");
        }
#endif
    }

    /// Informs the buffer object that a read accessor is done.
    inline void finishReadAccess() const {
#ifdef OVITO_DEBUG
        int oldValue = _activeAccessors.fetchAndSubRelease(1);
        OVITO_ASSERT(oldValue > 0);
#endif
    }

    /// Informs the buffer object that a read/write accessor is becoming active.
    inline void prepareWriteAccess() const {
#ifdef OVITO_DEBUG
        if(_activeAccessors.fetchAndStoreAcquire(-1) != 0) {
            OVITO_ASSERT_MSG(false, "DataBuffer::prepareWriteAccess()", "Property cannot be locked for write acccess while it is already locked.");
        }
#endif
    }

    /// Informs the buffer object that a write accessor is done.
    inline void finishWriteAccess() const {
#ifdef OVITO_DEBUG
        int oldValue = _activeAccessors.fetchAndStoreRelease(0);
        OVITO_ASSERT(oldValue == -1);
#endif
    }

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

private:

    /// The data type of the array (a Qt metadata type identifier).
    int _dataType = QMetaType::Void;

    /// The number of bytes per data type value.
    size_t _dataTypeSize = 0;

    /// The number of elements in the property storage.
    size_t _numElements = 0;

    /// The capacity of the allocated buffer.
    size_t _capacity = 0;

    /// The number of bytes per element.
    size_t _stride = 0;

    /// The number of vector components per element.
    size_t _componentCount = 0;

    /// The names of the vector components if this array stores more than one value per element.
    QStringList _componentNames;

    /// The internal memory buffer holding the data elements.
    std::unique_ptr<uint8_t[]> _data;

#ifdef OVITO_DEBUG
    /// In debug builds this counter keeps track of how many read or write accessors 
    /// are currently referencing this buffer object.
    mutable QAtomicInteger<int> _activeAccessors = 0;
#endif
};

/// Class template returning the Qt data type identifier for the components in the given C++ array structure.
template<typename T, typename = void> struct DataBufferPrimitiveType { static constexpr int value = qMetaTypeId<T>(); };
#ifdef Q_CC_MSVC // MSVC compiler doesn't treat qMetaTypeId() function as constexpr. Workaround:
template<> struct DataBufferPrimitiveType<int> { static constexpr DataBuffer::StandardDataType value = DataBuffer::Int; };
template<> struct DataBufferPrimitiveType<qlonglong> { static constexpr DataBuffer::StandardDataType value = DataBuffer::Int64; };
template<> struct DataBufferPrimitiveType<FloatType> { static constexpr DataBuffer::StandardDataType value = DataBuffer::Float; };
#endif
template<typename T, std::size_t N> struct DataBufferPrimitiveType<std::array<T,N>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Point_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Vector_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Point_2<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Vector_2<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Matrix_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<AffineTransformationT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<QuaternionT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<ColorT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<ColorAT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<SymmetricTensor2T<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<T, typename std::enable_if<std::is_enum<T>::value>::type> : public DataBufferPrimitiveType<std::make_signed_t<std::underlying_type_t<T>>> {};

}   // End of namespace
