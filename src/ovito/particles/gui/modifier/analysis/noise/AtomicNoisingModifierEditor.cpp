////////////////////////////////////////////////////////////////////////////////////////
//
//  Atomic noising modifier editor for m-ovito.
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/noise/AtomicNoisingModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSizePolicy>
#include "AtomicNoisingModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AtomicNoisingModifierEditor);
SET_OVITO_OBJECT_EDITOR(AtomicNoisingModifier, AtomicNoisingModifierEditor);

void AtomicNoisingModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Atomic noising"), rolloutParams, "manual:m-ovito.atomic_noising");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    auto* scaleModeUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::scaleMode));
    scaleModeUI->comboBox()->addItem(tr("Absolute coordinate sigma"),
                                     QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::AbsoluteCoordinateSigma)));
    scaleModeUI->comboBox()->addItem(tr("Fraction of nearest-neighbor distance"),
                                     QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::FractionOfNearestNeighbor)));
    scaleModeUI->comboBox()->addItem(tr("Lindemann RMS fraction"),
                                     QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::LindemannRmsFraction)));
    gridLayout->addWidget(new QLabel(tr("Noise scale:")), 0, 0);
    gridLayout->addWidget(scaleModeUI->comboBox(), 0, 1);

    auto* noiseTensorUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::noiseTensorMode));
    noiseTensorUI->comboBox()->addItem(tr("Isotropic"),
                                       QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::IsotropicNoise)));
    noiseTensorUI->comboBox()->addItem(tr("Diagonal in Cartesian XYZ"),
                                       QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::CartesianDiagonalNoise)));
    noiseTensorUI->comboBox()->addItem(tr("Diagonal along simulation-cell axes"),
                                       QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::CellAxisDiagonalNoise)));
    gridLayout->addWidget(new QLabel(tr("Noise tensor:")), 1, 0);
    gridLayout->addWidget(noiseTensorUI->comboBox(), 1, 1);

    FloatParameterUI* amplitudeUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::amplitude));
    gridLayout->addWidget(amplitudeUI->label(), 2, 0);
    gridLayout->addLayout(amplitudeUI->createFieldLayout(), 2, 1);

    FloatParameterUI* amplitudeYUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::amplitudeY));
    gridLayout->addWidget(amplitudeYUI->label(), 3, 0);
    gridLayout->addLayout(amplitudeYUI->createFieldLayout(), 3, 1);

    FloatParameterUI* amplitudeZUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::amplitudeZ));
    gridLayout->addWidget(amplitudeZUI->label(), 4, 0);
    gridLayout->addLayout(amplitudeZUI->createFieldLayout(), 4, 1);

    FloatParameterUI* nnDistanceUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::nearestNeighborDistance));
    gridLayout->addWidget(nnDistanceUI->label(), 5, 0);
    gridLayout->addLayout(nnDistanceUI->createFieldLayout(), 5, 1);

    auto* sigmaModeUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::sigmaSamplingMode));
    sigmaModeUI->comboBox()->addItem(tr("Fixed sigma"),
                                     QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::FixedSigma)));
    sigmaModeUI->comboBox()->addItem(tr("Uniform random sigma from 0 to maximum"),
                                     QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::UniformZeroToSigma)));
    gridLayout->addWidget(new QLabel(tr("Sigma sampling:")), 6, 0);
    gridLayout->addWidget(sigmaModeUI->comboBox(), 6, 1);

    auto* couplingUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::particleCouplingMode));
    couplingUI->comboBox()->addItem(tr("Independent particles"),
                                    QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::IndependentParticles)));
    couplingUI->comboBox()->addItem(tr("Rigid molecules"),
                                    QVariant::fromValue(static_cast<int>(AtomicNoisingModifier::RigidMolecules)));
    gridLayout->addWidget(new QLabel(tr("Particle coupling:")), 7, 0);
    gridLayout->addWidget(couplingUI->comboBox(), 7, 1);

    FloatParameterUI* correlationLengthUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::correlationLength));
    gridLayout->addWidget(correlationLengthUI->label(), 8, 0);
    gridLayout->addLayout(correlationLengthUI->createFieldLayout(), 8, 1);

    IntegerParameterUI* seedUI =
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::randomSeed));
    gridLayout->addWidget(seedUI->label(), 9, 0);
    gridLayout->addLayout(seedUI->createFieldLayout(), 9, 1);

    layout->addLayout(gridLayout);

    BooleanParameterUI* frameSeedUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::frameDependentSeed));
    layout->addWidget(frameSeedUI->checkBox());

    BooleanParameterUI* spatialCorrelationUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::spatialCorrelation));
    layout->addWidget(spatialCorrelationUI->checkBox());

    BooleanParameterUI* selectedUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::onlySelected));
    layout->addWidget(selectedUI->checkBox());

    BooleanParameterUI* wrapUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::wrapIntoCell));
    layout->addWidget(wrapUI->checkBox());

    BooleanParameterUI* preserveComUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::preserveCenterOfMass));
    layout->addWidget(preserveComUI->checkBox());

    BooleanParameterUI* writePropertiesUI =
        createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AtomicNoisingModifier::writeNoiseProperties));
    layout->addWidget(writePropertiesUI->checkBox());

    QLabel* hintLabel = new QLabel(
        tr("Nearest-neighbor distance 0 means automatic estimation. Isotropic mode uses only the X/isotropic amplitude. Spatial correlation Gaussian-smooths the random field over 3 correlation lengths and then renormalizes the RMS."),
        rollout);
    hintLabel->setWordWrap(true);
    hintLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(hintLabel);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_summaryLabel);

    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    statusWidget->setMinimumHeight(64);
    statusWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(statusWidget);

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &AtomicNoisingModifierEditor::updateSummary);
}

void AtomicNoisingModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        const PipelineFlowState& state = getPipelineOutput();
        const QVariant count = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.particle_count"));
        if(!count.isValid()) {
            _summaryLabel->setText(tr("No atomic noising results for the current frame."));
            return;
        }

        const QString mode = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.scale_mode")).toString();
        const QString tensor = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.noise_tensor")).toString();
        const QString coupling = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.particle_coupling")).toString();
        const bool spatialCorrelation = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.spatial_correlation")).toBool();
        const double correlationLength = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.correlation_length")).toDouble();
        const double sigmaX = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.coordinate_sigma_x")).toDouble();
        const double sigmaY = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.coordinate_sigma_y")).toDouble();
        const double sigmaZ = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.coordinate_sigma_z")).toDouble();
        const double sigma = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.coordinate_sigma")).toDouble();
        const double rms = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.rms_displacement")).toDouble();
        const double scaleDistance = state.getAttributeValue(modificationNode(), QStringLiteral("AtomicNoising.scale_distance")).toDouble();

        _summaryLabel->setText(tr("Noised particles: %1; scale: %2; tensor: %3; coupling: %4; spatial correlation: %5; correlation length: %6; scale distance: %7; sigma XYZ: (%8, %9, %10); mean coordinate sigma: %11; RMS displacement: %12.")
                                   .arg(count.toLongLong())
                                   .arg(mode)
                                   .arg(tensor)
                                   .arg(coupling)
                                   .arg(spatialCorrelation ? tr("yes") : tr("no"))
                                   .arg(correlationLength, 0, 'g', 6)
                                   .arg(scaleDistance, 0, 'g', 6)
                                   .arg(sigmaX, 0, 'g', 6)
                                   .arg(sigmaY, 0, 'g', 6)
                                   .arg(sigmaZ, 0, 'g', 6)
                                   .arg(sigma, 0, 'g', 6)
                                   .arg(rms, 0, 'g', 6));
    });
}

}  // namespace Ovito
