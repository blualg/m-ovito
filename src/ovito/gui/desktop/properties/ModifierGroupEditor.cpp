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
#include <ovito/gui/desktop/properties/ModifierGroupEditor.h>
#include <ovito/core/dataset/pipeline/ModifierGroup.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ModifierGroupEditor);
SET_OVITO_OBJECT_EDITOR(ModifierGroup, ModifierGroupEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ModifierGroupEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
	_rolloutParams = rolloutParams;
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ModifierGroupEditor::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	PropertiesEditor::referenceReplaced(field, oldTarget, newTarget, listIndex);
	if(field == PROPERTY_FIELD(editObject)) {
		updateSubEditors();
		disconnect(_modifierAddedConnection);
		disconnect(_modifierRemovedConnection);
		if(ModifierGroup* group = static_object_cast<ModifierGroup>(editObject())) {
			_modifierAddedConnection = connect(group, &ModifierGroup::modifierAdded, this, &ModifierGroupEditor::updateSubEditors, Qt::UniqueConnection);
			_modifierRemovedConnection = connect(group, &ModifierGroup::modifierRemoved, this, &ModifierGroupEditor::updateSubEditors, Qt::UniqueConnection);
		}
	}
}

/******************************************************************************
* Rebuilds the list of sub-editors for the current edit object.
******************************************************************************/
void ModifierGroupEditor::updateSubEditors()
{
	handleExceptions([&] {
		auto subEditorIter = _subEditors.begin();
		if(ModifierGroup* group = static_object_cast<ModifierGroup>(editObject())) {
			// Get the group's modifier applications.
			QVector<ModifierApplication*> modApps = group->modifierApplications();
			for(ModifierApplication* modApp : modApps) {
				// Open editor for this sub-object.
				if(subEditorIter != _subEditors.end() && (*subEditorIter)->editObject() != nullptr
						&& (*subEditorIter)->editObject()->getOOClass() == modApp->getOOClass()) {
					// Re-use existing editor.
					(*subEditorIter)->setEditObject(modApp);
					++subEditorIter;
				}
				else {
					// Create a new sub-editor for this sub-object.
					OORef<PropertiesEditor> editor = PropertiesEditor::create(mainWindow(), modApp);
					if(editor) {
						editor->initialize(container(), _rolloutParams, this);
						editor->setEditObject(modApp);
						_subEditors.erase(subEditorIter, _subEditors.end());
						_subEditors.push_back(std::move(editor));
						subEditorIter = _subEditors.end();
					}
				}
			}
		}
		// Close excess sub-editors.
		_subEditors.erase(subEditorIter, _subEditors.end());
	});
}

}	// End of namespace
