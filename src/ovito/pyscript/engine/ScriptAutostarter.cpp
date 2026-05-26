////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2017 Alexander Stukowski
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
#include <ovito/core/app/Application.h>
#include <ovito/core/app/StandaloneApplication.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include "ScriptAutostarter.h"
#include "ScriptEngine.h"

namespace PyScript {

namespace {

void writeDebugLog(const QString& message)
{
	if(qEnvironmentVariableIsEmpty("OVITO_PYSCRIPT_DEBUG_LOG"))
		return;

	QFile file(QDir::tempPath() + QStringLiteral("/ovito_pyscript_debug.log"));
	if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
		QTextStream stream(&file);
		stream << message << Qt::endl;
	}
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(ScriptAutostarter);

/******************************************************************************
* Destructor, which is called at program exit.
******************************************************************************/
ScriptAutostarter::~ScriptAutostarter()
{
	// Shut down Python interpreter.
	// This will run the Python functions registered with the 'atexit' module.
	if(_isStandaloneApplication && Py_IsInitialized()) {
		py::finalize_interpreter();
	}
}

/******************************************************************************
* Registers plugin-specific command line options.
******************************************************************************/
void ScriptAutostarter::registerCommandLineOptions(QCommandLineParser& cmdLineParser)
{
	// Register the --script command line option.
	cmdLineParser.addOption(QCommandLineOption("script", tr("Runs a Python script file."), tr("FILE")));

	// Register the --scriptarg command line option.
	cmdLineParser.addOption(QCommandLineOption("scriptarg", tr("Passes a command line option to the Python script."), tr("ARG")));

	// Register the --exec command line option.
	cmdLineParser.addOption(QCommandLineOption("exec", tr("Executes a single Python statement."), tr("CMD")));
}

/******************************************************************************
* Is called after the application has been completely initialized.
******************************************************************************/
void ScriptAutostarter::applicationStarted()
{
	writeDebugLog(QStringLiteral("applicationStarted() entered"));
	/// Application is running in standalone mode and is using the embedded Python interpreter.
	_isStandaloneApplication = true;

	// Execute the script commands and files passed on the command line.
	QStringList scriptCommands = StandaloneApplication::instance()->cmdLineParser().values("exec");
	QStringList scriptFiles = StandaloneApplication::instance()->cmdLineParser().values("script");
	writeDebugLog(QStringLiteral("scriptCommands=%1 scriptFiles=%2").arg(scriptCommands.size()).arg(scriptFiles.size()));

	if(!scriptCommands.empty() || !scriptFiles.empty()) {

		// Get the current dataset.
		DataSet* dataset = Application::instance()->datasetContainer().currentSet();
		if(!dataset)
		{
			writeDebugLog(QStringLiteral("No dataset available"));
			return;
		}
		writeDebugLog(QStringLiteral("Dataset available"));

		// Suppress undo recording. Actions performed by startup scripts cannot be undone.
		UndoSuspender noUndo;

		// Pass command line parameters to the script.
		QStringList scriptArguments = StandaloneApplication::instance()->cmdLineParser().values("scriptarg");

		// This task tracks script execution while running in terminal mode.
		MainThreadOperation scriptOperation(*Application::instance(), MainThreadOperation::Kind::Isolated, false);

		// Execute script commands.
		for(int index = scriptCommands.size() - 1; index >= 0; index--) {
			const QString& command = scriptCommands[index];
			try {
				writeDebugLog(QStringLiteral("Executing command: %1").arg(command));
				ScriptEngine::executeCommands(command, dataset, scriptOperation.task(), nullptr, true, scriptArguments);
				writeDebugLog(QStringLiteral("Command finished"));
			}
			catch(Exception& ex) {
				writeDebugLog(QStringLiteral("Command failed: %1").arg(ex.messages().join(QStringLiteral(" | "))));
				ex.prependGeneralMessage(tr("Error during Python script execution."));
				throw;
			}
		}

		// Execute script files.
		for(int index = scriptFiles.size() - 1; index >= 0; index--) {
			const QString& scriptFile = scriptFiles[index];
			try {
				writeDebugLog(QStringLiteral("Executing file: %1").arg(scriptFile));
				ScriptEngine::executeFile(scriptFile, dataset, scriptOperation.task(), nullptr, true, scriptArguments);
				writeDebugLog(QStringLiteral("File finished"));
			}
			catch(Exception& ex) {
				writeDebugLog(QStringLiteral("File failed: %1").arg(ex.messages().join(QStringLiteral(" | "))));
				throw;
			}
		}
	}
	writeDebugLog(QStringLiteral("applicationStarted() exiting"));
}

}	// End of namespace
