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
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

enum class access_mode
{
    read,
    write,
    read_write,
    discard_write,
    discard_read_write
};

namespace detail {
    // Forward declarations
    template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode> class BufferAccessBase;
    template<typename T, Ovito::access_mode AccessMode> class SyclBufferAccessTyped;
}

/**
 * \brief A one- or two-dimensional array of data elements.
 */
class OVITO_CORE_EXPORT DataBuffer : public DataObject
{
    OVITO_CLASS(DataBuffer);
    Q_CLASSINFO("DisplayName", "Data buffer");

public:

    // Make sure our type IDs are all platform-independent.
    static_assert(sizeof(int8_t) == sizeof(signed char)); // QMetaType::SChar
    static_assert(sizeof(int32_t) == sizeof(int)); // QMetaType::Int
    static_assert(sizeof(int64_t) == sizeof(qlonglong)); // QMetaType::LongLong
    static_assert(sizeof(float) == 4);  // QMetaType::Float
    static_assert(sizeof(double) == 8);  // QMetaType::Double

    /// \brief The most commonly used data types. Note that, at least in principle,
    ///        the class supports any data type registered with the Qt meta type system.
    enum DataTypes {

        Int8 = QMetaType::SChar,
        Int32 = QMetaType::Int,
        Int64 = QMetaType::LongLong,
        Float32 = QMetaType::Float,
        Float64 = QMetaType::Double,

        // Alias for high-precision floating-point type:
        FloatDefault = std::is_same_v<FloatType, double> ? Float64 : Float32,

        // Alias for low-precision floating-point type:
        FloatGraphics = std::is_same_v<GraphicsFloatType, double> ? Float64 : Float32,

        // Alias for data type used for storing selection flags:
        IntSelection = Int8,

        // Alias for data type used for storing unique identifiers:
        IntIdentifier = Int64
    };

    enum BufferInitialization {
        Uninitialized = 0,
        Initialized = 1
    };

    /// RAII utility class that guards read access to a DataBuffer.
    class ReadAccess
    {
    public:
        ReadAccess(const DataBuffer& buffer) noexcept : _buffer(buffer) { buffer.prepareReadAccess(); }
        ~ReadAccess() { _buffer.finishReadAccess(); }
    private:
        const DataBuffer& _buffer;
    };

    /// RAII utility class that guards write access to a DataBuffer.
    class WriteAccess
    {
    public:
        WriteAccess(DataBuffer& buffer) noexcept : _buffer(buffer) { buffer.prepareWriteAccess(); }
        ~WriteAccess() { _buffer.finishWriteAccess(); }
    private:
        DataBuffer& _buffer;
    };

public:

    /// \brief Creates an empty buffer.
    Q_INVOKABLE explicit DataBuffer(ObjectInitializationFlags flags) : DataObject(flags) {}

    /// \brief Constructor that creates and initializes a new buffer array.
    DataBuffer(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList());

    /// \brief Constructor that creates a new buffer array.
    DataBuffer(ObjectInitializationFlags flags, size_t elementCount, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) :
        DataBuffer(flags, BufferInitialization::Uninitialized, elementCount, dataType, componentCount, std::move(componentNames)) {}

    /// \brief Returns the number of elements stored in the buffer array.
    size_t size() const { return _numElements; }

    /// \brief Resizes the buffer.
    /// \param newSize The new number of elements.
    /// \param preserveData Controls whether the existing per-element data is preserved.
    ///                     This also determines whether newly allocated memory is initialized to zero.
    void resize(size_t newSize, bool preserveData);

    /// \brief Resizes the buffer and copies the data element from an existing buffer.
    void resizeCopyFrom(size_t newSize, const DataBuffer& original);

    /// \brief Grows the number of data elements while preserving the exiting data.
    /// Newly added elements are *not* initialized to zero by this method.
    /// \return True if the memory buffer was reallocated, because the current capacity was insufficient
    /// to accommodate the new elements.
    bool grow(size_t numAdditionalElements, bool callerAlreadyHasWriteAccess = false);

