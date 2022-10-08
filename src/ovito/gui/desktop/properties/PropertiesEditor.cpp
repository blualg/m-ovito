////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/data/DataVis.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(PropertiesEditor);
DEFINE_REFERENCE_FIELD(PropertiesEditor, editObject);

/******************************************************************************
* Returns the global editor registry, which can be used to look up the editor
* class for editable RefTarget class.
******************************************************************************/
PropertiesEditor::Registry& PropertiesEditor::registry()
{
	static Registry singleton;
	return singleton;
}

/******************************************************************************
* Creates a PropertiesEditor for an object.
******************************************************************************/
OORef<PropertiesEditor> PropertiesEditor::create(RefTarget* obj)
{
	OVITO_CHECK_POINTER(obj);
	try {
		// Look if an editor class has been registered for this RefTarget class and one of its super classes.
		for(OvitoClassPtr clazz = &obj->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
			OvitoClassPtr editorClass = registry().getEditorClass(clazz);
			if(editorClass) {
				if(!editorClass->isDerivedFrom(PropertiesEditor::OOClass()))
					throw Exception(tr("The editor class %1 assigned to the RefTarget-derived class %2 is not derived from PropertiesEditor.").arg(editorClass->name(), clazz->name()));
				return dynamic_object_cast<PropertiesEditor>(editorClass->createInstance());
			}
		}
	}
	catch(Exception& ex) {
		if(ex.context() == nullptr) ex.setContext(obj->dataset());
		ex.prependGeneralMessage(tr("Failed to create editor component for the '%1' object.").arg(obj->objectTitle()));
		ex.reportError();
	}
	return nullptr;
}

/******************************************************************************
* This will bind the editor to the given container.
******************************************************************************/
void PropertiesEditor::initialize(PropertiesPanel* container, const RolloutInsertionParameters& rolloutParams, PropertiesEditor* parentEditor)
{
	OVITO_CHECK_POINTER(container);
	OVITO_ASSERT_MSG(_container == nullptr, "PropertiesEditor::initialize()", "Editor can only be initialized once.");
	OVITO_ASSERT_MSG(_parentEditor == nullptr, "PropertiesEditor::initialize()", "Editor can only be initialized once.");
	_container = container;
	_mainWindow = &container->mainWindow();
	_parentEditor = parentEditor;
	// Forward signals emitted by the parent editor.
	if(parentEditor) {
		connect(parentEditor, &PropertiesEditor::pipelineOutputChanged, this, &PropertiesEditor::pipelineOutputChanged);
		connect(parentEditor, &PropertiesEditor::pipelineInputChanged, this, &PropertiesEditor::pipelineInputChanged);
	}
	createUI(rolloutParams);
	Q_EMIT contentsReplaced(nullptr);
}

/******************************************************************************
* Sets the object being edited in this editor.
******************************************************************************/
void PropertiesEditor::setEditObject(RefTarget* newObject) 
{
	OVITO_ASSERT_MSG(!editObject() || !newObject || newObject->getOOClass().isDerivedFrom(editObject()->getOOClass()),
			"PropertiesEditor::setEditObject()", "This properties editor was not made for this object class.");

	_editObject.set(this, PROPERTY_FIELD(editObject), newObject);
}

