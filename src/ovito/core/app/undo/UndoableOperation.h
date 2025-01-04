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

namespace Ovito {

/**
 * \brief Abstract base class for records of undoable operations.
 *
 * All atomic operations or functions that modify the scene in same way
 * should register an UndoableOperation with the UndoStack using UndoStack::push().
 *
 * For each specific operation a sub-class of UndoableOperation should be defined that
 * allows the UndoStack to undo or to re-do the operation at a later time.
 *
 * Multiple atomic operations can be combined into a CompoundOperation. They can then be undone
 * or redone at once.
 */
class OVITO_CORE_EXPORT UndoableOperation
{
public:

    /// \brief Default constructor.
    UndoableOperation() = default;

    /// \brief A virtual destructor.
    ///
    /// The default implementation does nothing.
    virtual ~UndoableOperation() = default;

    /// \brief Provides a localized, human readable description of this operation.
    /// \return A localized string that describes the operation. It is shown in the
    ///         edit menu of the application.
    ///
    /// The default implementation returns a default string, but it should be overridden
    /// by all sub-classes.
    virtual QString displayName() const { return QStringLiteral("Undoable operation"); }

    /// \brief Undoes the operation encapsulated by this object.
    ///
    /// This method is called by the UndoStack to undo the operation.
    virtual void undo() = 0;

    /// \brief Re-apply the change, assuming that it had been undone before.
    ///
    /// This method is called by the UndoStack to re-do the operation.
    /// The default implementation calls undo(). That means, undo() must be implemented such
    /// that it works both ways.
    virtual void redo() { undo(); }

    /// Undoable operation records are not copyable or movable.
    UndoableOperation(const UndoableOperation& other) = delete;
    UndoableOperation(UndoableOperation&& other) = delete;
    UndoableOperation& operator=(const UndoableOperation& other) = delete;
    UndoableOperation& operator=(UndoableOperation&& other) = delete;
};

/**
 * \brief Container class that holds a sequence of UndoableOperation objects.
 */
class OVITO_CORE_EXPORT CompoundOperation final : public UndoableOperation
{
public:

    /// Returns the currently active compound operation (either while recording operations or while undoing/redoing an operation).
    static CompoundOperation*& current();

    /// Indicates whether undo recording is currently active.
    static bool isUndoRecording();

    /// Indicates whether previously recorded operations are currently being undo or redone.
    static bool isUndoingOrRedoing();

public:

    /// \brief Creates an empty compound operation with the given display name.
    /// \param name The localized and human-readable display name for this compound operation.
    explicit CompoundOperation(const QString& name) : _displayName(name) {}

#ifdef OVITO_DEBUG
    /// \brief Destructor.
    virtual ~CompoundOperation() { OVITO_ASSERT(!_isUndoingOrRedoing); }
#endif

    /// \brief Provides a localized, human readable description of this operation.
    /// \return A localized string that describes the operation. It is shown in the
    ///         edit menu of the application.
    virtual QString displayName() const override { return _displayName; }

    /// \brief Sets this operation's display name to a new string.
    /// \param newName The localized and human-readable display name for this compound operation.
    /// \sa displayName()
    void setDisplayName(const QString& newName) { _displayName = newName; }

    /// Undo the operations that were made.
    virtual void undo() override;

    /// Re-apply the changes after they have been undone before.
    virtual void redo() override;

    /// \brief Adds a sub-record to this compound operation.
    /// \param operation An instance of a UndoableOperation derived class that encapsulates
    ///                  the operation. The CompoundOperation becomes the owner of
    ///                  this object and is responsible for its deletion.
    void addOperation(std::unique_ptr<UndoableOperation> operation) {
        OVITO_ASSERT(!_isUndoingOrRedoing);
        _subOperations.push_back(std::move(operation));
    }

    /// \brief Indicates whether this UndoableOperation is significant or can be ignored.
    /// \return \c true if the CompoundOperation contains at least one sub-operation; \c false it is empty.
    bool isSignificant() const { return _subOperations.empty() == false; }

    /// \brief Removes all sub-operations from this compound operation.
    void clear() {
        OVITO_ASSERT(!_isUndoingOrRedoing);
        _subOperations.clear();
    }

    /// \brief Undo all operations up to the given position and the remove those operations from the container.
    void revertTo(int position);

    /// \brief Returns the number of operations recorded in this container.
    int count() const { return (int)_subOperations.size(); }

    /// For debugging purposes only.
    void debugPrint(int level);

private:

    /// List of contained operations.
    std::vector<std::unique_ptr<UndoableOperation>> _subOperations;

    /// Stores the display name of this compound passed to the constructor.
    QString _displayName;

    /// Indicates if the operations in this container are currently being undone or redone.
    bool _isUndoingOrRedoing = false;
};

/**
 * \brief A RAII helper class that suspends recording of undoable operations while it exists.
 *
 * Create an instance of this class on the stack to temporarily suspend recording of operations.
 */
class OVITO_CORE_EXPORT UndoSuspender
{
public:

    /// Constructor.
    explicit UndoSuspender(CompoundOperation* operation = nullptr) noexcept
        : _previous(std::exchange(CompoundOperation::current(), operation)) {}

    /// Destructor.
    ~UndoSuspender() noexcept { CompoundOperation::current() = _previous; }

private:

    CompoundOperation* _previous;
};

}   // End of namespace
