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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>

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
    // Forward declarations:
    template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode> class BufferAccessBase;
    template<typename T, typename BufferType, bool StrongReference, Ovito::access_mode accessmode> class BufferAccessTyped;
#ifdef OVITO_USE_SYCL
    template<typename T, Ovito::access_mode AccessMode> class SyclBufferAccessTyped;
#endif

#ifdef OVITO_USE_SYCL
    /// Allocates a new SYCL buffer object of the given type and dimensions.
    /// If possible, creates a buffer that won't block upon destruction.
    template<typename T, int Dimensions = 1>
    inline sycl::buffer<T, Dimensions> allocateSyclBuffer(const sycl::range<Dimensions>& r) {
#ifdef OVITO_USE_SYCL_ACPP
        // Using AdaptiveCpp's make_async_buffer() to create a buffer with a destructor that won't block.
        return sycl::make_async_buffer<T, Dimensions>(r);
#else
        return sycl::buffer<T, Dimensions>(r);
#endif
    }
#endif
}

#ifdef OVITO_USE_SYCL
    // The OVITO_SYCL_PARALLEL_FOR macro is used to make definitions of SYCL kernels compatible with different SYCL implements.
    // For AdaptiveCpp, the kernels must be named. DPC++, on the other hand, doesn't accept named kernels.
    #ifdef OVITO_USE_SYCL_ACPP
        #define OVITO_SYCL_PARALLEL_FOR(cgh, kernel_name) (cgh).parallel_for<struct kernel_name>
    #else
        #define OVITO_SYCL_PARALLEL_FOR(cgh, kernel_name) (cgh).parallel_for
    #endif
#endif

/**
 * \brief A one- or two-dimensional array of data elements.
 */
class OVITO_CORE_EXPORT DataBuffer : public DataObject
{
    OVITO_CLASS(DataBuffer);

public:

    // Make sure our type IDs are all platform-independent.
    static_assert(sizeof(int8_t) == sizeof(signed char)); // QMetaType::SChar
    static_assert(sizeof(int32_t) == sizeof(int)); // QMetaType::Int
    static_assert(sizeof(int64_t) == sizeof(qlonglong)); // QMetaType::LongLong
    static_assert(sizeof(float) == 4);  // QMetaType::Float
    static_assert(sizeof(double) == 8);  // QMetaType::Double

    /// \brief The most commonly used data types. Note that, at least in principle,
    ///        the class supports any data type registered with the Qt meta type system.
    enum DataTypes
    {
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

    /// Class template returning the C++ type for a DataBuffer data type ID.
    /// Note: Template parameter 'dummy' is needed to work around C++ restrictions (GCC error: "explicit specialization in non-namespace scope")
    template<DataTypes DataType, typename = void> struct TypeFromDataTypeId {};
    template<typename dummy> struct TypeFromDataTypeId<Int8, dummy> { using type = int8_t; };
    template<typename dummy> struct TypeFromDataTypeId<Int32, dummy> { using type = int32_t; };
    template<typename dummy> struct TypeFromDataTypeId<Int64, dummy> { using type = int64_t; };
    template<typename dummy> struct TypeFromDataTypeId<Float32, dummy> { using type = float; };
    template<typename dummy> struct TypeFromDataTypeId<Float64, dummy> { using type = double; };

    enum BufferInitialization
    {
        Uninitialized = 0,
        Initialized = 1
    };

    /// Raw data type used for internal memory buffer.
#ifndef OVITO_USE_SYCL
    using Byte = std::byte;
#else
    using Byte = unsigned char;
#endif

    /// Type used for MD5 checksum of the buffer's data.
    using Checksum = std::array<std::uint64_t, 2>;

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

    /// \brief Null constructor
    void initializeObject(ObjectInitializationFlags flags) { DataObject::initializeObject(flags); }

    /// \brief Constructor that creates and initializes a new buffer array.
    void initializeObject(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList());

