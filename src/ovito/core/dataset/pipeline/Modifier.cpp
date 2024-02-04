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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/core/utilities/concurrent/LaunchTask.h>

#ifdef Q_OS_LINUX
    #include <malloc.h>
#endif

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(Modifier);
DEFINE_PROPERTY_FIELD(Modifier, isEnabled);
DEFINE_PROPERTY_FIELD(Modifier, title);
SET_PROPERTY_FIELD_LABEL(Modifier, isEnabled, "Enabled");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, isEnabled, ReferenceEvent::TargetEnabledOrDisabled);
SET_PROPERTY_FIELD_LABEL(Modifier, title, "Name");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, title, ReferenceEvent::TitleChanged);

// Export this class template specialization from the DLL under Windows.
template class OVITO_CORE_EXPORT Future<ModifierEnginePtr>;

/******************************************************************************
* Constructor.
******************************************************************************/
Modifier::Modifier(ObjectInitializationFlags flags) : RefTarget(flags),
    _isEnabled(true)
{
}

/******************************************************************************
* Creates a new modification node for inserting this modifier into a pipeline.
******************************************************************************/
OORef<ModificationNode> Modifier::createModificationNode()
{
    // Look which ModificationNode class has been registered for this Modifier class.
    for(OvitoClassPtr clazz = &getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
        if(OvitoClassPtr nodeClass = ModificationNode::registry().getModificationNodeType(clazz)) {
            if(!nodeClass->isDerivedFrom(ModificationNode::OOClass()))
                throw Exception(tr("The modification node class %1 assigned to the Modifier-derived class %2 is not derived from ModificationNode.").arg(nodeClass->name(), clazz->name()));
#ifdef OVITO_DEBUG
            for(OvitoClassPtr superClazz = clazz->superClass(); superClazz != nullptr; superClazz = superClazz->superClass()) {
                if(OvitoClassPtr nodeSuperClass = ModificationNode::registry().getModificationNodeType(superClazz)) {
                    if(!nodeClass->isDerivedFrom(*nodeSuperClass))
                        throw Exception(tr("The modification node class %1 assigned to the Modifier-derived class %2 is not derived from the ModificationNode specialization %3.").arg(nodeClass->name(), clazz->name(), nodeSuperClass->name()));
                }
            }
#endif
            return static_object_cast<ModificationNode>(nodeClass->createInstance());
        }
    }

    // Fall back to generic node type.
    return OORef<ModificationNode>::create();
}

