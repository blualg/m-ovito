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

#include <ovito/core/Core.h>
#include "ProgressingTask.h"
#include "detail/TaskCallback.h"

namespace Ovito {

constexpr static int MaxProgressEmitsPerSecond = 10;

/******************************************************************************
* Sets the current maximum value for progress reporting.
* The current progress value is reset to zero unless autoReset is false.
******************************************************************************/
void ProgressingTask::setProgressMaximum(qlonglong maximum, bool autoReset)
{
    if(!autoReset && _progressMaximum == maximum)
        return;

    const QMutexLocker locker(&taskMutex());

    _progressMaximum = maximum;
    _progressValue = 0;

    updateTotalProgress();

    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList)
        cb->callProgressChanged(_totalProgressValue, _totalProgressMaximum);
}

/******************************************************************************
* Sets the current progress value of the task.
******************************************************************************/
bool ProgressingTask::setProgressValue(qlonglong value)
{
    // When in the main thread, temporarily yield control back to the event loop to process UI events and
    // keep the UI responsive during long-running tasks.
    if(!(_state.load(std::memory_order_relaxed) & (DontYield | IsAsynchronous)) && ExecutionContext::isMainThread()) {
        const ExecutionContext& context = ExecutionContext::current();
        if(context.isValid() && context.ui().processUIEvents()) {
            cancel();
        }
    }

    const QMutexLocker locker(&taskMutex());

    auto state = _state.load(std::memory_order_relaxed);
    if(state & (Canceled | Finished) || value == _progressValue)
        return !(state & Canceled);

    _progressValue = value;
    updateTotalProgress();

    if(!_progressTime.isValid() || _totalProgressValue >= _totalProgressMaximum || _progressTime.elapsed() >= (1000 / MaxProgressEmitsPerSecond)) {
        _progressTime.start();

        for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList)
            cb->callProgressChanged(_totalProgressValue, _totalProgressMaximum);
    }

    return !(state & Canceled);
}

/******************************************************************************
* Increments the progress value of the task.
******************************************************************************/
bool ProgressingTask::incrementProgressValue(qlonglong increment)
{
    // When in the main thread, temporarily yield control back to the event loop to process UI events and
    // keep the UI responsive during long-running tasks.
    if(!(_state.load(std::memory_order_relaxed) & (DontYield | IsAsynchronous)) && ExecutionContext::isMainThread()) {
        const ExecutionContext& context = ExecutionContext::current();
        if(context.isValid() && context.ui().processUIEvents()) {
            cancel();
        }
    }

    const QMutexLocker locker(&taskMutex());

    auto state = _state.load(std::memory_order_relaxed);
    if(state & (Canceled | Finished))
        return !(state & Canceled);

    _progressValue += increment;
    updateTotalProgress();

    if(!_progressTime.isValid() || _totalProgressValue >= _totalProgressMaximum || _progressTime.elapsed() >= (1000 / MaxProgressEmitsPerSecond)) {
        _progressTime.start();

        for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList)
            cb->callProgressChanged(_totalProgressValue, _totalProgressMaximum);
    }

    return !(state & Canceled);
}

/******************************************************************************
* Sets the current progress value of the task, generating update events only occasionally.
******************************************************************************/
bool ProgressingTask::setProgressValueIntermittent(qlonglong progressValue, int updateEvery)
{
    if(_intermittentUpdateCounter >= updateEvery) {
        _intermittentUpdateCounter = 0;
        return setProgressValue(progressValue);
    }
    else {
        _intermittentUpdateCounter++;
        return !isCanceled();
    }
}

/******************************************************************************
* Changes the description of this task to be displayed in the GUI.
******************************************************************************/
void ProgressingTask::setProgressText(const QString& progressText)
{
    // When in the main thread, temporarily yield control back to the event loop to process UI events and
    // keep the UI responsive during long-running tasks.
    if(!(_state.load(std::memory_order_relaxed) & (DontYield | IsAsynchronous)) && ExecutionContext::isMainThread()) {
        const ExecutionContext& context = ExecutionContext::current();
        if(context.isValid() && context.ui().processUIEvents()) {
            cancel();
        }
    }

    const QMutexLocker locker(&taskMutex());

    auto state = _state.load(std::memory_order_relaxed);
    if(state & (Canceled | Finished))
        return;

    _progressText = progressText;

    // Print task messages to the console if logging is enabled.
    if((state & LoggingEnabled) && !progressText.isEmpty())
        qInfo().noquote() << "OVITO:" << progressText;

    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList)
        cb->callTextChanged();
}

/******************************************************************************
* Recomputes the total progress made so far based on the progress of the current sub-task.
******************************************************************************/
void ProgressingTask::updateTotalProgress()
{
    if(_subTaskProgressStack.empty()) {
        _totalProgressMaximum = _progressMaximum;
        _totalProgressValue = _progressValue;
    }
    else {
        double percentage;
        if(_progressMaximum > 0)
            percentage = (double)_progressValue / _progressMaximum;
        else
            percentage = 0;
        for(auto level = _subTaskProgressStack.crbegin(); level != _subTaskProgressStack.crend(); ++level) {
            OVITO_ASSERT(level->first >= 0 && level->first <= level->second.size());
            int weightSum1 = std::accumulate(level->second.cbegin(), level->second.cbegin() + level->first, 0);
            int weightSum2 = std::accumulate(level->second.cbegin() + level->first, level->second.cend(), 0);
            percentage = ((double)weightSum1 + percentage * (level->first < level->second.size() ? level->second[level->first] : 0)) / (weightSum1 + weightSum2);
        }
        _totalProgressMaximum = 1000;
        _totalProgressValue = (qlonglong)(percentage * 1000.0);
    }
}

/******************************************************************************
* Starts a sequence of sub-steps in the progress range of this task.
******************************************************************************/
void ProgressingTask::beginProgressSubStepsWithWeights(std::vector<int> weights)
{
    OVITO_ASSERT(std::accumulate(weights.cbegin(), weights.cend(), 0) > 0);

    _subTaskProgressStack.emplace_back(0, std::move(weights));
    _progressMaximum = 0;
    _progressValue = 0;
}

/******************************************************************************
* Completes the current sub-step in the sequence started with beginProgressSubSteps()
* or beginProgressSubStepsWithWeights() and moves to the next one.
******************************************************************************/
void ProgressingTask::nextProgressSubStep()
{
    const QMutexLocker locker(&taskMutex());

    if(auto state = _state.load(std::memory_order_relaxed); state & (Canceled | Finished))
        return;

    OVITO_ASSERT(!_subTaskProgressStack.empty());
    OVITO_ASSERT(_subTaskProgressStack.back().first < _subTaskProgressStack.back().second.size());
    _subTaskProgressStack.back().first++;

    _progressMaximum = 0;
    _progressValue = 0;
    updateTotalProgress();

    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList)
        cb->callProgressChanged(_totalProgressValue, _totalProgressMaximum);
}

/******************************************************************************
* Completes a sub-step sequence started with beginProgressSubSteps() or
* beginProgressSubStepsWithWeights().
******************************************************************************/
void ProgressingTask::endProgressSubSteps()
{
    OVITO_ASSERT(!_subTaskProgressStack.empty());
    _subTaskProgressStack.pop_back();
    _progressMaximum = 0;
    _progressValue = 0;
}

}   // End of namespace
