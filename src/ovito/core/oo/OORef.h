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
#include <ovito/core/utilities/concurrent/ExecutionContext.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/// Flags which may be passed to constructors of RefTarget-derived classes.
enum ObjectInitializationFlag
{
    NoFlags               = 0,        //< Selects standard object initialization behavior.
    DontInitializeObject  = (1 << 0), //< Indicates that an object is being cloned or deserialized from a file stream. Means do not initialize parameter values and don't create child objects.
    DontCreateVisElement  = (1 << 1), //< Do not automatically attach a visual element when creating a new data object.
};
Q_DECLARE_FLAGS(ObjectInitializationFlags, ObjectInitializationFlag);

/**
 * A custom allocator for OvitoObject-derived classes, which works
 * in conjunction with the OORef smart-pointer class.
 */
template<typename T>
class OOAllocator : public std::allocator<T>
{
public:
    template<typename U>
    void destroy(U* obj) {
        OVITO_ASSERT(obj);
        obj->deleteObjectInternal();
        obj->~U();
    }
};

/**
 * A weak reference to an OvitoObject.
*/
template<class T>
using OOWeakRef = std::weak_ptr<T>;

/**
 * \brief A smart-pointer reference to an OvitoObject.
 *
 * This smart-pointer class takes care of incrementing and decrementing
 * the reference counter of the object it is pointing to. As soon as no
 * OORef pointer to an object instance is left, the referenced object is
 * automatically freed.
 */
template<class T>
class OORef : public std::shared_ptr<T>
{
private:

    using this_type = OORef;
    using base_type = std::shared_ptr<T>;

    template<class U> friend class OORef;
    template<class T2, class U> OORef<T2> friend static_pointer_cast(OORef<U>&& p) noexcept;
    template<class T2, class U> OORef<T2> friend dynamic_pointer_cast(OORef<U>&& p) noexcept;
    template<class T2, class U> OORef<T2> friend const_pointer_cast(OORef<U>&& p) noexcept;

public:

    /// Default constructor.
    OORef() noexcept = default;

    /// Null constructor.
    OORef(std::nullptr_t) noexcept : base_type{nullptr} {}

    /// Construction from raw object pointer.
    template<typename U>
    OORef(const U* p) noexcept : base_type{p ? std::static_pointer_cast<T>(const_cast<U*>(p)->shared_from_this()) : base_type{}} {
        OVITO_ASSERT(!p || p->_isAllocatedOnTheHeap);
    }

    /// Construction from shared pointer.
    OORef(base_type&& p) noexcept : base_type{std::move(p)} {
        OVITO_ASSERT(!base_type::get() || base_type::get()->_isAllocatedOnTheHeap);
    }

    /// Copy constructor.
    OORef(const OORef& rhs) noexcept : base_type{rhs} {}

    /// Copy and conversion constructor.
    template<class U>
    OORef(const OORef<U>& rhs) noexcept : base_type{rhs} {}

    /// Move constructor.
    OORef(OORef&& rhs) noexcept : base_type{std::move(rhs)} {}

    /// Move and conversion constructor.
    template<class U>
    OORef(OORef<U>&& rhs) noexcept : base_type{std::move(rhs)} {}

    /// Allow implicit conversion from smart pointer to raw object pointer.
    inline operator T*() const noexcept {
        return base_type::get();
    }

    template<class U>
    inline OORef& operator=(const OORef<U>& rhs) {
        base_type::operator=(rhs);
        return *this;
    }

    inline OORef& operator=(const OORef& rhs) {
        base_type::operator=(rhs);
        return *this;
    }

    inline OORef& operator=(OORef&& rhs) noexcept {
        base_type::operator=(std::move(rhs));
        return *this;
    }

    template<class U>
    inline OORef& operator=(OORef<U>&& rhs) noexcept {
        base_type::operator=(std::move(rhs));
        return *this;
    }

    inline OORef& operator=(const T* rhs) {
        return *this = OORef(rhs);
    }

    inline void swap(OORef& rhs) noexcept {
        base_type::swap(rhs);
    }

    /// Factory method that instantiates and initializes a new object (any RefTarget derived class).
    template<typename... Args>
    static this_type create(ObjectInitializationFlags flags, Args&&... args) {
        using OType = std::remove_const_t<T>;
        static_assert(std::is_base_of_v<Ovito::RefTarget, OType>, "Object class must be a RefTarget derived class");
        // Don't record object initialization on the undo stack.
        UndoSuspender noUndo;
#ifndef OVITO_DEBUG
        OORef<OType> obj(new OType(flags, std::forward<Args>(args)...));
#else
        OType* obj_ = new OType(flags, std::forward<Args>(args)...);
        // Mark the object as having been allocated on the heap before moving it into the OORef<>.
        obj_->_isAllocatedOnTheHeap = true;
        OORef<OType> obj(obj_);
#endif
        if(ExecutionContext::isInteractive())
            obj->initializeParametersToUserDefaults();
        return obj;
    }

