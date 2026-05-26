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

#pragma once

#include <ovito/pyscript/PyScript.h>
#include <ovito/pyscript/engine/ScriptEngine.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <QVector>

namespace PyScript {

using namespace Ovito;

class OVITO_PYSCRIPT_EXPORT GilSafePyObject
{
public:
    GilSafePyObject() noexcept = default;
    explicit GilSafePyObject(py::object object) noexcept : _ptr(object.release().ptr()) {}

    GilSafePyObject(const GilSafePyObject& other) : _ptr(other._ptr)
    {
        incref(_ptr);
    }

    GilSafePyObject(GilSafePyObject&& other) noexcept : _ptr(std::exchange(other._ptr, nullptr)) {}

    ~GilSafePyObject()
    {
        decref(_ptr);
    }

    GilSafePyObject& operator=(const GilSafePyObject& other)
    {
        if(this != &other) {
            PyObject* newPtr = other._ptr;
            incref(newPtr);
            decref(_ptr);
            _ptr = newPtr;
        }
        return *this;
    }

    GilSafePyObject& operator=(GilSafePyObject&& other) noexcept
    {
        if(this != &other) {
            decref(_ptr);
            _ptr = std::exchange(other._ptr, nullptr);
        }
        return *this;
    }

    bool isValid() const noexcept { return _ptr != nullptr; }

    py::object toObject() const
    {
        if(!_ptr)
            return py::none();
        py::gil_scoped_acquire gil;
        return py::reinterpret_borrow<py::object>(_ptr);
    }

private:
    static void incref(PyObject* ptr)
    {
        if(!ptr || !Py_IsInitialized())
            return;
        py::gil_scoped_acquire gil;
        Py_INCREF(ptr);
    }

    static void decref(PyObject* ptr)
    {
        if(!ptr || !Py_IsInitialized())
            return;
        py::gil_scoped_acquire gil;
        Py_DECREF(ptr);
    }

    PyObject* _ptr = nullptr;
};

class OVITO_PYSCRIPT_EXPORT PythonScriptModifier : public Modifier
{
    OVITO_CLASS(PythonScriptModifier)
    Q_CLASSINFO("DisplayName", "Python script");
    Q_CLASSINFO("ModifierCategory", "Modification");

public:
    void initializeObject(ObjectInitializationFlags flags);

    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }
    virtual bool shouldDelayDeletionInPipeline() const override { return _activeScriptTasks != 0; }
    virtual void cancelActiveWorkInPipeline() override { cancelRunningScripts(); }

    void commitScript(const QString& scriptText);
    bool isScriptActivated() const { return _scriptActivated; }
    bool hasRunningScriptTasks() const { return _activeScriptTasks != 0; }
    bool isScriptBusy() const { return _scriptBusy; }
    void cancelRunningScripts(bool deactivateScript = false);

    py::object compiledScriptDefinition() {
        if(_scriptCompilationFuture)
            return _scriptCompilationFuture.result().toObject();
        return py::none();
    }

    void setCompiledScriptDefinition(py::object definition) {
        _scriptCompilationFuture = SharedFuture<GilSafePyObject>(GilSafePyObject(std::move(definition)));
        notifyTargetChanged();
    }

    void ensureScriptCompiled() { compileScriptDefinition(); }

    const QString& scriptCompilationOutput() const { return _scriptCompilationOutput; }
    const QString& scriptExecutionOutput() const { return _scriptExecutionOutput; }
    void setParameterValue(const QString& name, const QVariant& value);

protected:
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    SharedFuture<GilSafePyObject> compileScriptDefinition();
    void synchronizeParameterSchema(const QVariantList& schema);

private:
    void registerScriptTask(const TaskPtr& task);
    void unregisterScriptTask(Task* task) noexcept;
    void beginScriptTask() { ++_activeScriptTasks; }
    void endScriptTask() { if(_activeScriptTasks > 0) --_activeScriptTasks; }
    void refreshExecutionOutput();
    void setExecutionProgress(const QString& text, quint64 generation);
    void appendCompilationOutput(const QString& text);
    void appendExecutionOutput(const QString& text, quint64 generation);

private:
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, script, setScript, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, executionKey, setExecutionKey, PROPERTY_FIELD_DONT_SERIALIZE | PROPERTY_FIELD_NO_UNDO);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QVariantMap{}, parameterValues, setParameterValues, PROPERTY_FIELD_NO_FLAGS);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QVariantList{}, parameterSchema, setParameterSchema, PROPERTY_FIELD_DONT_SERIALIZE | PROPERTY_FIELD_NO_UNDO);

    QString _scriptCompilationOutput;
    QString _scriptExecutionOutput;
    QString _scriptExecutionLog;
    QString _scriptProgressStatus;
    SharedFuture<GilSafePyObject> _scriptCompilationFuture;
    quint64 _cacheGeneration = 0;
    quint64 _executionGeneration = 0;
    quint64 _canceledExecutionGeneration = 0;
    int _activeScriptTasks = 0;
    QVector<TaskPtr> _activeTaskHandles;
    bool _scriptActivated = false;
    bool _scriptBusy = false;
    bool _cancelRequested = false;
};

}  // namespace PyScript
