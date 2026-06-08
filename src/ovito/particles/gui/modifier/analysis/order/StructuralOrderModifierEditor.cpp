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
#include <ovito/particles/modifier/analysis/order/StructuralOrderModifier.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QComboBox>
#include "StructuralOrderModifierEditor.h"

namespace Ovito {

namespace {

constexpr double GasConstantJPerMolK = 8.31446261815324;
constexpr double JoulesPerCalorie = 4.184;

QString entropyUnitsText(double value)
{
    const double jPerMolK = value * GasConstantJPerMolK;
    const double calPerMolK = jPerMolK / JoulesPerCalorie;
    return StructuralOrderModifierEditor::tr("%1 kB = %2 J/mol K = %3 cal/mol K")
        .arg(value, 0, 'g', 8)
        .arg(jPerMolK, 0, 'g', 8)
        .arg(calPerMolK, 0, 'g', 8);
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(StructuralOrderModifierEditor);
SET_OVITO_OBJECT_EDITOR(StructuralOrderModifier, StructuralOrderModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void StructuralOrderModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Structural order"), rolloutParams, "manual:particles.modifiers.structural_order");

    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    VariantComboBoxParameterUI* orderParameterUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::orderParameter));
    orderParameterUI->comboBox()->addItem(tr("Translational entropy order (-s2^tr)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::TranslationalEntropyOrder)));
    orderParameterUI->comboBox()->addItem(tr("Orientational entropy order (-s2^or, dipole approx.)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::OrientationalEntropyOrder)));
    orderParameterUI->comboBox()->addItem(tr("Tetrahedral order parameter (q)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::TetrahedralOrderParameter)));
    orderParameterUI->comboBox()->addItem(tr("Radial tetrahedral order (S_k)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::RadialTetrahedralOrderParameter)));
    orderParameterUI->comboBox()->addItem(tr("Local structure index (LSI)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::LocalStructureIndexOrderParameter)));
    orderParameterUI->comboBox()->addItem(tr("Voronoi local density (1/V)"),
                                          QVariant::fromValue(static_cast<int>(StructuralOrderModifier::VoronoiLocalDensityOrderParameter)));
    _orderParameterCombo = orderParameterUI->comboBox();
    gridLayout->addWidget(new QLabel(tr("Order parameter:")), 0, 0);
    gridLayout->addWidget(orderParameterUI->comboBox(), 0, 1);

    FloatParameterUI* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::cutoff));
    _cutoffLabel = cutoffUI->label();
    _cutoffField = new QWidget(rollout);
    _cutoffField->setLayout(cutoffUI->createFieldLayout());
    gridLayout->addWidget(_cutoffLabel, 1, 0);
    gridLayout->addWidget(_cutoffField, 1, 1);

    IntegerParameterUI* radialBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::radialBins));
    _radialBinsLabel = radialBinsUI->label();
    _radialBinsField = new QWidget(rollout);
    _radialBinsField->setLayout(radialBinsUI->createFieldLayout());
    gridLayout->addWidget(_radialBinsLabel, 2, 0);
    gridLayout->addWidget(_radialBinsField, 2, 1);

    IntegerParameterUI* angularBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::angularBins));
    _angularBinsLabel = angularBinsUI->label();
    _angularBinsField = new QWidget(rollout);
    _angularBinsField->setLayout(angularBinsUI->createFieldLayout());
    gridLayout->addWidget(_angularBinsLabel, 3, 0);
    gridLayout->addWidget(_angularBinsField, 3, 1);

    IntegerParameterUI* distributionBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::distributionBins));
    _distributionBinsLabel = distributionBinsUI->label();
    _distributionBinsField = new QWidget(rollout);
    _distributionBinsField->setLayout(distributionBinsUI->createFieldLayout());
    gridLayout->addWidget(_distributionBinsLabel, 4, 0);
    gridLayout->addWidget(_distributionBinsField, 4, 1);

    FloatParameterUI* tetrahedralReferenceDistanceUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::tetrahedralReferenceDistance));
    _tetrahedralReferenceDistanceLabel = tetrahedralReferenceDistanceUI->label();
    _tetrahedralReferenceDistanceField = new QWidget(rollout);
    _tetrahedralReferenceDistanceField->setLayout(tetrahedralReferenceDistanceUI->createFieldLayout());
    gridLayout->addWidget(_tetrahedralReferenceDistanceLabel, 5, 0);
    gridLayout->addWidget(_tetrahedralReferenceDistanceField, 5, 1);

