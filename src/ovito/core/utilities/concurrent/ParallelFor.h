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
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include "ProgressingTask.h"
#include "LaunchTask.h"

#ifndef OVITO_DISABLE_THREADING
    #include <future>
#endif

namespace Ovito {

template<typename T, class Function>
bool parallelForWithProgress(
        T loopCount,
        Function&& kernel,
        T progressChunkSize = 1024)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::get()->isProgressingTask());
    ProgressingTask* task = static_cast<ProgressingTask*>(this_task::get());
    task->setProgressMaximum(loopCount / progressChunkSize);

#ifndef OVITO_DISABLE_THREADING
    std::vector<std::future<void>> workers;
    size_t num_threads = Application::instance()->idealThreadCount();
    T chunkSize = loopCount / num_threads;
    T startIndex = 0;
    T endIndex = chunkSize;
    for(size_t t = 0; t < num_threads; t++) {
        if(t == num_threads - 1)
            endIndex += loopCount % num_threads;
        workers.push_back(std::async(std::launch::async, [&kernel, startIndex, endIndex, progressChunkSize, context=ExecutionContext::current(), task]() mutable {
            Task::Scope taskScope(task);
            ExecutionContext::Scope execScope(std::move(context));
            for(T i = startIndex; i < endIndex;) {
                // Execute kernel.
                kernel(i);

                i++;

                // Update progress indicator.
                if((i % progressChunkSize) == 0) {
                    OVITO_ASSERT(i != 0);
                    task->incrementProgressValue();
                }
                if(task->isCanceled())
                    return;
            }
        }));
        startIndex = endIndex;
        endIndex += chunkSize;
    }

    for(auto& t : workers)
        t.wait();
    for(auto& t : workers)
        t.get();
#else
    for(T i = 0; i < loopCount; ) {
        // Execute kernel.
        kernel(i);
        i++;

        // Update progress indicator.
        if((i % progressChunkSize) == 0) {
            OVITO_ASSERT(i != 0);
            task->incrementProgressValue();
        }
        if(task->isCanceled())
            break;
    }
#endif

    task->incrementProgressValue(loopCount % progressChunkSize);
    return !task->isCanceled();
}

template<typename T, class Function>
void parallelFor(T loopCount, Function&& kernel)
{
#ifndef OVITO_DISABLE_THREADING
    std::vector<std::future<void>> workers;
    size_t num_threads = Application::instance()->idealThreadCount();
    if(num_threads > loopCount) {
        if(loopCount <= 0) return;
        num_threads = loopCount;
    }
    T chunkSize = loopCount / num_threads;
    T startIndex = 0;
    T endIndex = chunkSize;
    for(size_t t = 0; t < num_threads; t++) {
        if(t == num_threads - 1) {
            OVITO_ASSERT(endIndex + (loopCount % num_threads) == loopCount);
            endIndex = loopCount;
            for(T i = startIndex; i < endIndex; ++i) {
                kernel(i);
            }
        }
        else {
            OVITO_ASSERT(endIndex <= loopCount);
            workers.push_back(std::async(std::launch::async, [&kernel, startIndex, endIndex, context=ExecutionContext::current(), task=this_task::get()]() mutable {
                Task::Scope taskScope(std::move(task));
                ExecutionContext::Scope execScope(std::move(context));
                for(T i = startIndex; i < endIndex; ++i) {
                    kernel(i);
                }
            }));
        }
        startIndex = endIndex;
        endIndex += chunkSize;
    }

    for(auto& t : workers)
        t.wait();
    for(auto& t : workers)
        t.get();
#else
    for(T i = 0; i < loopCount; ++i) {
        kernel(i);
    }
#endif
}

template<typename T, class Kernel>
Future<> parallelForAsync(T loopCount, Kernel&& kernel, const QString& taskDescription, T progressChunkSize = 1024)
{
    class AsyncTask : public ProgressingTask
    {
    public:
        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = Future<>;

        AsyncTask(Kernel&& kernel, const QString& taskDescription) : ProgressingTask(Task::Started), _kernel(std::forward<Kernel>(kernel)) {
            setProgressText(taskDescription);
        }

        void operator()(T loopCount, T progressChunkSize)
        {
            setProgressMaximum(loopCount / progressChunkSize);
            size_t num_threads = Application::instance()->idealThreadCount();
            _remainingThreads.store(num_threads);
            T chunkSize = loopCount / num_threads;
            T startIndex = 0;
            T endIndex = chunkSize;
            for(size_t t = 0; t < num_threads; t++) {
                if(t == num_threads - 1)
                    endIndex += loopCount % num_threads;
                std::thread([context = ExecutionContext::current(), self = static_pointer_cast<AsyncTask>(this->shared_from_this()), startIndex, endIndex, progressChunkSize]() mutable {
                    Task::Scope taskScope(self);
                    ExecutionContext::Scope execScope(std::move(context));
                    try {
                        for(T i = startIndex; i < endIndex && !self->isCanceled(); ) {
                            self->_kernel(i++);
                            if((i % progressChunkSize) == 0) {
                                OVITO_ASSERT(i != 0);
                                self->incrementProgressValue();
                            }
                        }
                        // Last thread is responsible for finishing the master task object.
                        if(self->_remainingThreads.fetch_sub(1) == 1) {
                            self->setProgressValue(self->progressMaximum());
                            self->setFinished();
                        }
                    }
                    catch(...) {
                        self->captureException();
                        self->setFinished();
                    }
                }).detach();
                startIndex = endIndex;
                endIndex += chunkSize;
            }
        }
    private:
        std::decay_t<Kernel> _kernel;
        std::atomic<size_t> _remainingThreads;
    };

    // Launch the task and return a future to the caller.
    return launchTask<true>(
        std::make_shared<AsyncTask>(std::forward<Kernel>(kernel), taskDescription),
        loopCount, progressChunkSize);
}

