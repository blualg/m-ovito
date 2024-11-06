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
#include "Task.h"
#include "detail/TaskCallback.h"

namespace Ovito {

/**
 * Progress state information associated with a task that is being displayed in the UI.
 *
 * Tasks who which to report progress in the UI should create an instance of this class.
 * It serves as a communication channel between the task and the UI.
 */
class OVITO_CORE_EXPORT TaskProgress : public detail::TaskCallback<TaskProgress>
{
    Q_DISABLE_COPY_MOVE(TaskProgress)

public:

    /// A null progress state that ignores all progress reporting calls.
    /// It should be used in places where progress reporting is not needed.
    OVITO_CORE_EXPORT static TaskProgress Ignore; // static variable defined in source file Task.cpp.

public:

    /// Constructor for creating an object that ignores all progress reporting calls.
    /// Note: This constructor is not meant to be used directly. Use the static member "Ignore" instead.
    explicit TaskProgress(std::nullopt_t) noexcept {}

    /// Constructor that attached to an existing task object.
    /// The progress object is registered with the task's user interface
    /// and will be closed automatically when the task is finished.
    explicit TaskProgress(Task* task) noexcept {
        OVITO_ASSERT(task);
        OVITO_ASSERT(task->userInterface());
        if(!task->isFinished()) {
            registerProgress(task->userInterface().get());
            registerCallback(task, true);
        }
    }

    /// Constructor that attaches to the given abstract user interface.
    /// The progress indicator will be shown in the user interface.
    explicit TaskProgress(UserInterface* userInterface) noexcept {
        OVITO_ASSERT(userInterface);
        registerProgress(userInterface);
    }

    /// Constructor that attaches to the given abstract user interface.
    /// The progress indicator will be shown in the user interface.
    explicit TaskProgress(const std::shared_ptr<UserInterface>& userInterface) noexcept : TaskProgress(userInterface.get()) {}

    /// Destructor.
    ~TaskProgress() noexcept {
        unregisterProgress();
    }

    /// Sets the description of operation to be displayed in the UI.
    void setProgressText(const QString& progressText) {
        if(_mutex) {
            std::lock_guard<std::mutex> lock(*_mutex);
            _text = progressText;
            notifyUserInterface();
        }
    }

    /// Sets the current maximum value for progress reporting.
    /// The current progress value is reset to zero unless autoReset is false.
    void setProgressMaximum(qlonglong maximum, bool autoReset = true) {
        if(autoReset || _progressMaximum != maximum) {
            if(_mutex) {
                std::lock_guard<std::mutex> lock(*_mutex);
                _progressMaximum = maximum;
                _progressValue = 0;
                notifyUserInterface();
            }
        }
    }

    /// Sets the current progress value of the task.
    void setProgressValue(qlonglong progressValue) {
        this_task::throwIfCanceled();
        if(_mutex && progressValue != _progressValue) {
            std::lock_guard<std::mutex> lock(*_mutex);
            _progressValue = progressValue;
            notifyUserInterface();
        }
    }

    /// Increments the progress value of the task.
    void incrementProgressValue(qlonglong increment = 1) {
        this_task::throwIfCanceled();
        if(_mutex) {
            std::lock_guard<std::mutex> lock(*_mutex);
            _progressValue += increment;
            notifyUserInterface();
        }
    }

    /// Sets the current progress value of the task, generating update events only occasionally.
    void setProgressValueIntermittent(qlonglong progressValue, int updateEvery = 2000) {
        if(Q_UNLIKELY((progressValue % updateEvery) == 0))
            setProgressValue(progressValue);
        else
            this_task::throwIfCanceled();
    }

    /// Starts a sequence of sub-steps in the progress range of this task.
    /// This is used for long and complex operation, which consist of several logical sub-steps, each with a separate
    /// duration. Expects a vector of relative weights, one for each sub-step, which will be used to calculate the
    /// the total progress as sub-steps are completed.
    void beginProgressSubStepsWithWeights(std::vector<int> weights) {
        OVITO_ASSERT(std::accumulate(weights.cbegin(), weights.cend(), 0) > 0);
        if(_mutex) {
            std::lock_guard<std::mutex> lock(*_mutex);
            _subProgressStack.emplace_back(0, std::move(weights));
            _progressMaximum = 0;
            _progressValue = 0;
            notifyUserInterface();
        }
    }

    /// Convenience version of the function above, which creates *N* substeps, all with the same weight.
    void beginProgressSubSteps(int nsteps) { beginProgressSubStepsWithWeights(std::vector<int>(nsteps, 1)); }

