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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/StableComboBox.h>
#include "ParameterUI.h"

namespace Ovito {

/******************************************************************************
* A list view that shows the fixed set of delegates of a MultiDelegatingModifier,
* which can each be enabled or disabled by the user.
******************************************************************************/
class OVITO_GUI_EXPORT ModifierDelegateFixedListParameterUI : public ParameterUI
{
    OVITO_CLASS(ModifierDelegateFixedListParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor);

    /// Destructor.
    ~ModifierDelegateFixedListParameterUI();

    /// This returns the container widget managed by this class.
    QWidget* containerWidget() const { return _containerWidget; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to. The parameter UI should react to this change appropriately and
    /// show the properties value for the new edit object in the UI.
    virtual void resetUI() override;

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
    virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) override;

    /// Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override;

    /// Is called when a RefTarget has been replaced in a VectorReferenceField of this RefMaker.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private Q_SLOTS:

    /// Is called when the user toggles the enabled state of a delegate.
    void onDelegateToggled(bool checked);

    /// Is called when the user selects a different data object for a delegate.
    void onDataObjectSelected(int index);

private:

    /// Fills the combo box with the list of data objects the given delegate can handle.
    bool populateObjectList(ModifierDelegate* delegate, StableComboBox* comboBox, const DataObjectReference& selectedDataObject, const std::vector<PipelineFlowState>& pipelineInputs);

    /// The container widget managed by this parameter UI.
    QPointer<QWidget> _containerWidget;

    /// The QCheckBox widgets that allow to enable/disable each delegate.
    QVector<QCheckBox*> _delegateCheckBoxes;

    /// The QComboBox widgets that allow to select the target object for each delegate.
    QVector<StableComboBox*> _delegateObjectLists;

    /// The current list of delegates.
    DECLARE_VECTOR_REFERENCE_FIELD(OORef<ModifierDelegate>, delegates);
};

}   // End of namespace
