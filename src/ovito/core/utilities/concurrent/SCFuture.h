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
#include "Future.h"

namespace Ovito {

/**
 * A tagged variant of the Future class, which is used in structured concurrency constructs.
 */
template<typename R>
class SCFuture : public Future<R>
{
    Q_DISABLE_COPY(SCFuture)

public:

    /// The promise type for C++ coroutines returning a Future.
    using promise_type = CoroutinePromise<R, true>;

    /// Inherit constructors from base class.
    using Future<R>::Future;

    /// Move constructor.
    SCFuture(SCFuture&& other) noexcept : Future<R>(static_cast<Future<R>&&>(other)) {}

    /// Conversion constructor from Future<R> to SCFuture<R>.
    SCFuture(Future<R> future) noexcept : Future<R>(std::move(future)) {}

    /// Move assignment operator.
    SCFuture& operator=(SCFuture&& other) noexcept {
        Future<R>::operator=(static_cast<Future<R>&&>(other));
        return *this;
    }

    /// Conversion assignment operator from Future<R> to SCFuture<R>.
    SCFuture& operator=(Future<R> future) noexcept {
        Future<R>::operator=(std::move(future));
        return *this;
    }
};

}   // End of namespace
