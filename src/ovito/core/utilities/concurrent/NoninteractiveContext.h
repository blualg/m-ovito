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

namespace Ovito {

/**
 * This RAII helper class temporarily clears the interactive flag of the current task to establish a non-interactive execution context.
 */
class NoninteractiveContext
{
public:

    /// Constructor.
    NoninteractiveContext() noexcept : _wasInteractive(this_task::get()->setIsInteractive(false)) {
        OVITO_ASSERT(this_task::get());
    }

    /// Destructor.
    ~NoninteractiveContext() {
        OVITO_ASSERT(_task == this_task::get());
        if(_wasInteractive)
            this_task::get()->setIsInteractive(true);
    }

private:

    bool _wasInteractive;

#ifdef OVITO_DEBUG
    Task* _task = this_task::get();
#endif
};

}   // End of namespace
