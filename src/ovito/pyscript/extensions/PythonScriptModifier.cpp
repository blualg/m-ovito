////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/pyscript/PyScript.h>
#include <ovito/pyscript/binding/PythonBinding.h>
#include <ovito/pyscript/binding/ModifierRuntimeBinding.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/stdobj/table/DataTable.h>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include "PythonScriptModifier.h"

namespace PyScript {
namespace {

struct SharedCancelState
{
    std::atomic_bool cancelRequested{false};
};

std::shared_ptr<SharedCancelState> sharedCancelStateForKey(const QString& key)
{
    static QMutex mutex;
    static QHash<QString, std::weak_ptr<SharedCancelState>> states;

    QMutexLocker locker(&mutex);
    std::shared_ptr<SharedCancelState> state = states.value(key).lock();
    if(!state) {
        state = std::make_shared<SharedCancelState>();
        states.insert(key, state);
    }
    return state;
}

QString defaultModifierScript()
{
    return QStringLiteral(
        "import numpy as np\n\n"
        "def modify(frame, data, upstream, cache, progress):\n"
        "    progress.text(f\"Python modifier at frame {frame}\")\n"
        "    if data.particles is None:\n"
        "        return\n"
        "\n"
        "    if 'Position' in data.particles:\n"
        "        positions = data.particles['Position']\n"
        "        radii = np.linalg.norm(positions, axis=1)\n"
        "        data.particles.create_property('Distance from origin', radii)\n"
        "        data.attributes['PythonModifier.mean_radius'] = float(radii.mean()) if len(radii) else 0.0\n");
}

QVariantList choiceValues(const QVariantMap& schemaEntry)
{
    QVariantList values;
    const QVariantList rawChoices = schemaEntry.value(QStringLiteral("choices")).toList();
    for(const QVariant& entryVar : rawChoices) {
        const QVariantMap entry = entryVar.toMap();
        values.push_back(entry.value(QStringLiteral("value")));
    }
    return values;
}

QVariant coerceParameterValue(const QVariantMap& schemaEntry, const QVariant& rawValue)
{
    const QString kind = schemaEntry.value(QStringLiteral("kind")).toString();
    const QVariant defaultValue = schemaEntry.value(QStringLiteral("default"));
    QVariant value = rawValue.isValid() ? rawValue : defaultValue;

    if(kind == QStringLiteral("bool"))
        return value.toBool();
    if(kind == QStringLiteral("int"))
        return value.toInt();
    if(kind == QStringLiteral("float"))
        return value.toDouble();
    if(kind == QStringLiteral("string"))
        return value.toString();
    if(kind == QStringLiteral("choice")) {
        const QVariantList validChoices = choiceValues(schemaEntry);
        if(validChoices.contains(value))
            return value;
        if(validChoices.contains(defaultValue))
            return defaultValue;
        if(!validChoices.isEmpty())
            return validChoices.front();
        return {};
    }

    return value;
}

QVariantMap mergeParameterValues(const QVariantList& schema, const QVariantMap& currentValues)
{
    QVariantMap mergedValues;
    for(const QVariant& schemaEntryVar : schema) {
        const QVariantMap schemaEntry = schemaEntryVar.toMap();
        const QString name = schemaEntry.value(QStringLiteral("name")).toString();
        mergedValues.insert(name, coerceParameterValue(schemaEntry, currentValues.value(name)));
    }
    return mergedValues;
}

py::dict parameterValuesToPython(const QVariantMap& values)
{
    py::dict result;
    for(auto it = values.constBegin(); it != values.constEnd(); ++it)
        result[py::cast(it.key())] = py::cast(it.value());
    return result;
}

QString summarizeGeneratedResults(const DataCollection* data)
{
    if(!data)
        return {};

    QStringList tableNames;
    data->visitObjectsOfType<DataTable>([&](const DataTable* table) {
        QString name = table->identifier();
        if(name.isEmpty())
            name = table->title();
        if(name.isEmpty())
            name = table->objectTitle();
        if(!name.isEmpty() && !tableNames.contains(name))
            tableNames.push_back(name);
    });

    QStringList pythonAttributes;
    data->visitObjectsOfType<AttributeDataObject>([&](const AttributeDataObject* attr) {
        if(attr->identifier().startsWith(QStringLiteral("PythonModifier.")))
            pythonAttributes.push_back(attr->identifier());
    });

    QStringList summaryLines;
    if(!tableNames.isEmpty())
        summaryLines.push_back(QObject::tr("Generated table(s): %1").arg(tableNames.join(QStringLiteral(", "))));
    if(!pythonAttributes.isEmpty())
        summaryLines.push_back(QObject::tr("Generated attribute(s): %1").arg(pythonAttributes.join(QStringLiteral(", "))));
    if(summaryLines.isEmpty())
        return {};

    summaryLines.push_back(QObject::tr("Open the Data Inspector to inspect the generated results."));
    return summaryLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

py::object invokeModifierFunction(const py::function& scriptFunction, int frame, const py::object& data,
                                  const py::object& upstream, const py::dict& cache, const py::object& progress)
{
    py::object inspect_module = py::module::import("inspect");
    py::object signature = inspect_module.attr("signature")(scriptFunction);
    py::object parameters = signature.attr("parameters");
    py::list parameterValues = parameters.attr("values")();
    const int kindVarPositional = py::int_(inspect_module.attr("Parameter").attr("VAR_POSITIONAL"));
    const int kindPositionalOnly = py::int_(inspect_module.attr("Parameter").attr("POSITIONAL_ONLY"));
    const int kindPositionalOrKeyword = py::int_(inspect_module.attr("Parameter").attr("POSITIONAL_OR_KEYWORD"));

    bool hasVarArgs = false;
    py::ssize_t positionalCount = 0;
    for(py::handle parameter : parameterValues) {
        int kind = py::int_(parameter.attr("kind"));
        if(kind == kindVarPositional)
            hasVarArgs = true;
        else if(kind == kindPositionalOnly || kind == kindPositionalOrKeyword)
            positionalCount++;
    }

    if(hasVarArgs || positionalCount >= 5)
        return scriptFunction(frame, data, upstream, cache, progress);
    if(positionalCount == 4)
        return scriptFunction(frame, data, upstream, cache);
    if(positionalCount == 3)
        return scriptFunction(frame, data, upstream);
    if(positionalCount == 2)
        return scriptFunction(frame, data);
    if(positionalCount == 1)
        return scriptFunction(data);

    throw Exception(QStringLiteral("Invalid Python modifier script. The modify() function must accept at least one argument."));
}

py::object invokeCompiledModifierDefinition(const GilSafePyObject& scriptDefinition, const QVariantMap& parameterValues, int frame,
                                            const py::object& data, const py::object& upstream,
                                            const py::dict& cache, const py::object& progress)
{
    py::dict definition = scriptDefinition.toObject().cast<py::dict>();
    const QString kind = py::cast<QString>(definition[py::str("kind")]);
    if(kind == QStringLiteral("function"))
        return invokeModifierFunction(definition[py::str("callable")].cast<py::function>(), frame, data, upstream, cache, progress);

    if(kind == QStringLiteral("class")) {
        py::module apiModule = py::module::import("ovito_modifier");
        py::object modifierInstance = apiModule.attr("_instantiate_modifier_class")(definition[py::str("class")],
                                                                                   parameterValuesToPython(parameterValues));
        return invokeModifierFunction(modifierInstance.attr("modify").cast<py::function>(), frame, data, upstream, cache, progress);
    }

    throw Exception(QStringLiteral("Unsupported Python modifier entry kind '%1'.").arg(kind));
}

DataSet* currentApplicationDataset()
{
	if(Application::instance() == nullptr)
		return nullptr;
	return Application::instance()->datasetContainer().currentSet();
}

QVariant pythonToVariant(py::handle object)
{
    if(object.is_none())
        return {};
    if(PyBool_Check(object.ptr()))
        return QVariant::fromValue(object.cast<bool>());
    if(PyLong_Check(object.ptr()))
        return QVariant::fromValue(object.cast<qlonglong>());
    if(PyFloat_Check(object.ptr()))
        return QVariant::fromValue(object.cast<double>());
    if(PyUnicode_Check(object.ptr()))
        return QVariant::fromValue(object.cast<QString>());
    if(PyDict_Check(object.ptr())) {
        QVariantMap result;
        py::dict dictObject = object.cast<py::dict>();
        for(auto item : dictObject)
            result.insert(pythonToVariant(item.first).toString(), pythonToVariant(item.second));
        return result;
    }
    if(PyList_Check(object.ptr()) || PyTuple_Check(object.ptr())) {
        QVariantList result;
        py::sequence sequenceObject = object.cast<py::sequence>();
        for(py::handle item : sequenceObject)
            result.push_back(pythonToVariant(item));
        return result;
    }
    if(PyBytes_Check(object.ptr())) {
        const std::string value = object.cast<std::string>();
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }
    return py::str(object).cast<QString>();
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(PythonScriptModifier);
DEFINE_PROPERTY_FIELD(PythonScriptModifier, script);
DEFINE_PROPERTY_FIELD(PythonScriptModifier, executionKey);
DEFINE_PROPERTY_FIELD(PythonScriptModifier, parameterValues);
DEFINE_PROPERTY_FIELD(PythonScriptModifier, parameterSchema);
SET_PROPERTY_FIELD_LABEL(PythonScriptModifier, script, "script");

/******************************************************************************
* Initializes the object.
******************************************************************************/
void PythonScriptModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    _scriptCompilationOutput = tr("No script committed yet.\n");
    setExecutionKey(QUuid::createUuid().toString(QUuid::WithoutBraces));
    setScript(defaultModifierScript());
    _scriptActivated = false;
    _scriptBusy = false;
    _cancelRequested = false;
    sharedCancelStateForKey(executionKey())->cancelRequested.store(false);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void PythonScriptModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    Modifier::propertyChanged(field);

    if(field == PROPERTY_FIELD(script)) {
        _scriptCompilationFuture = {};
        _scriptCompilationOutput = _scriptActivated
            ? tr("<Script compilation pending>\n")
            : tr("Script updated. Click 'Commit and run script' to execute.\n");
        if(!_scriptActivated)
            _scriptBusy = false;
        _scriptExecutionOutput.clear();
        _cacheGeneration++;
        if(!parameterSchema().isEmpty())
            _parameterSchema.set(this, PROPERTY_FIELD(parameterSchema), QVariantList{});
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
    else if(field == PROPERTY_FIELD(parameterSchema)) {
        _cacheGeneration++;
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
    else if(field == PROPERTY_FIELD(parameterValues)) {
        notifyTargetChanged(PROPERTY_FIELD(parameterValues));
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
}

/******************************************************************************
* Activates the current script text and requests reevaluation.
******************************************************************************/
void PythonScriptModifier::commitScript(const QString& scriptText)
{
    sharedCancelStateForKey(executionKey())->cancelRequested.store(false);
    _scriptActivated = true;
    _scriptBusy = true;
    _cancelRequested = false;
    _canceledExecutionGeneration = 0;
    if(script() != scriptText) {
        setScript(scriptText);
    }
    else {
        _scriptCompilationFuture = {};
        _scriptCompilationOutput = tr("<Script compilation pending>\n");
        _scriptExecutionOutput.clear();
        _scriptExecutionLog.clear();
        _scriptProgressStatus.clear();
        _cacheGeneration++;
        notifyTargetChanged(PROPERTY_FIELD(script));
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
}

/******************************************************************************
* Requests cancellation of all currently active Python tasks owned by this modifier.
******************************************************************************/
void PythonScriptModifier::cancelRunningScripts(bool deactivateScript)
{
    advancePythonModifierCancelEpoch();
    sharedCancelStateForKey(executionKey())->cancelRequested.store(true);
    _cancelRequested = true;
    _canceledExecutionGeneration = std::max(_canceledExecutionGeneration, _executionGeneration);
    if(deactivateScript)
        _scriptActivated = false;
    for(const TaskPtr& task : std::as_const(_activeTaskHandles)) {
        if(task && !task->isFinished())
            task->cancel();
    }
    ScriptEngine::cancelActiveScriptTasks([this](RefTarget* contextObj) {
        Q_UNUSED(this);
        return dynamic_object_cast<PythonScriptModifier>(contextObj) != nullptr;
    });

    for(ModificationNode* node : nodes()) {
        if(!node)
            continue;
        node->cancelActiveEvaluations(true);
        const QSet<Pipeline*> owningPipelines = node->pipelines(false);
        for(Pipeline* pipeline : owningPipelines) {
            if(pipeline)
                pipeline->cancelActiveEvaluations();
        }
    }

    _scriptProgressStatus = tr("Canceling script...");
    refreshExecutionOutput();
}

void PythonScriptModifier::registerScriptTask(const TaskPtr& task)
{
    if(task && !_activeTaskHandles.contains(task))
        _activeTaskHandles.push_back(task);
}

void PythonScriptModifier::unregisterScriptTask(Task* task) noexcept
{
    if(!task)
        return;

    for(auto it = _activeTaskHandles.begin(); it != _activeTaskHandles.end();) {
        if(!*it || it->get() == task)
            it = _activeTaskHandles.erase(it);
        else
            ++it;
    }
}

/******************************************************************************
* Compiles the script entered by the user.
******************************************************************************/
SharedFuture<GilSafePyObject> PythonScriptModifier::compileScriptDefinition()
{
    if(!_scriptCompilationFuture) {
        _scriptCompilationOutput = tr("Compiling script...\n");
        auto scriptDefinition = std::make_shared<GilSafePyObject>();
        beginScriptTask();

        Future<void> execFuture = ScriptEngine::executeAsync(this, [this](const QString& text) {
            appendCompilationOutput(text);
        }, [this, scriptDefinition]() {
            py::module::import("ovito_modifier_runtime");
            py::module apiModule = py::module::import("ovito_modifier");
            py::dict localNamespace = py::globals().attr("copy")();
            localNamespace["__file__"] = py::none();
            PyObject* result = PyRun_String(script().toUtf8().constData(), Py_file_input, localNamespace.ptr(), localNamespace.ptr());
            if(!result)
                throw py::error_already_set();
            Py_XDECREF(result);

            *scriptDefinition = GilSafePyObject(apiModule.attr("_resolve_script_namespace")(localNamespace));

            return py::none();
        }, false, [this](const TaskPtr& task) {
            registerScriptTask(task);
        });

        execFuture.finally(ObjectExecutor(this), [this](Task& task) noexcept {
            unregisterScriptTask(&task);
            endScriptTask();
            if(task.isCanceled()) {
                _scriptCompilationOutput = tr("Script compilation canceled.\n");
            }
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
        });

        TaskPtr compileTask = execFuture.task();
        _scriptCompilationFuture = execFuture.then(ObjectExecutor(this), [this, scriptDefinition, compileTask]() {
            if(compileTask && compileTask->isCanceled())
                return *scriptDefinition;
            py::gil_scoped_acquire gil;
            py::dict definition = scriptDefinition->toObject().cast<py::dict>();
            synchronizeParameterSchema(pythonToVariant(definition[py::str("schema")]).toList());
            if(_scriptCompilationOutput.trimmed().isEmpty() || _scriptCompilationOutput == tr("Compiling script...\n"))
                _scriptCompilationOutput = tr("Script compiled successfully.\n");
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
            return *scriptDefinition;
        });
    }
    return _scriptCompilationFuture;
}

void PythonScriptModifier::synchronizeParameterSchema(const QVariantList& schema)
{
    if(parameterSchema() == schema)
        return;

    const QVariantMap mergedValues = mergeParameterValues(schema, parameterValues());
    _parameterSchema.set(this, PROPERTY_FIELD(parameterSchema), schema);
    _parameterValues.set(this, PROPERTY_FIELD(parameterValues), mergedValues);
}

void PythonScriptModifier::setParameterValue(const QString& name, const QVariant& value)
{
    QVariantMap updatedValues = parameterValues();
    updatedValues.insert(name, value);
    setParameterValues(updatedValues);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> PythonScriptModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!state)
        throw Exception(tr("Modifier input is empty."));

    const std::shared_ptr<SharedCancelState> cancelState = sharedCancelStateForKey(executionKey());
    TaskPtr evaluationTask;
    if(Task* currentTask = this_task::get()) {
        evaluationTask = currentTask->shared_from_this();
        registerScriptTask(evaluationTask);
    }

    if(!_scriptActivated) {
        _scriptBusy = false;
        Future<PipelineFlowState> future = Future<PipelineFlowState>::createImmediate(std::move(state));
        if(evaluationTask) {
            future.finally(ObjectExecutor(this), [this, evaluationTask](Task&) noexcept {
                unregisterScriptTask(evaluationTask.get());
            });
        }
        return future;
    }

    if((_cancelRequested || cancelState->cancelRequested.load()) && _activeScriptTasks == 0) {
        _cancelRequested = false;
        _scriptBusy = false;
        _scriptProgressStatus = tr("Script canceled.");
        refreshExecutionOutput();
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
        Future<PipelineFlowState> future = Future<PipelineFlowState>::createImmediate(std::move(state));
        if(evaluationTask) {
            future.finally(ObjectExecutor(this), [this, evaluationTask](Task&) noexcept {
                unregisterScriptTask(evaluationTask.get());
            });
        }
        return future;
    }

    const quint64 executionGeneration = ++_executionGeneration;
    _scriptBusy = true;
    _scriptExecutionLog.clear();
    _scriptProgressStatus = tr("Running script...");
    refreshExecutionOutput();
    notifyDependents(ReferenceEvent::ObjectStatusChanged);

    SharedFuture<GilSafePyObject> scriptDefinitionFuture = compileScriptDefinition();

    auto originalOutput = std::make_shared<PipelineFlowState>(state);
    auto output = std::make_shared<PipelineFlowState>(std::move(state));
    output->intersectStateValidity(request.time());

    OOWeakRef<const PipelineNode> currentNode = request.modificationNode();
    bool interactiveMode = request.interactiveMode();
    Future<PipelineFlowState> future = scriptDefinitionFuture.then(ObjectExecutor(this), [originalOutput = std::move(originalOutput), output = std::move(output), currentNode = std::move(currentNode), interactiveMode, time = request.time(), executionGeneration, cancelState, this](const GilSafePyObject& scriptDefinition) mutable {
        if(_cancelRequested || cancelState->cancelRequested.load() || executionGeneration <= _canceledExecutionGeneration) {
            _cancelRequested = false;
            if(executionGeneration == _executionGeneration) {
                _scriptBusy = false;
                _scriptProgressStatus = tr("Script canceled.");
                refreshExecutionOutput();
                notifyDependents(ReferenceEvent::ObjectStatusChanged);
            }
            return Future<PipelineFlowState>::createImmediate(std::move(*originalOutput));
        }

        py::gil_scoped_acquire gil;
        GilSafePyObject capturedDefinition = scriptDefinition;
        beginScriptTask();
        Future<void> execFuture = ScriptEngine::executeAsync(this, [this, executionGeneration](const QString& text) {
            appendExecutionOutput(text, executionGeneration);
        }, [this, time, output, currentNode, interactiveMode, capturedDefinition, executionGeneration, cancelState]() {
            DataSet* dataset = currentApplicationDataset();
            Q_UNUSED(dataset);
            int animationFrame = time.frame();
            std::shared_ptr<TaskProgress> progressHandle;
            if(const auto& ui = ScriptEngine::currentTask()->userInterface(); ui)
                progressHandle = std::make_shared<TaskProgress>(ui);
            py::module::import("ovito_modifier_runtime");
            output->mutableData();
            py::object dataWrapper = createModifierDataWrapper(output->data(), true, currentNode);
            py::object upstreamWrapper = createModifierUpstreamWrapper(currentNode, animationFrame, interactiveMode);
            py::dict cache = getModifierCache(reinterpret_cast<quintptr>(this), _cacheGeneration);
            py::object progressWrapper = createModifierProgressWrapper(progressHandle,
                [this, executionGeneration](const QString& status) {
                    setExecutionProgress(status, executionGeneration);
                },
                [this, executionGeneration, cancelState]() {
                    return _cancelRequested || cancelState->cancelRequested.load() || executionGeneration <= _canceledExecutionGeneration;
                });
            return invokeCompiledModifierDefinition(capturedDefinition, parameterValues(), animationFrame, dataWrapper, upstreamWrapper, cache, progressWrapper);
        }, false, [this](const TaskPtr& task) {
            registerScriptTask(task);
        });

        execFuture.finally(ObjectExecutor(this), [this, executionGeneration](Task& task) noexcept {
            unregisterScriptTask(&task);
            endScriptTask();
            if(executionGeneration == _executionGeneration && task.isCanceled()) {
                _cancelRequested = false;
                _scriptBusy = false;
                _scriptProgressStatus = tr("Script canceled.");
                refreshExecutionOutput();
                notifyDependents(ReferenceEvent::ObjectStatusChanged);
            }
        });

        TaskPtr executionTask = execFuture.task();
        return execFuture.then(ObjectExecutor(this), [originalOutput, output, executionGeneration, executionTask, this]() {
            if((executionTask && executionTask->isCanceled()) || executionGeneration <= _canceledExecutionGeneration || executionGeneration != _executionGeneration)
                return std::move(*originalOutput);
            if(const QString summary = summarizeGeneratedResults(output->data()); !summary.isEmpty())
                appendExecutionOutput(summary, executionGeneration);
            if(executionGeneration != _executionGeneration)
                return std::move(*originalOutput);
            _cancelRequested = false;
            _scriptBusy = false;
            _scriptProgressStatus = tr("Script finished successfully.");
            refreshExecutionOutput();
            notifyDependents(ReferenceEvent::ObjectStatusChanged);
            return std::move(*output);
        });
    });
    if(evaluationTask) {
        future.finally(ObjectExecutor(this), [this, evaluationTask](Task&) noexcept {
            unregisterScriptTask(evaluationTask.get());
        });
    }
    return future;
}

void PythonScriptModifier::refreshExecutionOutput()
{
    QString text = _scriptExecutionLog;
    if(!_scriptProgressStatus.isEmpty()) {
        if(!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
            text += QLatin1Char('\n');
        text += _scriptProgressStatus;
        if(!text.endsWith(QLatin1Char('\n')))
            text += QLatin1Char('\n');
    }
    _scriptExecutionOutput = text;
}

void PythonScriptModifier::setExecutionProgress(const QString& text, quint64 generation)
{
    if(generation != _executionGeneration)
        return;
    _scriptProgressStatus = text;
    refreshExecutionOutput();
}

/******************************************************************************
* Collects script output during compilation.
******************************************************************************/
void PythonScriptModifier::appendCompilationOutput(const QString& text)
{
    _scriptCompilationOutput += text;
    notifyDependents(ReferenceEvent::ObjectStatusChanged);
}

/******************************************************************************
* Collects script output during execution.
******************************************************************************/
void PythonScriptModifier::appendExecutionOutput(const QString& text, quint64 generation)
{
    if(generation != _executionGeneration)
        return;
    _scriptExecutionLog += text;
    refreshExecutionOutput();
    notifyDependents(ReferenceEvent::ObjectStatusChanged);
}

}  // namespace PyScript
