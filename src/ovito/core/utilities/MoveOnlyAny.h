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

/**
 * \file
 * \brief An implementation of std::any that specifically supports move-only types. The regular std::any implementation supports only copy-constructible types.
 */

#pragma once


#include <ovito/core/Core.h>

#include <initializer_list>
#include <type_traits>
#include <typeinfo>

namespace Ovito {

class any_moveonly final
{
private:
    // Holds either pointer to a heap object or the contained object itself.
    union Storage
    {
        constexpr Storage() : _ptr{nullptr} {}

        // Prevent trivial copies of this type, buffer might hold a non-POD.
        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;

        void* _ptr;
        std::aligned_storage<sizeof(_ptr), alignof(void*)>::type _buffer;
    };

    template<typename _Tp, typename _Safe = std::is_nothrow_move_constructible<_Tp>, bool _Fits = (sizeof(_Tp) <= sizeof(Storage)) && (alignof(_Tp) <= alignof(Storage))>
    using _Internal = std::integral_constant<bool, _Safe::value && _Fits>;

    template<typename _Tp>
    struct _Manager_internal; // uses small-object optimization

    template<typename _Tp>
    struct _Manager_external; // creates contained object on the heap

    template<typename _Tp>
    using _Manager = std::conditional_t<_Internal<_Tp>::value,
                    _Manager_internal<_Tp>,
                    _Manager_external<_Tp>>;

    template<typename _Tp, typename _VTp = std::decay_t<_Tp>>
    using _Decay_if_not_any = std::enable_if_t<!std::is_same<_VTp, any_moveonly>::value, _VTp>;

    /// Emplace with an object created from @p __args as the contained object.
    template <typename _Tp, typename... _Args, typename _Mgr = _Manager<_Tp>>
    void __do_emplace(_Args&&... __args)
    {
        reset();
        _Mgr::_create(_M_storage, std::forward<_Args>(__args)...);
        _manager = &_Mgr::_S_manage;
    }

    /// Emplace with an object created from @p __il and @p __args as
    /// the contained object.
    template <typename _Tp, typename _Up, typename... _Args, typename _Mgr = _Manager<_Tp>>
    void __do_emplace(std::initializer_list<_Up> __il, _Args&&... __args)
    {
        reset();
        _Mgr::_create(_M_storage, __il, std::forward<_Args>(__args)...);
        _manager = &_Mgr::_S_manage;
    }

    template <typename _Res, typename _Tp, typename... _Args>
    using __any_constructible = std::enable_if<std::is_move_constructible<_Tp>::value && std::is_constructible<_Tp, _Args...>::value, _Res>;

    template <typename _Tp, typename... _Args>
    using __any_constructible_t = typename __any_constructible<bool, _Tp, _Args...>::type;

    template<typename _VTp, typename... _Args>
    using __emplace_t = typename __any_constructible<_VTp&, _VTp, _Args...>::type;

public:
    // construct/destruct

    /// Default constructor, creates an empty object.
    constexpr any_moveonly() noexcept : _manager(nullptr) { }

    /// No copy constructor.
    any_moveonly(const any_moveonly& __other) = delete;

    /**
     * @brief Move constructor, transfer the state from @p __other
     */
    any_moveonly(any_moveonly&& __other) noexcept
    {
        if (!__other.has_value())
            _manager = nullptr;
        else
        {
            _Arg __arg;
            __arg._M_any = this;
            __other._manager(_Op_xfer, &__other, &__arg);
        }
    }

    /// Construct with a copy of @p __value as the contained object.
    template <typename _Tp, typename _VTp = _Decay_if_not_any<_Tp>, typename _Mgr = _Manager<_VTp>, std::enable_if_t<std::is_move_constructible<_VTp>::value, bool> = true>
    any_moveonly(_Tp&& __value) : _manager(&_Mgr::_S_manage)
    {
        _Mgr::_create(_M_storage, std::forward<_Tp>(__value));
    }

    /// Destructor, calls @c reset()
    ~any_moveonly() { reset(); }

    // assignments

    /**
     * @brief No copy assignment operator
     */
    any_moveonly& operator=(const any_moveonly& __rhs) noexcept = delete;

    /**
     * @brief Move assignment operator
     */
    any_moveonly& operator=(any_moveonly&& __rhs) noexcept
    {
        if (!__rhs.has_value())
            reset();
        else if (this != &__rhs)
        {
            reset();
            _Arg __arg;
            __arg._M_any = this;
            __rhs._manager(_Op_xfer, &__rhs, &__arg);
        }
        return *this;
    }

