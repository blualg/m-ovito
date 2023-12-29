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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/AffineTransformationParameterUI.h>
#include <ovito/gui/desktop/properties/ModifierDelegateFixedListParameterUI.h>
#include <ovito/stdmod/modifiers/AffineTransformationModifier.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "AffineTransformationModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AffineTransformationModifierEditor);
SET_OVITO_OBJECT_EDITOR(AffineTransformationModifier, AffineTransformationModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void AffineTransformationModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create the first rollout.
    QWidget* rollout = rollout = createRollout(tr("Transformation"), rolloutParams, "manual:particles.modifiers.affine_transformation");

    QVBoxLayout* topLayout = new QVBoxLayout(rollout);
    topLayout->setContentsMargins(8,8,8,8);
    topLayout->setSpacing(4);

    BooleanRadioButtonParameterUI* relativeModeUI = new BooleanRadioButtonParameterUI(this, PROPERTY_FIELD(AffineTransformationModifier::relativeMode));

    relativeModeUI->buttonTrue()->setText(tr("Transformation matrix:"));
    topLayout->addWidget(relativeModeUI->buttonTrue());

    QGridLayout* layout = new QGridLayout();
    layout->setContentsMargins(30,4,4,4);
    layout->setHorizontalSpacing(0);
    layout->setVerticalSpacing(2);
    topLayout->addLayout(layout);

    QGridLayout* sublayout = new QGridLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->setSpacing(0);
    sublayout->setColumnStretch(0, 1);
    sublayout->addWidget(new QLabel(tr("Rotate/Scale/Shear:")), 0, 0, Qt::Alignment(Qt::AlignBottom | Qt::AlignLeft));
    QAction* enterRotationAction = new QAction(tr("Enter rotation..."), this);
    QToolButton* enterRotationButton = new QToolButton();
    enterRotationButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    enterRotationButton->setDefaultAction(enterRotationAction);
    sublayout->addWidget(enterRotationButton, 0, 1, Qt::Alignment(Qt::AlignBottom | Qt::AlignRight));
    enterRotationAction->setEnabled(false);
    connect(relativeModeUI->buttonTrue(), &QRadioButton::toggled, enterRotationAction, &QAction::setEnabled);
    connect(enterRotationAction, &QAction::triggered, this, &AffineTransformationModifierEditor::onEnterRotation);
    layout->addLayout(sublayout, 0, 0, 1, 8);

    for(int col = 0; col < 3; col++) {
        layout->setColumnStretch(col*3 + 0, 1);
        if(col < 2) layout->setColumnMinimumWidth(col*3 + 2, 4);
        for(int row=0; row<4; row++) {
            QLineEdit* lineEdit = new QLineEdit(rollout);
            SpinnerWidget* spinner = new SpinnerWidget(rollout);
            lineEdit->setEnabled(false);
            spinner->setEnabled(false);
            if(row < 3) {
                elementSpinners[row][col] = spinner;
                spinner->setProperty("column", col);
                spinner->setProperty("row", row);
            }
            else {
                elementSpinners[col][row] = spinner;
                spinner->setProperty("column", row);
                spinner->setProperty("row", col);
            }
            spinner->setTextBox(lineEdit);

            int gridRow = (row == 3) ? 5 : (row+1);
            layout->addWidget(lineEdit, gridRow, col*3 + 0);
            layout->addWidget(spinner, gridRow, col*3 + 1);

            connect(spinner, &SpinnerWidget::spinnerValueChanged, this, &AffineTransformationModifierEditor::onSpinnerValueChanged);
            connect(spinner, &SpinnerWidget::spinnerDragStart, this, &AffineTransformationModifierEditor::onSpinnerDragStart);
            connect(spinner, &SpinnerWidget::spinnerDragStop, this, &AffineTransformationModifierEditor::onSpinnerDragStop);
            connect(spinner, &SpinnerWidget::spinnerDragAbort, this, &AffineTransformationModifierEditor::onSpinnerDragAbort);
            connect(relativeModeUI->buttonTrue(), &QRadioButton::toggled, spinner, &SpinnerWidget::setEnabled);
            connect(relativeModeUI->buttonTrue(), &QRadioButton::toggled, lineEdit, &QLineEdit::setEnabled);
        }
    }

    QHBoxLayout* hboxsublayout = new QHBoxLayout();
    hboxsublayout->setContentsMargins(0,4,0,0);
    hboxsublayout->setSpacing(4);

    hboxsublayout->addWidget(new QLabel(tr("Translation:")));
    layout->addLayout(hboxsublayout, 4, 0, 1, 8);

    BooleanParameterUI* translationReducedCoordinatesUI = new BooleanParameterUI(this, PROPERTY_FIELD(AffineTransformationModifier::translationReducedCoordinates));
    connect(relativeModeUI->buttonTrue(), &QRadioButton::toggled, translationReducedCoordinatesUI, &BooleanParameterUI::setEnabled);
    translationReducedCoordinatesUI->checkBox()->setText(tr("In reduced cell coordinates"));
    hboxsublayout->addWidget(translationReducedCoordinatesUI->checkBox(), 1, Qt::AlignRight | Qt::AlignVCenter);
    connect(translationReducedCoordinatesUI, &BooleanParameterUI::valueEntered, this, &AffineTransformationModifierEditor::onReducedCoordinatesOptionChanged);

    relativeModeUI->buttonFalse()->setText(tr("Transform to target cell:"));
    topLayout->addWidget(relativeModeUI->buttonFalse());

    layout = new QGridLayout();
    layout->setContentsMargins(30,4,4,4);
    layout->setHorizontalSpacing(0);
    layout->setVerticalSpacing(2);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);
    layout->setColumnStretch(4, 1);
    layout->setColumnMinimumWidth(1, 4);
    layout->setColumnMinimumWidth(3, 4);
    topLayout->addLayout(layout);
    AffineTransformationParameterUI* destinationCellUI;

    for(size_t v = 0; v < 3; v++) {
        layout->addWidget(new QLabel(tr("Cell vector %1:").arg(v+1)), v*2, 0, 1, 8);
        for(size_t r = 0; r < 3; r++) {
            destinationCellUI = new AffineTransformationParameterUI(this, PROPERTY_FIELD(AffineTransformationModifier::targetCell), r, v);
            destinationCellUI->setEnabled(false);
            layout->addLayout(destinationCellUI->createFieldLayout(), v*2+1, r*2);
            connect(relativeModeUI->buttonFalse(), &QRadioButton::toggled, destinationCellUI, &AffineTransformationParameterUI::setEnabled);
        }
    }

    layout->addWidget(new QLabel(tr("Cell origin:")), 6, 0, 1, 8);
    for(size_t r = 0; r < 3; r++) {
        destinationCellUI = new AffineTransformationParameterUI(this, PROPERTY_FIELD(AffineTransformationModifier::targetCell), r, 3);
        destinationCellUI->setEnabled(false);
        layout->addLayout(destinationCellUI->createFieldLayout(), 7, r*2);
        connect(relativeModeUI->buttonFalse(), &QRadioButton::toggled, destinationCellUI, &AffineTransformationParameterUI::setEnabled);
    }

    // Update spinner values when a new object has been loaded into the editor.
    connect(this, &PropertiesEditor::contentsChanged, this, &AffineTransformationModifierEditor::updateUI);

    // Create a second rollout.
    rollout = createRollout(tr("Operate on"), rolloutParams.after(rollout), "manual:particles.modifiers.slice");

    // Create the rollout contents.
    topLayout = new QVBoxLayout(rollout);
    topLayout->setContentsMargins(4,4,4,4);
    topLayout->setSpacing(12);

    ModifierDelegateFixedListParameterUI* delegatesPUI = new ModifierDelegateFixedListParameterUI(this, rolloutParams.after(rollout));
    topLayout->addWidget(delegatesPUI->listWidget(141));

    BooleanParameterUI* selectionUI = new BooleanParameterUI(this, PROPERTY_FIELD(AffineTransformationModifier::selectionOnly));
    topLayout->addWidget(selectionUI->checkBox());
}

