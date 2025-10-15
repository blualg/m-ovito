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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/io/AttributeFileExporter.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/gui/desktop/dialogs/FileExporterSettingsDialog.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/desktop/widgets/general/CopyableTableView.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "SimulationCellInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCellInspectionApplet);
OVITO_CLASSINFO(SimulationCellInspectionApplet, "DisplayName", "Simulation Cell");

/******************************************************************************
 * Determines whether the given pipeline dataset contains data that can be
 * displayed by this applet.
 ******************************************************************************/
bool SimulationCellInspectionApplet::appliesTo(const DataCollection& data)
{
    return data.containsObject<SimulationCell>();
}

/******************************************************************************
 * Lets the applet create the UI widget that is to be placed into the data
 * inspector panel.
 ******************************************************************************/
QWidget* SimulationCellInspectionApplet::createWidget()
{
    QWidget* container = new QWidget();
    QGridLayout* containerLayout = new QGridLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    QWidget* panel = new QWidget();
    panel->setMaximumWidth(800);
    panel->setContentsMargins(4, 4, 4, 4);
    containerLayout->addWidget(panel, 0, 0, Qt::AlignHCenter | Qt::AlignTop);

    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setRowStretch(0, 1);

    {
        constexpr std::array<const char*, 3> pbcDirectionNames = {"X:", "Y:", "Z:"};

        auto* pbcGroupBox = new QGroupBox(tr("Periodic boundary conditions"), panel);
        layout->addWidget(pbcGroupBox, 1, 0);

        auto* sublayout = new QHBoxLayout(pbcGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(4);

        for(int i = 0; i < 3; i++) {
            if(i != 0)
                sublayout->addSpacing(8);
            QLabel* dirLabel = new QLabel(pbcDirectionNames[i], panel);
            sublayout->addWidget(dirLabel);
            _checkboxPBC[i] = new QLabel(panel);
            _checkboxPBC[i]->setAlignment(Qt::AlignCenter);
            _checkboxPBC[i]->setAutoFillBackground(true);
            _checkboxPBC[i]->setBackgroundRole(QPalette::Base);
            sublayout->addWidget(_checkboxPBC[i], 1);
        }
        _pbcEnabledColor = _checkboxPBC[0]->palette();
        _pbcEnabledColor.setBrush(QPalette::Base, QColor(0, 255, 0, 50));
        _pbcDisabledColor = _checkboxPBC[0]->palette();
        _pbcDisabledColor.setBrush(QPalette::Base, QColor(255, 0, 0, 30));
    }
    {
        QGroupBox* dimensionalityGroupBox = new QGroupBox(tr("Dimensionality"), panel);
        layout->addWidget(dimensionalityGroupBox, 1, 1);

        auto* sublayout = new QHBoxLayout(dimensionalityGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);

        _dimensionalityDisplay = new QLabel(panel);
        _dimensionalityDisplay->setAlignment(Qt::AlignCenter);
        sublayout->addWidget(_dimensionalityDisplay, 1);
    }
    {
        QGroupBox* cellGroupBox = new QGroupBox(tr("Geometry"), panel);
        layout->addWidget(cellGroupBox, 0, 0);

        auto* boxLayout = new QVBoxLayout(cellGroupBox);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(0);

        auto* radioButtonRow = new QHBoxLayout;
        auto* cellVectorsButton = new QRadioButton(tr("Cell vectors"));
        auto* cellParamsButton = new QRadioButton(tr("Cell parameters"));
        radioButtonRow->addWidget(cellVectorsButton);
        radioButtonRow->addSpacing(8);
        radioButtonRow->addWidget(cellParamsButton);
        radioButtonRow->addStretch();

        auto* buttonGroup = new QButtonGroup(cellGroupBox);
        buttonGroup->setExclusive(true);
        buttonGroup->addButton(cellVectorsButton, 0);
        buttonGroup->addButton(cellParamsButton, 1);

        boxLayout->addLayout(radioButtonRow);

        // Stacked widget for toggling
        auto* stackedWidget = new QStackedWidget;
        boxLayout->addWidget(stackedWidget);

        // Connect radio button selection
        QObject::connect(buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), stackedWidget, &QStackedWidget::setCurrentIndex);

        // Set default view
        cellVectorsButton->setChecked(true);
        stackedWidget->setCurrentIndex(0);

        auto* cellWidget = new QWidget;

        QGridLayout* sublayout = new QGridLayout(cellWidget);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);
        sublayout->setColumnStretch(1, 1);
        sublayout->setColumnStretch(2, 1);
        sublayout->setColumnStretch(3, 1);
        sublayout->setRowStretch(6, 1);

        {  // First cell vector.
            sublayout->addWidget(new QLabel(tr("<b>a</b>:")), 1, 0);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[0][i] = new QLineEdit();
                _cellVectorFields[0][i]->setReadOnly(true);
                sublayout->addWidget(_cellVectorFields[0][i], 1, 1 + i);
            }
        }

        {  // Second cell vector.
            sublayout->addWidget(new QLabel(tr("<b>b</b>:")), 2, 0);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[1][i] = new QLineEdit();
                _cellVectorFields[1][i]->setReadOnly(true);
                sublayout->addWidget(_cellVectorFields[1][i], 2, 1 + i);
            }
        }

        {  // Third cell vector.
            sublayout->addWidget(new QLabel(tr("<b>c</b>:")), 3, 0);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[2][i] = new QLineEdit();
                _cellVectorFields[2][i]->setReadOnly(true);
                sublayout->addWidget(_cellVectorFields[2][i], 3, 1 + i);
            }
        }

        stackedWidget->addWidget(cellWidget);

        auto* cellParamsWidget = new QWidget;

        sublayout = new QGridLayout(cellParamsWidget);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);
        sublayout->setColumnStretch(1, 1);
        sublayout->setColumnStretch(4, 1);
        sublayout->setColumnMinimumWidth(2, 8);
        sublayout->setRowStretch(6, 1);

        sublayout->addWidget(new QLabel(tr("|<b>a</b>|:")), 1, 0);
        sublayout->addWidget(new QLabel(tr("|<b>b</b>|:")), 2, 0);
        sublayout->addWidget(new QLabel(tr("|<b>c</b>|:")), 3, 0);

        for(int i = 0; i < 3; ++i) {
            _cellParamsFields[0][i] = new QLineEdit();
            _cellParamsFields[0][i]->setReadOnly(true);
            sublayout->addWidget(_cellParamsFields[0][i], i + 1, 1);
        }

        sublayout->addWidget(new QLabel(tr("α:")), 1, 3);
        sublayout->addWidget(new QLabel(tr("β:")), 2, 3);
        sublayout->addWidget(new QLabel(tr("γ:")), 3, 3);

        for(int i = 0; i < 3; ++i) {
            _cellParamsFields[1][i] = new QLineEdit();
            _cellParamsFields[1][i]->setReadOnly(true);
            sublayout->addWidget(_cellParamsFields[1][i], i + 1, 4);
        }

        stackedWidget->addWidget(cellParamsWidget);

        {  // Cell origin.
            QGridLayout* sublayout = new QGridLayout();
            sublayout->setContentsMargins(4, 0, 4, 4);
            sublayout->setSpacing(2);
            sublayout->setColumnStretch(1, 1);
            sublayout->setColumnStretch(2, 1);
            sublayout->setColumnStretch(3, 1);
            sublayout->setRowStretch(6, 1);

            sublayout->addWidget(new QLabel(tr("Cell origin:")), 4, 1, 1, 3);
            sublayout->addWidget(new QLabel(tr("<b>o</b>:")), 5, 0);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[3][i] = new QLineEdit();
                _cellVectorFields[3][i]->setReadOnly(true);
                sublayout->addWidget(_cellVectorFields[3][i], 5, 1 + i);
            }

            boxLayout->addLayout(sublayout);
        }
    }
    {
        QGroupBox* sizeGroupBox = new QGroupBox(tr("Bounding box"), panel);
        layout->addWidget(sizeGroupBox, 0, 1);

        QGridLayout* sublayout = new QGridLayout(sizeGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(4);
        sublayout->setColumnStretch(1, 1);
        sublayout->setRowStretch(3, 1);

        sublayout->addWidget(new QLabel(tr("Width (X):")), 0, 0);
        sublayout->addWidget(new QLabel(tr("Length (Y):")), 1, 0);
        sublayout->addWidget(new QLabel(tr("Height (Z):")), 2, 0);

        for(int i = 0; i < 3; i++) {
            _bboxFields[i] = new QLineEdit(panel);
            _bboxFields[i]->setReadOnly(true);
            sublayout->addWidget(_bboxFields[i], i, 1);
        }
    }

    QScrollArea* scroller = new QScrollArea();
    scroller->setWidget(container);
    scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroller->setFrameShape(QFrame::NoFrame);

    return scroller;
}

