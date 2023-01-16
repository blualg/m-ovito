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
#include <boost/iterator/iterator_facade.hpp>

namespace Ovito {

namespace detail {

    template<typename KeyRange, typename ValueRange>
    struct zipped_val
    {
        using key_iterator = typename std::decay_t<KeyRange>::iterator;
        using value_iterator = typename std::decay_t<ValueRange>::iterator;
        typename std::iterator_traits<key_iterator>::value_type key;
        typename std::iterator_traits<value_iterator>::value_type value;
    };

    template<typename KeyRange, typename ValueRange>
    struct zipped_ref
    {
        using key_iterator = typename std::decay_t<KeyRange>::iterator;
        using value_iterator = typename std::decay_t<ValueRange>::iterator;
        typename std::iterator_traits<key_iterator>::value_type* key;
        typename std::iterator_traits<value_iterator>::value_type* value;

        zipped_ref& operator=(zipped_ref&& r) {
            *key = std::move(*r.key);
            *value = std::move(*r.value);
            return *this;
        }

        zipped_ref& operator=(const zipped_ref& r) = delete;

        zipped_ref& operator=(zipped_val<KeyRange, ValueRange>&& r) {
            *key = std::move(r.key);
            *value = std::move(r.value);
            return *this;
        }

        friend void swap(const zipped_ref& a, const zipped_ref& b) {
            using std::swap;
            swap(*a.key, *b.key);
            swap(*a.value, *b.value);
        }

        operator zipped_val<KeyRange, ValueRange>() && { return { std::move(*key), std::move(*value) }; }
    };

    template<typename KeyRange, typename ValueRange>
    struct zip_comparator 
    {
        bool operator()(const zipped_ref<KeyRange, ValueRange>& a, const zipped_val<KeyRange, ValueRange>& b) { return *a.key < b.key; }
        bool operator()(const zipped_val<KeyRange, ValueRange>& a, const zipped_ref<KeyRange, ValueRange>& b) { return a.key < *b.key; }
        bool operator()(const zipped_ref<KeyRange, ValueRange>& a, const zipped_ref<KeyRange, ValueRange>& b) { return *a.key < *b.key; }
    };
}

/**
 * Utility function that sorts two separate ranges based on the values in the first range (the sort keys).
 */
template<typename KeyRange, typename ValueRange>
void sort_zipped(KeyRange&& keys, ValueRange&& values)
{
    OVITO_ASSERT(std::size(keys) == std::size(values));

    using zipped_val = detail::zipped_val<KeyRange, ValueRange>;
    using zipped_ref = detail::zipped_ref<KeyRange, ValueRange>;

    struct sort_it : public boost::iterator_facade<
            sort_it, // Derived 
            zipped_val, // Value
            std::random_access_iterator_tag, // CategoryOrTraversal
            zipped_ref> // Reference
    {
        using difference_type = typename boost::iterator_facade<sort_it, zipped_val, std::random_access_iterator_tag, zipped_ref>::difference_type;
        using key_iterator = typename std::decay_t<KeyRange>::iterator;
        using value_iterator = typename std::decay_t<ValueRange>::iterator;

        key_iterator key;
        value_iterator value;

        sort_it(key_iterator&& k, value_iterator&& v) noexcept : key(std::move(k)), value(std::move(v)) {}

        void increment() noexcept {
            ++key;
            ++value;
        }

        void decrement() noexcept {
            --key;
            --value;
        }

        void advance(difference_type n) noexcept {
            std::advance(key, n);
            std::advance(value, n);
        }

        auto distance_to(const sort_it& other) const noexcept {
            return std::distance(this->key, other.key);
        }

        bool equal(const sort_it& other) const noexcept {
            return this->key == other.key;
        }

        zipped_ref dereference() const noexcept { return zipped_ref{ std::addressof(*key), std::addressof(*value) }; }
    };

    std::sort(
        sort_it{std::begin(keys), std::begin(values)}, 
        sort_it{std::end(keys), std::end(values)},
        detail::zip_comparator<KeyRange, ValueRange>{});
}

}   // End of namespace