/******************************************************************************
* Returns the number of animation frames this modifier provides.
******************************************************************************/
int Modifier::numberOfOutputFrames(ModificationNode* node) const
{
    OVITO_ASSERT(node);
    if(PipelineNode* input = node->input())
        return input->numberOfSourceFrames();
    return 1;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> Modifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    OVITO_ASSERT(!isUndoRecording());
    OVITO_ASSERT(ExecutionContext::current().isValid());

    // Get the modifier node, which stores cached computation results.
    OORef<const ModificationNode> modNode = request.modificationNode();

    // Check if there is an existing computation result that can be reused as is.
    if(const ModifierEnginePtr& engine = modNode->completedEngine()) {
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

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = Future<PipelineFlowState>;

        /// Constructor.
        EngineExecutionTask(const ModifierEvaluationRequest& request, ModifierEnginePtr engine, const PipelineFlowState& state, std::vector<ModifierEnginePtr> validStages = {}) :
                detail::ContinuationTask<std::tuple<PipelineFlowState>, Task>(Task::Started, std::forward_as_tuple(state)),
                _request(request),
                _modNode(request.modificationNode()),
                _engine(std::move(engine)),
                _validStages(std::move(validStages)) {}

        /// Starts running the next compute engine.
        void operator()() noexcept {
            OVITO_ASSERT(_modNode);
            OVITO_ASSERT(_engine);

            if(isCanceled()) {
                setFinished();
            }
            else if(!_engine->isFinished()) {
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
                this->whenTaskFinishes(std::move(future), *_modNode, std::bind_front(&EngineExecutionTask::executionFinished, static_pointer_cast<EngineExecutionTask>(this->shared_from_this())));
            }
            else if(ModifierEnginePtr continuationEngine = _engine->createContinuationEngine(_request, resultsStorage())) {

                // Restrict the validity of the continuation engine to the validity interval of the parent engine.
                TimeInterval iv = continuationEngine->validityInterval();
                iv.intersect(_engine->validityInterval());
                continuationEngine->setValidityInterval(iv);

                // Repeat the cycle with the new engine.
                _engine = std::move(continuationEngine);
                (*this)();
            }
            else {
                // If the current engine has no continuation, we are done.

                // Add the computed results to the input pipeline state.
                _engine->applyResults(_request, resultsStorage());
                resultsStorage().intersectStateValidity(_engine->validityInterval());
                _modNode->setCompletedEngine(std::move(_engine));
                _modNode->setValidStages(std::move(_validStages));
                setFinished();
            }
        }

        /// Is called by the system when the current compute engine finishes.
        void executionFinished() noexcept {
            Task::Scope taskScope(this);

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

            // Continue.
            (*this)();
        }

    private:

        ModifierEvaluationRequest _request;
        OORef<ModificationNode> _modNode;
        ModifierEnginePtr _engine;
        std::vector<ModifierEnginePtr> _validStages;
    };

    // Check if there are any partially completed computation results that can serve as starting point for a new computation.
    if(!modNode->validStages().empty() && modNode->validStages().back()->validityInterval().contains(request.time())) {
        // Create the asynchronous task object and continue the execution of engines.
        return launchTask(std::make_shared<EngineExecutionTask>(request, modNode->validStages().back(), input, modNode->validStages()));
    }
    else {
        // Otherwise, ask the subclass to create a new compute engine to perform the computation from scratch.
        Future<ModifierEnginePtr> engineFuture = createEngine(request, input);
        if(engineFuture.isValid()) {
            return std::move(engineFuture).then(*this, [this, request = request, input = input, modNodeWeak = OOWeakRef<const ModificationNode>(modNode)](ModifierEnginePtr engine) mutable {
                auto modNode = modNodeWeak.lock();
                if(!modNode || modNode->modifier() != this)
                    throw Exception(tr("Modifier has been deleted from the pipeline."));
                // Create the asynchronous task object and start running the engine.
                return launchTask(std::make_shared<EngineExecutionTask>(std::move(request), std::move(engine), std::move(input)));
            });
        }
        else {
            // This modifier does not support asynchronounous computation.
            // Perform the computation synchronously in the current thread.
            PipelineFlowState output = input;
            if(output)
                evaluateSynchronous(request, output);
            return Future<PipelineFlowState>::createImmediate(std::move(output));
        }
    }
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void Modifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    OVITO_ASSERT(!isUndoRecording());

    // If results are still available from the last pipeline evaluation, apply them to the input data.
    if(const ModifierEnginePtr& engine = request.modificationNode()->completedEngine()) {
        engine->applyResults(request, state);
        state.intersectStateValidity(engine->validityInterval());
    }
 }

/******************************************************************************
* Returns the list of pipeline nodes that reference this modifier.
******************************************************************************/
QVector<ModificationNode*> Modifier::nodes() const
{
    QVector<ModificationNode*> list;
    visitDependents([&](RefMaker* dependent) {
        ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent);
        if(node != nullptr && node->modifier() == this)
            list.push_back(node);
    });
    return list;
}

/******************************************************************************
* Returns one of the pipelines nodes referencing this modifier in a pipeline.
******************************************************************************/
ModificationNode* Modifier::someNode() const
{
    ModificationNode* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent);
        if(node != nullptr && node->modifier() == this)
            result = node;
    });
    return result;
}

/******************************************************************************
* Returns the current status of the modifier's pipeline node(s).
******************************************************************************/
PipelineStatus Modifier::globalStatus() const
{
    // Combine the status values of all ModificationNodes into a single status.
    PipelineStatus result;
    for(ModificationNode* node : nodes()) {
        PipelineStatus s = node->status();

        if(result.text().isEmpty())
            result.setText(s.text());
        else if(s.text() != result.text())
            result.setText(result.text() + QStringLiteral("\n") + s.text());

        if(s.type() == PipelineStatus::Error)
            result.setType(PipelineStatus::Error);
        else if(result.type() != PipelineStatus::Error && s.type() == PipelineStatus::Warning)
            result.setType(PipelineStatus::Warning);
    }
    return result;
}

#ifdef Q_OS_LINUX
/******************************************************************************
* Destructor.
******************************************************************************/
ModifierEngine::~ModifierEngine()
{
    // Some compute engines allocate considerable amounts of memory in small chunks,
    // which is sometimes not released back to the OS by the C memory allocator.
    // This call to malloc_trim() will explicitly trigger an attempt to release free memory
    // at the top of the heap.
    ::malloc_trim(0);
}
#endif

}   // End of namespace