    /// Moves @p __rhs into the container.
    template<typename _Tp>
    std::enable_if_t<std::is_move_constructible<_Decay_if_not_any<_Tp>>::value, any_moveonly&> operator=(_Tp&& __rhs)
    {
        *this = any_moveonly(std::move(__rhs));
        return *this;
    }

    /// Emplace with an object created from @p __args as the contained object.
    template <typename _Tp, typename... _Args>
    __emplace_t<std::decay_t<_Tp>, _Args...> emplace(_Args&&... __args)
    {
        using _VTp = std::decay_t<_Tp>;
        __do_emplace<_VTp>(std::forward<_Args>(__args)...);
        any_moveonly::_Arg __arg;
        this->_manager(any_moveonly::_Op_access, this, &__arg);
        return *static_cast<_VTp*>(__arg._M_obj);
    }

    // modifiers

    /// If not empty, destroy the contained object.
    void reset() noexcept {
        if(has_value()) {
            _manager(_Op_destroy, this, nullptr);
            _manager = nullptr;
        }
    }

    /// Exchange state with another object.
    void swap(any_moveonly& __rhs) noexcept {
        if(!has_value() && !__rhs.has_value())
            return;

        if(has_value() && __rhs.has_value()) {
            if(this == &__rhs)
                return;

            any_moveonly __tmp;
            _Arg __arg;
            __arg._M_any = &__tmp;
            __rhs._manager(_Op_xfer, &__rhs, &__arg);
            __arg._M_any = &__rhs;
            _manager(_Op_xfer, this, &__arg);
            __arg._M_any = this;
            __tmp._manager(_Op_xfer, &__tmp, &__arg);
        }
        else {
            any_moveonly* __empty = !has_value() ? this : &__rhs;
            any_moveonly* __full = !has_value() ? &__rhs : this;
            _Arg __arg;
            __arg._M_any = __empty;
            __full->_manager(_Op_xfer, __full, &__arg);
        }
    }

    // observers

    /// Reports whether there is a contained object or not.
    bool has_value() const noexcept { return _manager != nullptr; }

    /// The @c typeid of the contained object, or @c typeid(void) if empty.
    const std::type_info& type() const noexcept {
        if(!has_value())
            return typeid(void);
        _Arg __arg;
        _manager(_Op_get_type_info, this, &__arg);
        return *__arg._M_typeinfo;
    }

    template<typename _Tp>
    static constexpr bool __is_valid_cast() { return std::is_reference<_Tp>::value || std::is_copy_constructible<_Tp>::value; }

private:
    enum _Op {
        _Op_access, _Op_get_type_info, _Op_destroy, _Op_xfer
    };

    union _Arg
    {
        void* _M_obj;
        const std::type_info* _M_typeinfo;
        any_moveonly* _M_any;
    };

    void (*_manager)(_Op, const any_moveonly*, _Arg*);
    Storage _M_storage;

    template<typename _Tp> friend void* __any_caster(const any_moveonly* __any);

    // Manage in-place contained object.
    template<typename _Tp>
    struct _Manager_internal
    {
        static void _S_manage(_Op __which, const any_moveonly* __anyp, _Arg* __arg);

        template<typename _Up>
        static void _create(Storage& __storage, _Up&& __value)
        {
            void* __addr = &__storage._buffer;
            ::new (__addr) _Tp(std::forward<_Up>(__value));
        }

        template<typename... _Args>
        static void _create(Storage& __storage, _Args&&... __args)
        {
            void* __addr = &__storage._buffer;
            ::new (__addr) _Tp(std::forward<_Args>(__args)...);
        }
    };

    // Manage external contained object.
    template<typename _Tp>
    struct _Manager_external
    {
        static void _S_manage(_Op __which, const any_moveonly* __anyp, _Arg* __arg);

