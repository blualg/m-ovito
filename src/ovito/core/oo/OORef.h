////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/utilities/concurrent/Task.h>
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
    template<typename U, class = std::enable_if_t<std::is_convertible_v<const U*, const T*>>>
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

    /// Allow implicit conversion from l-value smart pointer to raw object pointer.
    inline operator T*() const& noexcept {
        return base_type::get();
    }

    /// Disallow inadvertent conversion from r-value smart pointer to a dangling raw pointer.
    operator T*() && = delete;

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
        std::shared_ptr<OType> obj = std::allocate_shared<OType>(OOAllocator<OType>{});

        // Second-phase construction function.
        obj->initializeObject(flags, std::forward<Args>(args)...);

        // Initialize the object's parameters to their user-defined default values (only when the creation happens in the interactive UI).
        if(this_task::isInteractive())
            obj->initializeParametersToUserDefaultsNonrecursive();

        // Clear the BeingInitialized flag.
        obj->completeObjectInitialization();

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

            // Instantiate the object on the heap.
            // We are using our custom allocator here, which ensures that OvitoObject::deleteObjectInternal() is called
            // just before object destruction.
            auto obj = std::allocate_shared<OType>(OOAllocator<OType>{});

            // Second-phase construction function.
            obj->initializeObject(std::forward<Args>(args)...);

            // Clear the BeingInitialized flag.
            obj->completeObjectInitialization();

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
class OOWeakRef : public std::weak_ptr<std::conditional_t<std::is_const_v<T>, const OvitoObject, OvitoObject>>
{
public:

    using base_class = std::weak_ptr<std::conditional_t<std::is_const_v<T>, const OvitoObject, OvitoObject>>;
    using element_type = T;

    /// Inherit all constructors from base class.
    constexpr OOWeakRef() noexcept = default;

    /// Copy construction from another weak reference that is implicitly convertible.
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(const OOWeakRef<U>& p) noexcept : base_class{p} {}

    /// Copy construction from another weak pointer that is implicitly convertible.
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(const std::weak_ptr<U>& p) noexcept : base_class{p} {}

    /// Move construction from another weak reference that is implicitly convertible.
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(OOWeakRef<U>&& p) noexcept : base_class{std::move(p)} {}

    /// Move construction from another weak pointer that is implicitly convertible.
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(std::weak_ptr<U>&& p) noexcept : base_class{std::move(p)} {}

    /// Construction from a strong reference that is implicitly convertible.
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(const OORef<U>& p) noexcept : base_class{p} {}

    /// Construction from raw pointer to an OvitoObject (may be null).
    template<typename U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    OOWeakRef(U* p) noexcept : base_class{p ? p->weak_from_this() : base_class{}} {
        OVITO_ASSERT(!p || !p->isBeingConstructed());
    }

    /// Converts the weak pointer to a strong pointer.
    OORef<T> lock() const noexcept {
        return static_object_cast<T>(base_class::lock());
    }

    /// Equal comparison operator (with another weak ref).
    template<class U, class = std::enable_if_t<std::is_convertible_v<const U*, const T*> || std::is_convertible_v<const T*, const U*>>>
    inline bool operator==(const OOWeakRef<U>& rhs) const noexcept {
        return !this->owner_before(rhs) && !rhs.owner_before(*this); // In C++26: implement this using owner_equal()
    }

    /// Equal comparison operator (with a raw object pointer).
    template<class U, class = std::enable_if_t<std::is_convertible_v<const U*, const T*> || std::is_convertible_v<const T*, const U*>>>
    inline bool operator==(U* rhs) const noexcept {
        return *this == OOWeakRef<U>(rhs);
    }

    /// Friend equality operator for ranges compatibility (raw pointer on left).
    template<class U>
    friend bool operator==(U* lhs, const OOWeakRef& rhs) noexcept
        requires std::is_convertible_v<const U*, const T*> || std::is_convertible_v<const T*, const U*> {
        return rhs == lhs;
    }

    /// Returns true of this weak reference has been initialized with an explicit null object pointer.
    /// Note: This is different from the case where the referenced object has expired.
    bool empty() const {
        return !this->owner_before(base_class{}) && !base_class{}.owner_before(*this);
    }
};

template<typename T>
struct is_weak_ref : std::false_type {};

template<typename T>
struct is_weak_ref<OOWeakRef<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_weak_ref_v = is_weak_ref<std::decay_t<T>>::value;

template<class T, class U> OORef<T> static_pointer_cast(const OORef<U>& p) noexcept
{
    static_assert(std::is_convertible_v<const U*, const T*> || std::is_base_of_v<U, T>);
    return std::static_pointer_cast<T, U>(p);
}

template<class T, class U> OORef<T> static_pointer_cast(OORef<U>&& p) noexcept
{
    static_assert(std::is_convertible_v<const U*, const T*> || std::is_base_of_v<U, T>);
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
    static_assert(std::is_convertible_v<const U*, const T*>);
    return std::const_pointer_cast<T, U>(p);
}

template<class T, class U> OORef<T> const_pointer_cast(OORef<U>&& p) noexcept
{
    static_assert(std::is_convertible_v<const U*, const T*>);
    if constexpr(std::is_same_v<T, U>)
        return std::move(p);
    else {
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
