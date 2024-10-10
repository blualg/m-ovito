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
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "AdjustViewDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AdjustViewDialog::AdjustViewDialog(MainWindow& mainWindow, Viewport* viewport, QWidget* parent) :
    QDockWidget(tr("Adjust View"), parent),
    _mainWindow(mainWindow)
{
    setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::NoDockWidgetArea);
    setFloating(true);
    setAttribute(Qt::WA_DeleteOnClose); // Make sure AdjustViewDialog instance gets deleted when the dialog is closed.

    QWidget* widget = new QWidget();
    setWidget(widget);

    _oldViewType = viewport->viewType();
    _oldCameraTM = viewport->cameraTransformation();
    _oldFOV = viewport->fieldOfView();

    _viewportListener.setTarget(viewport);
    _viewportListener.connect(this, [this](RefTarget* source, const ReferenceEvent& event) {
        // Update the values displayed in the dialog when the viewport camera is moved by the user.
        if(event.type() == ReferenceEvent::TargetChanged)
            updateGUI();
        // Close the dialog when the viewport is deleted.
        else if(event.type() == ReferenceEvent::TargetDeleted)
            close();
    });

    QVBoxLayout* mainLayout = new QVBoxLayout(widget);

    QGroupBox* viewPosBox = new QGroupBox(tr("Camera position"));
    mainLayout->addWidget(viewPosBox);

    QGridLayout* gridLayout = new QGridLayout(viewPosBox);
    gridLayout->setColumnStretch(1,1);
    gridLayout->setColumnStretch(2,1);
    gridLayout->setColumnStretch(3,1);
    gridLayout->addWidget(new QLabel(tr("XYZ:")), 0, 0);

    QHBoxLayout* fieldLayout;
    QLineEdit* textBox;

    _camPosXSpinner = new SpinnerWidget();
    _camPosYSpinner = new SpinnerWidget();
    _camPosZSpinner = new SpinnerWidget();
    _camPosXSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _camPosYSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _camPosZSpinner->setUnit(mainWindow.unitsManager().worldUnit());

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camPosXSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camPosXSpinner);
    gridLayout->addLayout(fieldLayout, 0, 1);
    connect(_camPosXSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camPosYSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camPosYSpinner);
    gridLayout->addLayout(fieldLayout, 0, 2);
    connect(_camPosYSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camPosZSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camPosZSpinner);
    gridLayout->addLayout(fieldLayout, 0, 3);
    connect(_camPosZSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    QGroupBox* viewDirBox = new QGroupBox(tr("View direction"));
    mainLayout->addWidget(viewDirBox);

    gridLayout = new QGridLayout(viewDirBox);
    gridLayout->setColumnStretch(1,1);
    gridLayout->setColumnStretch(2,1);
    gridLayout->setColumnStretch(3,1);
    gridLayout->addWidget(new QLabel(tr("XYZ:")), 0, 0);

    _camDirXSpinner = new SpinnerWidget();
    _camDirYSpinner = new SpinnerWidget();
    _camDirZSpinner = new SpinnerWidget();
    _camDirXSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _camDirYSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _camDirZSpinner->setUnit(mainWindow.unitsManager().worldUnit());

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camDirXSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camDirXSpinner);
    gridLayout->addLayout(fieldLayout, 0, 1);
    connect(_camDirXSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camDirYSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camDirYSpinner);
    gridLayout->addLayout(fieldLayout, 0, 2);
    connect(_camDirYSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camDirZSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camDirZSpinner);
    gridLayout->addLayout(fieldLayout, 0, 3);
    connect(_camDirZSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    QGroupBox* upDirBox = new QGroupBox(tr("Up direction"));
    mainLayout->addWidget(upDirBox);

    gridLayout = new QGridLayout(upDirBox);
    gridLayout->setColumnStretch(1,1);
    gridLayout->setColumnStretch(2,1);
    gridLayout->setColumnStretch(3,1);
    gridLayout->setVerticalSpacing(2);

    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0,0,0,0);
    hLayout->setSpacing(0);
    _constrainRotationBtn = new QRadioButton();
    hLayout->addWidget(_constrainRotationBtn);
    gridLayout->addLayout(hLayout, 0, 0, 1, 4);

    hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0,0,0,0);
    hLayout->setSpacing(4);
    _rollAngleBtn = new QRadioButton(tr("Specify roll angle:"));
    hLayout->addWidget(_rollAngleBtn);
    _rollAngleSpinner = new SpinnerWidget();
    _rollAngleSpinner->setUnit(mainWindow.unitsManager().angleUnit());
    _rollAngleSpinner->setEnabled(false);
    connect(_rollAngleBtn, &QRadioButton::toggled, _rollAngleSpinner, &SpinnerWidget::setEnabled);
    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _rollAngleSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_rollAngleSpinner);
    hLayout->addLayout(fieldLayout);
    hLayout->addStretch(1);
    gridLayout->addLayout(hLayout, 1, 0, 1, 4);

    connect(_constrainRotationBtn, &QRadioButton::clicked, this, [](bool checked) {
        ViewportSettings::getSettings().setConstrainCameraRotation(checked);
    });
    connect(_rollAngleBtn, &QRadioButton::clicked, this, [](bool checked) {
        ViewportSettings::getSettings().setConstrainCameraRotation(!checked);
    });
    connect(_rollAngleSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    gridLayout->addWidget(new QLabel(tr("XYZ (read-only):")), 2, 0);
    _upDirXSpinner = new SpinnerWidget();
    _upDirYSpinner = new SpinnerWidget();
    _upDirZSpinner = new SpinnerWidget();
    _upDirXSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _upDirYSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _upDirZSpinner->setUnit(mainWindow.unitsManager().worldUnit());

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _upDirXSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_upDirXSpinner);
    gridLayout->addLayout(fieldLayout, 2, 1);
