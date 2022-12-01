////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/AsynchronousModifierApplication.h>
#include <ovito/core/app/Application.h>
#include "AsynchronousModifier.h"

#ifdef Q_OS_LINUX
	#include <malloc.h>
#endif

namespace Ovito {

IMPLEMENT_OVITO_CLASS(AsynchronousModifier);

// Export this class template specialization from the DLL under Windows.
template class OVITO_CORE_EXPORT Future<AsynchronousModifier::EnginePtr>;

/******************************************************************************
* Asks the object for the result of the data pipeline.
******************************************************************************/
Future<PipelineFlowState> AsynchronousModifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
	OVITO_ASSERT(!isUndoRecording());

	// Get the modifier application, which stores cached computation results.
	const AsynchronousModifierApplication* asyncModApp = dynamic_object_cast<AsynchronousModifierApplication>(request.modApp());
	if(!asyncModApp) 
		return Future<PipelineFlowState>::createFailed(Exception(tr("Wrong type of modifier application.")));

	// Check if there is an existing computation result that can be reused as is.
	if(const EnginePtr& engine = asyncModApp->completedEngine()) {
		if(engine->validityInterval().contains(request.time())) {
			// Inject the cached computation result into the pipeline.
			PipelineFlowState output = input;
			engine->applyResults(request, output);
			output.intersectStateValidity(engine->validityInterval());
			return output;
		}
	}

	// Asynchrounous task managing the execution of the compute engine(s).
	class EngineExecutionTask : public detail::ContinuationTask<std::tuple<PipelineFlowState>, Task>
	{
	public:

		/// Constructor.
		EngineExecutionTask(const ModifierEvaluationRequest& request, EnginePtr engine, const PipelineFlowState& state, std::vector<EnginePtr> validStages = {}) : 
				detail::ContinuationTask<std::tuple<PipelineFlowState>, Task>(Task::Started, std::forward_as_tuple(state)),
				_request(request),
				_modApp(static_object_cast<AsynchronousModifierApplication>(request.modApp())),
				_engine(std::move(engine)),
				_validStages(std::move(validStages)) {}

		/// Starts running the next compute engine.
		void submitEngine() noexcept {
			OVITO_ASSERT(_modApp);
			OVITO_ASSERT(_engine);
			
			// Restrict the validity interval of the engine to the validity interval of the input pipeline state.
			TimeInterval iv = _engine->validityInterval();
			iv.intersect(resultsStorage().stateValidity());
			_engine->setValidityInterval(iv);
			_validStages.push_back(_engine);

			auto future = 
				_engine->preferSynchronousExecution() 
				? _engine->runImmediately(true)
				: _engine->runAsync(true);

			// Schedule next iteration upon completion of the future returned by the user function.
			this->whenTaskFinishes(std::move(future), _modApp->executor(), 
				[this_ = static_pointer_cast<EngineExecutionTask>(this->shared_from_this())](UNUSED_CONTINUATION_FUNC_PARAM) noexcept { this_->executionFinished(); });
		}

		/// Is called by the system when the current compute engine finishes.
		void executionFinished() noexcept {
			// Lock access to this task object.
			QMutexLocker locker(&this->taskMutex());

			// Get the task that did just finish.
			detail::TaskReference finishedTask = this->takeAwaitedTask();

			// Stop if the input task was canceled.
			if(!finishedTask || finishedTask->isCanceled()) {
				this->cancelAndFinishLocked(locker);
				return;
			}

			// Check if last function called resulted in an error.
			if(finishedTask->exceptionStore()) {
				this->exceptionLocked(finishedTask->copyExceptionStore());
				this->finishLocked(locker);
				return;
			}
			locker.unlock();

			processEngineResults();
		}

