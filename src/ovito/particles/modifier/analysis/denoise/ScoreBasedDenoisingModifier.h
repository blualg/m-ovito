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

namespace Ovito {

/**
 * \brief Runs the score-based denoising model from ovito-org/ScoreBasedDenoising.
 *
 * The native modifier delegates neural-network inference to an external Python
 * interpreter, avoiding a direct C++ dependency on PyTorch and torch-geometric.
 */
class OVITO_PARTICLES_EXPORT ScoreBasedDenoisingModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(ScoreBasedDenoisingModifier, OOMetaClass)

public:

    enum StructurePreset
    {
        NoDenoising = 0,
        FCC = 1,
        BCC = 2,
        HCP = 3,
        SiO2 = 4,
        Custom = 5
    };
    Q_ENUM(StructurePreset);

    enum ComputeDevice
    {
        Cpu = 0,
        Cuda = 1,
        Mps = 2
    };
    Q_ENUM(ComputeDevice);

    static constexpr QStringView ConvergenceTableIdentifier = u"score-denoising-convergence";
    static constexpr QStringView LogConvergenceTableIdentifier = u"score-denoising-log-convergence";

    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(StructurePreset{NoDenoising}, structurePreset, setStructurePreset, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{8}, steps, setSteps, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, nearestNeighborDistance, setNearestNeighborDistance, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, modelPath, setModelPath, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QStringLiteral("python"), pythonExecutable, setPythonExecutable, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ComputeDevice{Cpu}, device, setDevice, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelected, setOnlySelected);
};

}  // namespace Ovito