/******************************************************************************
* This method updates the displayed matrix values.
******************************************************************************/
void AffineTransformationModifierEditor::updateUI()
{
    AffineTransformationModifier* mod = dynamic_object_cast<AffineTransformationModifier>(editObject());
    if(!mod) return;

    const AffineTransformation& tm = mod->transformationTM();

    for(int row = 0; row < 3; row++) {
        for(int column = 0; column < 4; column++) {
            if(!elementSpinners[row][column]->isDragging())
                elementSpinners[row][column]->setFloatValue(tm(row, column));
        }
    }
}

/******************************************************************************
* Is called when the spinner value has changed.
******************************************************************************/
void AffineTransformationModifierEditor::onSpinnerValueChanged()
{
    if(!_undoTransaction.operation()) {
        performTransaction(tr("Change parameter"), [&] {
            updateParameterValue();
        });
    }
    else {
        _undoTransaction.revert();
        performActions(_undoTransaction, [&] {
            updateParameterValue();
        });
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in transformation controller.
******************************************************************************/
void AffineTransformationModifierEditor::updateParameterValue()
{
    AffineTransformationModifier* mod = dynamic_object_cast<AffineTransformationModifier>(editObject());
    if(!mod) return;

    // Get the spinner whose value has changed.
    SpinnerWidget* spinner = qobject_cast<SpinnerWidget*>(sender());
    OVITO_CHECK_POINTER(spinner);

    AffineTransformation tm = mod->transformationTM();

    int column = spinner->property("column").toInt();
    int row = spinner->property("row").toInt();

    tm(row, column) = spinner->floatValue();
    mod->setTransformationTM(tm);
}

/******************************************************************************
* Is called when the user begins dragging the spinner interactively.
******************************************************************************/
void AffineTransformationModifierEditor::onSpinnerDragStart()
{
    _undoTransaction.begin(mainWindow(), tr("Change parameter"));
}

/******************************************************************************
* Is called when the user stops dragging the spinner interactively.
******************************************************************************/
void AffineTransformationModifierEditor::onSpinnerDragStop()
{
    _undoTransaction.commit();
}

/******************************************************************************
* Is called when the user aborts dragging the spinner interactively.
******************************************************************************/
void AffineTransformationModifierEditor::onSpinnerDragAbort()
{
    _undoTransaction.cancel();
}

/******************************************************************************
* Is called when the user switches between Cartesian and reduced cell coordinates for the translation vector.
******************************************************************************/
void AffineTransformationModifierEditor::onReducedCoordinatesOptionChanged()
{
    AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(editObject());
    if(!mod) return;

    const PipelineFlowState& input = getPipelineInput();
    const SimulationCell* cell = input.getObject<SimulationCell>();
    if(!cell) return;

    // Automatically convert translation vector to/from reduced cell coordinates.
    AffineTransformation tm = mod->transformationTM();
    if(mod->translationReducedCoordinates()) {
        AffineTransformation inv;
        if((tm * cell->matrix()).inverse(inv))
            tm.translation() = inv * tm.translation();
    }
    else {
        tm.translation() = tm * (cell->matrix() * tm.translation());
    }
    for(size_t dim = 0; dim < 3; dim++)
        if(std::abs(tm.translation()[dim]) < FLOATTYPE_EPSILON)
            tm.translation()[dim] = 0;

    mod->setTransformationTM(tm);
}

/******************************************************************************
* Is called when the user presses the 'Enter rotation' button.
* Displays a dialog box, which lets the user enter a rotation axis and angle.
* Computes the rotation matrix from these parameters.
******************************************************************************/
void AffineTransformationModifierEditor::onEnterRotation()
{
    AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(editObject());
    if(!mod) return;

    _undoTransaction.begin(mainWindow(), tr("Set transformation matrix"));
    if(!handleExceptions([&] {
        QDialog dlg(container()->window());
        dlg.setWindowTitle(tr("Enter rotation axis and angle"));
        QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);

        QGridLayout* layout = new QGridLayout();
        layout->setContentsMargins(0,0,0,0);
        layout->addWidget(new QLabel(tr("Rotation axis:")), 0, 0, 1, 8);
        layout->setColumnStretch(0, 1);
        layout->setColumnStretch(3, 1);
        layout->setColumnStretch(6, 1);
        layout->setColumnMinimumWidth(2, 4);
        layout->setColumnMinimumWidth(5, 4);
        layout->setVerticalSpacing(2);
        layout->setHorizontalSpacing(0);
        QLineEdit* axisEditX = new QLineEdit();
        QLineEdit* axisEditY = new QLineEdit();
        QLineEdit* axisEditZ = new QLineEdit();
        SpinnerWidget* axisSpinnerX = new SpinnerWidget();
        SpinnerWidget* axisSpinnerY = new SpinnerWidget();
        SpinnerWidget* axisSpinnerZ = new SpinnerWidget();
        axisSpinnerX->setTextBox(axisEditX);
        axisSpinnerY->setTextBox(axisEditY);
        axisSpinnerZ->setTextBox(axisEditZ);
        axisSpinnerX->setUnit(mainWindow().unitsManager().worldUnit());
        axisSpinnerY->setUnit(mainWindow().unitsManager().worldUnit());
        axisSpinnerZ->setUnit(mainWindow().unitsManager().worldUnit());
        layout->addWidget(axisEditX, 1, 0);
        layout->addWidget(axisSpinnerX, 1, 1);
        layout->addWidget(axisEditY, 1, 3);
        layout->addWidget(axisSpinnerY, 1, 4);
        layout->addWidget(axisEditZ, 1, 6);
        layout->addWidget(axisSpinnerZ, 1, 7);
        layout->addWidget(new QLabel(tr("Angle:")), 2, 0, 1, 8);
        QLineEdit* angleEdit = new QLineEdit();
        SpinnerWidget* angleSpinner = new SpinnerWidget();
        angleSpinner->setTextBox(angleEdit);
        angleSpinner->setUnit(mainWindow().unitsManager().angleUnit());
        layout->addWidget(angleEdit, 3, 0);
        layout->addWidget(angleSpinner, 3, 1);
        layout->addWidget(new QLabel(tr("Center of rotation:")), 4, 0, 1, 8);
        QLineEdit* centerEditX = new QLineEdit();
        QLineEdit* centerEditY = new QLineEdit();
        QLineEdit* centerEditZ = new QLineEdit();
        SpinnerWidget* centerSpinnerX = new SpinnerWidget();
        SpinnerWidget* centerSpinnerY = new SpinnerWidget();
        SpinnerWidget* centerSpinnerZ = new SpinnerWidget();
        centerSpinnerX->setTextBox(centerEditX);
        centerSpinnerY->setTextBox(centerEditY);
        centerSpinnerZ->setTextBox(centerEditZ);
        centerSpinnerX->setUnit(mainWindow().unitsManager().worldUnit());
        centerSpinnerY->setUnit(mainWindow().unitsManager().worldUnit());
        centerSpinnerZ->setUnit(mainWindow().unitsManager().worldUnit());
        layout->addWidget(centerEditX, 5, 0);
        layout->addWidget(centerSpinnerX, 5, 1);
        layout->addWidget(centerEditY, 5, 3);
        layout->addWidget(centerSpinnerY, 5, 4);
        layout->addWidget(centerEditZ, 5, 6);
        layout->addWidget(centerSpinnerZ, 5, 7);
        mainLayout->addLayout(layout);

        // Decompose current transformation matrix into axis-angle form.
        const AffineTransformation tm = mod->effectiveAffineTransformation(getPipelineInput());
        Rotation rot(tm);
        angleSpinner->setFloatValue(rot.angle());
        axisSpinnerX->setFloatValue(rot.axis().x());
        axisSpinnerY->setFloatValue(rot.axis().y());
        axisSpinnerZ->setFloatValue(rot.axis().z());
        Matrix3 r = tm.linear();
        r(0,0) -= 1;
        r(1,1) -= 1;
        r(2,2) -= 1;
        Plane3 p1, p2;
        size_t i = 0;
        for(i = 0; i < 3; i++)
            if(!r.row(i).isZero()) {
                p1 = Plane3(r.row(i), -tm(i, 3));
                i++;
                break;
            }
        for(; i < 3; i++)
            if(!r.row(i).isZero()) {
                p2 = Plane3(r.row(i), -tm(i, 3));
                break;
            }
        if(i != 3) {
            p1.normalizePlane();
            p2.normalizePlane();
            FloatType d = p1.normal.dot(p2.normal);
            FloatType denom = (FloatType(1) - d*d);
            if(std::abs(denom) > FLOATTYPE_EPSILON) {
                FloatType c1 = (p1.dist  - p2.dist * d) / denom;
                FloatType c2 = (p2.dist  - p1.dist * d) / denom;
                Vector3 center = c1 * p1.normal + c2 * p2.normal;
                centerSpinnerX->setFloatValue(center.x());
                centerSpinnerY->setFloatValue(center.y());
                centerSpinnerZ->setFloatValue(center.z());
            }
        }
        else {
            Point3 center = Point3::Origin();
            if(Viewport* vp = activeViewport())
                center = vp->orbitCenter();
            centerSpinnerX->setFloatValue(center.x());
            centerSpinnerY->setFloatValue(center.y());
            centerSpinnerZ->setFloatValue(center.z());
        }

        auto updateMatrix = [mod, angleSpinner, axisSpinnerX, axisSpinnerY, axisSpinnerZ, centerSpinnerX, centerSpinnerY, centerSpinnerZ, this]() {
            Vector3 axis(axisSpinnerX->floatValue(), axisSpinnerY->floatValue(), axisSpinnerZ->floatValue());
            if(axis == Vector3::Zero()) axis = Vector3(0,0,1);
            Vector3 center(centerSpinnerX->floatValue(), centerSpinnerY->floatValue(), centerSpinnerZ->floatValue());
            Rotation rot(axis, angleSpinner->floatValue());
            AffineTransformation tm = AffineTransformation::translation(center) * AffineTransformation::rotation(rot) * AffineTransformation::translation(-center);
            _undoTransaction.revert();
            performActions(_undoTransaction, [&] {
                mod->setTranslationReducedCoordinates(false);
                mod->setTransformationTM(tm);
            });
        };

        connect(angleSpinner, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(axisSpinnerX, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(axisSpinnerY, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(axisSpinnerZ, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(centerSpinnerX, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(centerSpinnerY, &SpinnerWidget::spinnerValueChanged, updateMatrix);
        connect(centerSpinnerZ, &SpinnerWidget::spinnerValueChanged, updateMatrix);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        mainLayout->addWidget(buttonBox);
        if(dlg.exec() == QDialog::Accepted) {
            _undoTransaction.commit();
        }
        else {
            _undoTransaction.cancel();
        }
    })) _undoTransaction.cancel();
}

}   // End of namespace
