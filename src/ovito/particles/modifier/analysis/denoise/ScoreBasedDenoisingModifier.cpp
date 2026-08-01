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

#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "ScoreBasedDenoisingModifier.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ScoreBasedDenoisingModifier);
OVITO_CLASSINFO(ScoreBasedDenoisingModifier, "Description", "Denoise particle positions with score-based neural-network models.");
OVITO_CLASSINFO(ScoreBasedDenoisingModifier, "DisplayName", "Score-based denoising");
OVITO_CLASSINFO(ScoreBasedDenoisingModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, structurePreset);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, steps);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, nearestNeighborDistance);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, modelPath);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, pythonExecutable);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, device);
DEFINE_PROPERTY_FIELD(ScoreBasedDenoisingModifier, onlySelected);
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, structurePreset, "Structure / material");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, steps, "Denoising steps");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, nearestNeighborDistance, "Nearest-neighbor distance");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, modelPath, "Model path");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, pythonExecutable, "Python executable");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, device, "Device");
SET_PROPERTY_FIELD_LABEL(ScoreBasedDenoisingModifier, onlySelected, "Write back only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ScoreBasedDenoisingModifier, steps, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ScoreBasedDenoisingModifier, nearestNeighborDistance, WorldParameterUnit, 0);

namespace {

constexpr int MaximumNearestNeighborCount = 16;

QString presetName(ScoreBasedDenoisingModifier::StructurePreset preset)
{
    switch(preset) {
    case ScoreBasedDenoisingModifier::NoDenoising: return ScoreBasedDenoisingModifier::tr("None");
    case ScoreBasedDenoisingModifier::FCC: return QStringLiteral("FCC");
    case ScoreBasedDenoisingModifier::BCC: return QStringLiteral("BCC");
    case ScoreBasedDenoisingModifier::HCP: return QStringLiteral("HCP");
    case ScoreBasedDenoisingModifier::SiO2: return QStringLiteral("SiO2");
    case ScoreBasedDenoisingModifier::Custom: return ScoreBasedDenoisingModifier::tr("Custom");
    }
    return {};
}

QString deviceName(ScoreBasedDenoisingModifier::ComputeDevice device)
{
    switch(device) {
    case ScoreBasedDenoisingModifier::Cpu: return QStringLiteral("cpu");
    case ScoreBasedDenoisingModifier::Cuda: return QStringLiteral("cuda");
    case ScoreBasedDenoisingModifier::Mps: return QStringLiteral("mps");
    }
    return QStringLiteral("cpu");
}

int neighborCountForPreset(ScoreBasedDenoisingModifier::StructurePreset preset)
{
    switch(preset) {
    case ScoreBasedDenoisingModifier::BCC: return 8;
    case ScoreBasedDenoisingModifier::SiO2: return 4;
    case ScoreBasedDenoisingModifier::FCC:
    case ScoreBasedDenoisingModifier::HCP: return 12;
    default: return 0;
    }
}

double originalModelScaleForPreset(ScoreBasedDenoisingModifier::StructurePreset preset)
{
    switch(preset) {
    case ScoreBasedDenoisingModifier::FCC: return 2.42;
    case ScoreBasedDenoisingModifier::BCC: return 2.46;
    case ScoreBasedDenoisingModifier::HCP: return 2.41;
    case ScoreBasedDenoisingModifier::SiO2: return 1.59;
    default: return 0.0;
    }
}

QString typeNameForParticle(const Property* typeProperty, int typeId)
{
    if(!typeProperty)
        return {};
    if(const ElementType* type = typeProperty->elementType(typeId))
        return type->name();
    return {};
}

bool isSiliconParticle(const Property* typeProperty, int typeId)
{
    const QString name = typeNameForParticle(typeProperty, typeId).trimmed();
    return name.compare(QStringLiteral("Si"), Qt::CaseInsensitive) == 0;
}

double estimateNearestNeighborDistance(const Particles* particles,
                                       const SimulationCell* cell,
                                       ScoreBasedDenoisingModifier::StructurePreset preset)
{
    const int neighborCount = neighborCountForPreset(preset);
    if(neighborCount <= 0)
        return 0.0;

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);

    NearestNeighborFinder neighborFinder(neighborCount, positions, cell, {});
    NearestNeighborFinder::Query<MaximumNearestNeighborCount> query(neighborFinder);

    double distanceSum = 0.0;
    size_t distanceCount = 0;
    size_t centerCount = 0;

    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
        if(preset == ScoreBasedDenoisingModifier::SiO2) {
            if(!particleTypes || !isSiliconParticle(typeProperty, particleTypes[particleIndex]))
                continue;
        }

        centerCount++;
        query.findNeighbors(particleIndex);
        for(const NearestNeighborFinder::Neighbor& neighbor : query.results()) {
            distanceSum += std::sqrt(static_cast<double>(neighbor.distanceSq));
            distanceCount++;
        }
    }

    if(centerCount == 0 && preset == ScoreBasedDenoisingModifier::SiO2)
        throw Exception(ScoreBasedDenoisingModifier::tr("The SiO2 preset requires a named particle type 'Si'."));
    if(distanceCount == 0)
        throw Exception(ScoreBasedDenoisingModifier::tr("Unable to estimate the nearest-neighbor distance from the current particles."));
    return distanceSum / static_cast<double>(distanceCount);
}