    /// \brief Reduces the number of data elements while preserving the exiting data.
    /// Note: This method never reallocates the memory buffer. Thus, the capacity of the array remains unchanged and the
    /// memory of the truncated elements is not released by the method.
    void truncate(size_t numElementsToRemove, bool callerAlreadyHasWriteAccess = false);

    /// \brief Returns the data type of the property.
    /// \return The identifier of the data type used for the elements stored in
    ///         this property storage according to the Qt meta type system.
    int dataType() const { return _dataType; }

    /// \brief Returns the data type as a human-readable string.
    const char* dataTypeName() const { return QMetaType(dataType()).name(); }

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

    /// Changes the data type of the buffer in place (only if necessary) and converts the stored values.
    void convertToDataType(int dataType);

    /// \brief Sets all array elements to the given uniform value.
    template<typename T>
    void fill(const T value);

    /// \brief Sets all array elements for which the corresponding entries in the
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T value, const DataBuffer& selectionProperty);

    /// \brief Sets all array elements for which the corresponding entries in the
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T& value, const DataBuffer* selectionProperty) {
        if(selectionProperty)
            fillSelected(value, *selectionProperty);
        else
            fill(value);
    }

    /// Sets all stored elements to zeros.
    void fillZero();

    /// Replicates existing data N times.
    void replicateFrom(size_t n, const DataBuffer& original);

    /// Reduces the size of the storage array, deleting elements for are marked in the selection array.
    void filterResizeCopyFrom(size_t newSize, const DataBuffer& selection, const DataBuffer& original);

    /// Creates a copy of the array, not containing those elements for which
    /// the corresponding bits in the given bit array were set.
    OORef<DataBuffer> filterCopy(const boost::dynamic_bitset<>& mask) const;

    /// Copies the contents from the given source into this storage using a element mapping.
    void mappedCopyFrom(const DataBuffer& source, const std::vector<size_t>& mapping, bool discardOldContents);

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
    bool copyTo(Iter iter, size_t component = 0) const;

    /// Calls a functor provided by the caller for every value of the given vector component.
    template<typename F>
    bool forEach(size_t component, F&& func) const;

    /// Moves the values from one index of property container to another in all property arrays.
    void moveElement(size_t fromIndex, size_t toIndex, bool callerAlreadyHasWriteAccess = false);

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

#ifdef OVITO_USE_SYCL
    /// Provides direct access to the internal SYCL buffer managed by this class.
    cl::sycl::buffer<std::byte>& syclBuffer() {
        OVITO_ASSERT(_data);
        return *_data;
    }

    /// Blocks until all SYCL kernels in the queue that read from this buffer have finished running.
    /// Only then it is safe again to write into the buffer on the host. This function is used by the
    /// Python binding layer, which requires permanent write access to the buffer's underlying memory on the host.
    void blockUntilSyclKernelsFinished();
#endif

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

private:

#ifndef OVITO_USE_SYCL
    /// \brief Returns a read-only pointer to the raw element data stored in this buffer.
    const std::byte* cdata() const {
        return _data.get();
    }

    /// \brief Returns a read-write pointer to the raw element data stored in this buffer.
    std::byte* data() {
        return _data.get();
    }
#endif

private:

    /// The data type of the array (a Qt metadata type identifier).
    int _dataType = QMetaType::Void;

    /// The number of bytes per single value.
    size_t _dataTypeSize = 0;

    /// The number of elements in the property storage.
    size_t _numElements = 0;

#ifndef OVITO_USE_SYCL
    /// The capacity of the allocated buffer.
    size_t _capacity = 0;
#endif

    /// The number of bytes per element.
    size_t _stride = 0;

    /// The number of vector components per element.
    size_t _componentCount = 0;

    /// The names of the vector components if this array stores more than one value per element.
    QStringList _componentNames;

#ifdef OVITO_USE_SYCL
    /// The internal memory buffer holding the data elements.
    /// Note: We are using std::optional<> here, because SYCL won't allow us to allocate 0-size buffers.
    mutable std::optional<cl::sycl::buffer<std::byte>> _data;
