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

#ifdef OVITO_USE_SYCL

namespace Ovito {

/**
 * An associative container that can be accessed in host code and in SYCL kernels.
 * It uses a SYCL buffer as underlying storage.
*/
template<typename Key, typename T, typename Compare = std::less<Key>>
class SyclFlatMap
{
public:

    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;

    /// Constructor that initializes the map from an existing associative container, e.g. a std::map.
    template<typename Container>
    SyclFlatMap(const Container& container) : _storage(container.size()) {
        // Copy the contents of the provided container into our internal flat storage.
        sycl::host_accessor acc(_storage, sycl::write_only, sycl::no_init);
        std::copy(container.begin(), container.end(), acc.begin());
    }

    /// Copying is not supported yet.
    SyclFlatMap(const SyclFlatMap& other) = delete;

    /// Copy assignment is not supported yet.
    SyclFlatMap& operator=(const SyclFlatMap& other) = delete;

    /// Checks if the map is empty.
    bool empty() const { return _storage.empty(); }

    /// Returns the number of key-value pairs in the map.
    size_type size() const { return _storage.size(); }

    /// Returns the internal data storage of the container.
    const sycl::buffer<value_type>& storage() const { return _storage; }

    /// Returns the internal data storage of the container.
    sycl::buffer<value_type>& storage() { return _storage; }

private:

    /// The data storage.
    sycl::buffer<value_type> _storage;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatMap(const Container& container) -> SyclFlatMap<typename Container::key_type, typename Container::mapped_type, typename Container::key_compare>;

/**
 * An accessor to the SyclFlatMap container that that allows to perform map look-ups inside SYCL kernels.
*/
template<typename Key, typename T, typename Compare = std::less<Key>>
class SyclFlatMapAccessor
{
public:

    using container_type = SyclFlatMap<Key, T, Compare>;
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;

    // Constructor.
    SyclFlatMapAccessor(const container_type& map, sycl::handler& commandGroupHandler) : _accessor(const_cast<container_type&>(map).storage(), commandGroupHandler) {}

public:

    /// Returns an iterator to the beginning of the map.
    auto begin() const { return _accessor.cbegin(); }

    /// Returns an iterator to the end of the map.
    auto end() const { return _accessor.cend(); }

    /// Finds an element with a specific key in the map.
    auto find(const key_type& key) const {
        auto first = begin();
        auto last = end();
        auto key_comp = [comp = key_compare{}](const value_type& value, const key_type& key) { return comp(value.first, key); };
        first = std::lower_bound(first, last, key, key_comp);
        if(first != last && key_comp(*first, key))
            first = last;
        return first;
    }

private:

    /// The internal SYCL buffer accessor.
    sycl::accessor<value_type, 1, sycl::access_mode::read> _accessor;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatMapAccessor(const Container& map, sycl::handler& commandGroupHandler) -> SyclFlatMapAccessor<typename Container::key_type, typename Container::mapped_type, typename Container::key_compare>;

}   // End of namespace

#endif