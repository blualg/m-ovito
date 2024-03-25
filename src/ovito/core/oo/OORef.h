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
 *
 * This allocator behaves just like the C++ std::allocator, but calls the virtual
 * OvitoObject::deleteObjectInternal() method before it destroys an object.
 */
template<typename T>
struct OOAllocator : public std::allocator<T>
{
    using base_allocator = std::allocator<T>;
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = size_t;

    OOAllocator() noexcept = default;

    template<typename U>
    OOAllocator(const OOAllocator<U>&) noexcept {}

    template <typename U>
    struct rebind {
        using other = OOAllocator<U>;
    };

    pointer allocate(size_type n, const void* hint = nullptr) {
        return std::allocator_traits<base_allocator>::allocate(*this, n, hint);
    }

    void deallocate(pointer p, size_type n) {
        std::allocator_traits<base_allocator>::deallocate(*this, p, n);
    }

    template<typename U>
    void destroy(U* p) {
        p->deleteObjectInternal(); // Let the object perform last work before its destruction.
        std::allocator_traits<base_allocator>::destroy(*this, p);
    }
};

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

public:

    /// Default constructor.
    OORef() noexcept = default;

    /// Null constructor.
    OORef(std::nullptr_t) noexcept : base_type{nullptr} {}

    /// Construction from raw pointer to an OvitoObject (may be null).
    template<typename U>
    OORef(const U* p) noexcept : base_type{p ? std::static_pointer_cast<T>(const_cast<U*>(p)->shared_from_this()) : base_type{}} {
        OVITO_ASSERT(!p || !p->isBeingConstructed());
    }

    /// Construction from shared pointer.
    OORef(base_type&& p) noexcept : base_type{std::move(p)} {
        OVITO_ASSERT(!base_type::get() || !base_type::get()->isBeingConstructed());
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

    /// Factory method that instantiates and initializes a new object of a RefTarget-derived type.
    template<typename... Args>
    static this_type create(ObjectInitializationFlags flags, Args&&... args) {
        using OType = std::remove_const_t<T>;
        static_assert(std::is_base_of_v<Ovito::RefTarget, OType>, "Object class must be a RefTarget derived class");

        // Instantiate the object on the heap.
        // We are using our custom allocator here, which ensures that OvitoObject::deleteObjectInternal() is called
        // just before object destruction.
        std::shared_ptr<OType> obj = std::allocate_shared<OType>(OOAllocator<OType>{}, flags, std::forward<Args>(args)...);

        // Initialize the object's parameters to their user-defined default values (only when the creation happens in the interactive UI).
        if(ExecutionContext::isInteractive())
            obj->initializeParametersToUserDefaults();

        // Clear the BeingConstructed flag after the object's constructor has run.
        obj->ooConstructionComplete();

        // Finally, move the shared_ptr into an OORef.
        return OORef<OType>(std::move(obj));
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

            // Intantiate the object on the heap.
            // We are using our custom allocator here, which ensures that OvitoObject::deleteObjectInternal() is called
            // just before object destruction.
            auto obj = std::allocate_shared<OType>(OOAllocator<OType>{}, std::forward<Args>(args)...);

            // Clear the BeingConstructed flag after the object's constructor has run.
            obj->ooConstructionComplete();

            // Finally, move the shared_ptr into an OORef.
            return OORef<OType>(std::move(obj));
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

/**
 * A weak reference to an OvitoObject.
*/
template<class T>
class OOWeakRef : public std::weak_ptr<T>
{
public:

    /// Inherit all constructors from base class.
    using std::weak_ptr<T>::weak_ptr;

    /// Construction from raw pointer to an OvitoObject (may be null).
    template<typename U>
    OOWeakRef(const U* p) noexcept : std::weak_ptr<T>{OORef<T>(p)} {}

    /// Equal comparison operator (with another weak ref).
    template<class U>
    inline bool operator==(const OOWeakRef<U>& rhs) const noexcept {
        return !this->owner_before(rhs) && !rhs.owner_before(*this); // In C++26: implement this using owner_equal()
    }

    /// Equal comparison operator (with a regular object pointer).
    template<class U>
    inline bool operator==(U* rhs) const noexcept {
        const OORef<U> ref(rhs);
        return !this->owner_before(ref) && !ref.owner_before(*this); // In C++26: implement this using owner_equal()
    }
};

template<class T, class U> OORef<T> static_pointer_cast(const OORef<U>& p) noexcept
{
    return std::static_pointer_cast<T, U>(p);
}

template<class T, class U> OORef<T> static_pointer_cast(OORef<U>&& p) noexcept
{
#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 170000
    return std::static_pointer_cast<T, U>(std::move(p));
#else
    // Note: libc++ < 17 does not implement move semantics for std::static_pointer_cast (introduced by c++20).
    // We have to work around this limitation by explicitly resetting the source pointer.
    auto d = std::static_pointer_cast<T, U>(std::move(p));
    p.reset();
    return d;
#endif
}

template<class T, class U> OORef<T> const_pointer_cast(const OORef<U>& p) noexcept
{
    return std::const_pointer_cast<T, U>(p);
}

template<class T, class U> OORef<T> const_pointer_cast(OORef<U>&& p) noexcept
{
#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 170000
    return std::const_pointer_cast<T, U>(std::move(p));
#else
    // Note: libc++ < 17 does not implement move semantics for std::const_pointer_cast (introduced by c++20).
    // We have to work around this limitation by explicitly resetting the source pointer.
    auto d = std::const_pointer_cast<T, U>(std::move(p));
    p.reset();
    return d;
#endif
}

template<class T, class U> OORef<T> dynamic_pointer_cast(const OORef<U>& p) noexcept
{
    return std::dynamic_pointer_cast<T, U>(p);
}

template<class T, class U> OORef<T> dynamic_pointer_cast(OORef<U>&& p) noexcept
{
#if !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 170000
    return std::dynamic_pointer_cast<T, U>(std::move(p));
#else
    // Note: libc++ < 17 does not implement move semantics for std::dynamic_pointer_cast (introduced by c++20).
    // We have to work around this limitation by explicitly resetting the source pointer.
    auto d = std::dynamic_pointer_cast<T, U>(std::move(p));
    p.reset();
    return d;
#endif
}

template<class T> QDebug operator<<(QDebug debug, const OORef<T>& p)
{
    return debug << p.get();
}

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::ObjectInitializationFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Ovito::ObjectInitializationFlags);
