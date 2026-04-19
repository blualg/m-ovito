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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/transport/TransportModifier.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/stdobj/table/DataTable.h>
#include "TransportModifierEditor.h"
#include <qwt/qwt_plot_marker.h>
#include <qwt/qwt_scale_engine.h>
#include <QCheckBox>
#include <QPointer>
#include <QPushButton>

namespace Ovito {

namespace {

QString formatValue(const QVariant& value)
{
    if(!value.isValid())
        return QObject::tr("n/a");

    bool ok = false;
    const double numeric = value.toDouble(&ok);
    return ok ? QString::number(numeric, 'g', 6) : value.toString();
}

QString timeUnitLabel(TransportModifier::TimeUnit unit)
{
    switch(unit) {
    case TransportModifier::Femtoseconds: return QStringLiteral("fs");
    case TransportModifier::Picoseconds: return QStringLiteral("ps");
    case TransportModifier::Nanoseconds: return QStringLiteral("ns");
    }
    return QStringLiteral("ps");
}

QString lengthUnitLabel(TransportModifier::LengthUnit unit)
{
    switch(unit) {
    case TransportModifier::Angstroms: return QStringLiteral("A");
    case TransportModifier::Nanometers: return QStringLiteral("nm");
    case TransportModifier::Meters: return QStringLiteral("m");
    }
    return QStringLiteral("A");
}

QString chargeUnitLabel(TransportModifier::ChargeUnit unit)
{
    switch(unit) {
    case TransportModifier::ElementaryCharges: return QStringLiteral("e");
    case TransportModifier::Coulombs: return QStringLiteral("C");
    }
    return QStringLiteral("e");
}

bool transportAnalysisIsIdle(const TransportModifier* modifier, const ModificationNode* node)
{
    const auto* transportNode = dynamic_object_cast<const TransportModificationNode>(node);
    return modifier && transportNode && !transportNode->hasCachedResults() && modifier->runRequestId() <= transportNode->completedRunRequestId();
}

QString diffusionUnitLabel(const QString& lengthUnit, const QString& timeUnit)
{
    return QStringLiteral("%1^2/%2").arg(lengthUnit, timeUnit);
}

QString conductivityRawUnitLabel(const QString& chargeUnit, const QString& timeUnit, const QString& lengthUnit, int dimensionality)
{
    if(dimensionality == 2)
        return QStringLiteral("%1^2/(J*%2)").arg(chargeUnit, timeUnit);
    return QStringLiteral("%1^2/(J*%2*%3)").arg(chargeUnit, timeUnit, lengthUnit);
}

QString conductivitySIUnitLabel(int dimensionality)
{
    return (dimensionality == 2) ? QStringLiteral("S") : QStringLiteral("S/m");
}

QString formatDualValue(const QVariant& rawValue,
                        const QString& rawUnit,
                        const QVariant& siValue,
                        const QString& siUnit)
{
    return QObject::tr("%1 %2 | %3 %4").arg(formatValue(rawValue), rawUnit, formatValue(siValue), siUnit);
}

double timeScaleToSI(TransportModifier::TimeUnit unit)
{
    switch(unit) {
    case TransportModifier::Femtoseconds: return 1e-15;
    case TransportModifier::Picoseconds: return 1e-12;
    case TransportModifier::Nanoseconds: return 1e-9;
    }
    return 1e-12;
}

double lengthScaleToSI(TransportModifier::LengthUnit unit)
{
    switch(unit) {
    case TransportModifier::Angstroms: return 1e-10;
    case TransportModifier::Nanometers: return 1e-9;
    case TransportModifier::Meters: return 1.0;
    }
    return 1e-10;
}

double chargeScaleToSI(TransportModifier::ChargeUnit unit)
{
    switch(unit) {
    case TransportModifier::ElementaryCharges: return 1.602176634e-19;
    case TransportModifier::Coulombs: return 1.0;
    }
    return 1.602176634e-19;
}

double conductivityScaleToSI(const TransportModifier* modifier, int dimensionality)
{
    if(!modifier)
        return 1.0;
    return std::pow(chargeScaleToSI(modifier->chargeUnit()), 2.0) /
           (timeScaleToSI(modifier->timeUnit()) * std::pow(lengthScaleToSI(modifier->lengthUnit()), dimensionality - 2.0));
}

int findComponentIndex(const Property* yProperty, const QString& componentName)
{
    if(!yProperty)
        return -1;
    if(componentName.isEmpty())
        return 0;

    const QStringList& componentNames = yProperty->componentNames();
    const int index = componentNames.indexOf(componentName);
    return (index >= 0) ? index : -1;
}

DataOORef<const DataTable> createPreviewLineTable(const QString& title,
                                                  const QString& axisLabelX,
                                                  const QString& axisLabelY,
                                                  const QString& componentName,
                                                  const std::vector<double>& xValues,
                                                  const std::vector<double>& yValues)
{
    if(xValues.empty() || xValues.size() != yValues.size())
        return {};

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            xValues.size(),
                                                            Property::FloatDefault,
                                                            1,
                                                            axisLabelY,
                                                            0,
                                                            QStringList{componentName});
    BufferWriteAccess<FloatType*, access_mode::discard_write> yAcc(y);
    for(size_t i = 0; i < yValues.size(); ++i)
        yAcc.set(i, 0, static_cast<FloatType>(yValues[i]));

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            xValues.size(),
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Time"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t i = 0; i < xValues.size(); ++i)
        xAcc[i] = static_cast<FloatType>(xValues[i]);

