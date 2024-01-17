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
    SyclFlatMap(const Container& container) {
        if(!container.empty()) {
            _storage.emplace(detail::allocateSyclBuffer<value_type, 1>(container.size()));
            // Copy the contents of the provided container into our internal flat storage.
            sycl::host_accessor acc(*_storage, sycl::write_only, sycl::no_init);
            std::copy(container.begin(), container.end(), acc.begin());
            OVITO_ASSERT(std::is_sorted(acc.begin(), acc.end(), [](const value_type& a, const value_type& b) { return key_compare{}(a.first, b.first); }));
        }
    }

    /// Copying is not supported yet.
    SyclFlatMap(const SyclFlatMap& other) = delete;

    /// Copy assignment is not supported yet.
    SyclFlatMap& operator=(const SyclFlatMap& other) = delete;

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

public:

    /**
     * An accessor to the SyclFlatMap container that that allows to perform map look-ups inside SYCL kernels.
    */
    class Accessor
    {
    public:

        using container_type = SyclFlatMap;
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<Key, T>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using key_compare = Compare;
        using accessor_type = sycl::accessor<value_type, 1, sycl::access_mode::read>;

        // Constructor.
        Accessor(const container_type& map, sycl::handler& commandGroupHandler)
            : _accessor(!map.empty() ? accessor_type{const_cast<container_type&>(map).storage(), commandGroupHandler} : accessor_type{}) {}

    public:

        /// Returns whether this accessor is valid.
        inline explicit operator bool() const noexcept { return _accessor.get_range()[0] != 0; }

        /// Returns an iterator to the beginning of the map.
        auto begin() const noexcept { return _accessor.cbegin(); }

        /// Returns an iterator to the end of the map.
        auto end() const noexcept { return _accessor.cend(); }

        /// Finds an element with a specific key in the map.
        auto find(const key_type& key) const noexcept {
            auto first = begin();
            auto last = end();
            auto comp = key_compare{};
            auto key_comp = [&](const value_type& value, const key_type& key) { return comp(value.first, key); };
            first = std::lower_bound(first, last, key, key_comp);
            if(first != last && comp(key, (*first).first))
                first = last;
            return first;
        }

        /// Looks up the value associated with the given key. If the key is not found, a default value is returned.
        auto get(const key_type& key, const mapped_type defaultValue = mapped_type{}) const noexcept {
            auto iter = find(key);
            if(iter != end())
                return (*iter).second;
            else
                return defaultValue;
        }

    private:

        /// The internal SYCL buffer accessor.
        accessor_type _accessor;
    };

    /// Creates an accessor that can be used inside a SYCL kernel to access the contents of the map.
    Accessor get_access(sycl::handler& commandGroupHandler) const {
        return Accessor(*this, commandGroupHandler);
    }

private:

    /// The data storage.
    std::optional<sycl::buffer<value_type>> _storage;
};

// Class template deduction guide (CTAD):
template<class Container>
SyclFlatMap(const Container& container) -> SyclFlatMap<typename Container::key_type, typename Container::mapped_type, typename Container::key_compare>;
template<typename Key, typename T>
SyclFlatMap(const QMap<Key, T>& container) -> SyclFlatMap<Key, T>;

}   // End of namespace

#endif