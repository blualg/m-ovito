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

class QLabel;
class QCheckBox;
class QPushButton;
class QWidget;
class QwtPlotMarker;

namespace Ovito {

class TransportModifier;

class TransportModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(TransportModifierEditor)
    Q_OBJECT

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    void runAnalysis();
    void updatePlots();
    void updateSummary();
    void updateControlStates();
    void updateGreenKuboPreview(const PipelineFlowState& state);

private:

    TransportModifier* modifier() const;

    QPushButton* _runButton = nullptr;
    QCheckBox* _computeMSDCheckBox = nullptr;
    QCheckBox* _computeVACFCheckBox = nullptr;
    QCheckBox* _computeConductivityCheckBox = nullptr;
    QCheckBox* _useOnlySelectedParticlesCheckBox = nullptr;
    QCheckBox* _selectAsMoleculesCheckBox = nullptr;
    QWidget* _msdSection = nullptr;
    QWidget* _vacfSection = nullptr;
    QWidget* _conductivitySection = nullptr;
    QWidget* _distinctIonCorrelationSection = nullptr;
    QWidget* _gkPreviewSection = nullptr;
    DataTablePlotWidget* _msdPlot = nullptr;
    DataTablePlotWidget* _vacfPlot = nullptr;
    DataTablePlotWidget* _conductivityPlot = nullptr;
    DataTablePlotWidget* _distinctIonCorrelationPlot = nullptr;
    DataTablePlotWidget* _gkCorrelationPreviewPlot = nullptr;
    DataTablePlotWidget* _gkConductivityPreviewPlot = nullptr;
    QwtPlotMarker* _gkCorrelationStartMarker = nullptr;
    QwtPlotMarker* _gkCorrelationEndMarker = nullptr;
    QwtPlotMarker* _gkConductivityStartMarker = nullptr;
    QwtPlotMarker* _gkConductivityEndMarker = nullptr;
    QwtPlotMarker* _gkConductivityPlateauMarker = nullptr;
    QLabel* _summaryLabel = nullptr;
    bool _distinctIonCorrelationAvailable = false;
};

}   // End of namespace