    DataOORef<DataTable> table = DataOORef<DataTable>::create(ObjectInitializationFlag::DontCreateVisElement,
                                                              DataTable::Line,
                                                              title,
                                                              std::move(y),
                                                              std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

void configureMarker(QwtPlotMarker* marker, QwtPlotMarker::LineStyle style, const QPen& pen)
{
    if(!marker)
        return;
    marker->setLineStyle(style);
    marker->setLinePen(pen);
    marker->setVisible(false);
}

void hideMarker(QwtPlotMarker* marker)
{
    if(marker)
        marker->setVisible(false);
}

}

IMPLEMENT_CREATABLE_OVITO_CLASS(TransportModifierEditor);
SET_OVITO_OBJECT_EDITOR(TransportModifier, TransportModifierEditor);

TransportModifier* TransportModifierEditor::modifier() const
{
    return static_object_cast<TransportModifier>(editObject());
}

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void TransportModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* runRollout = createRollout(tr("Run"), rolloutParams);
    auto* runLayout = new QVBoxLayout(runRollout);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(6);

    QLabel* runInfoLabel = new QLabel(tr("Transport analysis stays idle until you explicitly start it. After changing any setting or the upstream data, click Run again to recompute the results."), runRollout);
    runInfoLabel->setWordWrap(true);
    runLayout->addWidget(runInfoLabel);

    _runButton = new QPushButton(tr("Run transport analysis"), runRollout);
    runLayout->addWidget(_runButton);
    connect(_runButton, &QPushButton::clicked, this, &TransportModifierEditor::runAnalysis);

