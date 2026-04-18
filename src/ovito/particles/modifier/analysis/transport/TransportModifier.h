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

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/DataCollection.h>

namespace Ovito {

class OVITO_PARTICLES_EXPORT TransportModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(TransportModifier, OOMetaClass)

public:

    enum TimeUnit {
        Femtoseconds = 0,
        Picoseconds = 1,
        Nanoseconds = 2
    };
    Q_ENUM(TimeUnit);

    enum LengthUnit {
        Angstroms = 0,
        Nanometers = 1,
        Meters = 2
    };
    Q_ENUM(LengthUnit);

    enum ChargeUnit {
        ElementaryCharges = 0,
        Coulombs = 1
    };
    Q_ENUM(ChargeUnit);

    static constexpr QStringView MSDTableId = u"transport-msd";
    static constexpr QStringView MSDTableSIId = u"transport-msd-si";
    static constexpr QStringView VACFTableId = u"transport-vacf";
    static constexpr QStringView VACFTableSIId = u"transport-vacf-si";
    static constexpr QStringView DiffusionMSDTableId = u"transport-diffusion-msd";
    static constexpr QStringView DiffusionMSDTableSIId = u"transport-diffusion-msd-si";
    static constexpr QStringView DiffusionVACFTableId = u"transport-diffusion-vacf";
    static constexpr QStringView DiffusionVACFTableSIId = u"transport-diffusion-vacf-si";
    static constexpr QStringView ChargeDisplacementTableId = u"transport-charge-displacement";
    static constexpr QStringView CurrentCorrelationTableId = u"transport-current-correlation";
    static constexpr QStringView ConductivityTableId = u"transport-conductivity";
    static constexpr QStringView ConductivityTableSIId = u"transport-conductivity-si";

    void initializeObject(ObjectInitializationFlags flags);

    virtual void inputCachingHints(ModifierEvaluationRequest& request) override;
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;

private:

    std::vector<int> sampledFrames(const ModificationNode* modNode) const;
    Future<PipelineFlowState> computeTransportData(const ModifierEvaluationRequest& request, PipelineFlowState&& state);
    PipelineFlowState applyCachedResults(const ModifierEvaluationRequest& request, PipelineFlowState state) const;

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, computeMSD, setComputeMSD, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, computeVACF, setComputeVACF, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, computeConductivity, setComputeConductivity, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, useOnlySelectedParticles, setUseOnlySelectedParticles, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, selectAsMolecules, setSelectAsMolecules, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, computePerType, setComputePerType, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, includeVACFCrossTerms, setIncludeVACFCrossTerms, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, useCustomFrameInterval, setUseCustomFrameInterval, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, intervalStart, setIntervalStart, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, intervalEnd, setIntervalEnd, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, samplingFrequency, setSamplingFrequency, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, maxLag, setMaxLag, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, summaryWindowStartLag, setSummaryWindowStartLag, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, summaryWindowEndLag, setSummaryWindowEndLag, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, usePyLATCompatibility, setUsePyLATCompatibility, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.075}, pyLatDiffusivityTolerance, setPyLatDiffusivityTolerance, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.001}, pyLatConductivityTolerance, setPyLatConductivityTolerance, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, deltaT, setDeltaT, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{300.0}, temperature, setTemperature, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(TimeUnit{Picoseconds}, timeUnit, setTimeUnit, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(LengthUnit{Angstroms}, lengthUnit, setLengthUnit, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ChargeUnit{ElementaryCharges}, chargeUnit, setChargeUnit, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, useManualMoleculeDefinitions, setUseManualMoleculeDefinitions, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, manualMoleculeDefinitions, setManualMoleculeDefinitions, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, useManualTypeCharges, setUseManualTypeCharges, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, manualTypeCharges, setManualTypeCharges, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, runRequestId, setRunRequestId, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

class OVITO_PARTICLES_EXPORT TransportModificationNode : public ModificationNode
{
    OVITO_CLASS(TransportModificationNode)

public:

    bool hasCachedResults() const { return cachedResults() != nullptr; }
    void invalidateCachedResults();

protected:

    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const DataCollection>, cachedResults, setCachedResults,
        PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_SUB_ANIM);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, cachedWarningText, setCachedWarningText,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, completedRunRequestId, setCompletedRunRequestId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, cacheGenerationId, setCacheGenerationId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

}   // End of namespace