#if 0
    connect(_upDirXSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);
#else
    _upDirXSpinner->hide();
    textBox->setReadOnly(true);
#endif

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _upDirYSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_upDirYSpinner);
    gridLayout->addLayout(fieldLayout, 2, 2);
#if 0
    connect(_upDirYSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);
#else
    _upDirYSpinner->hide();
    textBox->setReadOnly(true);
#endif

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _upDirZSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_upDirZSpinner);
    gridLayout->addLayout(fieldLayout, 2, 3);
#if 0
    connect(_camDirZSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);
#else
    _upDirZSpinner->hide();
    textBox->setReadOnly(true);
#endif

    QGroupBox* projectionBox = new QGroupBox(tr("Projection type"));
    mainLayout->addWidget(projectionBox);

    gridLayout = new QGridLayout(projectionBox);
    gridLayout->setColumnMinimumWidth(0, 30);
    gridLayout->setColumnStretch(3, 1);

    _camPerspective = new QRadioButton(tr("Perspective:"));
    connect(_camPerspective, &QRadioButton::clicked, this, &AdjustViewDialog::onAdjustCamera);
    gridLayout->addWidget(_camPerspective, 0, 0, 1, 3);

    gridLayout->addWidget(new QLabel(tr("View angle:")), 1, 1);
    _camFOVAngleSpinner = new SpinnerWidget();
    _camFOVAngleSpinner->setUnit(mainWindow.unitsManager().angleUnit());
    _camFOVAngleSpinner->setMinValue(FloatType(1e-4));
    _camFOVAngleSpinner->setMaxValue(FLOATTYPE_PI - FloatType(1e-2));
    _camFOVAngleSpinner->setFloatValue(qDegreesToRadians(FloatType(35)));
    _camFOVAngleSpinner->setEnabled(false);
    connect(_camPerspective, &QRadioButton::toggled, _camFOVAngleSpinner, &SpinnerWidget::setEnabled);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camFOVAngleSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camFOVAngleSpinner);
    gridLayout->addLayout(fieldLayout, 1, 2);
    connect(_camFOVAngleSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    _camParallel = new QRadioButton(tr("Parallel:"));
    connect(_camParallel, &QRadioButton::clicked, this, &AdjustViewDialog::onAdjustCamera);
    gridLayout->addWidget(_camParallel, 2, 0, 1, 3);

    gridLayout->addWidget(new QLabel(tr("Field of view:")), 3, 1);
    _camFOVSpinner = new SpinnerWidget();
    _camFOVSpinner->setUnit(mainWindow.unitsManager().worldUnit());
    _camFOVSpinner->setMinValue(FloatType(1e-4));
    _camFOVSpinner->setFloatValue(200);
    _camFOVSpinner->setEnabled(false);
    connect(_camParallel, &QRadioButton::toggled, _camFOVSpinner, &SpinnerWidget::setEnabled);

    fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0,0,0,0);
    fieldLayout->setSpacing(0);
    textBox = new QLineEdit();
    _camFOVSpinner->setTextBox(textBox);
    fieldLayout->addWidget(textBox);
    fieldLayout->addWidget(_camFOVSpinner);
    gridLayout->addLayout(fieldLayout, 3, 2);
    connect(_camFOVSpinner, &SpinnerWidget::valueChanged, this, &AdjustViewDialog::onAdjustCamera);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AdjustViewDialog::close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &AdjustViewDialog::onCancel);
    connect(buttonBox, &QDialogButtonBox::helpRequested, &mainWindow, [&mainWindow]() {
        mainWindow.actionManager()->openHelpTopic(QStringLiteral("manual:viewports.adjust_view_dialog"));
    });
    mainLayout->addWidget(buttonBox);

    updateGUI();

    connect(&ViewportSettings::getSettings(), &ViewportSettings::settingsChanged, this, &AdjustViewDialog::updateGUI);
}

