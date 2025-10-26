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
#include <ovito/gui/desktop/properties/NumericalParameterUI.h>
#include <ovito/gui/desktop/widgets/general/EnterLineEdit.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(NumericalParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void NumericalParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const QMetaObject* defaultParameterUnitType)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _parameterUnitType = defaultParameterUnitType;

    // Look up the ParameterUnit type for this parameter.
    if(propField->numericalParameterInfo() && propField->numericalParameterInfo()->unitType)
        _parameterUnitType = propField->numericalParameterInfo()->unitType;

    initUIControls(propField->displayName() + ":");
}

/******************************************************************************
* Creates the widgets for this parameter UI.
******************************************************************************/
void NumericalParameterUI::initUIControls(const QString& labelText)
{
    // Create UI widgets.
    _label = new QLabel(labelText);
    _textBox = new EnterLineEdit();
    _spinner = new SpinnerWidget();
    connect(spinner(), &SpinnerWidget::valueChanged, this, &NumericalParameterUI::updatePropertyValue);
    spinner()->setTextBox(_textBox);
    spinner()->enableAutomaticUndo(ui(), tr("Change parameter"));
    if(propertyField()->numericalParameterInfo() != nullptr) {
        spinner()->setMinValue(propertyField()->numericalParameterInfo()->minValue);
        spinner()->setMaxValue(propertyField()->numericalParameterInfo()->maxValue);
    }

    // Create the reset button -> will be added to the layout in createFieldLayout()
    if(propertyField()->flags().testFlag(PROPERTY_FIELD_RESETTABLE)) {
        createResetAction();
    }

    // Create animate button if parameter is animation (i.e. it's a reference to a Controller object).
    if(isReferenceFieldUI() && propertyField()->targetClass()->isDerivedFrom(Controller::OOClass())) {
        _animateButton = new QToolButton();
        _animateButton->setText(tr("A"));
        _animateButton->setFocusPolicy(Qt::NoFocus);
        static_cast<QToolButton*>(_animateButton.data())->setAutoRaise(true);
        static_cast<QToolButton*>(_animateButton.data())->setToolButtonStyle(Qt::ToolButtonTextOnly);
        _animateButton->setToolTip(tr("Animate this parameter..."));
        _animateButton->setEnabled(false);
        connect(_animateButton.data(), &QAbstractButton::clicked, this, &NumericalParameterUI::openAnimationKeyEditor);
    }
}

/******************************************************************************
* Destructor.
******************************************************************************/
NumericalParameterUI::~NumericalParameterUI()
{
    // Release widgets managed by this class.
    delete label();
    delete spinner();
    delete textBox();
    delete animateButton();
    delete _layout.data();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void NumericalParameterUI::resetUI()
{
    bool enable = editObject() && isEnabled() && !editor()->isReadOnly();
    if(spinner()) {
        spinner()->setEnabled(enable);
        if(editObject()) {
            if(spinner()->unit() == nullptr) {
                if(parameterUnitType())
                    spinner()->setUnit(ui().unitsManager().getUnit(parameterUnitType()));
            }
        }
        else {
            spinner()->setFloatValue(spinner()->hasStandardValue() ? spinner()->standardValue() : 0);
        }
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &NumericalParameterUI::updateUI, Qt::UniqueConnection);
    }

    PropertyParameterUI::resetUI();

    if(animateButton())
        animateButton()->setEnabled(enable && parameterObject());
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void NumericalParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(spinner()) {
        if(isReferenceFieldUI()) {
            spinner()->setEnabled(parameterObject() && isEnabled());
        }
        else {
            spinner()->setEnabled(editObject() && isEnabled());
        }
    }
    if(animateButton())
        animateButton()->setEnabled(editObject() && parameterObject() && isEnabled());
}

/******************************************************************************
* Shows or hides all widgets.
******************************************************************************/
void NumericalParameterUI::setVisible(bool visible)
{
    if(spinner())
        spinner()->setVisible(visible);
    if(textBox())
        textBox()->setVisible(visible);
    if(label())
        label()->setVisible(visible);
    if(animateButton())
        animateButton()->setVisible(visible);
    if(menuToolButton())
        menuToolButton()->setVisible(visible);
}

/******************************************************************************
* Creates a QLayout that contains the text box and the spinner widget.
******************************************************************************/
QLayout* NumericalParameterUI::createFieldLayout()
{
    if(!_layout.data()) {
        _layout = new QHBoxLayout();
        _layout->setContentsMargins(0,0,0,0);
        _layout->setSpacing(0);
        _layout->addWidget(textBox());
        _layout->addWidget(spinner());
        // Show menu button, if any actions are defined
        if(menuToolButton()) {
            _layout->addWidget(menuToolButton());
        }
        if(animateButton())
            _layout->addWidget(animateButton());
    }
    return _layout.data();
}

}  // namespace Ovito