/******************************************************************************
* Creates a new rollout in the rollout container and returns
* the empty widget that can then be filled with UI controls.
* The rollout is automatically deleted when the editor is deleted.
******************************************************************************/
QWidget* PropertiesEditor::createRollout(const QString& title, const RolloutInsertionParameters& params, const char* helpPage)
{
	OVITO_ASSERT_MSG(container(), "PropertiesEditor::createRollout()", "Editor has not been properly initialized.");
	QWidget* panel;
	if(params.container() == nullptr || !params.container()->layout() || params.container()->layout()->count() != 0) {
		panel = new QWidget();
		_rollouts.add(panel);

		// Create a new rollout in the rollout container.
		Rollout* rollout = container()->addRollout(panel, QString(), params, helpPage);

		// Helper function which updates the title of the rollout.
		auto updateRolloutTitle = [prefixTitle = params.title(), fixedTitle = title, rollout](RefTarget* target) {
			if(!rollout) return;
			QString effectiveTitle = fixedTitle;

			// If no fixed title has been specified, use the title of the current object being edited.
			if(effectiveTitle.isEmpty() && target) 
				effectiveTitle = target->objectTitle();

			// Let the rollout insertion parameters control the rollout title prefix.
			if(!prefixTitle.isEmpty()) {
				if(prefixTitle.contains(QStringLiteral("%1")))
					effectiveTitle = prefixTitle.arg(effectiveTitle);
				else
					effectiveTitle = prefixTitle;
			}
			rollout->setTitle(std::move(effectiveTitle));
		};
		updateRolloutTitle(editObject());

		// Automatically update rollout title each time a new object is loaded into the editor.
		connect(this, &PropertiesEditor::contentsReplaced, rollout, std::move(updateRolloutTitle));
	}
	else {
		panel = new QWidget(params.container());
		_rollouts.add(panel);

		// Instead of creating a new rollout for the widget, insert widget into a prescribed parent widget.
		params.container()->layout()->addWidget(panel);
	}
	return panel;
}

/******************************************************************************
* Returns the top-level window hosting this editor panel.
******************************************************************************/
QWidget* PropertiesEditor::parentWindow() const 
{ 
	return parentEditor() ? parentEditor()->parentWindow() : container()->window(); 
}

/******************************************************************************
* Completely disables the UI elements in the given rollout widget.
******************************************************************************/
void PropertiesEditor::disableRollout(QWidget* rolloutWidget, const QString& noticeText)
{
	rolloutWidget->setEnabled(false);
	if(Rollout* rollout = container()->findRolloutFromWidget(rolloutWidget)) {
		rollout->setNotice(noticeText);
		// Force a re-layout of the rollouts.
		QTimer::singleShot(100, container(), &RolloutContainer::updateRollouts);	
	}
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool PropertiesEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(source == editObject()) {
		if(event.type() == ReferenceEvent::TargetChanged) {
			Q_EMIT contentsChanged(source);
		}
		else if(event.type() == ReferenceEvent::PipelineCacheUpdated) {
			emitPipelineOutputChangedSignal(this);
		}
		else if(event.type() == ReferenceEvent::PipelineInputChanged) {
			emitPipelineInputChangedSignal(this);
		}
	}
	return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void PropertiesEditor::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(editObject)) {
		setDataset(editObject() ? editObject()->dataset() : nullptr);
		if(oldTarget) oldTarget->unsetObjectEditingFlag();
		if(newTarget) newTarget->setObjectEditingFlag();
		Q_EMIT contentsReplaced(editObject());
		Q_EMIT contentsChanged(editObject());
		emitPipelineInputChangedSignal(this);
		emitPipelineOutputChangedSignal(this);
	}
	RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Changes the value of a non-animatable property field of the object being edited.
******************************************************************************/
void PropertiesEditor::changePropertyFieldValue(const PropertyFieldDescriptor* field, const QVariant& newValue)
{
	if(editObject())
		editObject()->setPropertyFieldValue(field, newValue);
}

/******************************************************************************
* Returns the current input data from the upstream pipeline.
******************************************************************************/
PipelineFlowState PropertiesEditor::getPipelineInput() const
{
	// When editing a modifier, request pipeline input state from the modifier application being edit in the parent editor.
	if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(editObject())) {
		return modApp->evaluateInputSynchronous(PipelineEvaluationRequest(dataset()->animationSettings()->time()));
	}

	// When editing a DataVis element, request pipeline input state from the current scene node.
	if(DataVis* vis = dynamic_object_cast<DataVis>(editObject())) {
		if(PipelineSceneNode* pipelineNode = dynamic_object_cast<PipelineSceneNode>(dataset()->selection()->firstNode())) {
			OVITO_ASSERT(vis->pipelines(true).contains(pipelineNode));
			OVITO_ASSERT(pipelineNode->visElements().contains(vis));
			return pipelineNode->evaluatePipelineSynchronous(false);
		}
	}

	// Sub-editors inherit the information from their parent editor.
	if(parentEditor())
		return parentEditor()->getPipelineInput();
	
	return {};
}

