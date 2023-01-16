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
#include <type_traits>

namespace Ovito::detail {

/*
 * An implementation of C++20's std::bind_front().
 */

// Invoke the method, expanding the tuple of bound arguments.
template <class R, class Tuple, size_t... Idx, class... Args>
R Apply(Tuple&& bound, std::index_sequence<Idx...>, Args&&... free) 
{
    return std::invoke(std::get<Idx>(std::forward<Tuple>(bound))..., std::forward<Args>(free)...);
}

template <class F, class... BoundArgs>
class FrontBinder 
{
    using BoundArgsT = std::tuple<F, BoundArgs...>;
    using Idx = std::make_index_sequence<sizeof...(BoundArgs) + 1>;

    BoundArgsT bound_args_;

public:
    template <class... Ts>
    constexpr explicit FrontBinder(std::in_place_t, Ts&&... ts)
        : bound_args_(std::forward<Ts>(ts)...) {}

    template <class... FreeArgs, class R = detail::invoke_result_t<F&, BoundArgs&..., FreeArgs&&...>>
    R operator()(FreeArgs&&... free_args) & noexcept(std::is_nothrow_invocable_v<F&, BoundArgs&..., FreeArgs&&...>) {
        return Apply<R>(bound_args_, Idx(), std::forward<FreeArgs>(free_args)...);
    }

    template <class... FreeArgs, class R = detail::invoke_result_t<const F&, const BoundArgs&..., FreeArgs&&...>>
    R operator()(FreeArgs&&... free_args) const& noexcept(std::is_nothrow_invocable_v<const F&, const BoundArgs&..., FreeArgs&&...>) {
        return Apply<R>(bound_args_, Idx(), std::forward<FreeArgs>(free_args)...);
    }

    template <class... FreeArgs, class R = detail::invoke_result_t<F&&, BoundArgs&&..., FreeArgs&&...>>
    R operator()(FreeArgs&&... free_args) && noexcept(std::is_nothrow_invocable_v<F&&, BoundArgs&&..., FreeArgs&&...>) {
        // This overload is called when *this is an rvalue. If some of the bound
        // arguments are stored by value or rvalue reference, we move them.
        return Apply<R>(std::move(bound_args_), Idx(), std::forward<FreeArgs>(free_args)...);
    }

    template <class... FreeArgs, class R = detail::invoke_result_t<const F&&, const BoundArgs&&..., FreeArgs&&...>>
    R operator()(FreeArgs&&... free_args) const&& noexcept(std::is_nothrow_invocable_v<const F&&, const BoundArgs&&..., FreeArgs&&...>) {
        // This overload is called when *this is an rvalue. If some of the bound
        // arguments are stored by value or rvalue reference, we move them.
        return Apply<R>(std::move(bound_args_), Idx(), std::forward<FreeArgs>(free_args)...);
    }
};  

template <class F, class... BoundArgs>
using bind_front_t = FrontBinder<std::decay_t<F>, std::decay_t<BoundArgs>...>;

template <class F, class... BoundArgs>
constexpr bind_front_t<F, BoundArgs...> bind_front(F&& func, BoundArgs&&... args) 
{
    return bind_front_t<F, BoundArgs...>(
        std::in_place, std::forward<F>(func),
        std::forward<BoundArgs>(args)...);
}

} // End of namespace