/******************************************************************************
* Updates the values displayed in the dialog.
******************************************************************************/
void AdjustViewDialog::updateGUI()
{
    _isUpdatingGUI = true;
    Viewport* viewport = _viewportListener.target();

    const Point3& cameraPos = viewport->cameraPosition();
    _camPosXSpinner->setFloatValue(cameraPos.x());
    _camPosYSpinner->setFloatValue(cameraPos.y());
    _camPosZSpinner->setFloatValue(cameraPos.z());

    Vector3 oldCameraDir(_camDirXSpinner->floatValue(), _camDirYSpinner->floatValue(), _camDirZSpinner->floatValue());
    FloatType oldDirLength = oldCameraDir.length();
    if(oldDirLength == 0)
        oldDirLength = 1;
    const Vector3& cameraDir = viewport->cameraDirection();
    _camDirXSpinner->setFloatValue(cameraDir.x() * oldDirLength);
    _camDirYSpinner->setFloatValue(cameraDir.y() * oldDirLength);
    _camDirZSpinner->setFloatValue(cameraDir.z() * oldDirLength);

    QString constrainAxisName;
    switch(ViewportSettings::getSettings().upDirection()) {
    case ViewportSettings::X_AXIS: constrainAxisName = QStringLiteral("x"); break;
    case ViewportSettings::Y_AXIS: constrainAxisName = QStringLiteral("y"); break;
    case ViewportSettings::Z_AXIS: constrainAxisName = QStringLiteral("z"); break;
    }
    _constrainRotationBtn->setText(tr("Constrained: %1-axis pointing upward").arg(constrainAxisName));
    _constrainRotationBtn->setChecked(ViewportSettings::getSettings().constrainCameraRotation());
    _rollAngleBtn->setChecked(!ViewportSettings::getSettings().constrainCameraRotation());

    Vector3 upVector = viewport->cameraUpDirection();
    if(upVector.isZero())
        upVector = ViewportSettings::getSettings().upVector();

    // Compute current roll angle.
    AffineTransformation constrainedOrientation = AffineTransformation::lookAlong(cameraPos, cameraDir, upVector);
    AffineTransformation unconstrainedOrientation = viewport->cameraTransformation();
    AffineTransformation delta = constrainedOrientation * unconstrainedOrientation;
    FloatType rollAngle = std::atan2(delta(1,0), delta(0,0));
    if(std::abs(rollAngle) < FLOATTYPE_EPSILON) rollAngle = 0;
    _rollAngleSpinner->setFloatValue(rollAngle);

    Vector3 upDir = viewport->cameraTransformation().column(1);
    if(std::abs(upDir.x()) < FLOATTYPE_EPSILON) upDir.x() = 0;
    if(std::abs(upDir.y()) < FLOATTYPE_EPSILON) upDir.y() = 0;
    if(std::abs(upDir.z()) < FLOATTYPE_EPSILON) upDir.z() = 0;
    _upDirXSpinner->setFloatValue(upDir.x());
    _upDirYSpinner->setFloatValue(upDir.y());
    _upDirZSpinner->setFloatValue(upDir.z());
#if 0
    _upDirXSpinner->setEnabled(!_constrainRotationBox->isChecked());
    _upDirYSpinner->setEnabled(!_constrainRotationBox->isChecked());
    _upDirZSpinner->setEnabled(!_constrainRotationBox->isChecked());
#endif

    if(viewport->isPerspectiveProjection()) {
        _camPerspective->setChecked(true);
        _camFOVAngleSpinner->setFloatValue(viewport->fieldOfView());
    }
    else {
        _camParallel->setChecked(true);
        _camFOVSpinner->setFloatValue(viewport->fieldOfView());
    }
    _isUpdatingGUI = false;
}