    /// \brief Constructor that creates a new buffer array.
    void initializeObject(ObjectInitializationFlags flags, size_t elementCount, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) {
        initializeObject(flags, BufferInitialization::Uninitialized, elementCount, dataType, componentCount, std::move(componentNames));
    }

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

    /// \brief Sets all array elements to the given uniform value.
    template<typename T>
    void fill(const T value);

    /// \brief Sets all array elements for which the corresponding entries in the
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T value, const DataBuffer& selection);

    /// \brief Sets all array elements for which the corresponding entries in the
    ///        selection array are non-zero to the given uniform value.
    template<typename T>
    void fillSelected(const T& value, const DataBuffer* selection) {
        if(selection)
            fillSelected(value, *selection);
        else
            fill(value);
    }

    /// Sets all stored elements to zeros.
    void fillZero();

    /// Replicates existing data N times.
    void replicateFrom(size_t n, const DataBuffer& original);

    /// Reduces the size of the storage array, deleting elements that are marked in the boolean selection array.
    void filterResizeCopyFrom(size_t newSize, const DataBuffer& selection, const DataBuffer& original);

    /// Copies the contents from the given source buffer into this buffer using an index mapping.
    /// This method overload accepts a std::span with an integral value type as index mapping.
    template<std::integral MappingT>
    OVITO_CORE_EXPORT void mappedCopyFrom(const DataBuffer& source, std::span<const MappingT> mapping, bool discardOldContents);

    /// Copies the contents from the given source buffer into this buffer using an index mapping.
    /// This method overload accepts a std::vector with an integral value type as index mapping.
    template<std::integral MappingT>
    void mappedCopyFrom(const DataBuffer& source, const std::vector<MappingT>& mapping, bool discardOldContents) {
        mappedCopyFrom(source, std::span(mapping), discardOldContents);
    }

    /// Copies the contents from the given source buffer into this buffer using an index mapping.
    /// This method overload accepts a buffer accessor with an integral value type as index mapping.
    template<std::integral MappingT, typename BufferType, bool StrongReference, Ovito::access_mode accessmode>
    void mappedCopyFrom(const DataBuffer& source, const detail::BufferAccessTyped<MappingT, BufferType, StrongReference, accessmode>& mapping, bool discardOldContents) {
        mappedCopyFrom(source, std::span(mapping), discardOldContents);
    }

    /// Copies the elements from this buffer into the given destination buffer using an index mapping.
    /// This method overload accepts a std::span with an integral value type as index mapping.
    template<std::integral MappingT>
    OVITO_CORE_EXPORT void mappedCopyTo(DataBuffer& destination, std::span<const MappingT> mapping, bool allowOutOfBoundsIndices = false) const;

    /// Copies the elements from this buffer into the given destination buffer using an index mapping.
    /// This method overload accepts a std::vector with an integral value type as index mapping.
    template<std::integral MappingT>
    void mappedCopyTo(DataBuffer& destination, const std::vector<MappingT>& mapping, bool allowOutOfBoundsIndices = false) const {
        mappedCopyTo(destination, std::span(mapping), allowOutOfBoundsIndices);
    }

    /// Copies the elements from this buffer into the given destination buffer using an index mapping.
    /// This method overload accepts a buffer accessor with an integral value type as index mapping.
    template<std::integral MappingT, typename BufferType, bool StrongReference, Ovito::access_mode accessmode>
    void mappedCopyTo(DataBuffer& destination, const detail::BufferAccessTyped<MappingT, BufferType, StrongReference, accessmode>& mapping, bool allowOutOfBoundsIndices = false) const {
        mappedCopyTo(destination, std::span(mapping), allowOutOfBoundsIndices);
    }

    /// Reorders the existing elements in this storage array according to an index map.
    void reorderElements(const std::vector<size_t>& mapping);

    /// Copies the data elements from the given source buffer into this buffer.
    /// Size, component count, and data type of source and destination buffers must match exactly.
    void copyFrom(const DataBuffer& source);