#else
    /// The internal memory buffer holding the data elements.
    std::unique_ptr<std::byte[]> _data;
#endif

#ifdef OVITO_USE_SYCL
    /// Flag is set whenever new kernels have been scheduled for execution that read from the buffer.
    /// This indicates to blockUntilSyclKernelsFinished() that is needs to wait for these operation(s) to finish.
    mutable bool _hasScheduledSyclReadOperations = false;
#endif

#ifdef OVITO_DEBUG
    /// In debug builds, this counter is used to detect race conditions due to concurrent access to a buffer's
    /// memory and data fields. The counter keeps track of how many read or write accessors are currently
    /// operating on this buffer object. Write access must be exclusive and as is indicated by the special value -1.
    mutable QAtomicInteger<int> _activeAccessors = 0;
#endif

#ifdef OVITO_DEBUG
    /// Indicates whether this buffer's contents are still uninitialized.
    bool _isDataInitialized = false;
#endif

    template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode> friend class detail::BufferAccessBase;
    template<typename T, Ovito::access_mode AccessMode> friend class detail::SyclBufferAccessTyped;
};

/// Class template returning the data type identifier for the components in the given C++ array structure.
template<typename T, typename = void> struct DataBufferPrimitiveType {};
template<> struct DataBufferPrimitiveType<int8_t> { static constexpr DataBuffer::DataTypes value = DataBuffer::DataTypes::Int8; };
template<> struct DataBufferPrimitiveType<int32_t> { static constexpr DataBuffer::DataTypes value = DataBuffer::DataTypes::Int32; };
template<> struct DataBufferPrimitiveType<int64_t> { static constexpr DataBuffer::DataTypes value = DataBuffer::DataTypes::Int64; };
template<> struct DataBufferPrimitiveType<float> { static constexpr DataBuffer::DataTypes value = DataBuffer::DataTypes::Float32; };
template<> struct DataBufferPrimitiveType<double> { static constexpr DataBuffer::DataTypes value = DataBuffer::DataTypes::Float64; };
template<typename T, std::size_t N> struct DataBufferPrimitiveType<std::array<T,N>> : public DataBufferPrimitiveType<T> {};
#ifdef OVITO_USE_SYCL
template<typename T, std::size_t N> struct DataBufferPrimitiveType<cl::sycl::marray<T,N>> : public DataBufferPrimitiveType<T> {};
#endif
template<typename T> struct DataBufferPrimitiveType<Point_2<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Point_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Vector_2<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Vector_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Vector_4<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<Matrix_3<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<AffineTransformationT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<QuaternionT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<ColorT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<ColorAT<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<SymmetricTensor2T<T>> : public DataBufferPrimitiveType<T> {};
template<typename T> struct DataBufferPrimitiveType<T, typename std::enable_if<std::is_enum<T>::value>::type> : public DataBufferPrimitiveType<std::make_signed_t<std::underlying_type_t<T>>> {};

OVITO_STATIC_ASSERT(DataBufferPrimitiveType<IdentifierIntType>::value == DataBuffer::IntIdentifier);
OVITO_STATIC_ASSERT(DataBufferPrimitiveType<SelectionIntType>::value  == DataBuffer::IntSelection);
OVITO_STATIC_ASSERT(DataBufferPrimitiveType<GraphicsFloatType>::value == DataBuffer::FloatGraphics);