    FloatParameterUI* localStructureIndexCutoffUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::localStructureIndexCutoff));
    _localStructureIndexCutoffLabel = localStructureIndexCutoffUI->label();
    _localStructureIndexCutoffField = new QWidget(rollout);
    _localStructureIndexCutoffField->setLayout(localStructureIndexCutoffUI->createFieldLayout());
    gridLayout->addWidget(_localStructureIndexCutoffLabel, 6, 0);
    gridLayout->addWidget(_localStructureIndexCutoffField, 6, 1);

    VariantComboBoxParameterUI* localTargetModeUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::localOrderTargetMode));
    localTargetModeUI->comboBox()->addItem(tr("Current/selected particles"),
                                           QVariant::fromValue(static_cast<int>(StructuralOrderModifier::CurrentParticles)));
    localTargetModeUI->comboBox()->addItem(tr("Local sites near reference atoms"),
                                           QVariant::fromValue(static_cast<int>(StructuralOrderModifier::SitesWithinReferenceCutoff)));
    _localTargetModeLabel = new QLabel(tr("Local target:"));
    _localTargetModeCombo = localTargetModeUI->comboBox();
    gridLayout->addWidget(_localTargetModeLabel, 7, 0);
    gridLayout->addWidget(_localTargetModeCombo, 7, 1);

    StringParameterUI* referenceTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::referenceTypes));
    auto* referenceExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::referenceExpression));
    _referenceTypesLabel = new QLabel(tr("Reference atom type(s):"));
    _referenceTypesField = createSelectorPopupRow(
        rollout,
        referenceTypesUI->textBox(),
        referenceExpressionUI,
        tr("Reference expression override"),
        tr("Use this expression instead of the reference atom type list. Leave it empty to use the type field again."));
    gridLayout->addWidget(_referenceTypesLabel, 8, 0);
    gridLayout->addWidget(_referenceTypesField, 8, 1);

    StringParameterUI* localSiteTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::localSiteTypes));
    localSiteTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O"));
    auto* localSiteExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::localSiteExpression));
    _localSiteTypesLabel = new QLabel(tr("Local site atom type(s):"));
    _localSiteTypesField = createSelectorPopupRow(
        rollout,
        localSiteTypesUI->textBox(),
        localSiteExpressionUI,
        tr("Local site expression override"),
        tr("Use this expression instead of the local site atom type list. Leave it empty to use the type field again."));
    gridLayout->addWidget(_localSiteTypesLabel, 9, 0);
    gridLayout->addWidget(_localSiteTypesField, 9, 1);

    FloatParameterUI* localShellCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::localShellCutoff));
    _localShellCutoffLabel = localShellCutoffUI->label();
    _localShellCutoffField = new QWidget(rollout);
    _localShellCutoffField->setLayout(localShellCutoffUI->createFieldLayout());
    gridLayout->addWidget(_localShellCutoffLabel, 10, 0);
    gridLayout->addWidget(_localShellCutoffField, 10, 1);

    BooleanParameterUI* selectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructuralOrderModifier::onlySelected));
    gridLayout->addWidget(selectedUI->checkBox(), 11, 0, 1, 2);

    layout->addLayout(gridLayout);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_summaryLabel);

    _profilePlot = new DataTablePlotWidget();
    _profilePlot->setMinimumHeight(200);
    _profilePlot->setMaximumHeight(200);
    layout->addSpacing(8);
    _plotLabel = new QLabel(tr("Structural order profile:"));
    layout->addWidget(_plotLabel);
    layout->addWidget(_profilePlot);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show in data inspector"), StructuralOrderModifier::ProfileTableIdentifier, 1));

    layout->addSpacing(6);
    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    statusWidget->setMinimumHeight(64);
    layout->addWidget(statusWidget);

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &StructuralOrderModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &StructuralOrderModifierEditor::updateSummary);
    connect(orderParameterUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StructuralOrderModifierEditor::updateParameterVisibility);
    connect(localTargetModeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StructuralOrderModifierEditor::updateParameterVisibility);
    updateParameterVisibility();
}