QJsonArray pointToJson(const Point3& point)
{
    QJsonArray array;
    array.append(point.x());
    array.append(point.y());
    array.append(point.z());
    return array;
}

QJsonArray vectorToJson(const Vector3& vector)
{
    QJsonArray array;
    array.append(vector.x());
    array.append(vector.y());
    array.append(vector.z());
    return array;
}

Point3 pointFromJson(const QJsonValue& value)
{
    const QJsonArray array = value.toArray();
    if(array.size() != 3)
        throw Exception(ScoreBasedDenoisingModifier::tr("The denoising helper returned an invalid position array."));
    return Point3(static_cast<FloatType>(array[0].toDouble()),
                  static_cast<FloatType>(array[1].toDouble()),
                  static_cast<FloatType>(array[2].toDouble()));
}

QString denoisingHelperScript()
{
    // Adapted from the MIT-licensed ovito-org/ScoreBasedDenoising Python extension.
    return QStringLiteral(R"PYTHON(
import importlib.resources as impRes
import json
import math
import sys
import warnings
from pathlib import Path

import numpy as np
import torch

torch.serialization.add_safe_globals([slice])

from graphite.nn.utils.e3nn_initial_embedding import InitialEmbedding
from graphite.transforms import PeriodicRadiusGraph
from sklearn.preprocessing import LabelEncoder
from torch_geometric.data import Data

InitialEmbedding.__module__ = "__main__"
setattr(sys.modules["__main__"], "InitialEmbedding", InitialEmbedding)

warnings.filterwarnings(
    "ignore",
    category=UserWarning,
    message="The TorchScript type system doesn't support",
)


def _default_model_path(structure):
    model_dir = impRes.files("graphite") / "pretrained_models" / "denoiser"
    if structure == "SiO2":
        return model_dir.joinpath("SiO2-denoiser.pt")
    if structure in ("FCC", "BCC", "HCP"):
        return model_dir.joinpath("Cu-denoiser.pt")
    raise RuntimeError(f"No default model path is available for {structure}.")


def _numbers_from_payload(payload):
    structure = payload["structure"]
    if structure in ("FCC", "BCC", "HCP"):
        # Matches the upstream OVITO extension, which temporarily maps all atoms to type 1.
        return np.ones(len(payload["positions"]), dtype=np.int64)
    if structure == "SiO2":
        names = payload["type_names"]
        numbers = []
        counts = {"Si": 0, "O": 0}
        for name in names:
            if name not in counts:
                raise RuntimeError(
                    f"Unknown particle type '{name}' for SiO2 model. Rename particle types to exactly 'Si' and 'O'."
                )
            counts[name] += 1
            numbers.append(14 if name == "Si" else 8)
        for name, count in counts.items():
            if count == 0:
                raise RuntimeError(f"Type '{name}' not found. The SiO2 model requires both Si and O.")
        return np.array(numbers, dtype=np.int64)
    return np.array(payload["type_ids"], dtype=np.int64)


@torch.no_grad()
def main():
    if len(sys.argv) != 3:
        raise RuntimeError("Expected input and output JSON file paths.")

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    payload = json.loads(input_path.read_text(encoding="utf-8"))

    structure = payload["structure"]
    device_name = payload["device"]
    device = torch.device(device_name)
    model_path_text = payload.get("model_path", "")
    model_path = Path(model_path_text) if model_path_text else _default_model_path(structure)
    if not model_path.exists():
        raise RuntimeError(f"Model file does not exist: {model_path}")

    model = torch.load(model_path, map_location=device, weights_only=False)
    model = model.to(device)
    model.eval()

    positions = np.array(payload["positions"], dtype=np.float32)
    cell = np.array(payload["cell"], dtype=np.float32)
    pbc = np.array(payload["pbc"], dtype=bool)
    numbers = _numbers_from_payload(payload)
    x = LabelEncoder().fit_transform(numbers)

    data = Data(
        x=torch.tensor(x).long(),
        pos=torch.tensor(positions).float(),
        cell=torch.tensor(cell).float(),
        pbc=torch.tensor(pbc).bool(),
        numbers=torch.tensor(numbers).long(),
    )

    model_scale = float(payload["model_scale"])
    if not math.isfinite(model_scale) or model_scale <= 0:
        raise RuntimeError(f"Invalid model scale: {model_scale}")

    data.pos *= model_scale
    data.cell *= model_scale

    radius_graph = PeriodicRadiusGraph(cutoff=3.2)
    convergence = []
    for _ in range(int(payload["steps"])):
        data = radius_graph(data)
        disp = model(data.to(device))
        convergence.append(float(torch.mean(torch.square(disp)).to("cpu")))
        data.pos -= disp

    out_positions = (data.pos.to("cpu").numpy() / model_scale).tolist()
    output = {
        "positions": out_positions,
        "convergence": convergence,
        "model_path": str(model_path),
    }
    output_path.write_text(json.dumps(output), encoding="utf-8")


if __name__ == "__main__":
    main()
)PYTHON");
}

