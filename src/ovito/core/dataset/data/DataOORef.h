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
#include <ovito/core/oo/OORef.h>
#include <ovito/core/oo/CloneHelper.h>

namespace Ovito {

/**
 * \brief Strong object smart-pointer to a DataObject, which ensures that the DataObject is not being modified
 *        while being referenced by multiple pointers.
 */
template<typename T>
class DataOORef
{
private:

    /// The internal smart-pointer to the DataObject, which keeps the object instance alive.
    OORef<T> _ref;

	template<class U> friend class DataOORef;

public:

    using element_type = T;

    /// Default constructor.
#ifndef MSVC
    DataOORef() noexcept = default;
#else
    DataOORef() noexcept {}
#endif

    /// Null pointer constructor.
    DataOORef(std::nullptr_t) noexcept {}

    /// Initialization constructor.
    DataOORef(const T* p) noexcept : _ref(p) {
        if(_ref) _ref->incrementDataReferenceCount();
    }

    /// Copy constructor.
    DataOORef(const DataOORef& rhs) noexcept : _ref(rhs._ref) {
        if(_ref) _ref->incrementDataReferenceCount();
    }

    /// Conversion constructor.
    template<class U>
    DataOORef(const DataOORef<U>& rhs) noexcept : _ref(rhs._ref) {
        if(_ref) _ref->incrementDataReferenceCount();
    }

    /// Move constructor.
    DataOORef(DataOORef&& rhs) noexcept : _ref(std::move(rhs._ref)) {
        OVITO_ASSERT(!rhs._ref);
    }

    /// Move and conversion constructor from a OORef.
    template<class U>
    DataOORef(OORef<U>&& rhs) noexcept : _ref(std::move(rhs)) {
        if(_ref) _ref->incrementDataReferenceCount();
    }

    /// Move and conversion constructor.
    template<class U>
    DataOORef(DataOORef<U>&& rhs) noexcept : _ref(std::move(rhs._ref)) {
        OVITO_ASSERT(!rhs._ref);
    }

    /// Destructor.
    ~DataOORef() {
        if(_ref) _ref->decrementDataReferenceCount();
    }

    /// Copy assignment operator.
    DataOORef& operator=(T* rhs) {
    	DataOORef(rhs).swap(*this);
    	return *this;
    }

    /// Copy assignment operator.
    DataOORef& operator=(const DataOORef& rhs) {
    	DataOORef(rhs).swap(*this);
    	return *this;
    }

    /// Copy assignment and conversion operator.
    template<class U>
    DataOORef& operator=(const DataOORef<U>& rhs) {
    	DataOORef(rhs).swap(*this);
    	return *this;
    }

    /// Move assignment operator.
    DataOORef& operator=(DataOORef&& rhs) noexcept {
    	DataOORef(std::move(rhs)).swap(*this);
    	return *this;
    }

    /// Move assignment and conversion operator.
    template<class U>
    DataOORef& operator=(DataOORef<U>&& rhs) noexcept {
    	DataOORef(std::move(rhs)).swap(*this);
    	return *this;
    }

    /// Move assignment operator with OORef.
    template<class U>
    DataOORef& operator=(OORef<U>&& rhs) noexcept {
    	DataOORef(std::move(rhs)).swap(*this);
    	return *this;
    }

    void reset() {
    	DataOORef().swap(*this);
    }

    void reset(T* rhs) {
    	DataOORef(rhs).swap(*this);
    }

    inline T* get() const noexcept {
    	return _ref.get();
    }

    inline operator T*() const noexcept {
    	return _ref.get();
    }

    inline T& operator*() const noexcept {
    	return *_ref;
    }

    inline T* operator->() const noexcept {
    	OVITO_ASSERT(_ref);
    	return _ref.get();
    }

    inline void swap(DataOORef& rhs) noexcept {
    	_ref.swap(rhs._ref);
    }

    /// Factory method instantiating a new data object and returning a smart-pointer to it.
    template<typename... Args>
	static DataOORef create(Args&&... args) {
		return DataOORef(OORef<T>::create(std::forward<Args>(args)...));
	}

    /// Returns a copy of the data object, which can be safely modified.
    DataOORef<std::remove_const_t<T>> makeCopy() const {
        return CloneHelper().cloneObject(_ref, false);
    }

