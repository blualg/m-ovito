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

template<typename ResultType, typename InputRange, class Executor, typename Function>
auto reduce_sequential(ResultType&& initialResultValue, InputRange&& inputRange, Executor&& executor, Function&& f)
{
	return for_each_sequential(
		std::forward<InputRange>(inputRange),
		std::forward<Executor>(executor),
		// Iteration start function:
		std::forward<Function>(f),
		// Iteration completed function (a no-op):
		[](typename InputRange::const_reference iterValue) {},
		std::forward<ResultType>(initialResultValue));
}

}	// End of namespace
