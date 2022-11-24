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

#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/ExecutionContext.h>

namespace Ovito {

/// The active execution context in the current thread.
static thread_local ExecutionContext _current;

/*******************************************************x***********************
* Returns the context the current thread performs its actions in.
******************************************************************************/
const ExecutionContext& ExecutionContext::current() noexcept 
{
    return _current; 
}

/*******************************************************x***********************
* Sets the context the current thread performs its actions in.
******************************************************************************/
void ExecutionContext::setCurrent(const ExecutionContext& context) noexcept 
{ 
    _current = context; 
}

}	// End of namespace