    /// Copies the data elements from the given source buffer into this buffer while performing a numeric data type conversion.
    /// Array size and component count of source and destination must match but data type can be different.
    void copyFromAndConvert(const DataBuffer& source);

    /// Copies a range of data elements from the given source array into this array.
    /// Component count and data type of source and destination must be compatible.
    void copyRangeFrom(const DataBuffer& source, size_t sourceIndex, size_t destIndex, size_t count);

    /// Copies all values to the given output iterator, possibly doing data type conversion.
    template<typename Iter>
    void copyTo(Iter iter) const;

    /// Copies all values of a given vector component to the given output iterator, possibly doing data type conversion.
    template<typename Iter>
    void copyComponentTo(Iter iter, size_t component) const;

    /// Calls a functor provided by the caller for every value of the given vector component.
    template<typename F>
    bool forEach(size_t component, F&& func) const;

    /// Moves the values from one index of property container to another in all property arrays.
    void moveElement(size_t fromIndex, size_t toIndex, bool callerAlreadyHasWriteAccess = false);

    /// Checks if this buffer|s metadata and the contents exactly match those of another buffer.
    bool equals(const DataBuffer& other) const;

    /// Determines the value range (minimum & maximum value) of a particular vector component in the buffer.
    /// The results are returned as a pair of floating-point values - even if the buffer stores a different data type.
    /// Optionally, a selection flags array can be specified, which restricts the considered data elements to a subset.
    std::pair<FloatType, FloatType> minMax(size_t component = 0, const DataBuffer* selection = nullptr) const;

    /// Computes the axis-aligned bounding box of the 3d coordinates stored in the buffer.
    Box3 boundingBox3() const;

    /// Computes the axis-aligned bounding box of a sub-set of the 3d coordinates stored in the buffer.
    Box3 boundingBox3Indexed(const DataBuffer& indices) const;

    /// Based on a selection flag array as input, computes the mapping of original indices to a packed array.
    ConstDataBufferPtr computePackedMapping() const;

    /// Counts how often the given value occurs in the buffer.
    /// The data type T must be compatible with the buffer's data type.
    template<typename T>
    size_t count(const T value) const;

    /// Returns the number of non-zero entries in the array.
    size_t nonzeroCount() const;

    /// Caches the number of non-zero entries in the array (without actually counting them).
    /// This method can be called by user code if the number of non-zero elements is known
    /// from a previous computation.
    void setNonzeroCount(size_t count) {
        OVITO_ASSERT(count <= size());
        _nonzeroCount.store(count, std::memory_order_relaxed);
    }

    /// Returns the MD5 checksum of the buffer's data.
    Checksum checksum() const;

    /// Invalidates the cached count of non-zero array elements and the checksum.
    /// Normally, there is no need for user code to call this function.
    /// Any write access to the buffer implicitly invalidates the cached information.
    void invalidateCachedInfo() {
        _nonzeroCount.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
        for(size_t i = 0; i < _checksum.size(); i++)
            _checksum[i].store(0, std::memory_order_relaxed);
    }

    /// Invokes a generic lambda function with the current data type of the buffer.
    /// The lambda function must accept exactly one generic parameter ("auto _"), whose type
    /// will be the type of the DataBuffer. The value of the parameter is not used.
    template<typename F>
    void forAnyType(F&& f) const {
        forTypes<Float64, Float32, Int32, Int64, Int8>(std::forward<F>(f));
    }

    /// Invokes a generic lambda function with the current data type of the buffer.
    /// The lambda function must accept exactly one generic parameter ("auto _"), whose type
    /// will be the type of the DataBuffer. The value of the parameter is not used.
    template<DataTypes... TypeIds, typename F>
    void forTypes(F&& f) const {
        if(!((dataType() == TypeIds
            ? (std::forward<F>(f)(typename TypeFromDataTypeId<TypeIds>::type{}), true)
            : false)
        || ...)) {
            OVITO_ASSERT_MSG(false, "DataBuffer::forTypes()", qPrintable(QStringLiteral("DataBuffer has unexpected data type %1.").arg(dataType())));
            throw Exception(tr("Unexpected data buffer type %1").arg(dataType()));
        }
    }

