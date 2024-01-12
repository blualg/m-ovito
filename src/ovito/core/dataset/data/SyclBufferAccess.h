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

#ifdef OVITO_USE_SYCL

namespace Ovito {

namespace detail {

template<typename T, Ovito::access_mode AccessMode>
class SyclBufferAccessTyped : public sycl::accessor<
    std::conditional_t<AccessMode != access_mode::read, std::remove_pointer_t<T>, std::add_const_t<std::remove_pointer_t<T>>>,
    std::is_pointer_v<T> ? 2 : 1,
    (AccessMode == access_mode::read) ? sycl::access_mode::read : (AccessMode == access_mode::write || AccessMode == access_mode::discard_write ? sycl::access_mode::write : sycl::access_mode::read_write)
    >
{
public:

    using accessor_type = sycl::accessor<
        std::conditional_t<AccessMode != access_mode::read, std::remove_pointer_t<T>, std::add_const_t<std::remove_pointer_t<T>>>,
        std::is_pointer_v<T> ? 2 : 1,
        (AccessMode == access_mode::read) ? sycl::access_mode::read : (AccessMode == access_mode::write || AccessMode == access_mode::discard_write ? sycl::access_mode::write : sycl::access_mode::read_write)>;
    using element_type = typename accessor_type::value_type;
    using iterator = std::add_pointer_t<element_type>;
    using const_iterator = std::add_pointer_t<element_type>;
    using size_type = typename accessor_type::size_type;

    // Indicates whether buffer is treated as 2- or 1-dimensional.
    constexpr static bool ComponentWise = std::is_pointer_v<T>;

    using BufferPointer = std::conditional_t<AccessMode != access_mode::read, DataBuffer*, const DataBuffer*>;

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    SyclBufferAccessTyped() noexcept = default;

    /// Constructor that creates an accessor for a buffer object (which may be null).
    SyclBufferAccessTyped(
        BufferPointer buffer,
        sycl::handler& commandGroupHandler,
        DataBuffer::BufferInitialization initMode =
            (AccessMode == access_mode::discard_write || AccessMode == access_mode::discard_read_write)
                ? DataBuffer::BufferInitialization::Uninitialized
                : DataBuffer::BufferInitialization::Initialized)
    {
        OVITO_ASSERT(!buffer || buffer->size() == 0 || buffer->_data->get_range()[0] / buffer->stride() >= buffer->size());
        OVITO_ASSERT(!buffer || buffer->stride() == sizeof(element_type) * (ComponentWise ? buffer->componentCount() : 1));
        OVITO_ASSERT(!buffer || buffer->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!buffer || buffer->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : buffer->componentCount()));
        if(buffer && buffer->_data) {
#ifdef OVITO_DEBUG
            OVITO_ASSERT(buffer->_isDataInitialized || initMode == DataBuffer::BufferInitialization::Uninitialized);
            if constexpr(AccessMode != access_mode::read) {
                buffer->_isDataInitialized = true;
            }
#endif

            if constexpr(!ComponentWise) {
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 1>();
                *this = accessor_type{typedBuffer, commandGroupHandler, sycl::range(buffer->size()), (initMode == DataBuffer::BufferInitialization::Uninitialized) ? sycl::property_list{sycl::no_init} : sycl::property_list{}};
            }
            else {
                size_t capacity = buffer->_data->get_range()[0] / buffer->stride();
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 2>(sycl::range(capacity, buffer->componentCount()));
                *this = accessor_type{typedBuffer, commandGroupHandler, sycl::range(buffer->size(), buffer->componentCount()), (initMode == DataBuffer::BufferInitialization::Uninitialized) ? sycl::property_list{sycl::no_init} : sycl::property_list{}};
            }

            if constexpr(AccessMode == access_mode::read || AccessMode == access_mode::read_write) {
                buffer->_hasScheduledSyclReadOperations = true;
            }
        }
    }

