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
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

namespace Ovito {

class StructuralOrderModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(StructuralOrderModifierEditor)

public:

    Q_INVOKABLE StructuralOrderModifierEditor() {}

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    void updatePlot();
    void updateSummary();
    void updateParameterVisibility();
    void refreshSummaryGeometry();

    QPointer<DataTablePlotWidget> _profilePlot;
    QPointer<QLabel> _plotLabel;
    QPointer<QLabel> _summaryLabel;
    QPointer<QComboBox> _orderParameterCombo;
    QPointer<QLabel> _cutoffLabel;
    QPointer<QWidget> _cutoffField;
    QPointer<QLabel> _radialBinsLabel;
    QPointer<QWidget> _radialBinsField;
    QPointer<QLabel> _angularBinsLabel;
    QPointer<QWidget> _angularBinsField;
    QPointer<QLabel> _distributionBinsLabel;
    QPointer<QWidget> _distributionBinsField;
    QPointer<QLabel> _tetrahedralReferenceDistanceLabel;
    QPointer<QWidget> _tetrahedralReferenceDistanceField;
    QPointer<QLabel> _localStructureIndexCutoffLabel;
    QPointer<QWidget> _localStructureIndexCutoffField;
    QPointer<QLabel> _localTargetModeLabel;
    QPointer<QComboBox> _localTargetModeCombo;
    QPointer<QLabel> _referenceTypesLabel;
    QPointer<QWidget> _referenceTypesField;
    QPointer<QLabel> _localSiteTypesLabel;
    QPointer<QWidget> _localSiteTypesField;
    QPointer<QLabel> _localShellCutoffLabel;
    QPointer<QWidget> _localShellCutoffField;
};

}  // namespace Ovito