template<class Function>
bool parallelForChunksWithProgress(size_t loopCount, Function kernel)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::get()->isProgressingTask());
    ProgressingTask* task = static_cast<ProgressingTask*>(this_task::get());

#ifndef OVITO_DISABLE_THREADING
    std::vector<std::future<void>> workers;
    size_t num_threads = Application::instance()->idealThreadCount();
    if(num_threads > loopCount) {
        if(loopCount <= 0) return true;
        num_threads = loopCount;
    }
    size_t chunkSize = loopCount / num_threads;
    size_t startIndex = 0;
    for(size_t t = 0; t < num_threads; t++) {
        if(t == num_threads - 1) {
            chunkSize += loopCount % num_threads;
            OVITO_ASSERT(startIndex + chunkSize == loopCount);
            kernel(startIndex, chunkSize, *task);
        }
        else {
            workers.push_back(std::async(std::launch::async, [&kernel, startIndex, chunkSize, context=ExecutionContext::current(), task]() mutable {
                Task::Scope taskScope(task);
                ExecutionContext::Scope execScope(std::move(context));
                kernel(startIndex, chunkSize, *task);
            }));
        }
        startIndex += chunkSize;
    }
    for(auto& t : workers)
        t.wait();
    for(auto& t : workers)
        t.get();
#else
    kernel(0, loopCount, *task);
#endif

    return !task->isCanceled();
}

template<class Function>
void parallelForChunks(size_t loopCount, Function kernel)
{
#ifndef OVITO_DISABLE_THREADING
    std::vector<std::future<void>> workers;
    size_t num_threads = Application::instance()->idealThreadCount();
    if(num_threads > loopCount) {
        if(loopCount <= 0) return;
        num_threads = loopCount;
    }
    size_t chunkSize = loopCount / num_threads;
    size_t startIndex = 0;
    for(size_t t = 0; t < num_threads; t++) {
        if(t == num_threads - 1) {
            chunkSize += loopCount % num_threads;
            OVITO_ASSERT(startIndex + chunkSize == loopCount);
            kernel(startIndex, chunkSize);
        }
        else {
            workers.push_back(std::async(std::launch::async, [&kernel, startIndex, chunkSize, context=ExecutionContext::current(), task=this_task::get()]() {
                Task::Scope taskScope(std::move(task));
                ExecutionContext::Scope execScope(std::move(context));
                kernel(startIndex, chunkSize);
            }));
        }
        startIndex += chunkSize;
    }
    for(auto& t : workers)
        t.wait();
    for(auto& t : workers)
        t.get();
#else
    kernel(0, loopCount);
#endif
}

template<typename ResultObject, class Function>
std::vector<ResultObject> parallelForCollect(size_t loopCount, Function&& kernel, size_t progressChunkSize = 1024)
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::get()->isProgressingTask());
    ProgressingTask* task = static_cast<ProgressingTask*>(this_task::get());
    task->setProgressMaximum(loopCount / progressChunkSize);

#ifndef OVITO_DISABLE_THREADING
    std::vector<std::future<void>> workers;
    size_t num_threads = Application::instance()->idealThreadCount();
    std::vector<ResultObject> results{num_threads};
    size_t chunkSize = loopCount / num_threads;
    size_t startIndex = 0;
    size_t endIndex = chunkSize;
    for(size_t t = 0; t < num_threads; t++) {
        if(t == num_threads - 1)
            endIndex += loopCount % num_threads;
        workers.push_back(std::async(std::launch::async, [&kernel, startIndex, endIndex, progressChunkSize, threadResult = &results[t], context=ExecutionContext::current(), task]() {
            Task::Scope taskScope(task);
            ExecutionContext::Scope execScope(std::move(context));
            for(size_t i = startIndex; i < endIndex;) {
                // Execute kernel.
                kernel(i, *threadResult);

                i++;

                // Update progress indicator.
                if((i % progressChunkSize) == 0) {
                    OVITO_ASSERT(i != 0);
                    task->incrementProgressValue();
                }
                if(task->isCanceled())
                    return;
            }
        }));
        startIndex = endIndex;
        endIndex += chunkSize;
    }

    for(auto& t : workers)
        t.wait();
    for(auto& t : workers)
        t.get();
#else
    std::vector<ResultObject> results{1};
    for(size_t i = 0; i < loopCount; ) {
        // Execute kernel.
        kernel(i, results.front());
        i++;

        // Update progress indicator.
        if((i % progressChunkSize) == 0) {
            OVITO_ASSERT(i != 0);
            task.incrementProgressValue();
        }
        if(task.isCanceled())
            break;
    }
#endif

    task->incrementProgressValue(loopCount % progressChunkSize);
    if(task->isCanceled())
        results.clear();
    return results;
}

}   // End of namespace
