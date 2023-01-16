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
#include "ForEach.h"

namespace Ovito {

template<typename InputRange, class Executor, typename Function>
auto map_sequential(InputRange&& inputRange, Executor&& executor, Function&& f)
{
    // The type of future returned by the user function.
    using output_future_type = detail::invoke_result_t<Function, typename InputRange::const_reference>; // C++20: Use std::indirect_result_t instead.

    // The type of values produced by the user function.
    using result_value_type = std::conditional_t<std::tuple_size_v<typename output_future_type::tuple_type> == 1,
        std::tuple_element_t<0, typename output_future_type::tuple_type>,
        typename output_future_type::tuple_type>;

    return for_each_sequential(
        std::forward<InputRange>(inputRange),
        std::forward<Executor>(executor),
        // Iteration start function:
        [f = std::forward<Function>(f)](typename InputRange::const_reference iterValue, std::vector<result_value_type>&) mutable { 
            return std::forward<Function>(f)(iterValue);
        },
        // Iteration complete function:
        [](typename InputRange::const_reference iterValue, auto&& iterResult, std::vector<result_value_type>& taskOutput) {
            // Append the result of the future to our output list.
            taskOutput.push_back(std::forward<decltype(iterResult)>(iterResult));
        },
        std::vector<result_value_type>{});
}

}   // End of namespace
