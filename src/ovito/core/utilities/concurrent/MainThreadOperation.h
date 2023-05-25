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
    explicit MainThreadOperation(ExecutionContext::Type contextType, UserInterface& userInterface, bool visibleInUserInterface);

    /// Constructor creating a sub-task.
    explicit MainThreadOperation(bool visibleInUserInterface);

    /// Destructor, which puts the promise into the 'finished' state.
    ~MainThreadOperation();

    /// Returns the shared task, casting it to the ProgressingTask subclass.
    ProgressingTask& progressingTask() const {
        OVITO_ASSERT(isValid());
        OVITO_ASSERT(task()->isProgressingTask());
        return static_cast<ProgressingTask&>(*task());
    }

    /// Override this method from the Promise class to keep the UI responsive during long-running tasks.
    bool setProgressValue(qlonglong progressValue) const {
        // Temporarily yield control back to the event loop to process UI events and keep the UI responsive during long-running tasks.
        processUIEvents();
        return Promise<>::setProgressValue(progressValue);
    }

    /// Override this method from the Promise class to keep the UI responsive during long-running tasks.
    bool incrementProgressValue(qlonglong increment = 1) const {
        // Temporarily yield control back to the event loop to process UI events and keep the UI responsive during long-running tasks.
        processUIEvents();
        return Promise<>::incrementProgressValue(increment);
    }

    /// Override this method from the Promise class to keep the UI responsive during long-running tasks.
    void setProgressText(const QString& progressText) const {
        // Temporarily yield control back to the event loop to process UI events and keep the UI responsive during long-running tasks.
        processUIEvents();
        Promise<>::setProgressText(progressText);
    }

protected:

    /// Temporarily yield control back to the event loop to process UI events.
    void processUIEvents() const;

#if 0
    /// This object keeps the Qt event loop running while the operation is in progress.
    /// All main-thread operations must be completed before the application can quit.
    QEventLoopLocker _eventLoopLocker;
#endif
};

}   // End of namespace
