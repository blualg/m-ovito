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
#include "Task.h"

namespace Ovito {

/**
 * \brief The tasks that can report its progress.
 */
class OVITO_CORE_EXPORT ProgressingTask : public Task
{
public:

    /// Constructor.
    explicit ProgressingTask(State initialState = NoState, void* resultsStorage = nullptr) noexcept : Task(State(initialState | IsProgressing), resultsStorage) {}

    /// Returns the maximum value for progress reporting.
    qlonglong progressMaximum() const {
        const QMutexLocker lock(&taskMutex());
        return _totalProgressMaximum;
    }

    /// \brief Returns the current progress of the task.
    /// \return A value in the range 0 to progressMaximum().
    qlonglong progressValue() const {
        const QMutexLocker lock(&taskMutex());
        return _totalProgressValue;
    }

    /// \brief Returns the current status text of this task.
    /// \return A string describing the ongoing operation, which is displayed in the user interface.
    QString progressText() const {
        const QMutexLocker lock(&taskMutex());
        return _progressText;
    }

    /// \brief Sets the current maximum value for progress reporting. The current progress value is reset to zero unless autoReset is false.
    void setProgressMaximum(qlonglong maximum, bool autoReset = true);

    /// \brief Sets the current progress value of the task.
    /// \param progressValue The new value, which must be in the range 0 to progressMaximum().
    /// \return false if the task has been canceled.
    bool setProgressValue(qlonglong progressValue);

    /// \brief Increments the progress value of the task.
    /// \param increment The number of progress units to add to the current progress value.
    /// \return false if the task has been canceled.
    bool incrementProgressValue(qlonglong increment = 1);

    /// \brief Sets the current progress value of the task, generating update events only occasionally.
    /// \param progressValue The new value, which must be in the range 0 to progressMaximum().
    /// \param updateEvery Generate an update event only after the method has been called this many times.
    /// \return false if the task has been canceled.
    bool setProgressValueIntermittent(qlonglong progressValue, int updateEvery = 2000);

    /// \brief Changes the description of this task to be displayed in the GUI.
    /// \param progressText The text string that will be displayed in the user interface to describe the operation in progress.
    void setProgressText(const QString& progressText);

    /// \brief Starts a sequence of sub-steps in the progress range of this task.
    ///
    /// This is used for long and complex operation, which consist of several logical sub-steps, each with a separate
    /// duration.
    ///
    /// \param weights A vector of relative weights, one for each sub-step, which will be used to calculate the
    ///                the total progress as sub-steps are completed.
    void beginProgressSubStepsWithWeights(std::vector<int> weights);

    /// \brief Convenience version of the function above, which creates *N* substeps, all with the same weight.
    /// \param nsteps The number of sub-steps in the sequence.
    void beginProgressSubSteps(int nsteps) { beginProgressSubStepsWithWeights(std::vector<int>(nsteps, 1)); }

    /// \brief Completes the current sub-step in the sequence started with beginProgressSubSteps() or
    ///        beginProgressSubStepsWithWeights() and moves to the next one.
    void nextProgressSubStep();

    /// \brief Completes a sub-step sequence started with beginProgressSubSteps() or beginProgressSubStepsWithWeights().
    ///
    /// Call this method after the last sub-step has been completed.
    void endProgressSubSteps();

protected:

    /// Recomputes the total progress made so far based on the progress of the current sub-task.
    void updateTotalProgress();

    /// The progress value of the current sub-task.
    qlonglong _progressValue = 0;
    
    /// The maximum progress value of the current sub-task.
    qlonglong _progressMaximum = 0;

    /// The progress value of the task.
    qlonglong _totalProgressValue = 0;

    /// The maximum progress value of the task.
    qlonglong _totalProgressMaximum = 0;

    /// A description of what this task is currently doing, to be displayed in the GUI.
    QString _progressText;

    /// Keeps track of nested sub-tasks and their current progress.
    std::vector<std::pair<int, std::vector<int>>> _subTaskProgressStack;
    
    int _intermittentUpdateCounter = 0;
    
    QElapsedTimer _progressTime;
};

}   // End of namespace