/******************************************************************************
 * Hides controls that are irrelevant for the selected order parameter.
 ******************************************************************************/
void StructuralOrderModifierEditor::updateParameterVisibility()
{
    const int selectedMode = _orderParameterCombo ? _orderParameterCombo->currentData().toInt()
                                                  : static_cast<int>(StructuralOrderModifier::TranslationalEntropyOrder);
    const bool orientationalMode = selectedMode == static_cast<int>(StructuralOrderModifier::OrientationalEntropyOrder);
    const bool tetrahedralMode = selectedMode == static_cast<int>(StructuralOrderModifier::TetrahedralOrderParameter);
    const bool radialTetrahedralMode = selectedMode == static_cast<int>(StructuralOrderModifier::RadialTetrahedralOrderParameter);
    const bool localStructureIndexMode = selectedMode == static_cast<int>(StructuralOrderModifier::LocalStructureIndexOrderParameter);
    const bool voronoiLocalDensityMode = selectedMode == static_cast<int>(StructuralOrderModifier::VoronoiLocalDensityOrderParameter);
    const bool localScalarMode = tetrahedralMode
        || radialTetrahedralMode
        || localStructureIndexMode
        || voronoiLocalDensityMode;
    const bool entropyMode = !localScalarMode;
    const bool referenceShellMode = localScalarMode
        && _localTargetModeCombo
        && _localTargetModeCombo->currentData().toInt() == static_cast<int>(StructuralOrderModifier::SitesWithinReferenceCutoff);

    if(_cutoffLabel)
        _cutoffLabel->setVisible(entropyMode);
    if(_cutoffField)
        _cutoffField->setVisible(entropyMode);
    if(_radialBinsLabel)
        _radialBinsLabel->setVisible(entropyMode);
    if(_radialBinsField)
        _radialBinsField->setVisible(entropyMode);
    if(_angularBinsLabel)
        _angularBinsLabel->setVisible(orientationalMode);
    if(_angularBinsField)
        _angularBinsField->setVisible(orientationalMode);
    if(_distributionBinsLabel)
        _distributionBinsLabel->setVisible(localScalarMode);
    if(_distributionBinsField)
        _distributionBinsField->setVisible(localScalarMode);
    if(_tetrahedralReferenceDistanceLabel)
        _tetrahedralReferenceDistanceLabel->setVisible(radialTetrahedralMode);
    if(_tetrahedralReferenceDistanceField)
        _tetrahedralReferenceDistanceField->setVisible(radialTetrahedralMode);
    if(_localStructureIndexCutoffLabel)
        _localStructureIndexCutoffLabel->setVisible(localStructureIndexMode);
    if(_localStructureIndexCutoffField)
        _localStructureIndexCutoffField->setVisible(localStructureIndexMode);
    if(_localTargetModeLabel)
        _localTargetModeLabel->setVisible(localScalarMode);
    if(_localTargetModeCombo)
        _localTargetModeCombo->setVisible(localScalarMode);
    if(_referenceTypesLabel)
        _referenceTypesLabel->setVisible(referenceShellMode);
    if(_referenceTypesField)
        _referenceTypesField->setVisible(referenceShellMode);
    if(_localSiteTypesLabel)
        _localSiteTypesLabel->setVisible(referenceShellMode);
    if(_localSiteTypesField)
        _localSiteTypesField->setVisible(referenceShellMode);
    if(_localShellCutoffLabel)
        _localShellCutoffLabel->setVisible(referenceShellMode);
    if(_localShellCutoffField)
        _localShellCutoffField->setVisible(referenceShellMode);
    if(_plotLabel)
        _plotLabel->setText(localScalarMode ? tr("Local order distribution:")
                                            : tr("Structural order profile:"));
}

/******************************************************************************
 * Updates the structural-order profile plot.
 ******************************************************************************/
void StructuralOrderModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_profilePlot)
            return;
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), StructuralOrderModifier::ProfileTableIdentifier);
        _profilePlot->setTable(std::move(table));
    });
}

/******************************************************************************
 * Updates the scalar result summary.
 ******************************************************************************/
void StructuralOrderModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        const PipelineFlowState& state = getPipelineOutput();
        const QVariant order = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.translational_order"));
        const QVariant entropy = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.translational_pair_entropy"));
        const QVariant orientOrder = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.orientational_order"));
        const QVariant orientEntropy = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.orientational_pair_entropy"));
        const QVariant density = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.number_density"));
        const QVariant count = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.sampled_particle_count"));
        const QVariant moleculeCount = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.sampled_molecule_count"));
        const QVariant localOrderName = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.order_parameter"));
        const QVariant localOrderMean = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_mean"));
        const QVariant localOrderStddev = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_stddev"));
        const QVariant localOrderCount = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_sampled_particle_count"));
        const QVariant localOrderSkippedCount = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_skipped_particle_count"));
        const QVariant localOrderTargetMode = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_target_mode"));
        const QVariant localOrderNeighborCount = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_neighbor_candidate_count"));
        const QVariant localOrderProperty = state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_order_property"));
        const QVariant radialReferenceDistance =
            state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.radial_tetrahedral_reference_distance"));
        const QVariant lsiCutoff =
            state.getAttributeValue(modificationNode(), QStringLiteral("StructuralOrder.local_structure_index_cutoff"));

        if(!order.isValid() && !orientOrder.isValid() && !localOrderMean.isValid()) {
            _summaryLabel->clear();
            refreshSummaryGeometry();
            return;
        }

        QStringList lines;
        if(order.isValid()) {
            lines << tr("-s2^tr: %1").arg(entropyUnitsText(order.toDouble()));
            if(entropy.isValid())
                lines << tr("s2^tr: %1").arg(entropyUnitsText(entropy.toDouble()));
        }
        if(orientOrder.isValid()) {
            lines << tr("-s2^or approx.: %1").arg(entropyUnitsText(orientOrder.toDouble()));
            if(orientEntropy.isValid())
                lines << tr("s2^or approx.: %1").arg(entropyUnitsText(orientEntropy.toDouble()));
        }
        if(localOrderMean.isValid()) {
            const QString label = localOrderName.isValid() ? localOrderName.toString() : tr("Local order");
            lines << tr("%1 mean: %2").arg(label).arg(localOrderMean.toDouble(), 0, 'g', 8);
            if(localOrderStddev.isValid())
                lines << tr("%1 std. dev.: %2").arg(label).arg(localOrderStddev.toDouble(), 0, 'g', 8);
            if(localOrderProperty.isValid())
                lines << tr("Property: %1").arg(localOrderProperty.toString());
            if(localOrderTargetMode.isValid())
                lines << tr("Target mode: %1").arg(localOrderTargetMode.toString());
            if(radialReferenceDistance.isValid())
                lines << tr("Ideal tetrahedral distance: %1").arg(radialReferenceDistance.toDouble(), 0, 'g', 8);
            if(lsiCutoff.isValid())
                lines << tr("LSI shell cutoff: %1").arg(lsiCutoff.toDouble(), 0, 'g', 8);
        }
        if(density.isValid())
            lines << tr("Number density: %1").arg(density.toDouble(), 0, 'g', 8);
        if(count.isValid())
            lines << tr("Sampled particles: %1").arg(count.toLongLong());
        if(moleculeCount.isValid())
            lines << tr("Sampled molecules: %1").arg(moleculeCount.toLongLong());
        if(localOrderCount.isValid())
            lines << tr("Sampled particles: %1").arg(localOrderCount.toLongLong());
        if(localOrderSkippedCount.isValid() && localOrderSkippedCount.toLongLong() > 0)
            lines << tr("Skipped invalid particles: %1").arg(localOrderSkippedCount.toLongLong());
        if(localOrderNeighborCount.isValid())
            lines << tr("Neighbor candidates: %1").arg(localOrderNeighborCount.toLongLong());
        _summaryLabel->setText(lines.join(QLatin1Char('\n')));
        refreshSummaryGeometry();
    });
}

/******************************************************************************
 * Reflows the wrapped summary label after changing its contents.
 ******************************************************************************/
void StructuralOrderModifierEditor::refreshSummaryGeometry()
{
    if(!_summaryLabel)
        return;

    _summaryLabel->updateGeometry();
    _summaryLabel->adjustSize();
    for(QWidget* widget = _summaryLabel->parentWidget(); widget; widget = widget->parentWidget()) {
        if(QLayout* layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        widget->updateGeometry();
    }
}

}  // namespace Ovito