    ////////////////////////////// Data access management //////////////////////////////

    /// Indicates that there currently exists an external memory access to this buffer's data.
    /// This flag is set by the Python binding layer when a NumPy view is created that references this buffer's memory directly.
    bool isBeingAccessedExternally() const { return _isBeingAccessedExternally.load(std::memory_order_acquire); }

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
    inline void prepareWriteAccess() {
#ifdef OVITO_DEBUG
        if(_activeAccessors.fetchAndStoreAcquire(-1) != 0) {
            OVITO_ASSERT_MSG(false, "DataBuffer::prepareWriteAccess()", "Property cannot be locked for write access while it is already locked.");
        }
#endif
        invalidateCachedInfo();
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
    sycl::buffer<Byte>& syclBuffer() {
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
    const Byte* cdata() const {
        return _data.get();
    }

    /// \brief Returns a read-write pointer to the raw element data stored in this buffer.
    Byte* data() {
        return _data.get();
    }
#endif

private:

    /// The data type of the array (a Qt metadata type identifier).
    int _dataType = QMetaType::Void;

    /// Signals that a direct memory access to this buffer's memory is currently active.
    /// This is set by the Python binding layer when a NumPy view is created that references this buffer's memory directly.
    std::atomic<bool> _isBeingAccessedExternally{false};

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
    mutable std::optional<sycl::buffer<Byte>> _data;
#else
    /// The internal memory buffer holding the data elements.
    std::unique_ptr<Byte[]> _data;
#endif

    /// The number of non-zero entries in the array (if known).
    /// This field can provide a performance optimization if the number of
    /// selected elements is queried via nonzeroCount().
    std::atomic<size_t> _nonzeroCount{std::numeric_limits<size_t>::max()};

    /// The MD5 checksum computed from the data elements.
    /// This field can provide a performance optimization if the checksum is queried via checksum().
    std::array<std::atomic<std::uint64_t>, 2> _checksum{{std::uint64_t{0}, std::uint64_t{0}}};

#ifdef OVITO_USE_SYCL
    /// Flag indicating that new kernels have been scheduled for execution that read from the buffer.
    /// This signals blockUntilSyclKernelsFinished() to wait for these operation(s) to finish.
    mutable bool _hasScheduledSyclReadOperations = false;
#endif

#ifdef OVITO_DEBUG
    /// In debug builds, this counter is used to detect race conditions due to concurrent access to a buffer's
    /// memory and data fields. The counter keeps track of how many read or write accessors are currently
    /// operating on this buffer object. Write access must be exclusive and as is signaled by the special value -1.
    mutable QAtomicInteger<int> _activeAccessors = 0;
#endif

#ifdef OVITO_DEBUG
    /// Indicates whether this buffer's contents have been initialized already.
    bool _isDataInitialized = false;
#endif

    friend class RegisteredBufferAccess;
    template<typename BufferType, bool StrongReference, Ovito::access_mode accessmode> friend class detail::BufferAccessBase;
#ifdef OVITO_USE_SYCL
    template<typename T, Ovito::access_mode AccessMode> friend class detail::SyclBufferAccessTyped;
#endif
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
template<typename T, std::size_t N> struct DataBufferPrimitiveType<sycl::marray<T,N>> : public DataBufferPrimitiveType<T> {};
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

/// Class template returning the number of components in the given C++ array structure.
template<typename T, typename = void> struct DataBufferPrimitiveComponentCount { static constexpr size_t value = 1; };
template<typename T, std::size_t N> struct DataBufferPrimitiveComponentCount<std::array<T,N>> { static constexpr size_t value = N; };
#ifdef OVITO_USE_SYCL
template<typename T, std::size_t N> struct DataBufferPrimitiveComponentCount<sycl::marray<T,N>> { static constexpr size_t value = N; };
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
    this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
        SyclBufferAccess<T, access_mode::discard_write> accessor(this, cgh);
        // Note: Tried handler.fill() method, but it led to segfaults. Using a custom fill kernel instead:
        OVITO_SYCL_PARALLEL_FOR(cgh, databuffer_fill)(sycl::range(size()), [=](size_t i) {
            accessor[i] = value;
        });
    });
#else
    T* begin = reinterpret_cast<T*>(data());
    T* end = begin + this->size();
    std::fill(begin, end, value);
#endif
    if constexpr(std::is_same_v<T, SelectionIntType>) {
        setNonzeroCount(!value ? 0 : size());
    }
}

