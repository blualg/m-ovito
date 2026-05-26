////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2019 Alexander Stukowski
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
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <QFileInfo>
#include "ScriptEngine.h"

namespace PyScript {
namespace {

DataSet* resolveDatasetForContext(RefTarget* contextObj)
{
	if(auto* dataset = dynamic_object_cast<DataSet>(contextObj))
		return dataset;
	if(Application::instance()) {
		return Application::instance()->datasetContainer().currentSet();
	}
	return nullptr;
}

QString detectEmbeddedPythonHome()
{
	if(!qEnvironmentVariableIsEmpty("PYTHONHOME"))
		return qEnvironmentVariable("PYTHONHOME");

#if defined(Q_OS_WIN)
	const QString candidate = QDir::homePath() +
		QStringLiteral("/AppData/Local/Programs/Python/Python%1%2").arg(PY_MAJOR_VERSION).arg(PY_MINOR_VERSION);
	if(QFileInfo::exists(candidate + QStringLiteral("/Lib/os.py")))
		return candidate;
#endif
	return {};
}

} // namespace

/// Linked list of active script execution contexts.
ScriptEngine::ScriptExecutionContext* ScriptEngine::_activeContext = nullptr;

/// Head of linked list containing all initXXX functions.
PythonPluginRegistration* PythonPluginRegistration::linkedlist = nullptr;

// This helper class redirects Python script write calls to the sys.stdout stream to this script engine.
struct InterpreterOutputRedirector
{
	explicit InterpreterOutputRedirector(std::ostream& stream) : _stream(stream) {}

	void write(const QString& str) {
		for(ScriptEngine::ScriptExecutionContext* c = ScriptEngine::_activeContext; c != nullptr; c = c->next) {
			if(c->outputHandler) {
				c->outputHandler(str);
				return;
			}
		}
		_stream << str.toStdString();
	}

	void flush() {
		for(ScriptEngine::ScriptExecutionContext* c = ScriptEngine::_activeContext; c != nullptr; c = c->next) {
			if(c->outputHandler)
				return;
		}
		_stream << std::flush;
	}