    /// Constructor that creates a placeholder accessor for a buffer object (which may be null).
    SyclBufferAccessTyped(
        BufferPointer buffer,
        DataBuffer::BufferInitialization initMode =
            (AccessMode == access_mode::discard_write || AccessMode == access_mode::discard_read_write)
                ? DataBuffer::BufferInitialization::Uninitialized
                : DataBuffer::BufferInitialization::Initialized)
    {
        OVITO_ASSERT(!buffer || buffer->size() == 0 || buffer->_data->get_range()[0] / buffer->stride() >= buffer->size());
        OVITO_ASSERT(!buffer || buffer->stride() == sizeof(element_type) * (ComponentWise ? buffer->componentCount() : 1));
        OVITO_ASSERT(!buffer || buffer->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!buffer || buffer->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : buffer->componentCount()));
        if(buffer && buffer->_data) {
#ifdef OVITO_DEBUG
            OVITO_ASSERT(buffer->_isDataInitialized || initMode == DataBuffer::BufferInitialization::Uninitialized);
            if constexpr(AccessMode != access_mode::read) {
                buffer->_isDataInitialized = true;
            }
#endif

            if constexpr(!ComponentWise) {
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 1>();
                *this = accessor_type{typedBuffer, sycl::range(buffer->size()), (initMode == DataBuffer::BufferInitialization::Uninitialized) ? sycl::property_list{sycl::no_init} : sycl::property_list{}};
            }
            else {
                size_t capacity = buffer->_data->get_range()[0] / buffer->stride();
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 2>(sycl::range(capacity, buffer->componentCount()));
                *this = accessor_type{typedBuffer, sycl::range(buffer->size(), buffer->componentCount()), (initMode == DataBuffer::BufferInitialization::Uninitialized) ? sycl::property_list{sycl::no_init} : sycl::property_list{}};
            }

            if constexpr(AccessMode == access_mode::read || AccessMode == access_mode::read_write) {
                buffer->_hasScheduledSyclReadOperations = true;
            }
        }
    }

