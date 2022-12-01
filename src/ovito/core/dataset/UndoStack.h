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
};

/**
 * \brief Stores and manages the undo stack.
 *
 * The UndoStack records all user operations. Operations can be undone or reversed
 * one by one.
 */
class OVITO_CORE_EXPORT UndoStack : public QObject
{
	Q_OBJECT

#ifdef OVITO_QML_GUI
	Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
	Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
	Q_PROPERTY(QString undoText READ undoText NOTIFY undoTextChanged)
	Q_PROPERTY(QString redoText READ redoText NOTIFY redoTextChanged)
	Q_PROPERTY(bool isClean READ isClean NOTIFY cleanChanged)
#endif

public:

	/// Constructor.
	explicit UndoStack(UserInterface& userInterface, QObject* parent = nullptr);

	/// \brief Begins composition of a macro command with the given text description.
	/// \param displayName A human-readable name that is shown in the edit menu to describe the operation.
	///
	/// \note Each call to beginCompoundOperation() must be followed by a call to
	///       endCompoundOperation() to commit the operation. Multiple compound operations
	///       can be nested by multiple calls to beginCompoundOperation() followed by the same
	///       number of calls to endCompoundOperation().
	void beginCompoundOperation(const QString& displayName);

	/// \brief Ends composition of a macro command.
	/// \param commit If true, the macro operation is put on the undo stack. If false,
	///        all actions of the macro operation are undone, and nothing is put on the undo stack.
	void endCompoundOperation(bool commit = true);

	/// \brief Undoes all actions of the current compound operation.
	void resetCurrentCompoundOperation();

	/// \brief Returns whether the manager is currently recording undoable operations.
	/// \return \c true if the UndoStack currently records any changes made to the scene on its stack.
	///         \c false if changes to the scene are ignored by the UndoStack.
	///
	/// The recording state can be controlled via the suspend() and resume() methods.
	/// Or it can be temporarily suspended using the UndoSuspender helper class.
	bool isRecording() const { 
		OVITO_ASSERT_MSG(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread(), "UndoStack::isRecording()", "This method must only be called from the main thread.");
		return !isSuspended() && _compoundStack.empty() == false; 
	}

	/// \brief Returns whether the manager is currently recording undoable operations.
	/// \return \c true if this method is called from the main thread and if the UndoStack currently records any changes made to the scene on its stack.
	///         \c false if changes to the scene are ignored by the UndoStack or if this method is called from a worker thread.
	bool isRecordingThread() const {
		return (QThread::currentThread() == this->thread()) && isRecording(); 
	}

	/// \brief Records a single operation.
	/// \param operation An instance of a UndoableOperation derived class that encapsulates
	///                  the operation. The UndoStack becomes the owner of
	///                  this object and is responsible for its deletion.
	void push(std::unique_ptr<UndoableOperation> operation);

	/// \brief Pushes an operation onto the undo stack if the undo stack is currently recording.
	/// The undo record is created only if the undo stack is recording.
	template<class UndoableOperationClass, class... Args>
	void pushIfRecording(Args&&... args) {
		if(isRecording())
			push(std::make_unique<UndoableOperationClass>(std::forward<Args>(args)...));
	}

	/// \brief Suspends the recording of undoable operations.
	///
	/// Recording of operations is suspended by this method until a call to resume().
	/// If suspend() is called multiple times then resume() must be called the same number of
	/// times until recording is enabled again.
	///
	/// It is recommended to use the UndoSuspender helper class to suspend recording because
	/// this is more exception save than the suspend()/resume() combination.
	void suspend() { _suspendCount++; }

	/// \brief Returns true if the recording of operations is currently suspended.
	bool isSuspended() const { return _suspendCount != 0; }

	/// \brief Resumes the recording of undoable operations.
	///
	/// This re-enables recording of undoable operations after it has
	/// been suspended by a call to suspend().
	void resume() {
		OVITO_ASSERT_MSG(_suspendCount > 0, "UndoStack::resume()", "resume() has been called more often than suspend().");
		_suspendCount--;
	}

	/// \brief Indicates whether the undo stack is currently undoing a recorded operation.
	/// \return \c true if the UndoStack is currently restoring the application state before a user
	///         operation. This is usually due to a call to undo();
	///         \c false otherwise.
	bool isUndoing() const { return _isUndoing; }

	/// \brief Indicates whether the undo stack is currently redoing a previously undone operation.
	/// \return \c true if the UndoStack is currently replaying a recorded operation.
	///         This is usually due to a call to redo();
	///         \c false otherwise.
	bool isRedoing() const { return _isRedoing; }