/// Class template returning the  number of components in the given C++ array structure.
template<typename T, typename = void> struct DataBufferPrimitiveComponentCount { static constexpr size_t value = 1; };
template<typename T, std::size_t N> struct DataBufferPrimitiveComponentCount<std::array<T,N>> { static constexpr size_t value = N; };
#ifdef OVITO_USE_SYCL
template<typename T, std::size_t N> struct DataBufferPrimitiveComponentCount<cl::sycl::marray<T,N>> { static constexpr size_t value = N; };
#endif
template<typename T> struct DataBufferPrimitiveComponentCount<Point_2<T>> { static constexpr size_t value = 2; };
template<typename T> struct DataBufferPrimitiveComponentCount<Point_3<T>> { static constexpr size_t value = 3; };
template<typename T> struct DataBufferPrimitiveComponentCount<Vector_2<T>> { static constexpr size_t value = 2; };
template<typename T> struct DataBufferPrimitiveComponentCount<Vector_3<T>> { static constexpr size_t value = 3; };
template<typename T> struct DataBufferPrimitiveComponentCount<Vector_4<T>> { static constexpr size_t value = 4; };
template<typename T> struct DataBufferPrimitiveComponentCount<Matrix_3<T>> { static constexpr size_t value = 9; };
template<typename T> struct DataBufferPrimitiveComponentCount<AffineTransformationT<T>> { static constexpr size_t value = 12; };
template<typename T> struct DataBufferPrimitiveComponentCount<QuaternionT<T>> { static constexpr size_t value = 4; };
template<typename T> struct DataBufferPrimitiveComponentCount<ColorT<T>> { static constexpr size_t value = 3; };
template<typename T> struct DataBufferPrimitiveComponentCount<ColorAT<T>> { static constexpr size_t value = 4; };
template<typename T> struct DataBufferPrimitiveComponentCount<SymmetricTensor2T<T>> { static constexpr size_t value = 6; };

}   // End of namespace

#include "BufferAccess.h"
#include "SyclBufferAccess.h"

namespace Ovito {

/// Sets all array elements to the given uniform value.
template<typename T>
inline void DataBuffer::fill(const T value)
{
    OVITO_ASSERT(stride() == sizeof(T));
    if(size() == 0)
        return;
    WriteAccess writeAccess(*this);
#ifdef OVITO_DEBUG
    _isDataInitialized = true;
#endif
#ifdef OVITO_USE_SYCL
    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
        SyclBufferAccess<T, access_mode::discard_write> accessor(this, cgh);
        // Note: Tried handler.fill() method, but it led to segfaults. Using a custom fill kernel instead:
        cgh.parallel_for<class databuffer_fill>(cl::sycl::range(size()), [=](size_t i) {
            accessor[i] = value;
        });
    });
#else
    T* begin = reinterpret_cast<T*>(data());
    T* end = begin + this->size();
    std::fill(begin, end, value);
#endif
}

/// \brief Sets all array elements for which the corresponding entries in the
///        selection array are non-zero to the given uniform value.
template<typename T>
inline void DataBuffer::fillSelected(const T value, const DataBuffer& selectionProperty)
{
    OVITO_ASSERT(&selectionProperty != this); // Do not allow aliasing.
    OVITO_ASSERT(selectionProperty.size() == this->size());
    OVITO_ASSERT(selectionProperty.dataType() == IntSelection);
    OVITO_ASSERT(selectionProperty.componentCount() == 1);
    if(size() == 0)
        return;
    OVITO_ASSERT(_isDataInitialized);
    WriteAccess writeAccess(*this);
    ReadAccess readAccess(selectionProperty);
#ifdef OVITO_USE_SYCL
    ExecutionContext::current().ui().taskManager().syclQueue().submit([&](cl::sycl::handler& cgh) {
        SyclBufferAccess<T, access_mode::write> outputAccessor(this, cgh);
        SyclBufferAccess<SelectionIntType, access_mode::read> selectionAccessor(&selectionProperty, cgh);
        cgh.parallel_for<class databuffer_fillSelected>(cl::sycl::range(size()), [=](size_t i) {
            if(selectionAccessor[i])
                outputAccessor[i] = value;
        });
    });
#else
    const SelectionIntType* __restrict selectionIter = reinterpret_cast<const SelectionIntType*>(selectionProperty.cdata());
    for(T* __restrict v = reinterpret_cast<T*>(data()), *end = v + this->size(); v != end; ++v) {
        if(*selectionIter++)
            *v = value;
    }
#endif
}