/// \brief Sets all array elements for which the corresponding entries in the
///        selection array are non-zero to the given uniform value.
template<typename T>
inline void DataBuffer::fillSelected(const T value, const DataBuffer& selection)
{
    OVITO_ASSERT(&selection != this); // Do not allow aliasing.
    OVITO_ASSERT(selection.size() == this->size());
    OVITO_ASSERT(selection.dataType() == IntSelection);
    OVITO_ASSERT(selection.componentCount() == 1);
    OVITO_ASSERT(stride() == sizeof(T));
    if(size() == 0)
        return;
    OVITO_ASSERT(_isDataInitialized);
    WriteAccess writeAccess(*this);
    ReadAccess readAccess(selection);
#ifdef OVITO_USE_SYCL
    this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
        SyclBufferAccess<T, access_mode::write> outputAccessor(this, cgh);
        SyclBufferAccess<SelectionIntType, access_mode::read> selectionAccessor(&selection, cgh);
        OVITO_SYCL_PARALLEL_FOR(cgh, databuffer_fillSelected)(sycl::range(size()), [=](size_t i) {
            if(selectionAccessor[i])
                outputAccessor[i] = value;
        });
    });
#else
    const SelectionIntType* __restrict selectionIter = reinterpret_cast<const SelectionIntType*>(selection.cdata());
    for(T* __restrict v = reinterpret_cast<T*>(data()), *end = v + this->size(); v != end; ++v) {
        if(*selectionIter++)
            *v = value;
    }
#endif
}

/// Copies all values to the given output iterator, possibly doing data type conversion.
template<typename Iter>
inline void DataBuffer::copyTo(Iter iter) const
{
    if(size() == 0)
        return;
    OVITO_ASSERT(_isDataInitialized);
    ReadAccess readAccess(*this);
    const size_t cmpntCount = componentCount();
#ifdef OVITO_USE_SYCL
    OVITO_ASSERT(stride() == cmpntCount * dataTypeSize());
    _hasScheduledSyclReadOperations = true;
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        auto valueBuffer = _data->reinterpret<T, 1>();
        sycl::host_accessor valueAccess(valueBuffer, sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer(), *v_end = v + size() * cmpntCount; v != v_end;)
            *iter++ = *v++;
    });
#else
    OVITO_ASSERT(stride() == cmpntCount * dataTypeSize());
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        for(const T* __restrict v = reinterpret_cast<const T*>(cdata()), *v_end = v + size()*cmpntCount; v != v_end;)
            *iter++ = *v++;
    });
#endif
}

/// Copies all values of a given vector component to the given output iterator, possibly doing data type conversion.
template<typename Iter>
inline void DataBuffer::copyComponentTo(Iter iter, size_t component) const
{
    const size_t cmpntCount = componentCount();
    OVITO_ASSERT(component < cmpntCount);
    if(component >= cmpntCount)
        return;
    if(size() == 0)
        return;
    OVITO_ASSERT(_isDataInitialized);
    ReadAccess readAccess(*this);
#ifdef OVITO_USE_SYCL
    _hasScheduledSyclReadOperations = true;
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        auto valueBuffer = _data->reinterpret<T, 1>();
        sycl::host_accessor valueAccess(valueBuffer, sycl::read_only);
        for(const auto* __restrict v = valueAccess.get_pointer() + component, *v_end = v + size() * cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
    });