    /// Turns a const data object reference into a mutable data object reference. 
    /// Makes a copy of the data object if necessary.
    DataOORef<std::remove_const_t<T>> makeMutable() && {
        if(!this->_ref) {
            return {};
        }
        else if(this->_ref->isSafeToModify()) {
            // Transfer the internal OORef from the const input into a non-const output (without incrementing/descrementing the data object's ref count).
            DataOORef<std::remove_const_t<T>> result;
            result._ref = const_pointer_cast<std::remove_const_t<T>>(std::move(this->_ref));
            OVITO_ASSERT(!this->_ref);
            OVITO_ASSERT(result->isSafeToModify());
            return result;
        }
        else {
            return makeCopy();
        }
    }

    /// Makes a shallow copy of a data object.
    static DataOORef<std::remove_const_t<T>> makeCopy(const T* obj) {
        return CloneHelper().cloneObject(obj, false);
    }

    /// Makes a deep copy of a data object and its children.
    static DataOORef<std::remove_const_t<T>> makeDeepCopy(const T* obj) {
        return CloneHelper().cloneObject(obj, true);
    }

    template<class T2, class U> friend DataOORef<T2> static_pointer_cast(DataOORef<U>&& p) noexcept;
    template<class T2, class U> friend DataOORef<T2> dynamic_pointer_cast(DataOORef<U>&& p) noexcept;
};

template<class T> T* get_pointer(const DataOORef<T>& p) noexcept
{
    return p.get();
}

template<class T> void swap(DataOORef<T>& lhs, DataOORef<T>& rhs) noexcept
{
	lhs.swap(rhs);
}

template<class T, class U> DataOORef<T> static_pointer_cast(const DataOORef<U>& p) noexcept
{
    return static_cast<T*>(p.get());
}

template<class T, class U> DataOORef<T> static_pointer_cast(DataOORef<U>&& p) noexcept
{
    return static_pointer_cast<T>(std::move(p._ref));
}

template<class T, class U> DataOORef<T> const_pointer_cast(const DataOORef<U>& p) noexcept
{
    return const_cast<T*>(p.get());
}

template<class T, class U> DataOORef<T> dynamic_pointer_cast(const DataOORef<U>& p) noexcept
{
    return qobject_cast<T*>(p.get());
}

template<class T, class U> DataOORef<T> dynamic_pointer_cast(DataOORef<U>&& p) noexcept
{
    return dynamic_pointer_cast<T>(std::move(p._ref));
}

template<class T> QDebug operator<<(QDebug debug, const DataOORef<T>& p)
{
	return debug << p.get();
}

template<class T, class U> inline bool operator==(const DataOORef<T>& a, const DataOORef<U>& b) noexcept
{
    return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(const DataOORef<T>& a, const DataOORef<U>& b) noexcept
{
    return a.get() != b.get();
}

template<class T, class U> inline bool operator==(const DataOORef<T>& a, U* b) noexcept
{
    return a.get() == b;
}

template<class T, class U> inline bool operator!=(const DataOORef<T>& a, U* b) noexcept
{
    return a.get() != b;
}

template<class T, class U> inline bool operator==(T* a, const DataOORef<U>& b) noexcept
{
    return a == b.get();
}

template<class T, class U> inline bool operator!=(T* a, const DataOORef<U>& b) noexcept
{
    return a != b.get();
}

template<class T> inline bool operator==(const DataOORef<T>& p, std::nullptr_t) noexcept
{
    return p.get() == nullptr;
}

template<class T> inline bool operator==(std::nullptr_t, const DataOORef<T>& p) noexcept
{
    return p.get() == nullptr;
}

template<class T> inline bool operator!=(const DataOORef<T>& p, std::nullptr_t) noexcept
{
    return p.get() != nullptr;
}

template<class T> inline bool operator!=(std::nullptr_t, const DataOORef<T>& p) noexcept
{
    return p.get() != nullptr;
}

template<class T> inline bool operator<(const DataOORef<T>& a, const DataOORef<T>& b) noexcept
{
    return std::less<T*>()(a.get(), b.get());
}

}	// End of namespace
