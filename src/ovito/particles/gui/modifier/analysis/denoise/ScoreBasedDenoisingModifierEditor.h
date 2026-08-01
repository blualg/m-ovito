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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QComboBox;
class QLabel;
class QPushButton;
class QProcess;

namespace Ovito {

class ScoreBasedDenoisingModifier;

class ScoreBasedDenoisingModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ScoreBasedDenoisingModifierEditor)

public:

    Q_INVOKABLE ScoreBasedDenoisingModifierEditor() {}

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    void updateParameterVisibility();
    void updatePlot();
    void updateSummary();
    void installOrRepairPythonEnvironment();
    void readInstallOutput();
    void handleInstallFinished(int exitCode, bool normalExit);

private:

    ScoreBasedDenoisingModifier* modifier() const;

    QComboBox* _structureCombo = nullptr;
    QLabel* _scaleHintLabel = nullptr;
    QLabel* _summaryLabel = nullptr;
    QComboBox* _runtimeInstallCombo = nullptr;
    QPushButton* _installEnvironmentButton = nullptr;
    QLabel* _installStatusLabel = nullptr;
    QProcess* _installProcess = nullptr;
    QString _installLog;
    QString _installPythonPath;
    DataTablePlotWidget* _convergencePlot = nullptr;
};

}  // namespace Ovito
