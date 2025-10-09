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
bool SimulationCellInspectionApplet::appliesTo(const DataCollection& data) { return data.containsObject<SimulationCell>(); }

namespace {
constexpr std::array<const char*, 3> pbcFlags = {"X", "Y", "Z"};
}

/******************************************************************************
 * Lets the applet create the UI widget that is to be placed into the data
 * inspector panel.
 ******************************************************************************/
QWidget* SimulationCellInspectionApplet::createWidget()
{
    QWidget* panel = new QWidget();
    panel->setMaximumWidth(800);
    panel->setContentsMargins(4, 4, 4, 4);

    // panel->setAutoFillBackground(true);
    // QPalette pal = panel->palette();
    // pal.setColor(QPalette::Window, QApplication::palette().color(QPalette::Base));
    // panel->setPalette(pal);

    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QHBoxLayout* splitLayout = new QHBoxLayout();

    auto colorizeGroupbox = [](QGroupBox* groupBox) {
        // groupBox->setAutoFillBackground(true);
        // QPalette pal = groupBox->palette();
        // pal.setColor(QPalette::Window, QApplication::palette().color(QPalette::Base));
        // pal.setColor(QPalette::Base, QApplication::palette().color(QPalette::Base));
        // groupBox->setPalette(pal);
    };

    {
        QGroupBox* dimensionalityGroupBox = new QGroupBox(tr("Dimensionality"), panel);
        colorizeGroupbox(dimensionalityGroupBox);
        splitLayout->addWidget(dimensionalityGroupBox);

        auto* sublayout = new QHBoxLayout(dimensionalityGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);

        _checkbox2D = new QLabel("", panel);
        sublayout->addWidget(_checkbox2D);
        _checkbox3D = new QLabel("", panel);
        sublayout->addWidget(_checkbox3D);
    }

    {
        auto* pbcGroupBox = new QGroupBox(tr("Periodic boundary conditions"), panel);
        colorizeGroupbox(pbcGroupBox);
        splitLayout->addWidget(pbcGroupBox);

        auto* sublayout = new QHBoxLayout(pbcGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);

        for(int i = 0; i < 3; i++) {
            _checkboxPBC[i] = new QLabel("", panel);
            sublayout->addWidget(_checkboxPBC[i]);
        }
    }
    layout->addLayout(splitLayout, 1, 0);
    splitLayout = new QHBoxLayout();
    {
        QGroupBox* cellGroupBox = new QGroupBox(tr("Geometry"), panel);
        colorizeGroupbox(cellGroupBox);
        splitLayout->addWidget(cellGroupBox);

        QGridLayout* sublayout = new QGridLayout(cellGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(2);
        sublayout->setColumnStretch(1, 1);
        sublayout->setColumnStretch(2, 1);
        sublayout->setColumnStretch(3, 1);

        sublayout->addWidget(new QLabel(tr("Cell vectors:")), 0, 1, 1, 4);

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

        {  // Cell origin.
            sublayout->addWidget(new QLabel(tr("Cell origin:")), 4, 1, 1, 3);
            sublayout->addWidget(new QLabel(tr("<b>o</b>:")), 5, 0);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[3][i] = new QLineEdit();
                _cellVectorFields[3][i]->setReadOnly(true);
                sublayout->addWidget(_cellVectorFields[3][i], 5, 1 + i);
            }
        }
        sublayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), sublayout->rowCount(), 0, 1,
                           sublayout->columnCount());
    }
    {
        QGroupBox* sizeGroupBox = new QGroupBox(tr("Bounding box"), panel);
        colorizeGroupbox(sizeGroupBox);
        splitLayout->addWidget(sizeGroupBox);

        QGridLayout* sublayout = new QGridLayout(sizeGroupBox);
        sublayout->setContentsMargins(4, 4, 4, 4);
        sublayout->setSpacing(4);
        sublayout->setColumnStretch(1, 1);

        sublayout->addWidget(new QLabel(tr("Width (X):")), 0, 0);
        sublayout->addWidget(new QLabel(tr("Length (Y):")), 1, 0);
        sublayout->addWidget(new QLabel(tr("Height (Z):")), 2, 0);

        for(int i = 0; i < 3; i++) {
            _bboxFields[i] = new QLineEdit(panel);
            _bboxFields[i]->setReadOnly(true);
            sublayout->addWidget(_bboxFields[i], i, 1);
        }
        sublayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), sublayout->rowCount(), 0, 1,
                           sublayout->columnCount());
    }
    layout->addLayout(splitLayout, 0, 0);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    QScrollArea* scroller = new QScrollArea();
    scroller->setWidget(panel);
    scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
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
    if(!cell) return;

    if(_checkbox2D) _checkbox2D->setText(tr("2D: %2").arg(cell->is2D() ? QStringLiteral("✕") : QStringLiteral("")));
    if(_checkbox3D) _checkbox3D->setText(tr("3D: %2").arg(!cell->is2D() ? QStringLiteral("✕") : QStringLiteral("")));

    for(int i = 0; i < 3; i++) {
        if(_checkboxPBC[i])
            _checkboxPBC[i]->setText(
                tr("%1: %2").arg(pbcFlags[i]).arg(cell->pbcFlagsCorrected()[i] ? QStringLiteral("✕") : QStringLiteral("")));
    }

    ParameterUnit* worldUnit = ui().unitsManager().worldUnit();
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 3; ++j) {
            if(_cellVectorFields[i][j]) _cellVectorFields[i][j]->setText(worldUnit->formatValue(cell->cellMatrix().column(i)[j]));
        }
    }

    for(int i = 0; i < 3; i++) {
        if(_bboxFields[i]) {
            const FloatType size = std::max({std::abs(cell->cellMatrix().column(0)[i]), std::abs(cell->cellMatrix().column(1)[i]),
                                             std::abs(cell->cellMatrix().column(2)[i])});
            _bboxFields[i]->setText(worldUnit->formatValue(size));
        }
    }
}

}  // namespace Ovito