    QWidget* rollout = createRollout(tr("Transport"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* analysisBox = new QGroupBox(tr("Observables"), rollout);
    auto* analysisLayout = new QVBoxLayout(analysisBox);
    analysisLayout->setContentsMargins(4, 4, 4, 4);
    analysisLayout->setSpacing(4);
    _computeMSDCheckBox = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::computeMSD))->checkBox();
    analysisLayout->addWidget(_computeMSDCheckBox);
    _computeVACFCheckBox = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::computeVACF))->checkBox();
    analysisLayout->addWidget(_computeVACFCheckBox);
    _computeConductivityCheckBox = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::computeConductivity))->checkBox();
    analysisLayout->addWidget(_computeConductivityCheckBox);
    analysisLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::computePerType))->checkBox());
    _useOnlySelectedParticlesCheckBox =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::useOnlySelectedParticles))->checkBox();
    analysisLayout->addWidget(_useOnlySelectedParticlesCheckBox);

    auto* moleculeSelectionContainer = new QWidget(analysisBox);
    auto* moleculeSelectionLayout = new QVBoxLayout(moleculeSelectionContainer);
    moleculeSelectionLayout->setContentsMargins(20, 0, 0, 0);
    moleculeSelectionLayout->setSpacing(0);
    _selectAsMoleculesCheckBox = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::selectAsMolecules))->checkBox();
    _selectAsMoleculesCheckBox->setToolTip(tr("Promote any persistently selected atom to its full molecule and analyze molecule center-of-mass transport."));
    moleculeSelectionLayout->addWidget(_selectAsMoleculesCheckBox);
    analysisLayout->addWidget(moleculeSelectionContainer);

    analysisLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TransportModifier::includeVACFCrossTerms))->checkBox());
    layout->addWidget(analysisBox);

    auto* unitsBox = new QGroupBox(tr("Time And Units"), rollout);
    auto* unitsLayout = new QGridLayout(unitsBox);
    unitsLayout->setContentsMargins(4, 4, 4, 4);
    unitsLayout->setColumnStretch(1, 1);

    FloatParameterUI* deltaTUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TransportModifier::deltaT));
    unitsLayout->addWidget(deltaTUI->label(), 0, 0);
    unitsLayout->addLayout(deltaTUI->createFieldLayout(), 0, 1);

    VariantComboBoxParameterUI* timeUnitUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TransportModifier::timeUnit));
    timeUnitUI->comboBox()->addItem(tr("fs"), QVariant::fromValue((int)TransportModifier::Femtoseconds));
    timeUnitUI->comboBox()->addItem(tr("ps"), QVariant::fromValue((int)TransportModifier::Picoseconds));
    timeUnitUI->comboBox()->addItem(tr("ns"), QVariant::fromValue((int)TransportModifier::Nanoseconds));
    unitsLayout->addWidget(new QLabel(tr("dt unit:"), unitsBox), 1, 0);
    unitsLayout->addWidget(timeUnitUI->comboBox(), 1, 1);

    VariantComboBoxParameterUI* lengthUnitUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TransportModifier::lengthUnit));
    lengthUnitUI->comboBox()->addItem(tr("A"), QVariant::fromValue((int)TransportModifier::Angstroms));
    lengthUnitUI->comboBox()->addItem(tr("nm"), QVariant::fromValue((int)TransportModifier::Nanometers));
    lengthUnitUI->comboBox()->addItem(tr("m"), QVariant::fromValue((int)TransportModifier::Meters));
    unitsLayout->addWidget(new QLabel(tr("Length unit:"), unitsBox), 2, 0);
    unitsLayout->addWidget(lengthUnitUI->comboBox(), 2, 1);

    VariantComboBoxParameterUI* chargeUnitUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TransportModifier::chargeUnit));
    chargeUnitUI->comboBox()->addItem(tr("e"), QVariant::fromValue((int)TransportModifier::ElementaryCharges));
    chargeUnitUI->comboBox()->addItem(tr("C"), QVariant::fromValue((int)TransportModifier::Coulombs));
    unitsLayout->addWidget(new QLabel(tr("Charge unit:"), unitsBox), 3, 0);
    unitsLayout->addWidget(chargeUnitUI->comboBox(), 3, 1);

    FloatParameterUI* temperatureUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TransportModifier::temperature));
    unitsLayout->addWidget(temperatureUI->label(), 4, 0);
    unitsLayout->addLayout(temperatureUI->createFieldLayout(), 4, 1);
    layout->addWidget(unitsBox);

    auto* pyLatBox = new QGroupBox(tr("Fitting"), rollout);
    auto* pyLatLayout = new QGridLayout(pyLatBox);
    pyLatLayout->setContentsMargins(4, 4, 4, 4);
    pyLatLayout->setColumnStretch(1, 1);

    auto* fittingHelp = new QLabel(tr("Transport analysis follows the trajectory cadence directly. MSD-based quantities use the built-in automatic fit and Green-Kubo conductivity uses an automatic plateau window."),
                                   pyLatBox);
    fittingHelp->setWordWrap(true);
    pyLatLayout->addWidget(fittingHelp, 0, 0, 1, 2);

    FloatParameterUI* pyLatDiffTolUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TransportModifier::pyLatDiffusivityTolerance));
    pyLatLayout->addWidget(pyLatDiffTolUI->label(), 1, 0);
    pyLatLayout->addLayout(pyLatDiffTolUI->createFieldLayout(), 1, 1);

    FloatParameterUI* pyLatCondTolUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TransportModifier::pyLatConductivityTolerance));
    pyLatLayout->addWidget(pyLatCondTolUI->label(), 2, 0);
    pyLatLayout->addLayout(pyLatCondTolUI->createFieldLayout(), 2, 1);
    layout->addWidget(pyLatBox);

    auto* overridesBox = new QGroupBox(tr("Overrides"), rollout);
    auto* overridesLayout = new QVBoxLayout(overridesBox);
    overridesLayout->setContentsMargins(4, 4, 4, 4);
    overridesLayout->setSpacing(6);

    BooleanGroupBoxParameterUI* manualMoleculeDefinitionsGroupUI =
        createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(TransportModifier::useManualMoleculeDefinitions));
    overridesLayout->addWidget(manualMoleculeDefinitionsGroupUI->groupBox());
    auto* manualMoleculeLayout = new QVBoxLayout(manualMoleculeDefinitionsGroupUI->childContainer());
    manualMoleculeLayout->setContentsMargins(0, 0, 0, 0);
    manualMoleculeLayout->setSpacing(4);
    auto* manualMoleculeHelp = new QLabel(
        tr("Use one molecule template per line, for example:\nWater = O:1, H:2\nPF6 = P:1, F:6\nAtoms are matched as contiguous particle-type blocks in the reference-frame ordering. This override is used whenever whole molecules are analyzed, including selected atoms with 'Select as molecules' enabled."),
        manualMoleculeDefinitionsGroupUI->childContainer());
    manualMoleculeHelp->setWordWrap(true);
    manualMoleculeLayout->addWidget(manualMoleculeHelp);
    StringParameterUI* manualMoleculeDefinitionsUI =
        createParamUI<StringParameterUI>(PROPERTY_FIELD(TransportModifier::manualMoleculeDefinitions));
    auto* manualMoleculeDefinitionsEdit = new AutocompleteTextEdit(manualMoleculeDefinitionsGroupUI->childContainer());
    manualMoleculeDefinitionsEdit->setPlaceholderText(tr("Water = O:1, H:2\nPF6 = P:1, F:6"));
    manualMoleculeDefinitionsEdit->setMinimumHeight(84);
    manualMoleculeDefinitionsUI->setTextBox(manualMoleculeDefinitionsEdit);
    manualMoleculeLayout->addWidget(manualMoleculeDefinitionsUI->textBox());

    BooleanGroupBoxParameterUI* manualTypeChargesGroupUI =
        createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(TransportModifier::useManualTypeCharges));
    overridesLayout->addWidget(manualTypeChargesGroupUI->groupBox());
    auto* manualTypeChargesLayout = new QVBoxLayout(manualTypeChargesGroupUI->childContainer());
    manualTypeChargesLayout->setContentsMargins(0, 0, 0, 0);
    manualTypeChargesLayout->setSpacing(4);
    auto* manualTypeChargesHelp = new QLabel(
        tr("Override charges by particle type with one entry per line, for example:\nNa = 1\nCl = -1\n1 = 0.423\nUnlisted particle types keep their trajectory charge or fall back to q = 1 if no Charge property exists."),
        manualTypeChargesGroupUI->childContainer());
    manualTypeChargesHelp->setWordWrap(true);
    manualTypeChargesLayout->addWidget(manualTypeChargesHelp);
    StringParameterUI* manualTypeChargesUI =
        createParamUI<StringParameterUI>(PROPERTY_FIELD(TransportModifier::manualTypeCharges));
    auto* manualTypeChargesEdit = new AutocompleteTextEdit(manualTypeChargesGroupUI->childContainer());
    manualTypeChargesEdit->setPlaceholderText(tr("Na = 1\nCl = -1"));
    manualTypeChargesEdit->setMinimumHeight(84);
    manualTypeChargesUI->setTextBox(manualTypeChargesEdit);
    manualTypeChargesLayout->addWidget(manualTypeChargesUI->textBox());

    layout->addWidget(overridesBox);

    BooleanGroupBoxParameterUI* intervalGroupUI =
        createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(TransportModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TransportModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TransportModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    _summaryLabel = new QLabel(tr("Transport results are idle. Open the Run section and click 'Run transport analysis' to compute the selected observables."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _msdSection = new QWidget(rollout);
    auto* msdLayout = new QVBoxLayout(_msdSection);
    msdLayout->setContentsMargins(0, 0, 0, 0);
    msdLayout->setSpacing(4);
    _msdPlot = new DataTablePlotWidget();
    _msdPlot->setMinimumHeight(180);
    _msdPlot->setMaximumHeight(180);
    msdLayout->addWidget(new QLabel(tr("MSD:"), _msdSection));
    msdLayout->addWidget(_msdPlot);
    layout->addWidget(_msdSection);

    _vacfSection = new QWidget(rollout);
    auto* vacfLayout = new QVBoxLayout(_vacfSection);
    vacfLayout->setContentsMargins(0, 0, 0, 0);
    vacfLayout->setSpacing(4);
    _vacfPlot = new DataTablePlotWidget();
    _vacfPlot->setMinimumHeight(180);
    _vacfPlot->setMaximumHeight(180);
    vacfLayout->addWidget(new QLabel(tr("VACF:"), _vacfSection));
    vacfLayout->addWidget(_vacfPlot);
    layout->addWidget(_vacfSection);

    _conductivitySection = new QWidget(rollout);
    auto* conductivityLayout = new QVBoxLayout(_conductivitySection);
    conductivityLayout->setContentsMargins(0, 0, 0, 0);
    conductivityLayout->setSpacing(4);
    _conductivityPlot = new DataTablePlotWidget();
    _conductivityPlot->setMinimumHeight(180);
    _conductivityPlot->setMaximumHeight(180);
    conductivityLayout->addWidget(new QLabel(tr("Conductivity:"), _conductivitySection));
    conductivityLayout->addWidget(_conductivityPlot);
    layout->addWidget(_conductivitySection);

    auto* gkPreviewBox = new QGroupBox(tr("Green-Kubo Preview"), rollout);
    _gkPreviewSection = gkPreviewBox;
    auto* gkPreviewLayout = new QVBoxLayout(gkPreviewBox);
    gkPreviewLayout->setContentsMargins(4, 4, 4, 4);
    gkPreviewLayout->setSpacing(4);
    auto* gkPreviewHelp = new QLabel(tr("Paper-style Green-Kubo preview: normalized current autocorrelation and SI conductivity on a logarithmic time axis. The blue guide lines show the automatic plateau window and the horizontal line marks the reported conductivity plateau."), gkPreviewBox);
    gkPreviewHelp->setWordWrap(true);
    gkPreviewLayout->addWidget(gkPreviewHelp);

    _gkCorrelationPreviewPlot = new DataTablePlotWidget();
    _gkCorrelationPreviewPlot->setMinimumHeight(180);
    _gkCorrelationPreviewPlot->setMaximumHeight(180);
    _gkCorrelationPreviewPlot->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine());
    gkPreviewLayout->addWidget(new QLabel(tr("Normalized current autocorrelation:"), gkPreviewBox));
    gkPreviewLayout->addWidget(_gkCorrelationPreviewPlot);

    _gkConductivityPreviewPlot = new DataTablePlotWidget();
    _gkConductivityPreviewPlot->setMinimumHeight(180);
    _gkConductivityPreviewPlot->setMaximumHeight(180);
    _gkConductivityPreviewPlot->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine());
    gkPreviewLayout->addWidget(new QLabel(tr("Green-Kubo conductivity (SI):"), gkPreviewBox));
    gkPreviewLayout->addWidget(_gkConductivityPreviewPlot);
    layout->addWidget(gkPreviewBox);

    const QPen fitMarkerPen(QColor(65, 105, 225), 1.0, Qt::DashLine);
    const QPen plateauMarkerPen(QColor(65, 105, 225), 1.0, Qt::DashLine);
    _gkCorrelationStartMarker = new QwtPlotMarker();
    _gkCorrelationEndMarker = new QwtPlotMarker();
    _gkConductivityStartMarker = new QwtPlotMarker();
    _gkConductivityEndMarker = new QwtPlotMarker();
    _gkConductivityPlateauMarker = new QwtPlotMarker();
    configureMarker(_gkCorrelationStartMarker, QwtPlotMarker::VLine, fitMarkerPen);
    configureMarker(_gkCorrelationEndMarker, QwtPlotMarker::VLine, fitMarkerPen);
    configureMarker(_gkConductivityStartMarker, QwtPlotMarker::VLine, fitMarkerPen);
    configureMarker(_gkConductivityEndMarker, QwtPlotMarker::VLine, fitMarkerPen);
    configureMarker(_gkConductivityPlateauMarker, QwtPlotMarker::HLine, plateauMarkerPen);
    _gkCorrelationStartMarker->attach(_gkCorrelationPreviewPlot);
    _gkCorrelationEndMarker->attach(_gkCorrelationPreviewPlot);
    _gkConductivityStartMarker->attach(_gkConductivityPreviewPlot);
    _gkConductivityEndMarker->attach(_gkConductivityPreviewPlot);
    _gkConductivityPlateauMarker->attach(_gkConductivityPreviewPlot);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &TransportModifierEditor::updatePlots);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &TransportModifierEditor::updateSummary);
    if(_computeMSDCheckBox)
        connect(_computeMSDCheckBox, &QCheckBox::toggled, this, &TransportModifierEditor::updateControlStates);
    if(_computeVACFCheckBox)
        connect(_computeVACFCheckBox, &QCheckBox::toggled, this, &TransportModifierEditor::updateControlStates);
    if(_computeConductivityCheckBox)
        connect(_computeConductivityCheckBox, &QCheckBox::toggled, this, &TransportModifierEditor::updateControlStates);
    if(_useOnlySelectedParticlesCheckBox)
        connect(_useOnlySelectedParticlesCheckBox, &QCheckBox::toggled, this, &TransportModifierEditor::updateControlStates);
    if(_selectAsMoleculesCheckBox)
        connect(_selectAsMoleculesCheckBox, &QCheckBox::toggled, this, &TransportModifierEditor::updateControlStates);

    updatePlots();
    updateSummary();
    updateControlStates();
}

