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
#include "Promise.h"

namespace Ovito {

/**
 * A promise-like object that is used during long-running program operations that are performed synchronously by the program's main thread.
 *
 * The operation is automatically put into the 'finished' state by the class' destructor.
 */
class OVITO_CORE_EXPORT MainThreadOperation : public Promise<>, ExecutionContext::Scope, Task::Scope
{
public:

    /// Constructor.
    explicit MainThreadOperation(ExecutionContext::Type contextType = ExecutionContext::current().type(), UserInterface& userInterface = ExecutionContext::current().ui());

    /// Destructor.
    ~MainThreadOperation();
};

}   // End of namespace
