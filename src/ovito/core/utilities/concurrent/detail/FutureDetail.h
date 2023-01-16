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
#include <ovito/core/utilities/concurrent/ExecutionContext.h>
#include <type_traits>

namespace Ovito::detail {

/*
* is_future<T>
*
* Determines whether T is some specialization of the Future<> or SharedFuture<> class templates.
*/

template<typename T>
struct is_future : std::false_type {};

template<typename... T>
struct is_future<Future<T...>> : std::true_type {};

template<typename... T>
struct is_future<SharedFuture<T...>> : std::true_type {};

template<typename T>
inline constexpr bool is_future_v = is_future<std::decay_t<T>>::value;

/*
* is_shared_future<T>
*
* Determines whether T is some specialization of the SharedFuture<> class template.
*/

/// Determines whether a type T is some specialization of the SharedFuture class template.
template<typename T>
struct is_shared_future : std::false_type {};

template<typename... T>
struct is_shared_future<SharedFuture<T...>> : std::true_type {};

template<typename T>
inline constexpr bool is_shared_future_v = is_shared_future<std::decay_t<T>>::value;

/*
* callable_result<F,FutureType>
*
* Determines the return value type of some callable F, which gets called with the FutureType itself or the results of the future as arguments.
*/

template<typename F, typename... Args>
struct invoke_result_with_tuple : invoke_result<F, Args...> {};

template<typename F, typename... Args>
struct invoke_result_with_tuple<F, std::tuple<Args...>> : invoke_result<F, Args...> {};

template<typename F, typename FutureType, class = void>
struct callable_result : invoke_result_with_tuple<F, typename FutureType::tuple_type> {};

template<typename F, typename FutureType>
struct callable_result<F, FutureType, std::enable_if_t<detail::is_invocable_v<F, FutureType>>> : invoke_result<F, FutureType> {};

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

/*
* future_for<T>
*
* For some type T, which may be void, returns the corresponding Future<> type.
*/
template<typename T>
using future_for_t = std::conditional_t<std::is_void_v<T>, Future<>, Future<std::decay_t<T>>>;

/// Determines the Future type that results from a continuation function.
///
///     Future<...> func(...)   ->   Future<...>  (automatic unwrapping)
///               T func(...)   ->   Future<T>
///            void func(...)   ->   Future<>
///
template<typename F, typename FutureType>
using continuation_future_type = std::conditional_t<returns_future_v<F,FutureType>,
                                                    callable_result_t<F,FutureType>,
                                                    future_for_t<callable_result_t<F,FutureType>>>;

/// The simplest implementation of the Executor concept.
/// The inline executor runs a work function immediately and in place.
/// See ObjectExecutor for another implementation of the executor concept.
struct InlineExecutor {

    template<typename Function>
    static constexpr void execute(Function&& f) noexcept {
        std::invoke(std::forward<Function>(f));
    }

    template<typename Function>
    static constexpr auto schedule(Function&& f) noexcept {
        return [f = std::forward<Function>(f), context = ExecutionContext::current()]() mutable noexcept {
            ExecutionContext::Scope execScope(std::move(context));
            std::invoke(std::move(f));
        };
    }   
};

} // End of namespace