/******************************************************************************
 * Launches a non-interactive evaluation of the transport modifier.
 ******************************************************************************/
void TransportModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        TransportModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* transportNode = dynamic_object_cast<const TransportModificationNode>(node);
        const int startedGenerationId = transportNode ? transportNode->cacheGenerationId() : 0;
        _summaryLabel->setText(tr("Running transport analysis over the sampled trajectory..."));

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<TransportModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId](auto& task) noexcept {
            if(!task.isCanceled() && !task.exceptionStore())
                return;
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            TransportModifier* mod = self->modifier();
            auto* transportNode = dynamic_object_cast<TransportModificationNode>(self->modificationNode());
            if(!mod || !transportNode || mod->runRequestId() != startedRunRequestId || transportNode->cacheGenerationId() != startedGenerationId)
                return;

            transportNode->setCompletedRunRequestId(startedRunRequestId);
            self->updatePlots();
            self->updateSummary();
        });
        scheduleOperationAfter(std::move(future), [this, startedRunRequestId, startedGenerationId](const PipelineFlowState&) {
            TransportModifier* mod = modifier();
            const auto* transportNode = dynamic_object_cast<const TransportModificationNode>(modificationNode());
            if(!mod || !transportNode || mod->runRequestId() != startedRunRequestId || transportNode->cacheGenerationId() != startedGenerationId)
                return;
            updatePlots();
            updateSummary();
        });
    });
}

