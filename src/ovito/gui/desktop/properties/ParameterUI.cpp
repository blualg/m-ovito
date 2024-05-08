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
#include <ovito/gui/desktop/properties/ParameterUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParameterUI);
DEFINE_REFERENCE_FIELD(ParameterUI, editObject);

/******************************************************************************
* Constructor.
******************************************************************************/
ParameterUI::ParameterUI(PropertiesEditor* editor) : _editor(editor)
{
    OVITO_ASSERT(editor);

    if(editor->editObject())
        setEditObject(editor->editObject());

    // Connect to the contentsReplaced() signal of the editor to synchronize the
    // parameter UI's edit object with the editor's edit object.
    connect(editor, &PropertiesEditor::contentsReplaced, this, &ParameterUI::setEditObject);
}

}   // End of namespace
