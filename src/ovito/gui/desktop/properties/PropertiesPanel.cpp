////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/******************************************************************************
* Constructs the panel.
******************************************************************************/
PropertiesPanel::PropertiesPanel(MainWindow& mainWindow, QWidget* parent) :
    RolloutContainer(mainWindow, parent), _mainWindow(mainWindow)
{
}

/******************************************************************************
* Destructs the panel.
******************************************************************************/
PropertiesPanel::~PropertiesPanel()
{
}

/******************************************************************************
* Sets the target object being edited in the panel.
******************************************************************************/
void PropertiesPanel::setEditObject(RefTarget* newEditObject, OORef<PropertiesEditor> newEditor)
{
    if(newEditObject == editObject() && (newEditObject != nullptr) == (editor() != nullptr) && !newEditor)
        return;

    if(editor()) {
        OVITO_CHECK_OBJECT_POINTER(editor());

        // Can we re-use the old editor?
        if(newEditObject != nullptr && editor()->editObject() != nullptr
            && editor()->editObject()->getOOClass() == newEditObject->getOOClass()
            && !newEditor) {

            editor()->handleExceptions([&]() {
                editor()->setEditObject(newEditObject);
            });
            return;
        }
        else {
            // Close previous editor.
            _editor.reset();
        }
    }

    if(newEditObject) {
        // Open new properties editor.
        if(!mainWindow().handleExceptions([&]() {
            _editor = newEditor ? std::move(newEditor) : PropertiesEditor::create(mainWindow(), newEditObject);
            if(editor()) {
                if(!editor()->container())
                    editor()->initialize(this, RolloutInsertionParameters(), nullptr);
                editor()->setEditObject(newEditObject);
            }
        })) {
            _editor.reset();
        }
    }
}

/******************************************************************************
* Returns the target object being edited in the panel
******************************************************************************/
RefTarget* PropertiesPanel::editObject() const
{
    return editor() ? editor()->editObject() : nullptr;
}

}   // End of namespace
