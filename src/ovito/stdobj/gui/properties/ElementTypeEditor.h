////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the ElementType class.
 */
class ElementTypeEditor : public PropertiesEditor
{
    OVITO_CLASS(ElementTypeEditor)

public:

    /// Constructor.
    using PropertiesEditor::PropertiesEditor;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private Q_SLOTS:

    /// Saves the current settings as defaults for the element type.
    void onSaveAsDefault();

private:

    QLabel* _numericIdLabel;
    QPushButton* _setAsDefaultBtn;
    StringParameterUI* _namePUI;
};

}   // End of namespace