/******************************************************************************
 * Updates the contents displayed in the inspector.
 ******************************************************************************/
void SimulationCellInspectionApplet::updateDisplay()
{
    DataInspectionApplet::updateDisplay();
    const auto* cell = currentState().getObject<SimulationCell>();
    if(!cell)
        return;

    if(_dimensionalityDisplay)
        _dimensionalityDisplay->setText(cell->is2D() ? tr("2D") : tr("3D"));

    for(int i = 0; i < 3; i++) {
        if(_checkboxPBC[i]) {
            _checkboxPBC[i]->setText(cell->hasPbcCorrected(i) ? tr("yes") : tr("no"));
            _checkboxPBC[i]->setPalette(cell->hasPbcCorrected(i) ? _pbcEnabledColor : _pbcDisabledColor);
        }
    }

    // Set Cell Vectors and origin
    ParameterUnit* worldUnit = ui().unitsManager().worldUnit();
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 3; ++j) {
            if(_cellVectorFields[i][j]) _cellVectorFields[i][j]->setText(worldUnit->formatValue(cell->cellMatrix().column(i)[j]));
        }
    }

    // Set Cell Length and Angles
    {
        ParameterUnit* angleUnit = ui().unitsManager().angleUnit();

        const FloatType a = cell->cellMatrix().column(0).length();
        const FloatType b = cell->cellMatrix().column(1).length();
        const FloatType c = cell->cellMatrix().column(2).length();

        _cellParamsFields[0][0]->setText(worldUnit->formatValue(a));
        _cellParamsFields[0][1]->setText(worldUnit->formatValue(b));
        _cellParamsFields[0][2]->setText(worldUnit->formatValue(c));

        const FloatType alpha = (b != 0 && c != 0) ? std::acos(cell->cellMatrix().column(1).dot(cell->cellMatrix().column(2) / (b * c)))
                                                   : std::numeric_limits<FloatType>::quiet_NaN();
        const FloatType beta = (a != 0 && c != 0) ? std::acos(cell->cellMatrix().column(0).dot(cell->cellMatrix().column(2) / (a * c)))
                                                  : std::numeric_limits<FloatType>::quiet_NaN();
        const FloatType gamma = (a != 0 && c != 0) ? std::acos(cell->cellMatrix().column(0).dot(cell->cellMatrix().column(1) / (a * b)))
                                                   : std::numeric_limits<FloatType>::quiet_NaN();

        _cellParamsFields[1][0]->setText(std::isfinite(alpha) ? angleUnit->formatValue(angleUnit->nativeToUser(alpha)) : tr("undefined"));
        _cellParamsFields[1][1]->setText(std::isfinite(beta) ? angleUnit->formatValue(angleUnit->nativeToUser(beta)) : tr("undefined"));
        _cellParamsFields[1][2]->setText(std::isfinite(gamma) ? angleUnit->formatValue(angleUnit->nativeToUser(gamma)) : tr("undefined"));
    }

    // Set Boudning Box Size
    for(int i = 0; i < 3; i++) {
        if(_bboxFields[i]) {
            const FloatType size = std::max({std::abs(cell->cellMatrix().column(0)[i]), std::abs(cell->cellMatrix().column(1)[i]),
                                             std::abs(cell->cellMatrix().column(2)[i])});
            _bboxFields[i]->setText(worldUnit->formatValue(size));
        }
    }
}

}  // namespace Ovito