    /// Constructor that accesses a sub-range of a buffer.
    SyclBufferAccessTyped(
        BufferPointer buffer,
        size_type offset,
        size_type count,
        sycl::handler& commandGroupHandler)
    {
        OVITO_ASSERT(!buffer || buffer->size() == 0 || buffer->_data->get_range()[0] / buffer->stride() >= buffer->size());
        OVITO_ASSERT(!buffer || buffer->stride() == sizeof(element_type) * (ComponentWise ? buffer->componentCount() : 1));
        OVITO_ASSERT(!buffer || buffer->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!buffer || buffer->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : buffer->componentCount()));
        OVITO_ASSERT(buffer || (offset == 0 && count == 0));
        OVITO_ASSERT(!buffer || offset + count <= buffer->size());
        if(buffer && buffer->_data) {
            OVITO_ASSERT(buffer->_isDataInitialized);

            if constexpr(!ComponentWise) {
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 1>();
                *this = accessor_type{typedBuffer, commandGroupHandler, sycl::range(count), sycl::id(offset)};
            }
            else {
                size_t capacity = buffer->_data->get_range()[0] / buffer->stride();
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 2>(sycl::range(capacity, buffer->componentCount()));
                *this = accessor_type{typedBuffer, commandGroupHandler, sycl::range(count, buffer->componentCount()), sycl::id(offset, 0)};
            }

            if constexpr(AccessMode == access_mode::read || AccessMode == access_mode::read_write) {
                buffer->_hasScheduledSyclReadOperations = true;
            }
        }
    }

    /// Constructor for a placeholder accessor that accesses a sub-range of a buffer.
    SyclBufferAccessTyped(
        BufferPointer buffer,
        size_type offset,
        size_type count)
    {
        OVITO_ASSERT(!buffer || buffer->size() == 0 || buffer->_data->get_range()[0] / buffer->stride() >= buffer->size());
        OVITO_ASSERT(!buffer || buffer->stride() == sizeof(element_type) * (ComponentWise ? buffer->componentCount() : 1));
        OVITO_ASSERT(!buffer || buffer->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!buffer || buffer->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : buffer->componentCount()));
        OVITO_ASSERT(buffer || (offset == 0 && count == 0));
        OVITO_ASSERT(!buffer || offset + count <= buffer->size());
        if(buffer && buffer->_data) {
            OVITO_ASSERT(buffer->_isDataInitialized);

            if constexpr(!ComponentWise) {
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 1>();
                *this = accessor_type{typedBuffer, sycl::range(count), sycl::id(offset)};
            }
            else {
                size_t capacity = buffer->_data->get_range()[0] / buffer->stride();
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 2>(sycl::range(capacity, buffer->componentCount()));
                *this = accessor_type{typedBuffer, sycl::range(count, buffer->componentCount()), sycl::id(offset, 0)};
            }

            if constexpr(AccessMode == access_mode::read || AccessMode == access_mode::read_write) {
                buffer->_hasScheduledSyclReadOperations = true;
            }
        }
    }

    /// Copy constructor.
    SyclBufferAccessTyped(const SyclBufferAccessTyped& other) noexcept = default;

    /// Move constructor.
    SyclBufferAccessTyped(SyclBufferAccessTyped&& other) noexcept = default;

    /// Copy assignment (only enabled for read-only accessors).
    SyclBufferAccessTyped& operator=(const SyclBufferAccessTyped& other) noexcept = default;

    /// Move assignment.
    SyclBufferAccessTyped& operator=(SyclBufferAccessTyped&& other) noexcept = default;

    /// Move assignment.
    SyclBufferAccessTyped& operator=(accessor_type&& other) noexcept { static_cast<accessor_type&>(*this) = std::move(other); return *this; }

    /// Returns the number of elements in the data array.
    inline auto size() const noexcept {
        return accessor_type::get_range()[0];
    }

    /// Returns whether this accessor points to a valid DataBuffer.
    inline bool valid() const noexcept {
        return !accessor_type::empty();
    }

    // Note: Workaround for conflict with SYCL's marray::operator!
    inline operator bool() const noexcept { return valid(); }
    inline bool operator!() const noexcept { return !valid(); }

    /// Performs a reduction of the accessed buffer to a scalar value.
    /// This method must only be called on a placeholder accessor.
    /// The result is computed asynchronously and returned in a single-element SYCL buffer.
    template<typename BinaryOperation>
    auto reduction(BinaryOperation combiner, element_type identity, size_t component = 0) const {
        OVITO_STATIC_ASSERT(AccessMode == access_mode::read || AccessMode == access_mode::read_write || AccessMode == access_mode::discard_read_write);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());
        OVITO_ASSERT(ComponentWise || component == 0);
        OVITO_ASSERT(!ComponentWise || component < accessor_type::get_range()[1]);

        // The buffer the results will be stored in.
        sycl::buffer result = detail::allocateSyclBuffer<std::remove_const_t<element_type>>(sycl::range<1>{1});

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                cgh.require(*this);
#ifdef OVITO_USE_SYCL_ACPP
                auto reduction = sycl::reduction(sycl::accessor{result, cgh, sycl::no_init}, identity, std::move(combiner));
#else
                auto reduction = sycl::reduction(result, cgh, identity, std::move(combiner), sycl::reduction::initialize_to_identity);
