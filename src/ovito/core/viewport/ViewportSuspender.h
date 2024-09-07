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
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/ExecutionContext.h>

namespace Ovito {

/**
 * \brief RAII helper class that suspends redrawing of the interactive viewports while it exists.
 *
 * Use this to make your code exception-safe.
 * Just create an instance of this class on the stack to suspend viewport updates
 * during the lifetime of the class instance.
 */
class OVITO_CORE_EXPORT ViewportSuspender
{
    Q_DISABLE_COPY(ViewportSuspender)

public:

    /// Calls UserInterface::suspendViewportUpdates().
    ViewportSuspender(UserInterface& userInterface = ExecutionContext::current().ui()) noexcept : _userInterface(&userInterface) {
        userInterface.suspendViewportUpdates();
    }

    /// Calls UserInterface::resumeViewportUpdates().
    ~ViewportSuspender() {
        if(auto ui = _userInterface.lock())
            ui->resumeViewportUpdates();
    }

    /// Move constructor.
    ViewportSuspender(ViewportSuspender&& other) noexcept : _userInterface(std::move(other._userInterface)) {}

    /// Move assignment operator.
    ViewportSuspender& operator=(ViewportSuspender&& other) noexcept {
        _userInterface.swap(other._userInterface);
        return *this;
    }

private:

    OOWeakRef<UserInterface> _userInterface;
};

/**
 * \brief A helper class that suspends preliminary viewport updates while it exists.
 */
class OVITO_CORE_EXPORT PreliminaryViewportUpdatesSuspender
{
    Q_DISABLE_COPY(PreliminaryViewportUpdatesSuspender)

public:

    /// Calls UserInterface::suspendPreliminaryViewportUpdates().
    PreliminaryViewportUpdatesSuspender(UserInterface& userInterface) : _userInterface(&userInterface) {
        userInterface.suspendPreliminaryViewportUpdates();
    }

    /// Calls UserInterface::resumePreliminaryViewportUpdates().
    ~PreliminaryViewportUpdatesSuspender() {
        if(auto ui = _userInterface.lock())
            ui->resumePreliminaryViewportUpdates();
    }

private:

    OOWeakRef<UserInterface> _userInterface;
};

}   // End of namespace
