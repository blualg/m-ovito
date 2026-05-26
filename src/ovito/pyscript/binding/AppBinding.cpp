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
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "PythonBinding.h"

namespace PyScript {

using namespace Ovito;

namespace {

DataSet* currentApplicationDataset()
{
	if(Application::instance() == nullptr)
		return nullptr;
	return Application::instance()->datasetContainer().currentSet();
}

DataSetContainer* currentDatasetContainer()
{
	return Application::instance() ? &Application::instance()->datasetContainer() : nullptr;
}

} // namespace

void defineAppSubmodule(py::module m)
{
	py::class_<OvitoObject, OORef<OvitoObject>>(m, "OvitoObject")
		.def("__str__", [](py::object& pyobj) {
			return py::str("<{} at 0x{:x}>").format(pyobj.attr("__class__").attr("__name__"), (std::intptr_t)pyobj.cast<OvitoObject*>());
		})
		.def("__repr__", [](py::object& pyobj) {
			return py::str("{}()").format(pyobj.attr("__class__").attr("__name__"));
		});

	ovito_abstract_class<RefMaker, OvitoObject>{m}
		.def_property_readonly("dataset", [](RefMaker& obj) {
			Q_UNUSED(obj);
			return currentApplicationDataset();
		}, py::return_value_policy::reference);

	ovito_abstract_class<RefTarget, RefMaker>{m}
		.def_property_readonly("object_title", &RefTarget::objectTitle)
		.def("notify_object_changed", [](RefTarget& target) {
			target.notifyTargetChanged();
		});

	ovito_class<DataSet, RefTarget>{m, nullptr, "Scene"}
		.def_property_readonly("container", [](DataSet& dataset) {
			Q_UNUSED(dataset);
			return currentDatasetContainer();
		}, py::return_value_policy::reference);

	ovito_abstract_class<DataSetContainer, RefMaker>{m, nullptr, "DataSetContainer"};

	py::class_<CloneHelper>(m, "CloneHelper")
		.def(py::init<>())
		.def("clone", py::overload_cast<const RefTarget*, bool>(&CloneHelper::cloneObject<RefTarget>));
}

} // End of namespace