/******************************************************************************
* Is called when the user has changed the camera settings.
******************************************************************************/
void AdjustViewDialog::onAdjustCamera()
{
    if(_isUpdatingGUI)
        return;

    _mainWindow.handleExceptions([&] {
        Viewport* viewport = _viewportListener.target();

        FloatType newRollAngle = _rollAngleSpinner->floatValue();
        Point3 cameraPos(_camPosXSpinner->floatValue(), _camPosYSpinner->floatValue(), _camPosZSpinner->floatValue());
        Vector3 cameraDir(_camDirXSpinner->floatValue(), _camDirYSpinner->floatValue(), _camDirZSpinner->floatValue());

        if(_camPerspective->isChecked()) {
            viewport->setViewType(Viewport::VIEW_PERSPECTIVE);
            viewport->setFieldOfView(_camFOVAngleSpinner->floatValue());
        }
        else {
            viewport->setViewType(Viewport::VIEW_ORTHO);
            viewport->setFieldOfView(_camFOVSpinner->floatValue());
        }

        if(_constrainRotationBtn->isChecked()) {
            viewport->setCameraPosition(cameraPos);
            viewport->setCameraDirection(cameraDir);
        }
        else {
            // Compute the new camera transformation matrix including the roll angle.
            Vector3 upVector = ViewportSettings::getSettings().upVector();
            AffineTransformation cameraTM = AffineTransformation::lookAlong(cameraPos, cameraDir, upVector).inverse();
            AffineTransformation rollTM = AffineTransformation::rotationZ(newRollAngle);
            cameraTM = cameraTM * rollTM;
            viewport->setCameraTransformation(cameraTM);
        }
    });
}

/******************************************************************************
* Event handler for the Cancel button.
******************************************************************************/
void AdjustViewDialog::onCancel()
{
    setFocus(); // Remove focus from child widgets to make all input widgets commit their values.

    // Restore previous viewport camera settings.
    _mainWindow.handleExceptions([&] {
        Viewport* viewport = _viewportListener.target();
        viewport->setViewType(_oldViewType);
        viewport->setCameraTransformation(_oldCameraTM);
        viewport->setFieldOfView(_oldFOV);
    });
    close();
}

}   // End of namespace
