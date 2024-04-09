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
#include "DataObject.h"

namespace Ovito {

/**
 * \brief Utility class that is used to reference a particular data object in a DataCollection
 *        as a path through the hierarchy of nested data objects.
 */
template<typename DataObjectPtr>
class OVITO_CORE_EXPORT DataObjectPathTemplate
{
private:

    using storage_type = QVarLengthArray<DataObjectPtr, 3>;
    storage_type _array;

public:

    using size_type = typename storage_type::size_type;
    using value_type = typename storage_type::value_type;
    using reference = typename storage_type::reference;
    using const_reference = typename storage_type::const_reference;
    using iterator = typename storage_type::iterator;
    using const_iterator = typename storage_type::const_iterator;
    using pointer = typename storage_type::pointer;
    using const_pointer = typename storage_type::const_pointer;

    /// Constructors.
    DataObjectPathTemplate() = default;
    DataObjectPathTemplate(DataObjectPathTemplate&& other) = default;
    DataObjectPathTemplate(const DataObjectPathTemplate& other) = default;
    DataObjectPathTemplate(size_type size) : _array(size) {}
    DataObjectPathTemplate(std::initializer_list<value_type> list) : _array(list) {}
    template<typename InputIterator> DataObjectPathTemplate(InputIterator first, InputIterator last) : _array(first, last) {}

    // Assignment
    DataObjectPathTemplate& operator=(DataObjectPathTemplate&& other) = default;
    DataObjectPathTemplate& operator=(const DataObjectPathTemplate& other) = default;

    // Element access:
    DataObjectPtr& back() noexcept { return _array.back(); }
    const DataObjectPtr& back() const noexcept { return _array.back(); }
    DataObjectPtr& front() noexcept { return _array.front(); }
    const DataObjectPtr& front() const noexcept { return _array.front(); }
    auto begin() { return _array.begin(); }
    auto begin() const { return _array.begin(); }
    auto end() { return _array.end(); }
    auto end() const { return _array.end(); }
    DataObjectPtr& operator[](size_type i) { return _array[i]; }
    const DataObjectPtr& operator[](size_type i) const { return _array[i]; }

    // Queries:
    auto empty() const noexcept { return _array.empty(); }
    auto size() const noexcept { return _array.size(); }

    // Adding/removing elements:
    void push_back(const DataObjectPtr& t) { _array.push_back(t); }
    void push_back(DataObjectPtr&& t) { _array.push_back(std::move(t)); }
    void pop_back() { _array.pop_back(); }
    void resize(size_type size) { _array.resize(size); }
    void clear() { _array.clear(); }

    /// Converts the path to a string representation.
    QString toString() const {
        QString s;
        for(const auto& o : *this) {
            if(!s.isEmpty()) s += QChar('/');
            s += o->identifier();
        }
        return s;
    }

    /// Returns a string representation of the object path that is suitable for display in the user interface.
    template<typename T = DataObjectPtr>
    std::enable_if_t<std::is_same_v<T, const DataObject*>, QString> toUIString() const {
        if(this->empty()) return {};
        return this->back()->getOOMetaClass().formatDataObjectPath(*this);
    }

    /// Implicit conversion from DataObjectPath to ConstDataObjectPath.
    template<typename T = DataObjectPtr>
    operator std::enable_if_t<std::is_same_v<T, DataObject*>, const ConstDataObjectPath&>() const {
        return *reinterpret_cast<const ConstDataObjectPath*>(this);
    }

    /// Returns a data object path that includes all but the last data object from this path.
    DataObjectPathTemplate parentPath() const {
        return this->empty() ? DataObjectPathTemplate{} : DataObjectPathTemplate{this->begin(), std::prev(this->end())};
    }

    /// Returns the n-th to last data object in the path - or null if the path is shorter than requested.
    auto last(size_type n = 0) const {
        return this->size() <= n ? nullptr : to_address((*this)[this->size() - n - 1]);
    }

    /// Returns the n-th to last data object in the path if it's a specific kind of object - or null if the path is shorter than requested.
    template<class DataObjectType>
    auto lastAs(size_type n = 0) const {
        return this->size() <= n ? nullptr : dynamic_object_cast<DataObjectType>(to_address((*this)[this->size() - n - 1]));
    }

private:

    /// Obtains the object address represented by a fancy pointer.
    template<class U> static constexpr U* to_address(U* p) noexcept { return p; }
    template<class U> static constexpr auto to_address(const U& p) noexcept { return p.get(); }
};

}   // End of namespace