	/// \brief Indicates whether the undo stack is currently undoing or redoing a recorded operation.
	/// \return \c true if currently an operation from the undo stack is being undone or redone, i.e.
	///         isUndoing() or isRedoing() returns \c true; \c false otherwise.
	bool isUndoingOrRedoing() const { return isUndoing() || isRedoing(); }

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

	/**
	 * \brief This class is used to combine multiple UndoableOperation objects into one.
	 */
	class CompoundOperation : public UndoableOperation
	{
	public:

		/// \brief Creates an empty compound operation with the given display name.
		/// \param name The localized and human-readable display name for this compound operation.
		CompoundOperation(const QString& name) : _displayName(name) {}

		/// \brief Provides a localized, human readable description of this operation.
		/// \return A localized string that describes the operation. It is shown in the
		///         edit menu of the application.
		virtual QString displayName() const override { return _displayName; }

		/// \brief Sets this operation's display name to a new string.
		/// \param newName The localized and human-readable display name for this compound operation.
		/// \sa displayName()
		void setDisplayName(const QString& newName) { _displayName = newName; }

		/// Undo the edit operation that was made.
		virtual void undo() override;

		/// Re-apply the change, assuming that it has been undone.
		virtual void redo() override;

		/// \brief Adds a sub-record to this compound operation.
		/// \param operation An instance of a UndoableOperation derived class that encapsulates
		///                  the operation. The CompoundOperation becomes the owner of
		///                  this object and is responsible for its deletion.
		void addOperation(std::unique_ptr<UndoableOperation> operation) { _subOperations.push_back(std::move(operation)); }

		/// \brief Indicates whether this UndoableOperation is significant or can be ignored.
		/// \return \c true if the CompoundOperation contains at least one sub-operation; \c false it is empty.
		bool isSignificant() const { return _subOperations.empty() == false; }

		/// \brief Removes all sub-operations from this compound operation.
		void clear() { _subOperations.clear(); }

		/// For debugging purposes only.
		void debugPrint(int level);

	private:

		/// List of contained operations.
		std::vector<std::unique_ptr<UndoableOperation>> _subOperations;

		/// Stores the display name of this compound passed to the constructor.
		QString _displayName;
	};

private:

	/// The user interface this stack belongs to.
	UserInterface& _userInterface;

	/// The stack with records of undoable operations.
	std::deque<std::unique_ptr<UndoableOperation>> _operations;

	/// A call to suspend() increases this value by one.
	/// A call to resume() decreases it.
	int _suspendCount = 0;

	/// Current position in the undo stack. This is where
	/// new undoable edits will be inserted.
	int _index = -1;

	/// The state which has been marked as clean.
	int _cleanIndex = -1;

	/// The stack of open compound records.
	std::vector<std::unique_ptr<CompoundOperation>> _compoundStack;

	/// Maximum number of records in the undo stack.
	int _undoLimit = 20;

	/// Indicates if we are currently undoing an operation.
	bool _isUndoing = false;

	/// Indicates if we are currently redoing an operation.
	bool _isRedoing = false;

	friend class UndoSuspender;
};

/**
 * \brief A RAII helper class that suspends recording of undoable operations while it exists.
 *
 * The constructor of this class calls UndoStack::suspend() and
 * the destructor calls UndoStack::resume().
 *
 * Create an instance of this class on the stack to temporarily suspend recording of operations.
 */
class OVITO_CORE_EXPORT UndoSuspender 
{
public:
	UndoSuspender(UndoStack& undoStack) noexcept;
	UndoSuspender() noexcept;
	~UndoSuspender() { reset(); }
	void reset() noexcept;
private:
	int* _suspendCount;
};

/**
 * RAII helper class that begins a new compound operation.
 * Unless the operation is explicitly committed, the destructor of this class will undo all operations.
 */
class OVITO_CORE_EXPORT UndoableTransaction
{
public:

	/// Constructor calling UndoStack::beginCompoundOperation().
	explicit UndoableTransaction(UserInterface& userInterface, const QString& displayName);

	/// Destructor undoing all recorded operations unless commit() was called.
	~UndoableTransaction();

	/// Commits all recorded operations by calling UndoStack::endCompoundOperation().
	void commit();

	/// \brief Pushes an operation onto the undo stack if the undo stack is currently recording.
	/// The undo record is created only if the undo stack is recording.
	template<class UndoableOperationClass, class... Args>
	void pushIfRecording(Args&&... args) {
		if(_undoStack && _undoStack->isRecording())
			_undoStack->push(std::make_unique<UndoableOperationClass>(std::forward<Args>(args)...));
	}

private:

	UndoStack* _undoStack = nullptr;
	bool _committed = false;
};

}	// End of namespace