void writeTextFile(const QString& path, const QString& text)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw Exception(ScoreBasedDenoisingModifier::tr("Could not write temporary helper file: %1").arg(path));
    file.write(text.toUtf8());
}

void writeJsonFile(const QString& path, const QJsonObject& object)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly))
        throw Exception(ScoreBasedDenoisingModifier::tr("Could not write temporary denoising input file: %1").arg(path));
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QJsonObject readJsonFile(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        throw Exception(ScoreBasedDenoisingModifier::tr("The denoising helper did not create an output file."));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if(parseError.error != QJsonParseError::NoError || !document.isObject())
        throw Exception(ScoreBasedDenoisingModifier::tr("The denoising helper returned invalid JSON: %1").arg(parseError.errorString()));
    return document.object();
}

void createConvergenceTable(PipelineFlowState& state,
                            const QStringView identifier,
                            const QString& title,
                            const QString& yLabel,
                            const std::vector<double>& values,
                            const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(values.empty())
        return;

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            values.size(),
                                                            Property::FloatDefault,
                                                            1,
                                                            yLabel);
    BufferWriteAccess<FloatType, access_mode::discard_write> yAccess(y);
    for(size_t index = 0; index < values.size(); ++index)
        yAccess[index] = static_cast<FloatType>(values[index]);

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            values.size(),
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Step"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAccess(x);
    for(size_t index = 0; index < values.size(); ++index)
        xAccess[index] = static_cast<FloatType>(index + 1);

    DataTable* table = state.createObject<DataTable>(identifier.toString(),
                                                     createdByNode,
                                                     DataTable::Line,
                                                     title,
                                                     std::move(y),
                                                     std::move(x));
    table->setAxisLabelX(ScoreBasedDenoisingModifier::tr("Step"));
    table->setAxisLabelY(yLabel);
}

}  // namespace

bool ScoreBasedDenoisingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void ScoreBasedDenoisingModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                      PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                      TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

QVariant ScoreBasedDenoisingModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    return presetName(structurePreset());
}

