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
#include <type_traits>

namespace Ovito::detail {

/*
* is_future<T>
*
* Determines whether T is some specialization of the Future or SharedFuture class templates.
*/

template<typename T>
struct is_future : std::false_type {};

template<typename T>
struct is_future<Future<T>> : std::true_type {};

template<typename T>
struct is_future<SharedFuture<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_future_v = is_future<std::decay_t<T>>::value;

/*
* is_shared_future<T>
*
* Determines whether T is some specialization of the SharedFuture class template.
*/

/// Determines whether a type T is some specialization of the SharedFuture class template.
template<typename T>
struct is_shared_future : std::false_type {};

template<typename T>
struct is_shared_future<SharedFuture<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_shared_future_v = is_shared_future<std::decay_t<T>>::value;

/*
* callable_result<F,FutureType>
*
* Determines the return value type of some callable F, which gets called with the FutureType itself or the results of the future as arguments.
*/

template<typename F, typename FutureType, class = void>
struct callable_result : std::invoke_result<F, typename FutureType::result_type> {};

template<typename F, typename FutureType>
struct callable_result<F, FutureType, std::enable_if_t<!std::is_invocable_v<F, FutureType> && std::is_void_v<typename FutureType::result_type>>> : std::invoke_result<F> {};

template<typename F, typename FutureType>
struct callable_result<F, FutureType, std::enable_if_t<std::is_invocable_v<F, FutureType>>> : std::invoke_result<F, FutureType> {};

template<typename F, typename FutureType>
using callable_result_t = typename callable_result<F, FutureType>::type;

/*
    * returns_void<F,FutureType>
    *
    * Determines some callable F, which gets called with the FutureType itself or the results of the future as arguments, returns void.
    */

/// Determines whether the return type of a callable is 'void'.
template<typename F, typename FutureType>
inline constexpr bool returns_void_v = std::is_void_v<callable_result_t<F, FutureType>>;

/*
* returns_future<F,FutureType>
*
* Determines whether some callable F, which gets called with the FutureType itself or the results of the future as arguments, returns a future.
*/

template<typename F, typename FutureType>
inline constexpr bool returns_future_v = is_future_v<callable_result_t<F, FutureType>>;

/// Determines the Future type that results from a continuation function.
///
///               T func(...)   ->   Future<T>
///       Future<T> func(...)   ->   Future<T>  (automatic unwrapping)
///
template<typename F, typename FutureType>
using continuation_future_type = std::conditional_t<returns_future_v<F,FutureType>,
                                                    callable_result_t<F,FutureType>,
                                                    Future<callable_result_t<F,FutureType>>>;

} // End of namespace