/******************************************************************************
* Returns the current input data from the upstream pipeline.
******************************************************************************/
std::vector<PipelineFlowState> PropertiesEditor::getPipelineInputs() const
{
	std::vector<PipelineFlowState> inputStates;

	// Sub-editors inherit the information from their parent editor.
	if(parentEditor())
		inputStates = parentEditor()->getPipelineInputs();

	// When editing a modifier, get the pipeline state from the modifier applications.
	if(Modifier* modifier = dynamic_object_cast<Modifier>(editObject())) {
		PipelineEvaluationRequest request(dataset()->animationSettings()->time());
		for(ModifierApplication* modApp : modifier->modifierApplications()) {
			inputStates.push_back(modApp->evaluateInputSynchronous(request));
		}
	}

	// When editing a DataVis element, get the pipeline state from the scene nodes.
	if(DataVis* vis = dynamic_object_cast<DataVis>(editObject())) {
		for(PipelineSceneNode* pipeline : vis->pipelines(true)) {
			inputStates.push_back(pipeline->evaluatePipelineSynchronous(false));
		}
	}
	
	return inputStates;
}

/******************************************************************************
* Returns the current output data produced by the object being edited.
******************************************************************************/
PipelineFlowState PropertiesEditor::getPipelineOutput() const
{
	if(Modifier* modifier = dynamic_object_cast<Modifier>(editObject())) {
		// If it's a modifier being edited, request output from the parent editor, which hosts the ModifierApplication.
		if(parentEditor())
			return parentEditor()->getPipelineOutput();
	}
	else if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(editObject())) {
		// Request pipeline output state from the modifier application.
		return modApp->evaluateSynchronousAtCurrentTime();
	}
	return {};
}

/******************************************************************************
* Returns the first ModifierApplication of the modifier currently being edited.
******************************************************************************/
ModifierApplication* PropertiesEditor::modifierApplication() const
{
	if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(editObject()))
		return modApp;
	else if(parentEditor())
		return parentEditor()->modifierApplication();
	else
		return nullptr;
}

/******************************************************************************
* Returns the list of ModifierApplications of the modifier currently being edited.
******************************************************************************/
QVector<ModifierApplication*> PropertiesEditor::modifierApplications() const
{
	if(Modifier* modifier = dynamic_object_cast<Modifier>(editObject()))
		return modifier->modifierApplications();
	else if(parentEditor())
		return parentEditor()->modifierApplications();
	else
		return {};
}

/******************************************************************************
* For an editor of a DataVis element, returns the data collection path to 
* the DataObject which the DataVis element is attached to.
******************************************************************************/
ConstDataObjectRefPath PropertiesEditor::getVisDataObjectPath() const
{
	if(DataVis* vis = dynamic_object_cast<DataVis>(editObject())) {
		// We'll now try to find the DataObject this DataVis element is associated with.
		// Let's start looking in the output data collection of the currently selected pipeline scene node.
		if(PipelineSceneNode* pipelineNode = dynamic_object_cast<PipelineSceneNode>(dataset()->selection()->firstNode())) {
			const PipelineFlowState& state = pipelineNode->evaluatePipelineSynchronous(false);
			std::vector<ConstDataObjectPath> dataObjectPaths = pipelineNode->getDataObjectsForVisElement(state, vis);
			if(!dataObjectPaths.empty()) {
				// Return just the first path from the list.
				return ConstDataObjectRefPath(dataObjectPaths.front().begin(), dataObjectPaths.front().end());
			}
		}
		return {};
	}
	else if(parentEditor())
		return parentEditor()->getVisDataObjectPath();
	else
		return {};
}

/******************************************************************************
* For an editor of a DataVis element, returns the DataObject to which the 
* DataVis element is attached.
******************************************************************************/
ConstDataObjectRef PropertiesEditor::getVisDataObject() const
{
	if(ConstDataObjectRefPath path = getVisDataObjectPath(); !path.empty())
		return std::move(path.back());
	return {};
}

}	// End of namespace