/******************************************************************************
 * Updates the plot widgets from the modifier output tables.
 ******************************************************************************/
void TransportModifierEditor::updatePlots()
{
    handleExceptions([&]() {
        if(transportAnalysisIsIdle(modifier(), modificationNode())) {
            _msdPlot->setTable(nullptr);
            _vacfPlot->setTable(nullptr);
            _conductivityPlot->setTable(nullptr);
            if(_gkCorrelationPreviewPlot)
                _gkCorrelationPreviewPlot->setTable(nullptr);
            if(_gkConductivityPreviewPlot)
                _gkConductivityPreviewPlot->setTable(nullptr);
            hideMarker(_gkCorrelationStartMarker);
            hideMarker(_gkCorrelationEndMarker);
            hideMarker(_gkConductivityStartMarker);
            hideMarker(_gkConductivityEndMarker);
            hideMarker(_gkConductivityPlateauMarker);
            if(_gkPreviewSection)
                _gkPreviewSection->setVisible(false);
            updateControlStates();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        _msdPlot->setTable(state.getObjectBy<DataTable>(modificationNode(), TransportModifier::MSDTableId));
        _vacfPlot->setTable(state.getObjectBy<DataTable>(modificationNode(), TransportModifier::VACFTableId));
        _conductivityPlot->setTable(state.getObjectBy<DataTable>(modificationNode(), TransportModifier::ConductivityTableId));
        updateGreenKuboPreview(state);
        updateControlStates();
    });
}

/******************************************************************************
 * Updates the paper-style Green-Kubo preview plots.
 ******************************************************************************/
void TransportModifierEditor::updateGreenKuboPreview(const PipelineFlowState& state)
{
    if(!_gkCorrelationPreviewPlot || !_gkConductivityPreviewPlot)
        return;

    _gkCorrelationPreviewPlot->setTable(nullptr);
    _gkConductivityPreviewPlot->setTable(nullptr);
    hideMarker(_gkCorrelationStartMarker);
    hideMarker(_gkCorrelationEndMarker);
    hideMarker(_gkConductivityStartMarker);
    hideMarker(_gkConductivityEndMarker);
    hideMarker(_gkConductivityPlateauMarker);
    if(_gkPreviewSection)
        _gkPreviewSection->setVisible(false);

    const TransportModifier* mod = modifier();
    const DataTable* currentCorrelationTable = state.getObjectBy<DataTable>(modificationNode(), TransportModifier::CurrentCorrelationTableId);
    const DataTable* conductivityTable = state.getObjectBy<DataTable>(modificationNode(), TransportModifier::ConductivityTableId);
    if(!mod || !currentCorrelationTable || !conductivityTable || !currentCorrelationTable->y() || !conductivityTable->y())
        return;

    ConstPropertyPtr currentXProperty = currentCorrelationTable->getXValues();
    ConstPropertyPtr conductivityXProperty = conductivityTable->getXValues();
    if(!currentXProperty || !conductivityXProperty)
        return;

    const int currentComponent = findComponentIndex(currentCorrelationTable->y(), QStringLiteral("Current autocorrelation"));
    const int conductivityComponent = findComponentIndex(conductivityTable->y(), QStringLiteral("Green-Kubo (V)"));
    if(currentComponent < 0 || conductivityComponent < 0)
        return;

    BufferReadAccessAndRef<FloatType> currentX(currentXProperty);
    BufferReadAccessAndRef<FloatType*> currentY(currentCorrelationTable->y());
    BufferReadAccessAndRef<FloatType> conductivityX(conductivityXProperty);
    BufferReadAccessAndRef<FloatType*> conductivityY(conductivityTable->y());
    if(currentX.size() == 0 || conductivityX.size() == 0)
        return;

    double zeroLagValue = std::numeric_limits<double>::quiet_NaN();
    if(currentY.size() > 0)
        zeroLagValue = static_cast<double>(currentY.get(0, currentComponent));
    if(!std::isfinite(zeroLagValue) || zeroLagValue == 0.0)
        return;

    std::vector<double> normalizedCorrelationTimes;
    std::vector<double> normalizedCorrelationValues;
    normalizedCorrelationTimes.reserve(currentX.size());
    normalizedCorrelationValues.reserve(currentY.size());
    for(size_t i = 0; i < currentX.size(); ++i) {
        const double x = static_cast<double>(currentX[i]);
        const double y = static_cast<double>(currentY.get(i, currentComponent));
        if(x > 0.0 && std::isfinite(x) && std::isfinite(y)) {
            normalizedCorrelationTimes.push_back(x);
            normalizedCorrelationValues.push_back(y / zeroLagValue);
        }
    }

    const int dimensionality = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.dimensionality")).toInt();
    const double conductivityToSI = conductivityScaleToSI(mod, dimensionality > 0 ? dimensionality : 3);
    std::vector<double> conductivityTimes;
    std::vector<double> conductivityValuesSI;
    conductivityTimes.reserve(conductivityX.size());
    conductivityValuesSI.reserve(conductivityY.size());
    for(size_t i = 0; i < conductivityX.size(); ++i) {
        const double x = static_cast<double>(conductivityX[i]);
        const double y = static_cast<double>(conductivityY.get(i, conductivityComponent));
        if(x > 0.0 && std::isfinite(x) && std::isfinite(y)) {
            conductivityTimes.push_back(x);
            conductivityValuesSI.push_back(y * conductivityToSI);
        }
    }

    const QString timeLabel = timeUnitLabel(mod->timeUnit());
    const QString conductivitySIUnit = conductivitySIUnitLabel(dimensionality > 0 ? dimensionality : 3);

    _gkCorrelationPreviewPlot->setTable(createPreviewLineTable(tr("Normalized current autocorrelation"),
                                                               tr("Time (%1)").arg(timeLabel),
                                                               tr("<J(t)*J(0)> / <J(0)*J(0)>"),
                                                               tr("Green-Kubo"),
                                                               normalizedCorrelationTimes,
                                                               normalizedCorrelationValues));
    _gkConductivityPreviewPlot->setTable(createPreviewLineTable(tr("Green-Kubo conductivity"),
                                                                tr("Time (%1)").arg(timeLabel),
                                                                tr("Conductivity (%1)").arg(conductivitySIUnit),
                                                                tr("Green-Kubo"),
                                                                conductivityTimes,
                                                                conductivityValuesSI));
    if(_gkPreviewSection)
        _gkPreviewSection->setVisible(true);

    const int fitStartLag = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_gk_fit_start_lag")).toInt();
    const int fitEndLag = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_gk_fit_end_lag")).toInt();
    const QVariant sigmaGkSIValue = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_green_kubo_vavg"));
    const double sigmaGkSI = sigmaGkSIValue.toDouble();

    auto markerTimeForLag = [&](int lag) -> double {
        if(lag >= 0 && lag < static_cast<int>(currentX.size()))
            return static_cast<double>(currentX[lag]);
        return std::numeric_limits<double>::quiet_NaN();
    };

    const double fitStartTime = markerTimeForLag(fitStartLag);
    const double fitEndTime = markerTimeForLag(std::max(fitStartLag, fitEndLag - 1));
    if(std::isfinite(fitStartTime) && fitStartTime > 0.0) {
        _gkCorrelationStartMarker->setXValue(fitStartTime);
        _gkConductivityStartMarker->setXValue(fitStartTime);
        _gkCorrelationStartMarker->setVisible(true);
        _gkConductivityStartMarker->setVisible(true);
    }
    if(std::isfinite(fitEndTime) && fitEndTime > 0.0) {
        _gkCorrelationEndMarker->setXValue(fitEndTime);
        _gkConductivityEndMarker->setXValue(fitEndTime);
        _gkCorrelationEndMarker->setVisible(true);
        _gkConductivityEndMarker->setVisible(true);
    }
    if(std::isfinite(sigmaGkSI)) {
        _gkConductivityPlateauMarker->setYValue(sigmaGkSI);
        _gkConductivityPlateauMarker->setVisible(true);
    }

    _gkCorrelationPreviewPlot->replot();
    _gkConductivityPreviewPlot->replot();
}

/******************************************************************************
 * Updates the summary label based on the generated global attributes.
 ******************************************************************************/
void TransportModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        TransportModifier* mod = modifier();
        if(transportAnalysisIsIdle(mod, modificationNode())) {
            _summaryLabel->setText(tr("Transport results are idle. Open the Run section and click 'Run transport analysis' to compute the selected observables."));
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString timeLabel = mod ? timeUnitLabel(mod->timeUnit()) : QStringLiteral("ps");
        const QString lengthLabel = mod ? lengthUnitLabel(mod->lengthUnit()) : QStringLiteral("A");
        const QString chargeLabel = mod ? chargeUnitLabel(mod->chargeUnit()) : QStringLiteral("e");
        const QString diffusionRawUnit = diffusionUnitLabel(lengthLabel, timeLabel);
        const int dimensionality = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.dimensionality")).toInt();
        const QString conductivityRawUnit = conductivityRawUnitLabel(chargeLabel, timeLabel, lengthLabel, dimensionality > 0 ? dimensionality : 3);
        const QString conductivitySIUnit = conductivitySIUnitLabel(dimensionality > 0 ? dimensionality : 3);
        const QString warningPrefix = (state.status().type() == PipelineStatus::Warning && !state.status().text().isEmpty())
                                          ? tr("Warning: %1").arg(state.status().text())
                                          : QString{};

        const QVariant dMsdRaw = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.D_MSD"));
        const QVariant dMsdSI = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.D_MSD_SI"));
        const QVariant dVacfRaw = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.D_VACF"));
        const QVariant dVacfSI = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.D_VACF_SI"));
        const QVariant sigmaCorrRaw = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_correlated_einstein_vavg_raw"));
        const QVariant sigmaCorrSI = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_correlated_einstein_vavg"));
        const QVariant sigmaNeRaw = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_nernst_einstein_vavg_raw"));
        const QVariant sigmaNeSI = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_nernst_einstein_vavg"));
        const QVariant sigmaGkRaw = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_green_kubo_vavg_raw"));
        const QVariant sigmaGkSI = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.sigma_green_kubo_vavg"));
        const QVariant haven = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.haven_ratio_vavg"));
        const bool pyLatSelectedAtomsDirect = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_selected_atoms_direct")).toDouble() != 0.0;
        const int pyLatMsdFitStartLag = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_msd_fit_start_lag")).toInt();
        const int pyLatGkFitStartLag = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_gk_fit_start_lag")).toInt();
        const int pyLatGkFitEndLag = state.getAttributeValue(modificationNode(), QStringLiteral("Transport.pylat_gk_fit_end_lag")).toInt();

        if(!dMsdRaw.isValid() && !sigmaCorrRaw.isValid()) {
            _summaryLabel->setText(tr("Transport results are idle. Open the Run section and click 'Run transport analysis' to compute the selected observables."));
            return;
        }

        auto applySummaryText = [&](const QString& text) {
            if(!warningPrefix.isEmpty())
                _summaryLabel->setText(warningPrefix + QStringLiteral("\n\n") + text);
            else
                _summaryLabel->setText(text);
        };

        QStringList summaryNotes;
        if(pyLatSelectedAtomsDirect)
            summaryNotes.push_back(tr("Selected atoms are analyzed directly in this configuration."));
        if(pyLatMsdFitStartLag > 0)
            summaryNotes.push_back(tr("MSD fit starts at lag %1.").arg(pyLatMsdFitStartLag));
        if(pyLatGkFitStartLag > 0 && pyLatGkFitEndLag > pyLatGkFitStartLag)
            summaryNotes.push_back(tr("Green-Kubo plateau window: lag %1 to %2.").arg(pyLatGkFitStartLag).arg(pyLatGkFitEndLag - 1));

        const QString summaryPrefix = summaryNotes.join(QLatin1Char(' '));
        if(summaryPrefix.isEmpty()) {
            applySummaryText(
                tr("D(MSD, fitted): %1\nD(VACF, final integral): %2\nSigma correlated Einstein (fitted, V): %3\nSigma Nernst-Einstein (fitted D, V): %4\nSigma Green-Kubo (plateau, V): %5\nHaven ratio: %6")
                    .arg(formatDualValue(dMsdRaw, diffusionRawUnit, dMsdSI, QStringLiteral("m^2/s")),
                         formatDualValue(dVacfRaw, diffusionRawUnit, dVacfSI, QStringLiteral("m^2/s")),
                         formatDualValue(sigmaCorrRaw, conductivityRawUnit, sigmaCorrSI, conductivitySIUnit),
                         formatDualValue(sigmaNeRaw, conductivityRawUnit, sigmaNeSI, conductivitySIUnit),
                         formatDualValue(sigmaGkRaw, conductivityRawUnit, sigmaGkSI, conductivitySIUnit),
                         formatValue(haven)));
        }
        else {
            applySummaryText(
                tr("%1\n\nD(MSD, fitted): %2\nD(VACF, final integral): %3\nSigma correlated Einstein (fitted, V): %4\nSigma Nernst-Einstein (fitted D, V): %5\nSigma Green-Kubo (plateau, V): %6\nHaven ratio: %7")
                    .arg(summaryPrefix,
                         formatDualValue(dMsdRaw, diffusionRawUnit, dMsdSI, QStringLiteral("m^2/s")),
                         formatDualValue(dVacfRaw, diffusionRawUnit, dVacfSI, QStringLiteral("m^2/s")),
                         formatDualValue(sigmaCorrRaw, conductivityRawUnit, sigmaCorrSI, conductivitySIUnit),
                         formatDualValue(sigmaNeRaw, conductivityRawUnit, sigmaNeSI, conductivitySIUnit),
                         formatDualValue(sigmaGkRaw, conductivityRawUnit, sigmaGkSI, conductivitySIUnit),
                         formatValue(haven)));
        }
    });
}

/******************************************************************************
 * Updates dependent control states in the editor.
 ******************************************************************************/
void TransportModifierEditor::updateControlStates()
{
    const bool computeMSD = _computeMSDCheckBox && _computeMSDCheckBox->isChecked();
    const bool computeVACF = _computeVACFCheckBox && _computeVACFCheckBox->isChecked();
    const bool computeConductivity = _computeConductivityCheckBox && _computeConductivityCheckBox->isChecked();
    const bool useOnlySelectedParticles = _useOnlySelectedParticlesCheckBox && _useOnlySelectedParticlesCheckBox->isChecked();

    if(_msdSection)
        _msdSection->setVisible(computeMSD);
    if(_vacfSection)
        _vacfSection->setVisible(computeVACF);
    if(_conductivitySection)
        _conductivitySection->setVisible(computeConductivity);
    if(_gkPreviewSection && !computeConductivity)
        _gkPreviewSection->setVisible(false);

    if(_selectAsMoleculesCheckBox)
        _selectAsMoleculesCheckBox->setEnabled(useOnlySelectedParticles);
}

}   // End of namespace