#else
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        for(const T* __restrict v = reinterpret_cast<const T*>(cdata()) + component, *v_end = v + size()*cmpntCount; v != v_end; v += cmpntCount)
            *iter++ = *v;
    });
#endif
}

/// Calls a functor provided by the caller for every value of the given vector component.
template<typename F>
inline bool DataBuffer::forEach(size_t component, F&& func) const
{
    size_t cmpntCount = componentCount();
    if(component >= cmpntCount)
        return false;
    size_t s = size();
    if(s == 0)
        return true;
    OVITO_ASSERT(_isDataInitialized);
    ReadAccess readAccess(*this);
#ifdef OVITO_USE_SYCL
    _hasScheduledSyclReadOperations = true;
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        auto valueBuffer = _data->reinterpret<T, 1>();
        sycl::host_accessor valueAccess(valueBuffer, sycl::read_only);
        auto v = valueAccess.get_pointer() + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
    });
#else
    forAnyType([&](auto _) {
        using T = decltype(_);
        OVITO_ASSERT(sizeof(T) == dataTypeSize());
        auto v = reinterpret_cast<const T*>(cdata()) + component;
        for(size_t i = 0; i < s; i++, v += cmpntCount)
            std::invoke(std::forward<F>(func), i, *v);
    });
#endif
    return true;
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
    sycl::host_accessor valueAccess(*_data, sycl::read_write);
    std::memmove(valueAccess.get_pointer() + toIndex * stride(), valueAccess.get_pointer() + fromIndex * stride(), stride());
#else
    std::memmove(data() + toIndex * stride(), cdata() + fromIndex * stride(), stride());
#endif
    invalidateCachedInfo();
}

/// Counts how often the given value occurs in the buffer.
/// The data type T must be compatible with the buffer's data type.
template<typename T>
inline size_t DataBuffer::count(const T value) const
{
    size_t count = 0;

#ifdef OVITO_USE_SYCL
    if(size() != 0) {
        sycl::buffer<size_t> countBuf(&count, 1);
        this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            SyclBufferAccess<T, access_mode::read> acc(this, cgh);
#ifdef OVITO_USE_SYCL_ACPP
            auto reduction = sycl::reduction(sycl::accessor{countBuf, cgh, sycl::no_init}, size_t{0}, sycl::plus<size_t>());
#else
            auto reduction = sycl::reduction(countBuf, cgh, size_t{0}, sycl::plus<size_t>(), sycl::property::reduction::initialize_to_identity{});
#endif
            OVITO_SYCL_PARALLEL_FOR(cgh, DataBuffer_count)(sycl::range(acc.size()), reduction, [=](size_t i, auto& red) {
                if(acc[i] == value)
                    red += (size_t)1;
            });
        });
    }
#else
    for(const auto& s : BufferReadAccess<T>(this)) {
        if(s == value)
            count++;
    }
#endif
    return count;
}

// Instantiate function templates for different integral types.
#ifndef OVITO_BUILD_MONOLITHIC
    #if !defined(Core_EXPORTS)
        extern template OVITO_CORE_EXPORT void DataBuffer::mappedCopyFrom(const DataBuffer& source, std::span<const size_t> mapping, bool discardOldContents);
        extern template OVITO_CORE_EXPORT void DataBuffer::mappedCopyFrom(const DataBuffer& source, std::span<const int> mapping, bool discardOldContents);
        extern template OVITO_CORE_EXPORT void DataBuffer::mappedCopyTo(DataBuffer& destination, std::span<const size_t> mapping, bool allowOutOfBoundsIndices) const;
        extern template OVITO_CORE_EXPORT void DataBuffer::mappedCopyTo(DataBuffer& destination, std::span<const int> mapping, bool allowOutOfBoundsIndices) const;
    #endif
#endif

}   // End of namespace