    /// Factory method that instantiates a new object.
    template<typename... Args>
    static this_type create(ObjectInitializationFlag extraFlag, Args&&... args) {
        return create(ObjectInitializationFlags(extraFlag), std::forward<Args>(args)...);
    }

    /// Factory method that instantiates a new object.
    template<typename... Args>
    static this_type create(Args&&... args) {
        using OType = std::remove_const_t<T>;
        if constexpr(std::is_base_of_v<Ovito::RefTarget, OType>) {
            // All RefTarget derived classes expect initialization flags.
            return create(ObjectInitializationFlag::NoFlags, std::forward<Args>(args)...);
        }
        else {
            // Objects not derived from RefTarget do not expect initialization flags.
#ifndef OVITO_DEBUG
            return new OType(std::forward<Args>(args)...);
#else
            OType* obj = new OType(std::forward<Args>(args)...);
            // Mark the object as having been allocated on the heap before moving it into the OORef<>.
            obj->_isAllocatedOnTheHeap = true;
            return obj;
#endif
        }
    }

    /// Internal factory method, which is only used to implement OvitoClass::createInstance().
    static this_type createInstanceInternal(ObjectInitializationFlags flags) {
        if constexpr(std::is_base_of_v<Ovito::RefTarget, std::remove_const_t<T>>)
            return create(flags);
        else
            return create();
    }
};

#if 0
template<class T, class U> inline bool operator==(const OORef<T>& a, const OORef<U>& b) noexcept
{
    return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(const OORef<T>& a, const OORef<U>& b) noexcept
{
    return a.get() != b.get();
}

template<class T, class U> inline bool operator==(const OORef<T>& a, U* b) noexcept
{
    return a.get() == b;
}

template<class T, class U> inline bool operator!=(const OORef<T>& a, U* b) noexcept
{
    return a.get() != b;
}

template<class T, class U> inline bool operator==(T* a, const OORef<U>& b) noexcept
{
    return a == b.get();
}

template<class T, class U> inline bool operator!=(T* a, const OORef<U>& b) noexcept
{
    return a != b.get();
}

template<class T> inline bool operator==(const OORef<T>& p, std::nullptr_t) noexcept
{
    return p.get() == nullptr;
}

template<class T> inline bool operator==(std::nullptr_t, const OORef<T>& p) noexcept
{
    return p.get() == nullptr;
}

template<class T> inline bool operator!=(const OORef<T>& p, std::nullptr_t) noexcept
{
    return p.get() != nullptr;
}

template<class T> inline bool operator!=(std::nullptr_t, const OORef<T>& p) noexcept
{
    return p.get() != nullptr;
}

template<class T> inline bool operator<(const OORef<T>& a, const OORef<T>& b) noexcept
{
    return std::less<T*>()(a.get(), b.get());
}

template<class T> void swap(OORef<T>& lhs, OORef<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<class T> T* get_pointer(const OORef<T>& p) noexcept
{
    return p.get();
}
#endif

template<class T, class U> OORef<T> static_pointer_cast(const OORef<U>& p) noexcept
{
    return std::static_pointer_cast<T, U>(static_cast<const std::shared_ptr<U>&>(p));
}

template<class T, class U> OORef<T> static_pointer_cast(OORef<U>&& p) noexcept
{
    return std::static_pointer_cast<T, U>(static_cast<std::shared_ptr<U>&&>(p));
}

template<class T, class U> OORef<T> const_pointer_cast(const OORef<U>& p) noexcept
{
    return std::const_pointer_cast<T, U>(static_cast<const std::shared_ptr<U>&>(p));
}

template<class T, class U> OORef<T> const_pointer_cast(OORef<U>&& p) noexcept
{
    return std::const_pointer_cast<T, U>(static_cast<std::shared_ptr<U>&&>(p));
}

template<class T, class U> OORef<T> dynamic_pointer_cast(const OORef<U>& p) noexcept
{
    return std::dynamic_pointer_cast<T, U>(static_cast<const std::shared_ptr<U>&>(p));
}

template<class T, class U> OORef<T> dynamic_pointer_cast(OORef<U>&& p) noexcept
{
    return std::dynamic_pointer_cast<T, U>(static_cast<std::shared_ptr<U>&&>(p));
}

template<class T> QDebug operator<<(QDebug debug, const OORef<T>& p)
{
    return debug << p.get();
}

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::ObjectInitializationFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Ovito::ObjectInitializationFlags);