/// Copies the values stored this buffer to the given output iterator if it is compatible.
/// Returns false if copying was not possible, because the data type of the array and the output iterator
/// are not compatible.
template<typename Iter>
inline bool DataBuffer::copyTo(Iter iter, size_t component) const
{
    OVITO_ASSERT(_isDataInitialized);
    const size_t cmpntCount = componentCount();
    if(component >= cmpntCount)
        return false;
    if(size() == 0)
        return true;
    ReadAccess readAccess(*this);
#ifdef OVITO_USE_SYCL
    if(dataType() == DataBuffer::Int8) {
        cl::sycl::buffer<int8_t> valueBuffer = _data->reinterpret<int8_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Int32) {
        cl::sycl::buffer<int32_t> valueBuffer = _data->reinterpret<int32_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Int64) {
        cl::sycl::buffer<int64_t> valueBuffer = _data->reinterpret<int64_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Float32) {
        cl::sycl::buffer<float> valueBuffer = _data->reinterpret<float, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Float64) {
        cl::sycl::buffer<double> valueBuffer = _data->reinterpret<double, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
#else
    if(dataType() == DataBuffer::Int8) {
        for(const int8_t* __restrict v = reinterpret_cast<const int8_t*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Int32) {
        for(const int32_t* __restrict v = reinterpret_cast<const int32_t*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Int64) {
        for(const int64_t* __restrict v = reinterpret_cast<const int64_t*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Float32) {
        for(const float* __restrict v = reinterpret_cast<const float*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
    else if(dataType() == DataBuffer::Float64) {
        for(const double* __restrict v = reinterpret_cast<const double*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
        return true;
    }
#endif
    return false;
}

/// Calls a functor provided by the caller for every value of the given vector component.
template<typename F>
inline bool DataBuffer::forEach(size_t component, F&& func) const
{
    OVITO_ASSERT(_isDataInitialized);
    size_t cmpntCount = componentCount();
    if(component >= cmpntCount)
        return false;
    size_t s = size();
    if(s == 0)
        return true;
    ReadAccess readAccess(*this);
#ifdef OVITO_USE_SYCL
    if(dataType() == DataBuffer::Int8) {
        cl::sycl::buffer<int8_t> valueBuffer = _data->reinterpret<int8_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Int32) {
        cl::sycl::buffer<int32_t> valueBuffer = _data->reinterpret<int32_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Int64) {
        cl::sycl::buffer<int64_t> valueBuffer = _data->reinterpret<int64_t, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Float32) {
        cl::sycl::buffer<float> valueBuffer = _data->reinterpret<float, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Float64) {
        cl::sycl::buffer<double> valueBuffer = _data->reinterpret<double, 1>();
        cl::sycl::host_accessor valueAccess(valueBuffer, cl::sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
#else
    if(dataType() == DataBuffer::Int8) {
        auto v = reinterpret_cast<const int8_t*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Int32) {
        auto v = reinterpret_cast<const int32_t*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Int64) {
        auto v = reinterpret_cast<const int64_t*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Float32) {
        auto v = reinterpret_cast<const float*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
    else if(dataType() == DataBuffer::Float64) {
        auto v = reinterpret_cast<const double*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
        return true;
    }
#endif
    return false;
}

/// Moves the values from one index of property container to another in all property arrays.
inline void DataBuffer::moveElement(size_t fromIndex, size_t toIndex, bool callerAlreadyHasWriteAccess)
{
    OVITO_ASSERT(fromIndex < size() && toIndex < size());
    OVITO_ASSERT(_isDataInitialized);
#ifdef OVITO_DEBUG
    std::optional<WriteAccess> writeAccess;
    if(!callerAlreadyHasWriteAccess)
        writeAccess.emplace(*this);
#endif
#ifdef OVITO_USE_SYCL
    OVITO_ASSERT(_data);
    cl::sycl::host_accessor valueAccess(*_data, cl::sycl::read_write);
    std::memmove(valueAccess.get_pointer() + toIndex * stride(), valueAccess.get_pointer() + fromIndex * stride(), stride());
#else
    std::memmove(data() + toIndex * stride(), cdata() + fromIndex * stride(), stride());
#endif
}

}   // End of namespace