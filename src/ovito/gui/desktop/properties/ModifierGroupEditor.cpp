////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModifierGroupEditor);
SET_OVITO_OBJECT_EDITOR(ModifierGroup, ModifierGroupEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ModifierGroupEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    _rolloutParams = rolloutParams;
    connect(this, &PropertiesEditor::contentsReplaced, this, &ModifierGroupEditor::updateSubEditors);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ModifierGroupEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ModifierGroup::ModifierAddedOrRemoved) {
        updateSubEditors();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Rebuilds the list of sub-editors for the current edit object.
******************************************************************************/
void ModifierGroupEditor::updateSubEditors()
{
    handleExceptions([&] {
        auto subEditorIter = _subEditors.begin();
        if(ModifierGroup* group = static_object_cast<ModifierGroup>(editObject())) {
            // Get the group's modifier nodes.
            QVector<ModificationNode*> nodes = group->nodes();
            for(ModificationNode* node : nodes) {
                // Open editor for this sub-object.
                if(subEditorIter != _subEditors.end() && (*subEditorIter)->editObject() != nullptr
                        && (*subEditorIter)->editObject()->getOOClass() == node->getOOClass()) {
                    // Re-use existing editor.
                    (*subEditorIter)->setEditObject(node);
                    ++subEditorIter;
                }
                else {
                    // Create a new sub-editor for this sub-object.
                    OORef<PropertiesEditor> editor = PropertiesEditor::create(mainWindow(), node);
                    if(editor) {
                        editor->initialize(container(), _rolloutParams, this);
                        editor->setEditObject(node);
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

}   // End of namespace
