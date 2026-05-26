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
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/StaticSource.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/pyscript/extensions/PythonScriptModifier.h>
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

AnimationTime frameArgumentToTime(const py::object& frame)
{
	if(frame.is_none())
		return Application::instance() ? Application::instance()->datasetContainer().currentAnimationTime() : AnimationTime::fromFrame(0);
	if(py::isinstance<py::int_>(frame))
		return AnimationTime::fromFrame(frame.cast<int>());
	return frame.cast<AnimationTime>();
}

} // namespace

void defineSceneSubmodule(py::module m)
{
	auto PipelineStatus_py = py::class_<PipelineStatus>(m, "PipelineStatus")
		.def(py::init<>())
		.def(py::init<PipelineStatus::StatusType, const QString&>())
		.def_property_readonly("type", &PipelineStatus::type)
		.def_property_readonly("text", &PipelineStatus::text);

	ovito_enum<PipelineStatus::StatusType>(PipelineStatus_py, "Type")
		.value("Success", PipelineStatus::Success)
		.value("Warning", PipelineStatus::Warning)
		.value("Error", PipelineStatus::Error);

	auto DataObject_py = ovito_abstract_class<DataObject, RefTarget>{m, nullptr, "DataObject"};
	DataObject_py
		.def_property_readonly("num_strong_references", &DataObject::dataReferenceCount)
		.def_property_readonly("is_safe_to_modify", &DataObject::isSafeToModify)
		.def("make_mutable", [](DataObject& parent, const DataObject* subobj) -> DataObject* {
			if(!subobj)
				return nullptr;
			if(!parent.hasReferenceTo(subobj))
				throw Exception("Object to be made mutable is not a sub-object of this parent.");
			return parent.makeMutable(subobj);
		}, py::arg("subobj"));
	createDataPropertyAccessors(DataObject_py, "identifier", &DataObject::identifier, &DataObject::setIdentifier,
		"The optional identifier string of the data object.");

	ovito_class<AttributeDataObject, DataObject>{m, nullptr, "AttributeDataObject"}
		.def_property("value", &AttributeDataObject::value, [](AttributeDataObject& obj, py::object value) {
			if(!obj.isSafeToModify())
				throw Exception(QStringLiteral("You tried to set the value of a global attribute that is not exclusively owned."));
			if(PyLong_Check(value.ptr()))
				obj.setValue(QVariant::fromValue(PyLong_AsLong(value.ptr())));
			else if(PyFloat_Check(value.ptr()))
				obj.setValue(QVariant::fromValue(PyFloat_AsDouble(value.ptr())));
			else
				obj.setValue(QVariant::fromValue(castToQString(value.cast<py::str>())));
		});

	auto DataCollection_py = ovito_class<DataCollection, DataObject>(m, nullptr, "DataCollection");
	expose_mutable_subobject_list(DataCollection_py,
		std::mem_fn(&DataCollection::objects),
		std::mem_fn(&DataCollection::insertObject),
		std::mem_fn(&DataCollection::removeObjectByIndex),
		"objects", "DataCollectionObjectsList");
	DataCollection_py.def("_assign_objects", [](DataCollection& self, const DataCollection& other) {
		self.setObjects(other.objects());
	});

	auto PipelineFlowState_py = py::class_<PipelineFlowState>(m, "PipelineFlowState")
		.def_property_readonly("status", &PipelineFlowState::status)
		.def_property_readonly("data", &PipelineFlowState::data)
		.def_property_readonly("mutable_data", &PipelineFlowState::mutableData);

	auto PipelineNode_py = ovito_abstract_class<PipelineNode, RefTarget>(m, nullptr, "PipelineObject")
		.def_property_readonly("status", &PipelineNode::status)
		.def("anim_time_to_source_frame", &PipelineNode::animationTimeToSourceFrame)
		.def("source_frame_to_anim_time", &PipelineNode::sourceFrameToAnimationTime)
		.def("_evaluate", [](PipelineNode& obj, py::object frame) {
			PipelineEvaluationResult future = obj.evaluate(PipelineEvaluationRequest(frameArgumentToTime(frame), true, false));
			if(!ScriptEngine::waitForFuture(future)) {
				PyErr_SetString(PyExc_KeyboardInterrupt, "Operation has been canceled by the user.");
				throw py::error_already_set();
			}
			return future.result();
		}, py::arg("frame") = py::none());

	ovito_abstract_class<Modifier, RefTarget>(m, nullptr, "Modifier")
		.def_property("enabled", &Modifier::isEnabled, &Modifier::setEnabled)
		.def_property_readonly("modifier_applications", [](Modifier& mod) -> py::list {
			py::list list;
			for(ModificationNode* node : mod.nodes())
				list.append(py::cast(node));
			return list;
		})
		.def("create_modifier_application", &Modifier::createModificationNode)
		.def("initialize_modifier", [](Modifier& mod, ModificationNode& node) {
			const AnimationTime time = Application::instance() ? Application::instance()->datasetContainer().currentAnimationTime() : AnimationTime::fromFrame(0);
			mod.initializeModifier(ModifierInitializationRequest(time, false, false, &node));
		})
		.def_property_readonly("some_modifier_application", &Modifier::someNode);

	ovito_class<ModificationNode, PipelineNode>(m, nullptr, "ModifierApplication")
		.def_property("modifier",
			[](ModificationNode& node) { return node.modifier(); },
			[](ModificationNode& node, Modifier* modifier) { node.setModifier(modifier); })
		.def_property("input",
			[](ModificationNode& node) { return node.input(); },
			[](ModificationNode& node, PipelineNode* input) { node.setInput(input); });

	ovito_class<StaticSource, PipelineNode>(m, nullptr, "StaticSource")
		.def_property("data",
			[](StaticSource& source) { return source.dataCollection(); },
			[](StaticSource& source, const DataCollection* data) { source.setDataCollection(data); })
		.def("compute", [](StaticSource& source, py::object frame) {
			PipelineEvaluationResult future = source.evaluate(PipelineEvaluationRequest(frameArgumentToTime(frame), true, false));
			if(!ScriptEngine::waitForFuture(future)) {
				PyErr_SetString(PyExc_KeyboardInterrupt, "Operation has been canceled by the user.");
				throw py::error_already_set();
			}
			return future.result().data();
		}, py::arg("frame") = py::none());

	ovito_class<PythonScriptModifier, Modifier>(m, nullptr, "PythonScriptModifier")
		.def_property("script", &PythonScriptModifier::script, &PythonScriptModifier::setScript)
		.def_property("compiled_definition", &PythonScriptModifier::compiledScriptDefinition, &PythonScriptModifier::setCompiledScriptDefinition);
}

} // End of namespace
