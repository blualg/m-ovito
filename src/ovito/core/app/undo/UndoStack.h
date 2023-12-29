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
#include "UndoableOperation.h"

namespace Ovito {

/**
 * \brief Stores and manages the undo stack.
 *
 * The UndoStack records all user operations. Operations can be undone or reversed
 * one by one.
 */
class OVITO_CORE_EXPORT UndoStack : public QObject
{
    Q_OBJECT

public:

    /// Constructor.
    explicit UndoStack(UserInterface& userInterface, QObject* parent = nullptr);

    /// \brief Records an operation.
    /// \param operation The operation to put on the stack.
    void push(std::unique_ptr<CompoundOperation> operation);

    /// \brief Returns true if there is an operation available for undo; otherwise returns false.
    bool canUndo() const { return index() >= 0; }

    /// \brief Returns true if there is an operation available for redo; otherwise returns false.
    bool canRedo() const { return index() < count() - 1; }

    /// \brief Returns the text of the command which will be undone in the next call to undo().
    QString undoText() const { return canUndo() ? _operations[index()]->displayName() : QString(); }

    /// \brief Returns the text of the command which will be redone in the next call to redo().
    QString redoText() const { return canRedo() ? _operations[index() + 1]->displayName() : QString(); }

    /// \brief Returns the index of the current operation.
    ///
    /// This is the operation that will be undone on the next call to undo().
    /// It is not always the top-most operation on the stack, since a number of operations may have been undone.
    int index() const { return _index; }

    /// \brief Returns the number of operations on the stack. Compound operations are counted as one operation.
    int count() const { return (int)_operations.size(); }

    /// \brief If the stack is in the clean state, returns true; otherwise returns false.
    bool isClean() const { return index() == cleanIndex(); }

    /// \brief Returns the clean index.
    int cleanIndex() const { return _cleanIndex; }

    /// \brief Gets the maximum number of undo steps to hold in memory.
    /// \return The maximum number of steps the UndoStack maintains.
    ///         A negative value means infinite number of undo steps.
    ///
    /// If the maximum number of undo steps is reached then the oldest operation at the bottom of the
    /// stack are removed.
    int undoLimit() const { return _undoLimit; }

    /// \brief Sets the maximum number of undo steps to hold in memory.
    /// \param steps The maximum height of the undo stack.
    ///              A negative value means infinite number of undo steps.
    void setUndoLimit(int steps) { _undoLimit = steps; limitUndoStack(); }

    /// \brief Shrinks the undo stack to maximum number of undo steps.
    ///
    /// If the current stack is higher then the maximum number of steps then the oldest entries
    /// from the bottom of the stack are removed.
    void limitUndoStack();

    /// \brief Prints a text representation of the undo stack to the console. This is for debugging purposes only.
    void debugPrint();

public Q_SLOTS:

    /// \brief Resets the undo stack.
    void clear();

    /// \brief Undoes the last operation in the undo stack.
    void undo();

    /// \brief Re-does the last undone operation in the undo stack.
    void redo();

    /// \brief Marks the stack as clean and emits cleanChanged() if the stack was not already clean.
    void setClean();

    /// \brief Marks the stack as dirty and emits cleanChanged() if the stack was not already dirty.
    void setDirty();

Q_SIGNALS:

    /// This signal is emitted whenever the value of canUndo() changes.
    void canUndoChanged(bool canUndo);

    /// This signal is emitted whenever the value of canRedo() changes.
    void canRedoChanged(bool canRedo);

    /// This signal is emitted whenever the value of undoText() changes.
    void undoTextChanged(const QString& undoText);

    /// This signal is emitted whenever the value of redoText() changes.
    void redoTextChanged(const QString& redoText);

    /// This signal is emitted whenever an operation modifies the state of the document.
    void indexChanged(int index);

    /// This signal is emitted whenever the stack enters or leaves the clean state.
    void cleanChanged(bool clean);

private:

    /// The user interface this stack belongs to.
    UserInterface& _userInterface;

    /// The stack with records of undoable operations.
    std::deque<std::unique_ptr<CompoundOperation>> _operations;

    /// Current position in the undo stack. This is where
    /// new undoable edits will be inserted.
    int _index = -1;

    /// The state which has been marked as clean.
    int _cleanIndex = -1;

    /// Maximum number of records in the undo stack.
    int _undoLimit = 20;
};

}   // End of namespace
