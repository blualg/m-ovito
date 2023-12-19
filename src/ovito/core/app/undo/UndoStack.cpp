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

#include <ovito/core/Core.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/app/UserInterface.h>
#include "UndoStack.h"

namespace Ovito {

/******************************************************************************
* Initializes the undo manager.
******************************************************************************/
UndoStack::UndoStack(UserInterface& userInterface, QObject* parent) : QObject(parent), _userInterface(userInterface)
{
}

/******************************************************************************
* Records an operation.
* This object will be put onto the undo stack.
******************************************************************************/
void UndoStack::push(std::unique_ptr<CompoundOperation> operation)
{
    OVITO_ASSERT(operation);
    OVITO_ASSERT_MSG(ExecutionContext::isMainThread(), "UndoStack::push()", "This function may only be called from the main thread.");
    OVITO_ASSERT_MSG(CompoundOperation::isUndoingOrRedoing() == false, "UndoStack::push()", "Cannot record an operation while undoing or redoing another operation.");
    OVITO_ASSERT(CompoundOperation::isUndoRecording() == false);
    OVITO_ASSERT(!CompoundOperation::current());

    // Discard previously undone operations.
    _operations.resize(index() + 1);
    if(cleanIndex() > index())
        _cleanIndex = -1;

    _operations.push_back(std::move(operation));
    _index++;
    OVITO_ASSERT(index() == count() - 1);
    limitUndoStack();
    Q_EMIT indexChanged(index());
    Q_EMIT cleanChanged(false);
    Q_EMIT canUndoChanged(true);
    Q_EMIT undoTextChanged(undoText());
    Q_EMIT canRedoChanged(false);
    Q_EMIT redoTextChanged(QString());
}

/******************************************************************************
* Shrinks the undo stack to maximum number of undo steps.
******************************************************************************/
void UndoStack::limitUndoStack()
{
    OVITO_ASSERT(CompoundOperation::isUndoingOrRedoing() == false);
    OVITO_ASSERT(CompoundOperation::isUndoRecording() == false);
    OVITO_ASSERT(!CompoundOperation::current());

    if(_undoLimit < 0)
        return;
    int n = count() - _undoLimit;
    if(n > 0) {
        if(index() >= n) {
            _operations.erase(_operations.begin(), _operations.begin() + n);
            _index -= n;
            Q_EMIT indexChanged(index());
        }
    }
}

/******************************************************************************
* Resets the undo system. The undo stack will be cleared.
******************************************************************************/
void UndoStack::clear()
{
    OVITO_ASSERT(CompoundOperation::isUndoingOrRedoing() == false);

    _operations.clear();
    _index = -1;
    _cleanIndex = -1;
    Q_EMIT indexChanged(index());
    Q_EMIT cleanChanged(isClean());
    Q_EMIT canUndoChanged(false);
    Q_EMIT canRedoChanged(false);
    Q_EMIT undoTextChanged(QString());
    Q_EMIT redoTextChanged(QString());
}

/******************************************************************************
* Marks the stack as clean and emits cleanChanged() if the stack was not already clean.
******************************************************************************/
void UndoStack::setClean()
{
    if(!isClean()) {
        _cleanIndex = index();
        Q_EMIT cleanChanged(true);
    }
}

/******************************************************************************
* Marks the stack as dirty and emits cleanChanged() if the stack was not already dirty.
******************************************************************************/
void UndoStack::setDirty()
{
    bool signal = isClean();
    _cleanIndex = -2;
    if(signal)
        Q_EMIT cleanChanged(false);
}

/******************************************************************************
* Undoes the last operation in the undo stack.
******************************************************************************/
void UndoStack::undo()
{
    OVITO_ASSERT(CompoundOperation::isUndoingOrRedoing() == false);
    OVITO_ASSERT(CompoundOperation::isUndoRecording() == false);
    OVITO_ASSERT(!CompoundOperation::current());

    if(!canUndo())
        return;

    CompoundOperation* curOp = _operations[index()].get();
    _userInterface.handleExceptions([&] {
        curOp->undo();
    });
    _index--;
    Q_EMIT indexChanged(index());
    Q_EMIT cleanChanged(isClean());
    Q_EMIT canUndoChanged(canUndo());
    Q_EMIT undoTextChanged(undoText());
    Q_EMIT canRedoChanged(canRedo());
    Q_EMIT redoTextChanged(redoText());
}

/******************************************************************************
* Redoes the last undone operation in the undo stack.
******************************************************************************/
void UndoStack::redo()
{
    OVITO_ASSERT(CompoundOperation::isUndoingOrRedoing() == false);
    OVITO_ASSERT(CompoundOperation::isUndoRecording() == false);
    OVITO_ASSERT(!CompoundOperation::current());

    if(!canRedo())
        return;

    CompoundOperation* nextOp = _operations[index() + 1].get();
    _userInterface.handleExceptions([&] {
        nextOp->redo();
    });
    _index++;
    Q_EMIT indexChanged(index());
    Q_EMIT cleanChanged(isClean());
    Q_EMIT canUndoChanged(canUndo());
    Q_EMIT undoTextChanged(undoText());
    Q_EMIT canRedoChanged(canRedo());
    Q_EMIT redoTextChanged(redoText());
}

/******************************************************************************
* Prints a text representation of the undo stack to the console.
* This is for debugging purposes only.
******************************************************************************/
void UndoStack::debugPrint()
{
    qDebug() << "Undo stack (index=" << _index << "clean index=" << _cleanIndex << "):";
    int index = 0;
    for(const auto& op : _operations) {
        qDebug() << "  " << index << ":" << qPrintable(op->displayName());
        if(CompoundOperation* compOp = dynamic_cast<CompoundOperation*>(op.get())) {
            compOp->debugPrint(2);
        }
        index++;
    }
}

}   // End of namespace
