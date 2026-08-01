////////////////////////////////////////////////////////////////////////////////////////
//
//  Atomic noising modifier for m-ovito.
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Adds controlled Gaussian positional noise to particles.
 */
class OVITO_PARTICLES_EXPORT AtomicNoisingModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(AtomicNoisingModifier, OOMetaClass)

public:

    enum ScaleMode
    {
        AbsoluteCoordinateSigma = 0,
        FractionOfNearestNeighbor = 1,
        LindemannRmsFraction = 2
    };
    Q_ENUM(ScaleMode);

    enum SigmaSamplingMode
    {
        FixedSigma = 0,
        UniformZeroToSigma = 1
    };
    Q_ENUM(SigmaSamplingMode);

    enum NoiseTensorMode
    {
        IsotropicNoise = 0,
        CartesianDiagonalNoise = 1,
        CellAxisDiagonalNoise = 2
    };
    Q_ENUM(NoiseTensorMode);

    enum ParticleCouplingMode
    {
        IndependentParticles = 0,
        RigidMolecules = 1
    };
    Q_ENUM(ParticleCouplingMode);

    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ScaleMode{FractionOfNearestNeighbor}, scaleMode, setScaleMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(NoiseTensorMode{IsotropicNoise}, noiseTensorMode, setNoiseTensorMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.05}, amplitude, setAmplitude, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.05}, amplitudeY, setAmplitudeY, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.05}, amplitudeZ, setAmplitudeZ, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, nearestNeighborDistance, setNearestNeighborDistance, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(SigmaSamplingMode{FixedSigma}, sigmaSamplingMode, setSigmaSamplingMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ParticleCouplingMode{IndependentParticles}, particleCouplingMode, setParticleCouplingMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, spatialCorrelation, setSpatialCorrelation, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, correlationLength, setCorrelationLength, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{12345}, randomSeed, setRandomSeed, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, frameDependentSeed, setFrameDependentSeed, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelected, setOnlySelected, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, wrapIntoCell, setWrapIntoCell, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, preserveCenterOfMass, setPreserveCenterOfMass, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, writeNoiseProperties, setWriteNoiseProperties, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
