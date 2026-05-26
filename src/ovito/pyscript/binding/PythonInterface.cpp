////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2014 Alexander Stukowski
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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/Application.h>
#include "ModifierRuntimeBinding.h"
#include "PythonBinding.h"

namespace PyScript {

void defineAppSubmodule(py::module parentModule);	// Defined in AppBinding.cpp
void defineSceneSubmodule(py::module parentModule);	// Defined in SceneBinding.cpp

using namespace Ovito;

PYBIND11_MODULE(PyScript, m)
{
	py::options options;
	options.disable_function_signatures();

	// Register Ovito-to-Python exception translator.
	py::register_exception_translator([](std::exception_ptr p) {
		try {
			if(p) std::rethrow_exception(p);
		}
		catch(const Exception& ex) {
			PyErr_SetString(PyExc_RuntimeError, ex.messages().join(QChar('\n')).toUtf8().constData());
		}
	});

	if(!Application::instance()) {
		throw std::runtime_error("External Python import is not supported in this build.");
	}
	OVITO_ASSERT(QCoreApplication::instance() != nullptr);

	// Register submodules.
	defineAppSubmodule(m);
	defineSceneSubmodule(m);
	defineModifierRuntimeBindings(m);

	// Make Ovito program version number available to script.
	m.attr("version") = py::make_tuple(Application::applicationVersionMajor(), Application::applicationVersionMinor(), Application::applicationVersionRevision());
	m.attr("version_string") = py::cast(Application::applicationVersionString());

	// Make environment information available to the script.
	m.attr("gui_mode") = py::cast(Application::guiEnabled());
	m.attr("headless_mode") = py::cast(!Application::guiEnabled());

	// Add an attribute to the ovito module that provides access to the active dataset.
	DataSet* activeDataset = Application::instance()->datasetContainer().currentSet();
	m.attr("scene") = py::cast(activeDataset, py::return_value_policy::reference);

	// This is for backward compatibility with OVITO 2.9.0:
	m.attr("dataset") = py::cast(activeDataset, py::return_value_policy::reference);
}

OVITO_REGISTER_PLUGIN_PYTHON_INTERFACE(PyScript);

/// Helper method that checks if the given data object is safe to modify without unwanted side effects.
/// If it is not, an exception is raised to inform the user that a mutable version of the data object
/// should be explicitly requested.
void ensureDataObjectIsMutable(DataObject& obj)
{
	if(!obj.isSafeToModify()) {
		QString className = py::cast<QString>(py::str(py::cast(obj).attr("__class__").attr("__name__")));
		throw Exception(QStringLiteral("You tried to modify a %1 object that is currently shared by multiple owners. "
			"Please explicitly request a mutable version of the data object by using the '_' notation.").arg(className));
	}
}

} // End of namespace