#endif
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_reduction)(sycl::range(size()), reduction, [=, *this](size_t i, auto& red) {
                    if constexpr(!ComponentWise)
                        red.combine((*this)[i]);
                    else
                        red.combine((*this)[sycl::id<2>(i, component)]);
                });
            });
        }
        else {
            // If the range is empty, we should still initialize the result buffer to a valid value.
            sycl::host_accessor(result, sycl::write_only, sycl::no_init)[0] = identity;
        }
        return result;
    }

    /// Performs two types of reduction simultaneously.
    /// This method must only be called on a placeholder accessor.
    /// The results are computed asynchronously and returned in a two-element SYCL buffer.
    /// Optionally, a selection flags array can be specified, which restricts the reduction to a subset of the array elements.
    template<typename BinaryOperation1, typename BinaryOperation2>
    auto reduction2(BinaryOperation1 combiner1, BinaryOperation2 combiner2, element_type identity1, element_type identity2, size_t component = 0, const DataBuffer* selection = nullptr) const {
        OVITO_STATIC_ASSERT(AccessMode == access_mode::read || AccessMode == access_mode::read_write || AccessMode == access_mode::discard_read_write);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());
        OVITO_ASSERT(ComponentWise || component == 0);
        OVITO_ASSERT(!ComponentWise || component < accessor_type::get_range()[1]);
        OVITO_ASSERT(!selection || selection->size() == this->size());
        OVITO_ASSERT(!selection || (selection->dataType() == DataBuffer::IntSelection && selection->componentCount() == 1));

        // The buffers the results will be stored in.
        auto result = std::make_pair(
            detail::allocateSyclBuffer<std::remove_const_t<element_type>>(sycl::range<1>{1}),
            detail::allocateSyclBuffer<std::remove_const_t<element_type>>(sycl::range<1>{1}));

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {

                // Access selection flags array (optional).
                SyclBufferAccessTyped<const SelectionIntType, access_mode::read> selectionAcc(selection, cgh);

                cgh.require(*this);
#ifdef OVITO_USE_SYCL_ACPP
                auto reduction1 = sycl::reduction(sycl::accessor{result.first,  cgh, sycl::no_init}, identity1, std::move(combiner1));
                auto reduction2 = sycl::reduction(sycl::accessor{result.second, cgh, sycl::no_init}, identity2, std::move(combiner2));
#else
                auto reduction1 = sycl::reduction(result.first,  cgh, identity1, std::move(combiner1), sycl::reduction::initialize_to_identity);
                auto reduction2 = sycl::reduction(result.second, cgh, identity2, std::move(combiner2), sycl::reduction::initialize_to_identity);
#endif
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_reduction2)(sycl::range(size()), reduction1, reduction2, [=, *this](size_t i, auto& red1, auto& red2) {
                    if(selectionAcc.empty() || selectionAcc[i]) {
                        if constexpr(!ComponentWise) {
                            auto v = (*this)[i];
                            red1.combine(v);
                            red2.combine(v);
                        }
                        else {
                            auto v = (*this)[sycl::id<2>(i, component)];
                            red1.combine(v);
                            red2.combine(v);
                        }
                    }
                });
            });
        }
        else {
            // If the range is empty, we should still initialize the result buffers to a valid value.
            sycl::host_accessor(result.first,  sycl::write_only, sycl::no_init)[0] = identity1;
            sycl::host_accessor(result.second, sycl::write_only, sycl::no_init)[0] = identity2;
        }
        return result;
    }

    /// Determines the maximum value in the accessed buffer.
    /// This method must only be called on a placeholder accessor.
    /// The result is computed asynchronously and returned in a single-element SYCL buffer.
    auto max(size_t component = 0) const {
        return reduction(sycl::maximum<element_type>(), std::numeric_limits<element_type>::lowest(), component);
    }

    /// Determines the minimum value in the accessed buffer.
    /// This method must only be called on a placeholder accessor.
    /// The result is computed asynchronously and returned in a single-element SYCL buffer.
    auto min(size_t component = 0) const {
        return reduction(sycl::minimum<element_type>(), std::numeric_limits<element_type>::max(), component);
    }

    /// Determines the minimum and maximum value in the accessed buffer.
    /// This method must only be called on a placeholder accessor.
    /// The results are computed asynchronously and returned in two single-element SYCL buffers.
    /// Optionally, a selection flags array can be specified, which restricts the reduction to a subset of the array elements.
    auto minMax(size_t component = 0, const DataBuffer* selection = nullptr) const {
        return reduction2(sycl::minimum<element_type>(), sycl::maximum<element_type>(), std::numeric_limits<element_type>::max(), std::numeric_limits<element_type>::lowest(), component, selection);
    }

    /// Calculates the sum of all values in the accessed buffer.
    /// This method must only be called on a placeholder accessor.
    /// The result is computed asynchronously and returned in a single-element SYCL buffer.
    auto sum(size_t component = 0) const {
        return reduction(sycl::plus<element_type>(), element_type{}, component);
    }

    /// Fills the accessed range of the buffer with a consecutive sequence of numbers.
    /// The method can only be used with placeholder accessors.
    void iota(const element_type baseValue = 0, size_t component = 0) {
        OVITO_STATIC_ASSERT(AccessMode != access_mode::read);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                cgh.require(*this);
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_iota)(sycl::range(size()), [=, *this](size_t i) {
                    if constexpr(!ComponentWise)
                        (*this)[i] = baseValue + i;
                    else
                        (*this)[sycl::id<2>(i, component)] = baseValue + i;
                });
            });
        }
    }

    /// Fills the accessed range of the buffer with a consecutive sequence of numbers.
    /// This overload of the method accepts a single-element SYCL buffer as input that contains the dynamic base value of the sequence.
    /// Additionally, a static offset can be specified.
    /// The method can only be used with placeholder accessors.
    template<typename BaseT>
    void iota(sycl::buffer<BaseT,1>& baseValue, element_type offset = 0, size_t component = 0) {
        OVITO_STATIC_ASSERT(AccessMode != access_mode::read);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());
        OVITO_ASSERT(baseValue.get_range()[0] == 1);

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                cgh.require(*this);
                sycl::accessor<BaseT> baseValueAcc{baseValue, cgh, sycl::read_only};
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_iota_buf)(sycl::range(size()), [=, *this](size_t i) {
                    if constexpr(!ComponentWise)
                        (*this)[i] = baseValueAcc[0] + offset + i;
                    else
                        (*this)[sycl::id<2>(i, component)] = baseValueAcc[0] + offset + i;
                });
            });
        }
    }

    /// Adds a uniform value to all array values.
    /// The method can only be used with placeholder accessors.
    void add(const element_type increment) {
        OVITO_STATIC_ASSERT(AccessMode != access_mode::read && AccessMode != access_mode::discard_write);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                cgh.require(*this);
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_add)(accessor_type::get_range(), [=, *this](auto id) {
                    (*this)[id] += increment;
                });
            });
        }
    }

    /// Increments all array entries by a given uniform value.
    /// This overload of the method reads the increment value from a single-element SYCL buffer.
    /// The method can only be used with placeholder accessors.
    template<typename BaseT>
    void add(sycl::buffer<BaseT>& incrementBuffer) {
        OVITO_STATIC_ASSERT(AccessMode != access_mode::read && AccessMode != access_mode::discard_write);
        OVITO_STATIC_ASSERT(!ComponentWise);
        OVITO_ASSERT(accessor_type::empty() || accessor_type::is_placeholder());
        OVITO_ASSERT(incrementBuffer.get_range()[0] == 1);

        if(!accessor_type::empty()) {
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                cgh.require(*this);
                sycl::accessor<BaseT> incrementAcc{incrementBuffer, cgh, sycl::read_only};
                OVITO_SYCL_PARALLEL_FOR(cgh, SyclBufferAccess_add_buf)(sycl::range(size()), [=, *this](size_t i) {
                    (*this)[i] += incrementAcc[0];
                });
            });
        }
    }
};

} // End of namespace detail.

template<typename T, access_mode AccessMode>
using SyclBufferAccess = detail::SyclBufferAccessTyped<T, AccessMode>;

}   // End of namespace

#endif