////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "UndoableOperation.h"

namespace Ovito {

/**
 * RAII helper class that begins a new compound operation.
 * Unless the operation is explicitly committed, the destructor of this class will undo all operations.
 */
class OVITO_CORE_EXPORT UndoableTransaction
{
public:

    /// Constructor.
    UndoableTransaction() = default;

    /// Constructor opening a new transaction.
    explicit UndoableTransaction(UserInterface& userInterface, const QString& undoOperationName) {
        begin(userInterface, undoOperationName);
    }

    /// Destructor reverts all operations recorded so far, unless commit() has been called.
    ~UndoableTransaction() {
        if(operation())
            cancel();
    }

    /// Opens a new transaction.
    void begin(UserInterface& userInterface, const QString& undoOperationName) {
        OVITO_ASSERT(!operation());
        _userInterface = userInterface.shared_from_this();
        _operation = std::make_unique<CompoundOperation>(undoOperationName);
    }

    /// Commits the recorded operations by placing them on the undo stack.
    void commit();

    /// Undo all actions recorded so far and keep the current transaction open.
    void revert();

    /// Undo all actions recorded after the given snapshot and keep the current transaction open.
    void revertTo(int snapshot);

    /// Undo all actions recorded so far and close the current transaction.
    void cancel();

    /// Returns the current number of recorded operations.
    int snapshot() const { OVITO_ASSERT(operation()); return operation()->count(); }

    /// Returns the CompoundOperation object managed by this class.
    CompoundOperation* operation() const { return _operation.get(); }

    /// Returns the user interface this transaction is associated with.
    UserInterface& userInterface() const { OVITO_ASSERT(_userInterface); return *_userInterface; }

private:

    std::shared_ptr<UserInterface> _userInterface;
    std::unique_ptr<CompoundOperation> _operation;
};

}   // End of namespace

#include <ovito/core/app/UserInterface.h>