    /// Completes the current sub-step in the sequence started with beginProgressSubSteps() or
    /// beginProgressSubStepsWithWeights() and moves to the next one.
    void nextProgressSubStep() {
        if(_mutex) {
            std::lock_guard<std::mutex> lock(*_mutex);
            OVITO_ASSERT(!_subProgressStack.empty());
            OVITO_ASSERT(_subProgressStack.back().first < _subProgressStack.back().second.size());
            _subProgressStack.back().first++;
            _progressMaximum = 0;
            _progressValue = 0;
            notifyUserInterface();
        }
    }

    /// Completes a sub-step sequence started with beginProgressSubSteps() or beginProgressSubStepsWithWeights().
    void endProgressSubSteps() {
        if(_mutex) {
            std::lock_guard<std::mutex> lock(*_mutex);
            OVITO_ASSERT(!_subProgressStack.empty());
            _subProgressStack.pop_back();
            _progressMaximum = 0;
            _progressValue = 0;
            notifyUserInterface();
        }
    }

    /// Computes overall progress of the task, taking into account nested sub-steps.
    std::pair<int, int> computeTotalProgress() const {
        float percentage;
        int totalProgressMaximum;
        if(_progressMaximum > 0) {
            percentage = (float)_progressValue / _progressMaximum;
            totalProgressMaximum = 1000;
        }
        else if(!_subProgressStack.empty()) {
            percentage = 0;
            totalProgressMaximum = 1000;
        }
        else {
            percentage = 0;
            totalProgressMaximum = 0;
        }
        for(auto level = _subProgressStack.crbegin(); level != _subProgressStack.crend(); ++level) {
            OVITO_ASSERT(level->first >= 0 && level->first <= level->second.size());
            int weightSum1 = std::accumulate(level->second.cbegin(), level->second.cbegin() + level->first, 0);
            int weightSum2 = std::accumulate(level->second.cbegin() + level->first, level->second.cend(), 0);
            percentage = ((float)weightSum1 + percentage * (level->first < level->second.size() ? level->second[level->first] : 0)) / (weightSum1 + weightSum2);
        }
        int totalProgressValue = static_cast<int>(percentage * totalProgressMaximum);
        return std::make_pair(totalProgressValue, totalProgressMaximum);
    }

    /// Returns the status text of the task.
    const QString& text() const { return _text; }

    /// Returns the next registered task in the linked list.
    TaskProgress* nextInList() const { return _next; }

    /// Returns the previous registered task in the linked list.
    TaskProgress* prevInList() const { return _prev; }

    /// Inserts this object into a linked list.
    void setNextInList(TaskProgress* next) { _next = next; }

    /// Inserts this object into a linked list.
    void setPrevInList(TaskProgress* prev) { _prev = prev; }

private:

    /// Task callback implementation.
    void taskStateChangedCallback(int state, Task::MutexLock& lock) noexcept {
        if(state & Task::Finished)
            unregisterProgress();
    }

    /// Registers this progress object with the abstract user interface.
    void registerProgress(UserInterface* ui) noexcept {
        _mutex = ui->taskProgressBegin(this);
        if(_mutex)
            _userInterface = ui;
    }

    /// Unregisters this progress object from the abstract user interface.
    void unregisterProgress() noexcept {
        if(_mutex && _userInterface) {
            std::lock_guard<std::mutex> lock(*_mutex);
            if(_userInterface) {
                _userInterface->taskProgressEnd(this);
                _userInterface = nullptr;
            }
        }
    }

    /// Notifies the abstract user interface that this task's progress information has changed.
    void notifyUserInterface() {
        OVITO_ASSERT(_mutex);
        if(_userInterface)
            _userInterface->taskProgressChanged(this);
    }

private:

    /// A mutex provided by the abstract user interface. It is used to manage
    /// concurrent access to the entire task list and to the member fields of this object.
    std::mutex* _mutex = nullptr;

    /// The abstract user interface this progress object is currently registered with.
    UserInterface* _userInterface = nullptr;

    /// Linked list pointers for the tasks registered with the same abstract user interface.
    TaskProgress* _next;
    TaskProgress* _prev;

    /// A text describing the current activity of the task.
    QString _text;

    /// Progress value (of the current sub-step).
    qlonglong _progressValue = 0;

    /// Maximum progress value (of the current sub-step).
    qlonglong _progressMaximum = 0;

    /// Nested progress sub-steps.
    std::vector<std::pair<int, std::vector<int>>> _subProgressStack;

    template<typename Derived> friend class detail::TaskCallback;
};

}   // End of namespace
