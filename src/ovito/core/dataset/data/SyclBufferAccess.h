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

#ifdef OVITO_USE_SYCL

namespace Ovito {

namespace detail {

template<typename T, Ovito::access_mode AccessMode>
class SyclBufferAccessTyped : public cl::sycl::accessor<
    std::conditional_t<AccessMode != access_mode::read, std::remove_pointer_t<T>, std::add_const_t<std::remove_pointer_t<T>>>,
    std::is_pointer_v<T> ? 2 : 1,
    (AccessMode == access_mode::read) ? cl::sycl::access_mode::read : (AccessMode == access_mode::write || AccessMode == access_mode::discard_write ? cl::sycl::access_mode::write : cl::sycl::access_mode::read_write)
    >
{
public:

    using accessor_type = cl::sycl::accessor<
        std::conditional_t<AccessMode != access_mode::read, std::remove_pointer_t<T>, std::add_const_t<std::remove_pointer_t<T>>>,
        std::is_pointer_v<T> ? 2 : 1,
        (AccessMode == access_mode::read) ? cl::sycl::access_mode::read : (AccessMode == access_mode::write || AccessMode == access_mode::discard_write ? cl::sycl::access_mode::write : cl::sycl::access_mode::read_write)>;
    using element_type = typename accessor_type::value_type;
    using iterator = std::add_pointer_t<element_type>;
    using const_iterator = std::add_pointer_t<element_type>;
    using size_type = typename accessor_type::size_type;

    // Indicates whether buffer is treated as 2- or 1-dimensional.
    constexpr static bool ComponentWise = std::is_pointer_v<T>;

    using BufferPointer = std::conditional_t<AccessMode != access_mode::read, DataBuffer*, const DataBuffer*>;

    /// Constructor that initializes the accessor in a null state, i.e. not associated with any underlying buffer.
    SyclBufferAccessTyped() noexcept = default;

    /// Constructor that associates the access object with a buffer object (reference may be null).
    SyclBufferAccessTyped(
        BufferPointer buffer,
        cl::sycl::handler& commandGroupHandlerRef,
        const cl::sycl::property_list& propList =
            (AccessMode == access_mode::discard_write || AccessMode == access_mode::discard_read_write) ? cl::sycl::property_list{cl::sycl::no_init} : cl::sycl::property_list{}) : accessor_type() {
        OVITO_ASSERT(!buffer || buffer->size() == 0 || buffer->_data->get_range()[0] / buffer->stride() >= buffer->size());
        OVITO_ASSERT(!buffer || buffer->stride() == sizeof(element_type) * (ComponentWise ? buffer->componentCount() : 1));
        OVITO_ASSERT(!buffer || buffer->dataType() == DataBufferPrimitiveType<std::remove_cv_t<element_type>>::value);
        OVITO_ASSERT(!buffer || buffer->dataTypeSize() == sizeof(element_type) / (ComponentWise ? 1 : buffer->componentCount()));
        if(buffer && buffer->_data) {
#ifdef OVITO_DEBUG
            OVITO_ASSERT(buffer->_isDataInitialized || propList.has_property<cl::sycl::property::no_init>());
            if constexpr(AccessMode != access_mode::read) {
                buffer->_isDataInitialized = true;
            }
#endif

            if constexpr(!ComponentWise) {
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 1>();
                *this = accessor_type{typedBuffer, commandGroupHandlerRef, cl::sycl::range(buffer->size()), propList};
            }
            else {
                size_t capacity = buffer->_data->get_range()[0] / buffer->stride();
                auto typedBuffer = buffer->_data->template reinterpret<element_type, 2>(cl::sycl::range(capacity, buffer->componentCount()));
                *this = accessor_type{typedBuffer, commandGroupHandlerRef, cl::sycl::range(buffer->size(), buffer->componentCount()), propList};
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
};

} // End of namespace detail.

template<typename T, access_mode AccessMode>
using SyclBufferAccess = detail::SyclBufferAccessTyped<T, AccessMode>;

}   // End of namespace

#endif