        template<typename _Up>
        static void _create(Storage& __storage, _Up&& __value) {
            __storage._ptr = new _Tp(std::forward<_Up>(__value));
        }
        template<typename... _Args>
        static void _create(Storage& __storage, _Args&&... __args) {
            __storage._ptr = new _Tp(std::forward<_Args>(__args)...);
        }
    };
};

/// Exchange the states of two @c any_moveonly objects.
inline void swap(any_moveonly& __x, any_moveonly& __y) noexcept { __x.swap(__y); }

/**
 * @brief Access the contained object.
 *
 * @tparam  _ValueType  A reference or CopyConstructible type.
 * @param   __any       The object to access.
 * @return  The contained object.
 * @throw   bad_cast If <code>
 *          __any.type() != typeid(remove_reference_t<_ValueType>)
 *          </code>
 */
template<typename _ValueType>
inline _ValueType any_cast(any_moveonly& __any)
{
    using _Up = std::remove_cv_t<std::remove_reference_t<_ValueType>>;
    static_assert(any_moveonly::__is_valid_cast<_ValueType>(), "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up&>::value, "Template argument must be constructible from an lvalue.");
    auto __p = any_cast<_Up>(&__any);
    if(__p)
        return static_cast<_ValueType>(*__p);
    throw std::bad_cast{};
}

template<typename _ValueType>
inline _ValueType any_cast(any_moveonly&& __any)
{
    using _Up = std::remove_cv_t<std::remove_reference_t<_ValueType>>;
    static_assert(any_moveonly::__is_valid_cast<_ValueType>(), "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up>::value, "Template argument must be constructible from an rvalue.");
    auto __p = any_cast<_Up>(&__any);
    if (__p)
        return static_cast<_ValueType>(std::move(*__p));
    throw std::bad_cast{};
}

template<typename _Tp>
void* __any_caster(const any_moveonly* __any)
{
    // any_cast<T> returns non-null if __any->type() == typeid(T) and
    // typeid(T) ignores cv-qualifiers so remove them:
    using _Up = std::remove_cv_t<_Tp>;
    // The contained value has a decayed type, so if decay_t<U> is not U,
    // then it's not possible to have a contained value of type U:
    if(!std::is_same<std::decay_t<_Up>, _Up>::value)
        return nullptr;
    // Only movable types can be used for contained values:
    else if(!std::is_move_constructible<_Up>::value)
        return nullptr;
    // First try comparing function addresses, which works without RTTI
    else if (__any->_manager == &any_moveonly::_Manager<_Up>::_S_manage || __any->type() == typeid(_Tp)) {
        any_moveonly::_Arg __arg;
        __any->_manager(any_moveonly::_Op_access, __any, &__arg);
        return __arg._M_obj;
    }
    return nullptr;
}

/**
 * @brief Access the contained object.
 *
 * @tparam  _ValueType  The type of the contained object.
 * @param   __any       A pointer to the object to access.
 * @return  The address of the contained object if <code>
 *          __any != nullptr && __any.type() == typeid(_ValueType)
 *          </code>, otherwise a null pointer.
 */
template<typename _ValueType>
inline const _ValueType* any_cast(const any_moveonly* __any) noexcept
{
    if(std::is_object<_ValueType>::value)
        if(__any)
            return static_cast<_ValueType*>(__any_caster<_ValueType>(__any));
    return nullptr;
}

template<typename _ValueType>
inline _ValueType* any_cast(any_moveonly* __any) noexcept
{
    if(std::is_object<_ValueType>::value)
        if(__any)
            return static_cast<_ValueType*>(__any_caster<_ValueType>(__any));
    return nullptr;
}

template<typename _Tp>
void any_moveonly::_Manager_internal<_Tp>::_S_manage(_Op __which, const any_moveonly* __any, _Arg* __arg)
{
    // The contained object is in _M_storage._buffer
    auto __ptr = reinterpret_cast<const _Tp*>(&__any->_M_storage._buffer);
    switch (__which)
    {
    case _Op_access:
        __arg->_M_obj = const_cast<_Tp*>(__ptr);
        break;
    case _Op_get_type_info:
        __arg->_M_typeinfo = &typeid(_Tp);
        break;
    case _Op_destroy:
        __ptr->~_Tp();
        break;
    case _Op_xfer:
        ::new(&__arg->_M_any->_M_storage._buffer) _Tp
            (std::move(*const_cast<_Tp*>(__ptr)));
        __ptr->~_Tp();
        __arg->_M_any->_manager = __any->_manager;
        const_cast<any_moveonly*>(__any)->_manager = nullptr;
        break;
    }
}

template<typename _Tp>
void any_moveonly::_Manager_external<_Tp>::_S_manage(_Op __which, const any_moveonly* __any, _Arg* __arg)
{
    // The contained object is *_M_storage._ptr
    auto __ptr = static_cast<const _Tp*>(__any->_M_storage._ptr);
    switch (__which)
    {
    case _Op_access:
        __arg->_M_obj = const_cast<_Tp*>(__ptr);
        break;
    case _Op_get_type_info:
        __arg->_M_typeinfo = &typeid(_Tp);
        break;
    case _Op_destroy:
        delete __ptr;
        break;
    case _Op_xfer:
        __arg->_M_any->_M_storage._ptr = __any->_M_storage._ptr;
        __arg->_M_any->_manager = __any->_manager;
        const_cast<any_moveonly*>(__any)->_manager = nullptr;
        break;
    }
}

}   // End of namespace