Future<PipelineFlowState> ScoreBasedDenoisingModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                        PipelineFlowState&& state)
{
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    if(structurePreset() == NoDenoising) {
        state.combineStatus(PipelineStatus::Warning, tr("Select a structure preset or Custom model to run score-based denoising."));
        return std::move(state);
    }

    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                particles->tryToAdoptProperties(cachedParticles, {
                    cachedParticles->getProperty(Particles::PositionProperty)
                }, {particles});
            }
            if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(
                   request.modificationNode(), ConvergenceTableIdentifier)) {
                state.addObject(cachedTable);
            }
            if(const DataTable* cachedLogTable = cachedState.getObjectBy<DataTable>(
                   request.modificationNode(), LogConvergenceTableIdentifier)) {
                state.addObject(cachedLogTable);
            }
            state.adoptAttributesFrom(cachedState, request.modificationNodeWeak());
        }
        return std::move(state);
    }

    if(steps() <= 0)
        throw Exception(tr("The number of denoising steps must be positive."));
    if(pythonExecutable().trimmed().isEmpty())
        throw Exception(tr("Please specify a Python executable."));
    if(structurePreset() == Custom) {
        if(modelPath().trimmed().isEmpty())
            throw Exception(tr("The Custom denoising mode requires a model path."));
        if(nearestNeighborDistance() <= 0)
            throw Exception(tr("The Custom denoising mode requires a positive scale value in the nearest-neighbor distance field."));
    }

    const SimulationCell* simulationCell = state.expectObject<SimulationCell>();
    if(simulationCell->isDegenerate())
        throw Exception(tr("Score-based denoising requires a non-degenerate simulation cell."));

    const Property* selectionProperty = onlySelected() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(structurePreset() == SiO2 && (!particleTypeProperty || !particleTypeProperty->isTypedProperty()))
        throw Exception(tr("The SiO2 denoising model requires typed particle types named 'Si' and 'O'."));

    return asyncLaunch([state = std::move(state),
                        particles,
                        simulationCell,
                        selectionProperty,
                        structure = structurePreset(),
                        denoisingSteps = steps(),
                        nnDistance = nearestNeighborDistance(),
                        explicitModelPath = modelPath(),
                        python = pythonExecutable(),
                        computeDevice = device(),
                        selectedOnly = onlySelected(),
                        createdByNode = request.modificationNodeWeak()]() mutable {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Running score-based denoising"));

        const size_t particleCount = particles->elementCount();
        if(particleCount == 0)
            throw Exception(tr("The input contains no particles."));

        BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
        BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
        const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
        BufferReadAccess<SelectionIntType> selection(selectionProperty);

        if(selectedOnly && selectionProperty->nonzeroCount() == 0) {
            state.combineStatus(PipelineStatus::Warning, tr("No selected particles; denoising was skipped."));
            return std::move(state);
        }

        double scaleDistance = static_cast<double>(nnDistance);
        double modelScale = 0.0;
        if(structure == Custom) {
            modelScale = scaleDistance;
        }
        else {
            if(!(scaleDistance > 0.0))
                scaleDistance = estimateNearestNeighborDistance(particles, simulationCell, structure);
            modelScale = originalModelScaleForPreset(structure) / scaleDistance;
        }
        if(!(modelScale > 0.0) || !std::isfinite(modelScale))
            throw Exception(tr("The computed denoising model scale is invalid."));

        QJsonObject input;
        input.insert(QStringLiteral("structure"), presetName(structure));
        input.insert(QStringLiteral("steps"), denoisingSteps);
        input.insert(QStringLiteral("model_scale"), modelScale);
        input.insert(QStringLiteral("device"), deviceName(computeDevice));
        input.insert(QStringLiteral("model_path"), explicitModelPath.trimmed());

        QJsonArray pbc;
        const std::array<bool, 3> pbcFlags = simulationCell->pbcFlagsCorrected();
        pbc.append(pbcFlags[0]);
        pbc.append(pbcFlags[1]);
        pbc.append(pbcFlags[2]);
        input.insert(QStringLiteral("pbc"), pbc);

        QJsonArray cell;
        cell.append(vectorToJson(simulationCell->cellVector1()));
        cell.append(vectorToJson(simulationCell->cellVector2()));
        cell.append(vectorToJson(simulationCell->cellVector3()));
        input.insert(QStringLiteral("cell"), cell);

        QJsonArray positionArray;
        QJsonArray typeIds;
        QJsonArray typeNames;
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            this_task::throwIfCanceled();
            positionArray.append(pointToJson(positions[particleIndex]));
            const int typeId = particleTypes ? particleTypes[particleIndex] : 1;
            typeIds.append(typeId);
            typeNames.append(typeNameForParticle(typeProperty, typeId));
        }
        input.insert(QStringLiteral("positions"), positionArray);
        input.insert(QStringLiteral("type_ids"), typeIds);
        input.insert(QStringLiteral("type_names"), typeNames);

        QTemporaryDir temporaryDir;
        if(!temporaryDir.isValid())
            throw Exception(tr("Could not create a temporary directory for score-based denoising."));
        const QString scriptPath = temporaryDir.filePath(QStringLiteral("score_based_denoising_helper.py"));
        const QString inputPath = temporaryDir.filePath(QStringLiteral("input.json"));
        const QString outputPath = temporaryDir.filePath(QStringLiteral("output.json"));
        writeTextFile(scriptPath, denoisingHelperScript());
        writeJsonFile(inputPath, input);

        QProcess process;
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(python.trimmed(), QStringList{scriptPath, inputPath, outputPath});
        if(!process.waitForStarted())
            throw Exception(tr("Could not start Python executable '%1'.").arg(python.trimmed()));
        while(!process.waitForFinished(250)) {
            this_task::throwIfCanceled();
        }

        const QString standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QString standardOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            QString message = tr("Score-based denoising failed in Python.");
            if(!standardError.isEmpty())
                message += QStringLiteral("\n\n") + standardError;
            else if(!standardOutput.isEmpty())
                message += QStringLiteral("\n\n") + standardOutput;
            throw Exception(message);
        }

        const QJsonObject output = readJsonFile(outputPath);
        const QJsonArray denoisedPositions = output.value(QStringLiteral("positions")).toArray();
        if(static_cast<size_t>(denoisedPositions.size()) != particleCount)
            throw Exception(tr("The denoising helper returned %1 positions for %2 particles.")
                                .arg(denoisedPositions.size())
                                .arg(particleCount));

        BufferWriteAccess<Point3, access_mode::read_write> outputPositions =
            particles->expectMutableProperty(Particles::PositionProperty);
        size_t writtenParticleCount = 0;
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            if(selectedOnly && !selection[particleIndex])
                continue;
            outputPositions[particleIndex] = pointFromJson(denoisedPositions[static_cast<int>(particleIndex)]);
            writtenParticleCount++;
        }
        outputPositions.reset();

        const QJsonArray convergenceArray = output.value(QStringLiteral("convergence")).toArray();
        std::vector<double> convergence;
        convergence.reserve(convergenceArray.size());
        std::vector<double> logConvergence;
        logConvergence.reserve(convergenceArray.size());
        for(const QJsonValue& value : convergenceArray) {
            const double convergenceValue = value.toDouble();
            convergence.push_back(convergenceValue);
            logConvergence.push_back(convergenceValue > 0.0 ? std::log10(convergenceValue)
                                                            : -std::numeric_limits<double>::infinity());
        }

        createConvergenceTable(state,
                               ConvergenceTableIdentifier,
                               tr("Score-based denoising convergence"),
                               tr("Convergence"),
                               convergence,
                               createdByNode);
        createConvergenceTable(state,
                               LogConvergenceTableIdentifier,
                               tr("Score-based denoising log convergence"),
                               tr("Log10(Convergence)"),
                               logConvergence,
                               createdByNode);

        const QString resolvedModelPath = output.value(QStringLiteral("model_path")).toString(explicitModelPath.trimmed());
        state.setAttribute(QStringLiteral("ScoreDenoising.structure"), presetName(structure), createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.steps"), denoisingSteps, createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.nearest_neighbor_distance"), scaleDistance, createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.model_scale"), modelScale, createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.device"), deviceName(computeDevice), createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.model_path"), resolvedModelPath, createdByNode);
        state.setAttribute(QStringLiteral("ScoreDenoising.written_particle_count"),
                           QVariant::fromValue(static_cast<qlonglong>(writtenParticleCount)),
                           createdByNode);
        if(!convergence.empty())
            state.setAttribute(QStringLiteral("ScoreDenoising.final_convergence"), convergence.back(), createdByNode);

        state.combineStatus(PipelineStatus::Success,
                            tr("Score-based denoising complete: %1 positions updated; structure %2; steps %3; model scale %4.")
                                .arg(writtenParticleCount)
                                .arg(presetName(structure))
                                .arg(denoisingSteps)
                                .arg(modelScale, 0, 'g', 6));
        return std::move(state);
    });
}

}  // namespace Ovito