	std::ostream& _stream;
};

/******************************************************************************
* Initializes the embedded Python interpreter and sets up the global namespace.
******************************************************************************/
void ScriptEngine::initializeEmbeddedInterpreter(RefTarget* contextObj)
{
	// This is a one-time global initialization.
	static bool isInterpreterInitialized = false;
	if(isInterpreterInitialized)
		return;	// Interpreter is already initialized.

	try {
		const QString detectedPythonHome = detectEmbeddedPythonHome();
		if(!detectedPythonHome.isEmpty())
			qputenv("PYTHONHOME", QDir::toNativeSeparators(detectedPythonHome).toUtf8());

		// Call Py_SetProgramName() because the Python interpreter uses the path of the main executable to determine the
		// location of Python standard library, which gets shipped with the static build of OVITO.
#if PY_MAJOR_VERSION >= 3
		static std::wstring programName = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
		Py_SetProgramName(const_cast<wchar_t*>(programName.data()));
#else
		static QByteArray programName = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toLocal8Bit();
		Py_SetProgramName(programName.data());
#endif

		// Make our internal script modules available by registering their initXXX functions with the Python interpreter.
		// This is required for static builds where all Ovito plugins are linked into the main executable file.
		// On Windows this is needed, because OVITO plugins have an .dll extension and the Python interpreter
		// only looks for modules that have a .pyd extension.
		for(const PythonPluginRegistration* r = PythonPluginRegistration::linkedlist; r != nullptr; r = r->_next) {
			const char* name = r->_moduleName.c_str();
			// Note: const_cast<> is used for backward compatibility with Python 2.6.
			PyImport_AppendInittab(const_cast<char*>(name), r->_initFunc);
		}

		// Initialize the Python interpreter.
		py::initialize_interpreter();

		py::module sys_module = py::module::import("sys");

#ifdef OVITO_BUILD_MONOLITHIC
		// Let the ovito.plugins module know that it is running in a statically linked
		// interpreter.
		sys_module.attr("__OVITO_BUILD_MONOLITHIC") = py::cast(true);
#endif

		// Install output redirection (don't do this in console mode as it interferes with the interactive interpreter).
		if(Application::guiEnabled()) {
			// Register the output redirector class.
			py::class_<InterpreterOutputRedirector>(sys_module, "__StdStreamRedirectorHelper")
					.def("write", &InterpreterOutputRedirector::write)
					.def("flush", &InterpreterOutputRedirector::flush);
			// Replace stdout and stderr streams.
			sys_module.attr("stdout") = py::cast(new InterpreterOutputRedirector(std::cout), py::return_value_policy::take_ownership);
			sys_module.attr("stderr") = py::cast(new InterpreterOutputRedirector(std::cerr), py::return_value_policy::take_ownership);
		}

		// Determine path where Python source files are located.
		QDir prefixDir(QCoreApplication::applicationDirPath());
#if defined(Q_OS_WIN)
		QString pythonModulePath = prefixDir.absolutePath() + QStringLiteral("/plugins/python");
#elif defined(Q_OS_MAC)
		QString pythonModulePath = prefixDir.absolutePath() + QStringLiteral("/../Resources/python");
#else
		QString pythonModulePath = prefixDir.absolutePath() + QStringLiteral("/../lib/ovito/plugins/python");
#endif

		// Prepend directory containing OVITO's Python source files to sys.path.
		py::object sys_path = sys_module.attr("path");
		PyList_Insert(sys_path.ptr(), 0, py::cast(QDir::toNativeSeparators(pythonModulePath)).ptr());

		if(!detectedPythonHome.isEmpty()) {
			QString sitePackagesPath = QDir::cleanPath(detectedPythonHome + QStringLiteral("/Lib/site-packages"));
			if(QDir(sitePackagesPath).exists())
				PyList_Insert(sys_path.ptr(), 0, py::cast(QDir::toNativeSeparators(sitePackagesPath)).ptr());
		}

		// Prepend current directory to sys.path.
		PyList_Insert(sys_path.ptr(), 0, py::str().ptr());

#if defined(Q_OS_WIN)
		// Workaround for error "unable to find Qt5Core.dll on PATH" on Windows:
		// Prepend OVITO executable directory to PATH environment variable (os.environ['PATH'])
		// so that the PyQt5 module will find the location of the Qt5 DLLs.
		py::module os_module = py::module::import("os");
		py::object os_path = os_module.attr("environ");
		if(os_path.contains("PATH")) {
			os_path["PATH"] = py::str("{};{}").format(QDir::toNativeSeparators(prefixDir.absolutePath()), os_path["PATH"]);
		}
		else {
			os_path["PATH"] = QDir::toNativeSeparators(prefixDir.absolutePath());
		}
#endif
	}
	catch(Exception& ex) {
		throw;
	}
	catch(py::error_already_set& ex) {
		ex.restore();
		if(PyErr_Occurred())
			PyErr_PrintEx(0);
		throw Exception(DataSet::tr("Failed to initialize Python interpreter. %1").arg(ex.what()));
	}
	catch(const std::exception& ex) {
		throw Exception(DataSet::tr("Failed to initialize Python interpreter: %1").arg(ex.what()));
	}
	catch(...) {
		throw Exception(DataSet::tr("Unhandled exception thrown by Python interpreter."));
	}

	isInterpreterInitialized = true;
}

/******************************************************************************
* Returns the DataSet which is the current context for scripts.
******************************************************************************/
DataSet* ScriptEngine::currentDataset()
{
	OVITO_ASSERT_MSG(_activeContext != nullptr, "ScriptEngine::currentDataset()", "This method may only be called during script execution.");
	OVITO_ASSERT(_activeContext->contextObj != nullptr);

	if(!_activeContext)
		throw Exception("Invalid program state. ScriptEngine::currentDataset() was called from outside a script execution context.");

	if(DataSet* dataset = resolveDatasetForContext(_activeContext->contextObj))
		return dataset;
	throw Exception("Invalid program state. No active dataset is associated with the current script execution context.");
}

/******************************************************************************
* Blocks execution until the given future has completed.
******************************************************************************/
bool ScriptEngine::waitForFuture(const FutureBase& future)
{
	OVITO_ASSERT_MSG(_activeContext != nullptr, "ScriptEngine::waitForFuture()", "This method may only be called during script execution.");
	OVITO_ASSERT(_activeContext->task);

	if(!_activeContext)
		throw Exception("Invalid program state. ScriptEngine::waitForFuture() was called from outside a script execution context.");

	bool finished = Task::waitFor(future.task(), true, true, true);
	if(!finished) {
		const TaskPtr& awaitedTask = future.task();
		if(awaitedTask && !awaitedTask->isFinished() && !awaitedTask->isCanceled())
			awaitedTask->cancel();
	}
	return finished;
}

/******************************************************************************
* Requests cancellation of currently active script tasks matching the predicate.
******************************************************************************/
void ScriptEngine::cancelActiveScriptTasks(const std::function<bool(RefTarget*)>& predicate)
{
    if(!predicate)
        return;

    QVector<TaskPtr> tasksToCancel;
    for(ScriptExecutionContext* c = _activeContext; c != nullptr; c = c->next) {
        if(c->contextObj && predicate(c->contextObj) && c->task && !c->task->isFinished())
            tasksToCancel.push_back(c->task);
    }

    for(const TaskPtr& task : std::as_const(tasksToCancel)) {
        if(task && !task->isFinished())
            task->cancel();
    }
}

/******************************************************************************
* Returns the asynchronous task object that represents the current script execution.
******************************************************************************/
const TaskPtr& ScriptEngine::currentTask()
{
	OVITO_ASSERT_MSG(_activeContext != nullptr, "ScriptEngine::currentTask()", "This method may only be called during script execution.");
	OVITO_ASSERT(_activeContext->task);

	if(!_activeContext)
		throw Exception("Invalid program state. ScriptEngine::currentTask() was called from outside a script execution context.");

	return _activeContext->task;
}

/******************************************************************************
* Executes the given C++ function, which in turn may invoke Python functions in the
* context of this engine, and catches possible exceptions.
******************************************************************************/
int ScriptEngine::executeSync(RefTarget* contextObj, const TaskPtr& task, OutputHandler outputHandler, const std::function<void()>& func, bool importOvitoPackage)
{
	OVITO_ASSERT(contextObj != nullptr);
	if(QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread())
		throw Exception(DataSet::tr("Python scripts can only be run from the main thread."));
	DataSet* dataset = resolveDatasetForContext(contextObj);
	if(!dataset)
		throw Exception(DataSet::tr("Python scripts require an active dataset context."));
	Task::Scope taskScope(task);

	// Create an information record on the stack that indicates which script execution is currently in progress.
	ScriptExecutionContext execContext(contextObj, std::move(outputHandler), task);

	int returnValue = 0;
	try {
		// Initialize the embedded Python interpreter if it isn't running already.
		if(!Py_IsInitialized())
			initializeEmbeddedInterpreter(contextObj);

		try {
			if(importOvitoPackage) {
				// Import the main OVITO Python package and let it expose the active dataset
				// through its own bindings.
				py::module::import("ovito");
			}

			// Invoke the supplied C++ function that executes scripting functions.
			func();
		}
		catch(const OperationCanceled&) {
			throw;
		}
		catch(py::error_already_set& ex) {
			if(ex.matches(PyExc_KeyboardInterrupt))
				throw OperationCanceled();
			returnValue = handlePythonException(ex);
		}
		catch(const std::exception& ex) {
			throw Exception(DataSet::tr("Script execution error: %1").arg(ex.what()));
		}
		catch(...) {
			throw Exception(DataSet::tr("Unhandled exception thrown by Python interpreter."));
		}
	}
	catch(Exception& ex) {
		if(execContext.outputHandler && !task->isCanceled())
			execContext.outputHandler(ex.messages().join(QChar('\n')));
		throw;
	}

	return returnValue;
}

/******************************************************************************
* Executes one or more Python statements.
******************************************************************************/
int ScriptEngine::executeCommands(const QString& commands, RefTarget* contextObj, const TaskPtr& task, OutputHandler outputHandler, bool modifyGlobalNamespace, const QStringList& cmdLineArguments)
{
	return executeSync(contextObj, task, std::move(outputHandler), [&]() {
		// Pass command line parameters to the script.
		py::list argList;
		argList.append(py::cast("-c"));
		for(const QString& a : cmdLineArguments)
			argList.append(py::cast(a));
		py::module::import("sys").attr("argv") = std::move(argList);

		py::dict global;
		if(modifyGlobalNamespace)
			global = py::globals();
		else
			global = py::globals().attr("copy")();

		global["__file__"] = py::none();
		PyObject* result = PyRun_String(commands.toUtf8().constData(), Py_file_input, global.ptr(), global.ptr());
		if(!result) throw py::error_already_set();
		Py_XDECREF(result);
	});
}

/******************************************************************************
* Executes a Python program.
******************************************************************************/
int ScriptEngine::executeFile(const QString& filename, RefTarget* contextObj, const TaskPtr& task, OutputHandler outputHandler, bool modifyGlobalNamespace, const QStringList& cmdLineArguments)
{
	return executeSync(contextObj, task, std::move(outputHandler), [&]() {

		// Pass command line parameters to the script.
		py::list argList;
		argList.append(py::cast(filename));
		for(const QString& a : cmdLineArguments)
			argList.append(py::cast(a));
		py::module::import("sys").attr("argv") = argList;

		py::dict global;
		if(modifyGlobalNamespace)
			global = py::globals();
		else
			global = py::globals().attr("copy")();

		py::str nativeFilename(py::cast(QDir::toNativeSeparators(filename)));
		global["__file__"] = nativeFilename;
		py::eval_file(nativeFilename, global);
	});
}

/******************************************************************************
* Handles an exception raised by the Python side.
******************************************************************************/
int ScriptEngine::handlePythonException(py::error_already_set& ex, const QString& filename)
{
	ex.restore();

	// Handle calls to sys.exit()
	if(PyErr_ExceptionMatches(PyExc_SystemExit)) {
		return handleSystemExit();
	}

	// Prepare C++ exception object.
	Exception exception(filename.isEmpty() ?
		DataSet::tr("The Python script has exited with an error.") :
		DataSet::tr("The Python script '%1' has exited with an error.").arg(filename));

	// Retrieve Python error message and traceback.
	PyObject* extype;
	PyObject* value;
	PyObject* traceback;
	PyErr_Fetch(&extype, &value, &traceback);
	PyErr_NormalizeException(&extype, &value, &traceback);
	if(extype) {
		py::object o_extype = py::reinterpret_borrow<py::object>(extype);
		py::object o_value = py::reinterpret_borrow<py::object>(value);
		try {
			if(traceback) {
				py::object o_traceback = py::reinterpret_borrow<py::object>(traceback);
				py::object mod_traceback = py::module::import("traceback");
				bool chain = PyObject_IsInstance(value, extype) == 1;
				py::sequence lines = mod_traceback.attr("format_exception")(o_extype, o_value, o_traceback, py::none(), chain);
				if(py::isinstance<py::sequence>(lines)) {
					QString tracebackString;
					for(int i = 0; i < py::len(lines); ++i)
						tracebackString += lines[i].cast<QString>();
					exception.appendDetailMessage(tracebackString);
				}
			}
			else {
				exception.appendDetailMessage(py::str(o_value).cast<QString>());
			}
		}
		catch(py::error_already_set& ex) {
			ex.restore();
			PyErr_PrintEx(0);
		}
	}

	// Raise C++ exception.
	throw exception;
}

/******************************************************************************
* Handles a call to sys.exit() in the Python interpreter.
* Returns the program exit code.
******************************************************************************/
int ScriptEngine::handleSystemExit()
{
	PyObject *exception, *value, *tb;
	int exitcode = 0;

	PyErr_Fetch(&exception, &value, &tb);

#if PY_MAJOR_VERSION < 3
	if(Py_FlushLine())
		PyErr_Clear();
#endif

	// Interpret sys.exit() argument.
	if(value && value != Py_None) {
#ifdef PyExceptionInstance_Check
		if(PyExceptionInstance_Check(value)) {	// Python 2.6 or newer
#else
		if(PyInstance_Check(value)) {			// Python 2.4
#endif
			// The error code should be in the code attribute.
			PyObject *code = PyObject_GetAttrString(value, "code");
			if(code) {
				Py_DECREF(value);
				value = code;
				if(value == Py_None)
					goto done;
			}
			// If we failed to dig out the 'code' attribute, just let the else clause below print the error.
		}
#if PY_MAJOR_VERSION >= 3
		if(PyLong_Check(value))
			exitcode = (int)PyLong_AsLong(value);
#else
		if(PyInt_Check(value))
			exitcode = (int)PyInt_AsLong(value);
#endif
		else {
			// Send sys.exit() argument to stderr.
			py::str s(value);
	        try {
				auto write = py::module::import("sys").attr("stderr").attr("write");
    	        write(s);
				write("\n");
			}
			catch(const py::error_already_set&) {}
			exitcode = 1;
		}
	}

done:
	PyErr_Restore(exception, value, tb);
	PyErr_Clear();
	return exitcode;
}

/******************************************************************************
* Executes the given C++ function in the context of an object and a scripting engine.
******************************************************************************/
Future<void> ScriptEngine::executeAsync(RefTarget* context, OutputHandler outputHandler, const std::function<py::object()>& func, bool importOvitoPackage, TaskCreatedHandler taskCreatedHandler)
{
	OVITO_ASSERT(context);
	OVITO_ASSERT(func);

	// Keep the scripting context object alive until the queued Python work has either
	// executed or been canceled. This prevents use-after-free crashes when the user
	// removes a Python-driven object (e.g. a Python modifier) while its script is
	// still compiling or running.
	OORef<RefTarget> contextRef(context);

	Promise<void> promise = Promise<void>::create();
	TaskPtr task = promise.takeTask();
	Future<void> future = Future<void>::createFromTask(task);
	if(Task* parentTask = this_task::get()) {
		task->setUserInterface(parentTask->userInterface());
		task->setIsInteractive(parentTask->isInteractive());
		if(parentTask->isHighPriorityTask())
			task->setHighPriorityTask();
	}
	auto handler = std::make_shared<OutputHandler>(std::move(outputHandler));
	auto taskHolder = std::make_shared<TaskPtr>(std::move(task));
	if(taskCreatedHandler)
		taskCreatedHandler(*taskHolder);
	auto functionHolder = std::make_shared<std::function<py::object()>>(func);
	auto executeOnMainThread = [contextRef = std::move(contextRef), handler, taskHolder, functionHolder, importOvitoPackage]() mutable {
		if((*taskHolder)->isCanceled()) {
			(*taskHolder)->setFinished();
			return;
		}

		UndoSuspender noUndo;

		try {
			ScriptEngine::executeSync(contextRef, *taskHolder, *handler, [functionHolder]() {
				[[maybe_unused]] py::object functionResult = (*functionHolder)();
			}, importOvitoPackage);
			(*taskHolder)->setFinished();
		}
		catch(const OperationCanceled&) {
			(*taskHolder)->cancel();
			(*taskHolder)->setFinished();
		}
		catch(...) {
			(*taskHolder)->captureExceptionAndFinish();
		}
	};

	if(QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
		executeOnMainThread();
	}
	else {
		QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(executeOnMainThread), Qt::QueuedConnection);
	}

	return future;
}

/******************************************************************************
* This is called to set up an ad-hoc environment when the Ovito Python module is loaded from
* an external Python interpreter.
******************************************************************************/
void ScriptEngine::initializeExternalInterpreter(DataSet* dataset, TaskPtr scriptExecutionTask)
{
	Q_UNUSED(dataset);
	Q_UNUSED(scriptExecutionTask);
	throw Exception("External Python import is not supported in this build.");
}

} // End of namespace
