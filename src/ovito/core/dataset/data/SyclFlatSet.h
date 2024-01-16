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
#include <ovito/core/dataset/data/DataBuffer.h>

#ifdef OVITO_USE_SYCL

namespace Ovito {

/**
 * An associative container that can be accessed in host code and in SYCL kernels.
 * It uses a SYCL buffer as underlying storage.
*/
template<typename Key, typename Compare = std::less<Key>>
class SyclFlatSet
{
public:

    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;

    /// Constructor that initializes the set from an existing associative container, e.g. a std::set.
    template<typename Container>
    SyclFlatSet(const Container& container) {
        if(!container.empty()) {
            _storage.emplace(detail::allocateSyclBuffer<value_type, 1>(container.size()));
            // Copy the contents of the provided container into our internal flat storage.
            sycl::host_accessor acc(*_storage, sycl::write_only, sycl::no_init);
            std::copy(container.begin(), container.end(), acc.begin());
        }
    }

    /// Copying is not supported yet.
    SyclFlatSet(const SyclFlatSet& other) = delete;

    /// Copy assignment is not supported yet.
    SyclFlatSet& operator=(const SyclFlatSet& other) = delete;

    /// Checks if the map is empty.
    bool empty() const { return !_storage.has_value(); }

    /// Returns the number of key-value pairs in the map.
    size_type size() const { return _storage ? _storage->size() : 0; }

    /// Returns the internal data storage of the container.
    const sycl::buffer<value_type>& storage() const {
        OVITO_ASSERT(_storage.has_value());
        return *_storage;
    }

    /// Returns the internal data storage of the container.
    sycl::buffer<value_type>& storage() {
        OVITO_ASSERT(_storage.has_value());
        return *_storage;
    }

private:

    /// The data storage.
    std::optional<sycl::buffer<value_type>> _storage;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatSet(const Container& container) -> SyclFlatSet<typename Container::key_type, typename Container::key_compare>;
template<typename Key>
SyclFlatSet(const QSet<Key>& container) -> SyclFlatSet<Key>;

/**
 * An accessor to the SyclFlatSet container that that allows to perform look-ups inside SYCL kernels.
*/
template<typename Key, typename Compare = std::less<Key>>
class SyclFlatSetAccessor
{
public:

    using container_type = SyclFlatSet<Key, Compare>;
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;
    using accessor_type = sycl::accessor<value_type, 1, sycl::access_mode::read>;

    // Constructor.
    SyclFlatSetAccessor(const container_type& set, sycl::handler& commandGroupHandler) :
        _accessor(!set.empty() ? accessor_type{const_cast<container_type&>(set).storage(), commandGroupHandler} : accessor_type{}) {}

public:

    /// Returns whether this accessor is valid.
    inline explicit operator bool() const noexcept { return _accessor.get_range()[0] != 0; }

    /// Returns an iterator to the beginning of the set.
    auto begin() const { return _accessor.cbegin(); }

    /// Returns an iterator to the end of the set.
    auto end() const { return _accessor.cend(); }

    /// Finds a specific key in the set.
    auto find(const key_type& key) const {
        auto first = begin();
        auto last = end();
        first = std::lower_bound(first, last, key, key_compare{});
        if(first != last && key_compare{}(key, *first))
            first = last;
        return first;
    }

    /// Checks if the given key is in the set.
    bool contains(const key_type& key) const {
        return find(key) != end();
    }

    /// Finds the index of a specific key in the set.
    auto index_of(const key_type& key) const {
        return find(key) - begin();
    }

    /// Returns the number of keys in the set.
    size_type size() const { return _accessor.get_range()[0]; }

private:

    /// The internal SYCL buffer accessor.
    accessor_type _accessor;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatSetAccessor(const Container& set, sycl::handler& commandGroupHandler) -> SyclFlatSetAccessor<typename Container::key_type, typename Container::key_compare>;

}   // End of namespace

#endif