		void processEngineResults() noexcept {
			// Ask the compute engine for a continuation engine.
			if(EnginePtr continuationEngine = _engine->createContinuationEngine(_request, resultsStorage())) {

				// Restrict the validity of the continuation engine to the validity interval of the parent engine.
				TimeInterval iv = continuationEngine->validityInterval();
				iv.intersect(_engine->validityInterval());
				continuationEngine->setValidityInterval(iv);

				// Repeat the cycle with the new engine.
				_engine = std::move(continuationEngine);
				submitEngine();
			}
			else {
				// If the current engine has no continuation, we are done.

				// Add the computed results to the input pipeline state.
				_engine->applyResults(_request, resultsStorage());
				resultsStorage().intersectStateValidity(_engine->validityInterval());
				_modApp->setCompletedEngine(std::move(_engine));
				_modApp->setValidStages(std::move(_validStages));
				setFinished();
			}
		}

	private:

		ModifierEvaluationRequest _request;
		OORef<AsynchronousModifierApplication> _modApp;
		EnginePtr _engine;
		std::vector<EnginePtr> _validStages;
	};

	// Check if there are any partially completed computation results that can serve as starting point for a new computation.
	if(!asyncModApp->validStages().empty() && asyncModApp->validStages().back()->validityInterval().contains(request.time())) {
		// Create the asynchronous task object and continue the execution of engines.
		auto task = std::make_shared<EngineExecutionTask>(request, asyncModApp->validStages().back(), input, asyncModApp->validStages());
		task->processEngineResults();
		return Future<PipelineFlowState>::createFromTask(std::move(task));
	}
	else {
		// Otherwise, ask the subclass to create a new compute engine to perform the computation from scratch.
		return createEngine(request, input).then(executor(), [this, request = request, input = input, modApp = QPointer<const AsynchronousModifierApplication>(asyncModApp)](EnginePtr engine) mutable {
			if(!modApp || modApp->modifier() != this)
				throw Exception(tr("Modifier has been deleted from the pipeline."));
			// Create the asynchronous task object and start running the engine.
			auto task = std::make_shared<EngineExecutionTask>(std::move(request), std::move(engine), std::move(input));
			task->submitEngine();
			return Future<PipelineFlowState>::createFromTask(std::move(task));
		});
	}
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void AsynchronousModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	OVITO_ASSERT(!isUndoRecording());

	// If results are still available from the last pipeline evaluation, apply them to the input data.
	applyCachedResultsSynchronous(request, state);
	
	// Call base implementation.
	Modifier::evaluateSynchronous(request, state);
}

/******************************************************************************
* This function is called from AsynchronousModifier::evaluateSynchronous() to 
* apply the results from the last asycnhronous compute engine during a 
* synchronous pipeline evaluation.
******************************************************************************/
bool AsynchronousModifier::applyCachedResultsSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	OVITO_ASSERT(!isUndoRecording());

	// If results are still available from the last pipeline evaluation, apply them to the input data.
	if(const AsynchronousModifierApplication* asyncModApp = dynamic_object_cast<AsynchronousModifierApplication>(request.modApp())) {
		if(const AsynchronousModifier::EnginePtr& engine = asyncModApp->completedEngine()) {
			engine->applyResults(request, state);
			state.intersectStateValidity(engine->validityInterval());
			return true;
		}
	}
	return false;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void AsynchronousModifier::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
	Modifier::saveToStream(stream, excludeRecomputableData);
	stream.beginChunk(0x02);
	// Chunk reserved for future use.
	stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void AsynchronousModifier::loadFromStream(ObjectLoadStream& stream)
{
	Modifier::loadFromStream(stream);
	stream.expectChunk(0x02);
	// Chunk reserved for future use.
	stream.closeChunk();
}

#ifdef Q_OS_LINUX
/******************************************************************************
* Destructor.
******************************************************************************/
AsynchronousModifier::Engine::~Engine()
{
	// Some engines allocate considerable amounts of memory in small chunks,
	// which is sometimes not released back to the OS by the C memory allocator.
	// This call to malloc_trim() will explicitly trigger an attempt to release free memory
	// at the top of the heap.
	::malloc_trim(0);
}
#endif

}	// End of namespace
