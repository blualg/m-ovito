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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "TransportModifier.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <unordered_map>

namespace Ovito {

namespace {

constexpr double BoltzmannConstant = 1.380649e-23;

struct GroupInfo {
    int typeId = 0;
    QString name;
    std::vector<size_t> memberIndices;
    double representativeChargeRaw = 0.0;
};

struct FrameSample {
    double timeRaw = 0.0;
    double timeSI = 0.0;
    double volumeRaw = 0.0;
    double volumeSI = 0.0;
    std::vector<Point3> positions;
    std::vector<Vector3> velocities;
};

struct PreparedData {
    std::vector<FrameSample> frames;
    std::vector<GroupInfo> groups;
    std::vector<double> chargesRaw;
    bool hasPerTypeGroups = false;
    bool usedUnitCharges = false;
    bool usedUnitMasses = false;
    bool usedStoredVelocities = false;
    bool usedFiniteDifferenceVelocities = false;
    bool velocityAnalysisAvailable = false;
    bool conductivityVelocityAnalysisAvailable = false;
    bool exactPyLATMode = false;
    bool usedFallbackMoleculeIds = false;
    bool usedFallbackMoleculeTypes = false;
    bool selectionPromotedToWholeMolecules = false;
    bool selectedAtomsAnalyzedDirectlyInPyLATMode = false;
    int dimensionality = 3;
    double averageVolumeRaw = 0.0;
    double averageVolumeSI = 0.0;
    double fixedVolumeRaw = 0.0;
    double fixedVolumeSI = 0.0;
    Vector3 fixedCellLengthsRaw = Vector3::Zero();
    Vector3 fixedHalfCellLengthsRaw = Vector3::Zero();
    bool manualMoleculeDefinitionsApplied = false;
    bool manualTypeChargesApplied = false;
    bool manualTypeChargesPartiallyApplied = false;
};

struct TransportComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

std::vector<std::vector<int>> buildFrameBatches(const std::vector<int>& frames, size_t batchSize)
{
    OVITO_ASSERT(batchSize > 0);

    std::vector<std::vector<int>> batches;
    batches.reserve((frames.size() + batchSize - 1) / batchSize);
    for(size_t begin = 0; begin < frames.size(); begin += batchSize) {
        const size_t end = std::min(begin + batchSize, frames.size());
        batches.emplace_back(frames.begin() + static_cast<ptrdiff_t>(begin), frames.begin() + static_cast<ptrdiff_t>(end));
    }
    return batches;
}

struct ManualMoleculeTemplate {
    QString name;
    std::map<int, int> composition;
    size_t atomCount = 0;
};

struct MoleculeInfo {
    IdentifierIntType id = 0;
    std::vector<size_t> referenceAtomIndices;
    double totalMassRaw = 0.0;
    double chargeRaw = 0.0;
    QString groupName;
};

QHash<QString, int> buildParticleTypeNameMap(const Property* typeProperty, const BufferReadAccess<int32_t>& typeValues)
{
    QHash<QString, int> result;
    if(!typeProperty || !typeValues)
        return result;

    std::map<int, bool> seenTypes;
    for(int32_t typeId : typeValues) {
        if(seenTypes.contains(typeId))
            continue;
        seenTypes[typeId] = true;

        result.insert(QString::number(typeId).trimmed().toLower(), typeId);
        if(const ElementType* elementType = typeProperty->elementType(typeId); elementType && !elementType->name().trimmed().isEmpty())
            result.insert(elementType->name().trimmed().toLower(), typeId);
    }
    return result;
}

int resolveParticleTypeToken(const QString& token,
                             const QHash<QString, int>& particleTypeNameMap,
                             const QString& contextDescription)
{
    const QString trimmedToken = token.trimmed();
    if(trimmedToken.isEmpty())
        throw Exception(TransportModifier::tr("Encountered an empty particle-type token while parsing %1.").arg(contextDescription));

    bool ok = false;
    const int numericTypeId = trimmedToken.toInt(&ok);
    if(ok)
        return numericTypeId;

    const QString normalized = trimmedToken.toLower();
    auto mapIter = particleTypeNameMap.constFind(normalized);
    if(mapIter != particleTypeNameMap.constEnd())
        return mapIter.value();

    throw Exception(TransportModifier::tr("Unknown particle type token '%1' while parsing %2. Use a particle-type ID or an existing particle-type name.")
                        .arg(trimmedToken, contextDescription));
}

QString stripCommentAndTrim(const QString& line)
{
    const int commentIndex = line.indexOf(QLatin1Char('#'));
    return (commentIndex >= 0 ? line.left(commentIndex) : line).trimmed();
}

std::vector<ManualMoleculeTemplate> parseManualMoleculeTemplates(const QString& overrideText,
                                                                 const Property* typeProperty,
                                                                 const BufferReadAccess<int32_t>& typeValues)
{
    if(!typeProperty || !typeValues)
        throw Exception(TransportModifier::tr("Manual molecule definitions require a Particle Type property."));

    const QHash<QString, int> particleTypeNameMap = buildParticleTypeNameMap(typeProperty, typeValues);
    const QRegularExpression entryPattern(QStringLiteral("^(.+?)\\s*(?:[:xX\\*])\\s*(\\d+)\\s*$"));

    std::vector<ManualMoleculeTemplate> templates;
    const QStringList lines = overrideText.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);
    for(const QString& rawLine : lines) {
        const QString line = stripCommentAndTrim(rawLine);
        if(line.isEmpty())
            continue;

        const int separatorIndex = line.indexOf(QLatin1Char('='));
        if(separatorIndex <= 0 || separatorIndex >= line.size() - 1) {
            throw Exception(TransportModifier::tr(
                "Invalid manual molecule-definition line '%1'. Use the form 'Name = Type:Count, Type:Count'.")
                                .arg(line));
        }

        ManualMoleculeTemplate templ;
        templ.name = line.left(separatorIndex).trimmed();
        if(templ.name.isEmpty())
            throw Exception(TransportModifier::tr("Manual molecule-definition line '%1' is missing the molecule name.").arg(line));

        const QString rhs = line.mid(separatorIndex + 1).trimmed();
        const QStringList entries = rhs.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if(entries.isEmpty())
            throw Exception(TransportModifier::tr("Manual molecule-definition line '%1' does not list any particle-type counts.").arg(line));

        for(const QString& rawEntry : entries) {
            const QString entry = rawEntry.trimmed();
            const QRegularExpressionMatch match = entryPattern.match(entry);
            if(!match.hasMatch()) {
                throw Exception(TransportModifier::tr(
                    "Invalid molecule entry '%1' in line '%2'. Use 'Type:Count', 'TypexCount', or 'Type*Count'.")
                                    .arg(entry, line));
            }

            const int typeId = resolveParticleTypeToken(match.captured(1), particleTypeNameMap, templ.name);
            const int count = match.captured(2).toInt();
            if(count <= 0)
                throw Exception(TransportModifier::tr("The entry '%1' in line '%2' uses a non-positive atom count.").arg(entry, line));

            templ.composition[typeId] += count;
            templ.atomCount += static_cast<size_t>(count);
        }

        if(templ.atomCount == 0)
            throw Exception(TransportModifier::tr("Manual molecule definition '%1' does not contain any atoms.").arg(templ.name));

        templates.push_back(std::move(templ));
    }

    if(templates.empty())
        throw Exception(TransportModifier::tr("Manual molecule definitions are enabled, but no valid molecule templates were provided."));

    return templates;
}

QHash<int, double> parseManualTypeChargeOverrides(const QString& overrideText,
                                                  const Property* typeProperty,
                                                  const BufferReadAccess<int32_t>& typeValues)
{
    if(!typeProperty || !typeValues)
        throw Exception(TransportModifier::tr("Manual type-charge overrides require a Particle Type property."));

    const QHash<QString, int> particleTypeNameMap = buildParticleTypeNameMap(typeProperty, typeValues);
    QHash<int, double> overrides;

    const QStringList lines = overrideText.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);
    for(const QString& rawLine : lines) {
        const QString line = stripCommentAndTrim(rawLine);
        if(line.isEmpty())
            continue;

        const int separatorIndex = line.indexOf(QLatin1Char('='));
        if(separatorIndex <= 0 || separatorIndex >= line.size() - 1) {
            throw Exception(TransportModifier::tr(
                "Invalid manual type-charge line '%1'. Use the form 'Type = Charge'.")
                                .arg(line));
        }

        const int typeId = resolveParticleTypeToken(line.left(separatorIndex), particleTypeNameMap, TransportModifier::tr("manual type-charge overrides"));
        bool ok = false;
        const double charge = line.mid(separatorIndex + 1).trimmed().toDouble(&ok);
        if(!ok)
            throw Exception(TransportModifier::tr("Could not parse the charge value in manual type-charge line '%1'.").arg(line));

        overrides.insert(typeId, charge);
    }

    if(overrides.isEmpty())
        throw Exception(TransportModifier::tr("Manual type-charge overrides are enabled, but no valid override lines were provided."));

    return overrides;
}

std::vector<MoleculeInfo> buildMoleculesFromManualTemplates(const std::vector<int32_t>& referenceParticleTypes,
                                                            const std::vector<ManualMoleculeTemplate>& templates)
{
    std::vector<MoleculeInfo> molecules;
    size_t atomIndex = 0;
    IdentifierIntType nextMoleculeId = 1;

    while(atomIndex < referenceParticleTypes.size()) {
        const ManualMoleculeTemplate* matchedTemplate = nullptr;
        for(const ManualMoleculeTemplate& templ : templates) {
            if(atomIndex + templ.atomCount > referenceParticleTypes.size())
                continue;

            std::map<int, int> windowComposition;
            for(size_t offset = 0; offset < templ.atomCount; ++offset)
                windowComposition[referenceParticleTypes[atomIndex + offset]]++;

            if(windowComposition == templ.composition) {
                matchedTemplate = &templ;
                break;
            }
        }

        if(!matchedTemplate) {
            throw Exception(TransportModifier::tr(
                "Manual molecule definitions could not match the particle-type pattern starting at atom %1. "
                "The manual override assumes the atoms belonging to each molecule appear contiguously in the reference-frame ordering.")
                                .arg(atomIndex + 1));
        }

        MoleculeInfo molecule;
        molecule.id = nextMoleculeId++;
        molecule.groupName = matchedTemplate->name;
        molecule.referenceAtomIndices.reserve(matchedTemplate->atomCount);
        for(size_t offset = 0; offset < matchedTemplate->atomCount; ++offset)
            molecule.referenceAtomIndices.push_back(atomIndex + offset);

        molecules.push_back(std::move(molecule));
        atomIndex += matchedTemplate->atomCount;
    }

    return molecules;
}

template<typename T>
std::vector<T> trimVector(const std::vector<T>& input, size_t newSize)
{
    return std::vector<T>(input.begin(), input.begin() + std::min(input.size(), newSize));
}

size_t effectiveSizeFromCounts(const std::vector<size_t>& counts)
{
    size_t size = counts.size();
    while(size > 1 && counts[size - 1] == 0)
        --size;
    return size;
}

double timeUnitToSeconds(TransportModifier::TimeUnit unit)
{
    switch(unit) {
    case TransportModifier::Femtoseconds: return 1e-15;
    case TransportModifier::Picoseconds: return 1e-12;
    case TransportModifier::Nanoseconds: return 1e-9;
    }
    return 1e-12;
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

double lengthUnitToMeters(TransportModifier::LengthUnit unit)
{
    switch(unit) {
    case TransportModifier::Angstroms: return 1e-10;
    case TransportModifier::Nanometers: return 1e-9;
    case TransportModifier::Meters: return 1.0;
    }
    return 1e-10;
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

double chargeUnitToCoulombs(TransportModifier::ChargeUnit unit)
{
    switch(unit) {
    case TransportModifier::ElementaryCharges: return 1.602176634e-19;
    case TransportModifier::Coulombs: return 1.0;
    }
    return 1.602176634e-19;
}

QString chargeUnitLabel(TransportModifier::ChargeUnit unit)
{
    switch(unit) {
    case TransportModifier::ElementaryCharges: return QStringLiteral("e");
    case TransportModifier::Coulombs: return QStringLiteral("C");
    }
    return QStringLiteral("e");
}

QString conductivityUnitLabel(int dimensionality)
{
    return (dimensionality == 2) ? QStringLiteral("S") : QStringLiteral("S/m");
}

QString conductivityRawUnitLabel(const QString& chargeUnit, const QString& timeUnit, const QString& lengthUnit, int dimensionality)
{
    if(dimensionality == 2)
        return QStringLiteral("%1^2/(J*%2)").arg(chargeUnit, timeUnit);
    return QStringLiteral("%1^2/(J*%2*%3)").arg(chargeUnit, timeUnit, lengthUnit);
}

QString msdUnitLabel(const QString& lengthUnit)
{
    return QStringLiteral("%1^2").arg(lengthUnit);
}

QString vacfUnitLabel(const QString& lengthUnit, const QString& timeUnit)
{
    return QStringLiteral("%1^2/%2^2").arg(lengthUnit, timeUnit);
}

QString diffusionUnitLabel(const QString& lengthUnit, const QString& timeUnit)
{
    return QStringLiteral("%1^2/%2").arg(lengthUnit, timeUnit);
}

QString currentCorrelationUnitLabel(const QString& chargeUnit, const QString& lengthUnit, const QString& timeUnit)
{
    return QStringLiteral("%1^2*%2^2/%3^2").arg(chargeUnit, lengthUnit, timeUnit);
}

std::vector<FloatType> readScalarProperty(const Property* property)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(property->componentCount() == 1);
    std::vector<FloatType> values(property->size());
    property->copyComponentTo(values.begin(), 0);
    return values;
}

template<typename ValueType>
std::unordered_map<ValueType, size_t> buildIndexMap(const BufferReadAccess<ValueType>& ids, const QString& what)
{
    std::unordered_map<ValueType, size_t> result;
    result.reserve(ids.size());
    size_t index = 0;
    for(const ValueType id : ids) {
        if(!result.insert({id, index}).second)
            throw Exception(TransportModifier::tr("Detected duplicate %1 ID %2 while preparing the transport analysis.").arg(what).arg(id));
        ++index;
    }
    return result;
}

DataTable* createLineTable(DataCollection* collection,
                           const QStringView identifier,
                           const QString& title,
                           const std::vector<double>& xValues,
                           const std::vector<std::vector<double>>& columns,
                           QStringList componentNames,
                           const QString& axisLabelX,
                           const QString& axisLabelY,
                           const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(columns.empty() || xValues.empty())
        return nullptr;

    const size_t rowCount = xValues.size();
    const int componentCount = static_cast<int>(columns.size());
    OVITO_ASSERT(std::ranges::all_of(columns, [rowCount](const std::vector<double>& c) { return c.size() == rowCount; }));
    if(componentNames.size() != componentCount)
        componentNames.clear();

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            componentCount,
                                                            axisLabelY,
                                                            0,
                                                            std::move(componentNames));
    BufferWriteAccess<FloatType*, access_mode::discard_write> yAcc(y);
    for(size_t i = 0; i < rowCount; ++i) {
        for(int c = 0; c < componentCount; ++c)
            yAcc.set(i, c, static_cast<FloatType>(columns[c][i]));
    }

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Time"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t i = 0; i < rowCount; ++i)
        xAcc[i] = static_cast<FloatType>(xValues[i]);

    DataTable* table = collection->createObject<DataTable>(identifier.toString(),
                                                           createdByNode,
                                                           DataTable::Line,
                                                           title,
                                                           std::move(y),
                                                           std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

void setSummaryAttribute(DataCollection* collection, const QStringView key, double value, const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(std::isfinite(value))
        collection->setAttribute(key, value, createdByNode);
}

double averageWindowValue(const std::vector<double>& values, const std::vector<double>& times, int firstLag, int lastLag)
{
    if(values.empty() || values.size() != times.size())
        return std::numeric_limits<double>::quiet_NaN();

    const int begin = std::clamp(firstLag, 1, static_cast<int>(values.size()) - 1);
    const int end = std::clamp((lastLag > 0 ? lastLag : static_cast<int>(values.size()) - 1), begin, static_cast<int>(values.size()) - 1);

    double sum = 0.0;
    int count = 0;
    for(int i = begin; i <= end; ++i) {
        if(times[i] > 0 && std::isfinite(values[i])) {
            sum += values[i];
            ++count;
        }
    }
    return (count > 0) ? (sum / count) : std::numeric_limits<double>::quiet_NaN();
}

std::vector<double> cumulativeTrapezoid(const std::vector<double>& x, const std::vector<double>& y)
{
    std::vector<double> result(y.size(), 0.0);
    for(size_t i = 1; i < y.size(); ++i) {
        const double dx = x[i] - x[i - 1];
        result[i] = result[i - 1] + ((dx > 0) ? (0.5 * (y[i] + y[i - 1]) * dx) : 0.0);
    }
    return result;
}

struct PyLATDiffusivityFit {
    int fitStartLag = 1;
    double diffusionRaw = std::numeric_limits<double>::quiet_NaN();
    double diffusionSI = std::numeric_limits<double>::quiet_NaN();
};

struct PyLATConvergenceWindow {
    int beginLag = 0;
    int endLag = 0;
};

struct PyLATMSDCurves {
    std::vector<double> timesRaw;
    std::vector<double> timesSI;
    std::vector<double> overall;
    std::vector<std::vector<double>> perType;
};

struct PyLATChargeTransportCurves {
    std::vector<double> timesRaw;
    std::vector<double> timesSI;
    std::vector<double> correlatedChargeDisplacement;
    std::vector<double> nernstEinsteinNumerator;
};

struct PyLATGreenKuboCurves {
    std::vector<double> timesRaw;
    std::vector<double> timesSI;
    std::vector<double> totalCurrentCorrelationRaw;
    std::vector<std::vector<double>> perTypeCurrentCorrelationRaw;
    std::vector<double> totalConductivityRaw;
    std::vector<double> totalConductivitySI;
    std::vector<std::vector<double>> perTypeConductivityRaw;
    std::vector<std::vector<double>> perTypeConductivitySI;
};

double linearRegressionSlope(const std::vector<double>& x, const std::vector<double>& y, int firstIndex)
{
    if(x.size() != y.size() || x.empty())
        return std::numeric_limits<double>::quiet_NaN();

    const int start = std::clamp(firstIndex, 0, static_cast<int>(x.size()) - 1);
    const size_t count = x.size() - static_cast<size_t>(start);
    if(count < 2)
        return std::numeric_limits<double>::quiet_NaN();

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    for(size_t i = static_cast<size_t>(start); i < x.size(); ++i) {
        sumX += x[i];
        sumY += y[i];
        sumXX += x[i] * x[i];
        sumXY += x[i] * y[i];
    }

    const double denominator = static_cast<double>(count) * sumXX - sumX * sumX;
    if(denominator == 0)
        return std::numeric_limits<double>::quiet_NaN();

    return (static_cast<double>(count) * sumXY - sumX * sumY) / denominator;
}

int findPyLATLinearRegionStart(const std::vector<double>& values, const std::vector<double>& times, double deltaTInSelectedUnits, double tolerance)
{
    if(values.size() != times.size() || values.size() < 3)
        return 1;

    std::vector<double> logValues;
    std::vector<double> logTimes;
    logValues.reserve(values.size() - 1);
    logTimes.reserve(times.size() - 1);
    for(size_t i = 1; i < values.size(); ++i) {
        if(values[i] <= 0 || times[i] <= 0)
            return 1;
        logValues.push_back(std::log(values[i]));
        logTimes.push_back(std::log(times[i]));
    }

    const int maxTime = static_cast<int>(logValues.size());
    const int timestepSkip = std::max(1, static_cast<int>(std::ceil(500.0 / std::max(deltaTInSelectedUnits, 1e-12))));
    int numSkip = 1;
    while(true) {
        if(numSkip * timestepSkip + 1 > maxTime) {
            return std::clamp(maxTime - 1 - (numSkip - 1) * timestepSkip, 1, static_cast<int>(values.size()) - 1);
        }

        const int t1 = maxTime - 1 - (numSkip - 1) * timestepSkip;
        const int t2 = maxTime - 1 - numSkip * timestepSkip;
        if(t1 < 0 || t2 < 0 || t1 >= maxTime || t2 >= maxTime)
            return 1;

        const double denominator = logTimes[t1] - logTimes[t2];
        if(denominator == 0)
            return 1;

        const double slope = (logValues[t1] - logValues[t2]) / denominator;
        if(std::abs(slope - 1.0) < tolerance)
            ++numSkip;
        else
            return std::clamp(t1, 1, static_cast<int>(values.size()) - 1);
    }
}

PyLATDiffusivityFit fitPyLATDiffusivity(const std::vector<double>& timesRaw,
                                        const std::vector<double>& msdRaw,
                                        int dimensionality,
                                        double deltaTInSelectedUnits,
                                        double timeScaleToSI,
                                        double lengthScaleToSI,
                                        double tolerance)
{
    PyLATDiffusivityFit fit;
    fit.fitStartLag = findPyLATLinearRegionStart(msdRaw, timesRaw, deltaTInSelectedUnits, tolerance);
    const double slopeRaw = linearRegressionSlope(timesRaw, msdRaw, fit.fitStartLag);
    if(std::isfinite(slopeRaw)) {
        fit.diffusionRaw = slopeRaw / (2.0 * dimensionality);
        fit.diffusionSI = fit.diffusionRaw * lengthScaleToSI * lengthScaleToSI / timeScaleToSI;
    }
    return fit;
}

PyLATConvergenceWindow findPyLATConvergenceWindow(const std::vector<double>& signal, double tolerance)
{
    PyLATConvergenceWindow window;
    if(signal.empty()) {
        window.beginLag = 0;
        window.endLag = 0;
        return window;
    }

    const int signalSize = static_cast<int>(signal.size());
    const double reference = signal.front() * signal.front();
    bool converged = false;
    int i = 3;
    int begin = std::min(std::max(1, i), signalSize - 1);
    int end = signalSize - 1;

    while(!converged) {
        if(i >= signalSize) {
            begin = i - 1;
            converged = true;
        }
        else {
            const double test = signal[i - 3] * signal[i - 3] + signal[i - 2] * signal[i - 2] +
                                signal[i - 1] * signal[i - 1] + signal[i] * signal[i];
            if(test < tolerance * reference) {
                begin = i;
                ++i;
                converged = true;
            }
            else {
                ++i;
            }
        }
    }

    while(converged) {
        if(i >= signalSize) {
            end = i - 1;
            converged = false;
        }
        else {
            const double test = signal[i - 3] * signal[i - 3] + signal[i - 2] * signal[i - 2] +
                                signal[i - 1] * signal[i - 1] + signal[i] * signal[i];
            if(test > tolerance * reference) {
                end = i;
                converged = false;
            }
            else {
                ++i;
            }
        }
    }

    window.beginLag = std::clamp(begin, 1, signalSize - 1);
    window.endLag = std::clamp(end, window.beginLag + 1, signalSize);
    return window;
}

double averagePyLATRange(const std::vector<double>& values, int beginLag, int endLag)
{
    if(values.empty())
        return std::numeric_limits<double>::quiet_NaN();

    const int begin = std::clamp(beginLag, 0, static_cast<int>(values.size()) - 1);
    const int end = std::clamp(endLag, begin + 1, static_cast<int>(values.size()));
    double sum = 0.0;
    int count = 0;
    for(int i = begin; i < end; ++i) {
        if(std::isfinite(values[i])) {
            sum += values[i];
            ++count;
        }
    }
    return (count > 0) ? (sum / count) : std::numeric_limits<double>::quiet_NaN();
}

double lastFiniteValue(const std::vector<double>& values)
{
    for(auto iter = values.rbegin(); iter != values.rend(); ++iter) {
        if(std::isfinite(*iter))
            return *iter;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

PyLATMSDCurves computePyLATMSDCurves(const PreparedData& prepared)
{
    PyLATMSDCurves curves;
    const size_t frameCount = prepared.frames.size();
    if(frameCount < 2)
        return curves;

    const size_t numInit = std::max<size_t>(1, frameCount / 2);
    const size_t lenMSD = frameCount - numInit;
    if(lenMSD == 0)
        return curves;

    curves.timesRaw.resize(lenMSD, 0.0);
    curves.timesSI.resize(lenMSD, 0.0);
    curves.overall.assign(lenMSD, 0.0);
    curves.perType.assign(prepared.groups.size(), std::vector<double>(lenMSD, 0.0));

    for(size_t lag = 0; lag < lenMSD; ++lag) {
        curves.timesRaw[lag] = prepared.frames[lag].timeRaw - prepared.frames.front().timeRaw;
        curves.timesSI[lag] = prepared.frames[lag].timeSI - prepared.frames.front().timeSI;
    }

    const size_t particleCount = prepared.frames.front().positions.size();
    struct MSDPartial {
        std::vector<double> overall;
        std::vector<std::vector<double>> perType;
    };
    std::vector<MSDPartial> partials;
    parallelForChunks(numInit, 32,
        [&](size_t workerCount) {
            partials.resize(workerCount);
            for(MSDPartial& partial : partials) {
                partial.overall.assign(lenMSD, 0.0);
                partial.perType.assign(prepared.groups.size(), std::vector<double>(lenMSD, 0.0));
            }
        },
        [&](size_t workerIndex, size_t fromOrigin, size_t toOrigin) {
            MSDPartial& partial = partials[workerIndex];
            for(size_t origin = fromOrigin; origin < toOrigin; ++origin) {
                for(size_t lag = 0; lag < lenMSD; ++lag) {
                    const size_t target = origin + lag;
                    double totalSquaredDisplacement = 0.0;

                    for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                        const Vector3 displacement = prepared.frames[target].positions[particleIndex] - prepared.frames[origin].positions[particleIndex];
                        totalSquaredDisplacement += displacement.squaredLength();
                    }

                    partial.overall[lag] += totalSquaredDisplacement;
                    for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                        double groupedSquaredDisplacement = 0.0;
                        for(size_t particleIndex : prepared.groups[groupIndex].memberIndices) {
                            const Vector3 displacement = prepared.frames[target].positions[particleIndex] - prepared.frames[origin].positions[particleIndex];
                            groupedSquaredDisplacement += displacement.squaredLength();
                        }
                        partial.perType[groupIndex][lag] += groupedSquaredDisplacement;
                    }
                }
            }
        });

    for(const MSDPartial& partial : partials) {
        for(size_t lag = 0; lag < lenMSD; ++lag)
            curves.overall[lag] += partial.overall[lag];
        for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
            for(size_t lag = 0; lag < lenMSD; ++lag)
                curves.perType[groupIndex][lag] += partial.perType[groupIndex][lag];
        }
    }

    const double normalizationOverall = static_cast<double>(numInit * particleCount);
    for(double& value : curves.overall)
        value /= normalizationOverall;

    for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
        const size_t groupSize = prepared.groups[groupIndex].memberIndices.size();
        if(groupSize == 0)
            continue;
        const double normalization = static_cast<double>(numInit * groupSize);
        for(double& value : curves.perType[groupIndex])
            value /= normalization;
    }

    return curves;
}

PyLATChargeTransportCurves computePyLATChargeTransportCurves(const PreparedData& prepared)
{
    PyLATChargeTransportCurves curves;
    const size_t frameCount = prepared.frames.size();
    if(frameCount < 2)
        return curves;

    const size_t numInit = std::max<size_t>(1, frameCount / 2);
    const size_t lenMSD = frameCount - numInit;
    if(lenMSD == 0)
        return curves;

    curves.timesRaw.resize(lenMSD, 0.0);
    curves.timesSI.resize(lenMSD, 0.0);
    curves.correlatedChargeDisplacement.assign(lenMSD, 0.0);
    curves.nernstEinsteinNumerator.assign(lenMSD, 0.0);

    for(size_t lag = 0; lag < lenMSD; ++lag) {
        curves.timesRaw[lag] = prepared.frames[lag].timeRaw - prepared.frames.front().timeRaw;
        curves.timesSI[lag] = prepared.frames[lag].timeSI - prepared.frames.front().timeSI;
    }

    const size_t particleCount = prepared.frames.front().positions.size();
    struct ChargePartial {
        std::vector<double> correlated;
        std::vector<double> nernstEinstein;
    };
    std::vector<ChargePartial> partials;
    parallelForChunks(numInit, 32,
        [&](size_t workerCount) {
            partials.resize(workerCount);
            for(ChargePartial& partial : partials) {
                partial.correlated.assign(lenMSD, 0.0);
                partial.nernstEinstein.assign(lenMSD, 0.0);
            }
        },
        [&](size_t workerIndex, size_t fromOrigin, size_t toOrigin) {
            ChargePartial& partial = partials[workerIndex];
            for(size_t origin = fromOrigin; origin < toOrigin; ++origin) {
                for(size_t lag = 0; lag < lenMSD; ++lag) {
                    const size_t target = origin + lag;
                    Vector3 collectiveChargeDisplacement = Vector3::Zero();
                    double chargeWeightedSelfDisplacement = 0.0;

                    for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                        const Vector3 displacement = prepared.frames[target].positions[particleIndex] - prepared.frames[origin].positions[particleIndex];
                        const double charge = prepared.chargesRaw[particleIndex];
                        collectiveChargeDisplacement += displacement * static_cast<FloatType>(charge);
                        chargeWeightedSelfDisplacement += charge * charge * displacement.squaredLength();
                    }

                    partial.correlated[lag] += collectiveChargeDisplacement.squaredLength();
                    partial.nernstEinstein[lag] += chargeWeightedSelfDisplacement;
                }
            }
        });

    for(const ChargePartial& partial : partials) {
        for(size_t lag = 0; lag < lenMSD; ++lag) {
            curves.correlatedChargeDisplacement[lag] += partial.correlated[lag];
            curves.nernstEinsteinNumerator[lag] += partial.nernstEinstein[lag];
        }
    }

    const double normalization = static_cast<double>(numInit);
    for(double& value : curves.correlatedChargeDisplacement)
        value /= normalization;
    for(double& value : curves.nernstEinsteinNumerator)
        value /= normalization;

    return curves;
}

std::vector<double> directCorrelation(const std::vector<double>& a, const std::vector<double>& b)
{
    OVITO_ASSERT(a.size() == b.size());
    std::vector<double> result(a.size(), 0.0);
    parallelForChunks(a.size(), 32, [&](size_t, size_t fromLag, size_t toLag) {
        for(size_t lag = fromLag; lag < toLag; ++lag) {
            double sum = 0.0;
            size_t count = 0;
            for(size_t index = 0; index + lag < a.size(); ++index) {
                sum += a[index + lag] * b[index];
                ++count;
            }
            if(count > 0)
                result[lag] = sum / static_cast<double>(count);
        }
    });
    return result;
}

PyLATGreenKuboCurves computePyLATGreenKuboCurves(const PreparedData& prepared,
                                                 double timeScaleToSI,
                                                 double lengthScaleToSI,
                                                 double chargeScaleToSI,
                                                 double conductivityScaleToSI,
                                                 double temperature)
{
    PyLATGreenKuboCurves curves;
    if(!prepared.conductivityVelocityAnalysisAvailable || prepared.frames.empty() || prepared.fixedVolumeSI <= 0)
        return curves;

    const size_t frameCount = prepared.frames.size();
    const size_t entityCount = prepared.frames.front().velocities.size();
    if(frameCount == 0 || entityCount == 0)
        return curves;

    std::vector<double> totalJx(frameCount, 0.0);
    std::vector<double> totalJy(frameCount, 0.0);
    std::vector<double> totalJz(frameCount, 0.0);

    for(size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        for(size_t entityIndex = 0; entityIndex < entityCount; ++entityIndex) {
            const Vector3& velocity = prepared.frames[frameIndex].velocities[entityIndex];
            const double charge = prepared.chargesRaw[entityIndex];
            totalJx[frameIndex] += charge * velocity.x();
            totalJy[frameIndex] += charge * velocity.y();
            totalJz[frameIndex] += charge * velocity.z();
        }
    }

    const std::vector<double> corrX = directCorrelation(totalJx, totalJx);
    const std::vector<double> corrY = directCorrelation(totalJy, totalJy);
    const std::vector<double> corrZ = directCorrelation(totalJz, totalJz);
    curves.totalCurrentCorrelationRaw.resize(frameCount, 0.0);
    for(size_t lag = 0; lag < frameCount; ++lag)
        curves.totalCurrentCorrelationRaw[lag] = corrX[lag] + corrY[lag] + corrZ[lag];

    curves.timesRaw.resize(frameCount, 0.0);
    curves.timesSI.resize(frameCount, 0.0);
    const double lagStepRaw = (frameCount > 1) ? (prepared.frames[1].timeRaw - prepared.frames[0].timeRaw) : 0.0;
    const double lagStepSI = lagStepRaw * timeScaleToSI;
    for(size_t lag = 0; lag < frameCount; ++lag) {
        curves.timesRaw[lag] = static_cast<double>(lag) * lagStepRaw;
        curves.timesSI[lag] = static_cast<double>(lag) * lagStepSI;
    }

    const double conductivityFactorSI = chargeScaleToSI * chargeScaleToSI * lengthScaleToSI * lengthScaleToSI /
                                        (BoltzmannConstant * temperature * prepared.fixedVolumeSI *
                                         static_cast<double>(prepared.dimensionality) * timeScaleToSI);

    const std::vector<double> totalIntegralRaw = cumulativeTrapezoid(curves.timesRaw, curves.totalCurrentCorrelationRaw);
    curves.totalConductivitySI.resize(totalIntegralRaw.size(), 0.0);
    curves.totalConductivityRaw.resize(totalIntegralRaw.size(), 0.0);
    for(size_t index = 0; index < totalIntegralRaw.size(); ++index) {
        curves.totalConductivitySI[index] = totalIntegralRaw[index] * conductivityFactorSI;
        if(conductivityScaleToSI > 0)
            curves.totalConductivityRaw[index] = curves.totalConductivitySI[index] / conductivityScaleToSI;
    }

    return curves;
}

PreparedData prepareTransportData(const std::vector<PipelineFlowState>& sampleStates,
                                 bool useOnlySelectedParticles,
                                 bool selectAsMolecules,
                                 bool computePerType,
                                 bool needVelocityAnalysis,
                                 bool allowFiniteDifferenceVelocities,
                                 bool exactPyLATMode,
                                 bool useManualMoleculeDefinitions,
                                 const QString& manualMoleculeDefinitions,
                                 bool useManualTypeCharges,
                                 const QString& manualTypeCharges,
                                  double deltaTInSelectedUnits,
                                  double timeScaleToSI,
                                  double lengthScaleToSI,
                                  QString& warningText)
{
    if(sampleStates.size() < 2)
        throw Exception(TransportModifier::tr("Transport analysis requires at least two sampled trajectory frames."));

    const Particles* referenceParticles = sampleStates.front().expectObject<Particles>();
    referenceParticles->verifyIntegrity();
    const Property* referencePositionProperty = referenceParticles->expectProperty(Particles::PositionProperty);
    const Property* referenceIdProperty = referenceParticles->getProperty(Particles::IdentifierProperty);
    const Property* referenceSelectionProperty = useOnlySelectedParticles ? referenceParticles->getProperty(Particles::SelectionProperty) : nullptr;
    const Property* referenceTypeProperty = referenceParticles->getProperty(Particles::TypeProperty);
    const Property* referenceChargeProperty = referenceParticles->getProperty(Particles::ChargeProperty);
    const Property* referenceMoleculeProperty = referenceParticles->getProperty(Particles::MoleculeProperty);
    const Property* referenceMoleculeTypeProperty = referenceParticles->getProperty(Particles::MoleculeTypeProperty);
    const SimulationCell* referenceCell = sampleStates.front().getObject<SimulationCell>();
    const ConstPropertyPtr referenceMassProperty = referenceParticles->inputParticleMasses();

    BufferReadAccess<IdentifierIntType> referenceIds(referenceIdProperty);

    const bool useIdentifiers = static_cast<bool>(referenceIds);
    if(useOnlySelectedParticles && !referenceSelectionProperty)
        throw Exception(TransportModifier::tr("The 'Use only selected particles' option requires a Selection property in every sampled frame."));

    const size_t referenceCount = referencePositionProperty->size();
    std::vector<std::vector<size_t>> mappedIndicesByReference(referenceCount, std::vector<size_t>(sampleStates.size(), 0));
    std::vector<bool> atomPresentInAllFrames(referenceCount, true);
    std::vector<bool> atomSelectedInAllFrames(referenceCount, true);

    if(useIdentifiers) {
        std::vector<std::unordered_map<IdentifierIntType, size_t>> idMaps;
        idMaps.reserve(sampleStates.size());
        for(const PipelineFlowState& sampleState : sampleStates) {
            const Particles* particles = sampleState.getObject<Particles>();
            if(!particles)
                throw Exception(TransportModifier::tr("One of the sampled trajectory frames does not contain any particles."));
            particles->verifyIntegrity();
            BufferReadAccess<IdentifierIntType> ids(particles->getProperty(Particles::IdentifierProperty));
            if(!ids)
                throw Exception(TransportModifier::tr("All sampled frames must provide particle identifiers when transport analysis relies on dynamic particle membership."));
            idMaps.push_back(buildIndexMap<IdentifierIntType>(ids, particles->getOOMetaClass().elementDescriptionName()));
        }

        for(size_t refIndex = 0; refIndex < referenceIds.size(); ++refIndex) {
            const IdentifierIntType id = referenceIds[refIndex];
            for(size_t frameIndex = 0; frameIndex < sampleStates.size(); ++frameIndex) {
                const Particles* particles = sampleStates[frameIndex].getObject<Particles>();
                auto mapIter = idMaps[frameIndex].find(id);
                if(mapIter == idMaps[frameIndex].end()) {
                    atomPresentInAllFrames[refIndex] = false;
                    break;
                }
                mappedIndicesByReference[refIndex][frameIndex] = mapIter->second;
                if(useOnlySelectedParticles) {
                    BufferReadAccess<SelectionIntType> selection(particles->getProperty(Particles::SelectionProperty));
                    atomSelectedInAllFrames[refIndex] =
                        atomSelectedInAllFrames[refIndex] && selection && selection[mappedIndicesByReference[refIndex][frameIndex]];
                }
            }
        }
    }
    else {
        for(const PipelineFlowState& sampleState : sampleStates) {
            const Particles* particles = sampleState.getObject<Particles>();
            if(!particles)
                throw Exception(TransportModifier::tr("One of the sampled trajectory frames does not contain any particles."));
            particles->verifyIntegrity();
            if(particles->expectProperty(Particles::PositionProperty)->size() != referenceCount) {
                throw Exception(TransportModifier::tr(
                    "The transport analysis currently requires a stable particle count if no particle identifiers are available."));
            }
        }

        for(size_t particleIndex = 0; particleIndex < referenceCount; ++particleIndex) {
            for(size_t frameIndex = 0; frameIndex < sampleStates.size(); ++frameIndex)
                mappedIndicesByReference[particleIndex][frameIndex] = particleIndex;
            if(useOnlySelectedParticles) {
                for(const PipelineFlowState& sampleState : sampleStates) {
                    const Particles* particles = sampleState.getObject<Particles>();
                    BufferReadAccess<SelectionIntType> selection(particles->getProperty(Particles::SelectionProperty));
                    atomSelectedInAllFrames[particleIndex] = atomSelectedInAllFrames[particleIndex] && selection && selection[particleIndex];
                }
            }
        }
    }

    PreparedData result;
    result.frames.resize(sampleStates.size());
    result.exactPyLATMode = exactPyLATMode;

    const bool storedVelocitiesAvailable = needVelocityAnalysis && std::ranges::all_of(sampleStates, [](const PipelineFlowState& sampleState) {
        const Particles* particles = sampleState.getObject<Particles>();
        return particles && particles->getProperty(Particles::VelocityProperty);
    });

    result.usedStoredVelocities = storedVelocitiesAvailable;
    result.usedFiniteDifferenceVelocities = needVelocityAnalysis && allowFiniteDifferenceVelocities && !storedVelocitiesAvailable;
    result.velocityAnalysisAvailable = storedVelocitiesAvailable || result.usedFiniteDifferenceVelocities;
    result.conductivityVelocityAnalysisAvailable = !exactPyLATMode && needVelocityAnalysis;
    const bool manualMoleculeOverridesActive = useManualMoleculeDefinitions && !manualMoleculeDefinitions.trimmed().isEmpty();
    const bool manualTypeChargeOverridesActive = useManualTypeCharges && !manualTypeCharges.trimmed().isEmpty();

    BufferReadAccess<int32_t> referenceParticleTypes(referenceTypeProperty);
    std::vector<FloatType> massValues = readScalarProperty(referenceMassProperty);
    std::vector<FloatType> chargeValues(referenceCount, 1.0);
    if(referenceChargeProperty)
        chargeValues = readScalarProperty(referenceChargeProperty);
    else
        result.usedUnitCharges = true;

    if(manualTypeChargeOverridesActive) {
        const QHash<int, double> chargeOverrides = parseManualTypeChargeOverrides(manualTypeCharges, referenceTypeProperty, referenceParticleTypes);
        bool missingOverrideForSomeAtoms = false;
        bool appliedAnyOverride = false;
        for(size_t atomIndex = 0; atomIndex < referenceCount; ++atomIndex) {
            auto overrideIter = chargeOverrides.constFind(referenceParticleTypes[atomIndex]);
            if(overrideIter != chargeOverrides.constEnd()) {
                chargeValues[atomIndex] = static_cast<FloatType>(overrideIter.value());
                appliedAnyOverride = true;
            }
            else {
                missingOverrideForSomeAtoms = true;
            }
        }
        result.manualTypeChargesApplied = appliedAnyOverride;
        result.manualTypeChargesPartiallyApplied = appliedAnyOverride && missingOverrideForSomeAtoms;
        if(referenceChargeProperty == nullptr)
            result.usedUnitCharges = missingOverrideForSomeAtoms;
    }

    auto appendTimeAndVolume = [&](FrameSample& frame, const PipelineFlowState& sampleState, size_t frameIndex) {
        const QVariant timestepAttr = sampleState.getAttributeValue(QStringLiteral("Timestep"));
        bool ok = false;
        const double timestepValue = timestepAttr.isValid() ? timestepAttr.toDouble(&ok) : static_cast<double>(frameIndex);
        frame.timeRaw = (ok ? timestepValue : static_cast<double>(frameIndex)) * deltaTInSelectedUnits;
        frame.timeSI = frame.timeRaw * timeScaleToSI;

        if(referenceCell) {
            const SimulationCell* cell = sampleState.getObject<SimulationCell>();
            if(!cell)
                throw Exception(TransportModifier::tr("The simulation cell is missing in one of the sampled trajectory frames."));
            frame.volumeRaw = cell->is2D() ? cell->volume2D() : cell->volume3D();
            frame.volumeSI = frame.volumeRaw * std::pow(lengthScaleToSI, cell->is2D() ? 2.0 : 3.0);
        }
    };

    size_t analyzedEntityCount = 0;
    const bool useMoleculeEntities = (useOnlySelectedParticles && selectAsMolecules) || (!useOnlySelectedParticles && exactPyLATMode);

    if(exactPyLATMode || useMoleculeEntities) {
        if(!referenceCell) {
            throw Exception(useMoleculeEntities && !exactPyLATMode
                                ? TransportModifier::tr("Selecting particles as molecules requires a simulation cell.")
                                : TransportModifier::tr("This transport calculation requires a simulation cell."));
        }
        if(!referenceCell->isAxisAligned()) {
            throw Exception(useMoleculeEntities && !exactPyLATMode
                                ? TransportModifier::tr("Selecting particles as molecules currently requires an axis-aligned simulation cell.")
                                : TransportModifier::tr("This transport calculation currently requires an axis-aligned simulation cell."));
        }

        result.dimensionality = referenceCell->is2D() ? 2 : 3;
        result.fixedCellLengthsRaw = Vector3(referenceCell->cellVector1().length(),
                                             referenceCell->cellVector2().length(),
                                             referenceCell->is2D() ? 0.0 : referenceCell->cellVector3().length());
        result.fixedHalfCellLengthsRaw = result.fixedCellLengthsRaw / FloatType(2);
        result.fixedVolumeRaw = referenceCell->is2D() ? referenceCell->volume2D() : referenceCell->volume3D();
        result.fixedVolumeSI = result.fixedVolumeRaw * std::pow(lengthScaleToSI, result.dimensionality);
    }
    else if(referenceCell) {
        result.dimensionality = referenceCell->is2D() ? 2 : 3;
    }

    if(useMoleculeEntities || exactPyLATMode) {
        if(!referenceCell)
            throw Exception(useMoleculeEntities && !exactPyLATMode
                                ? TransportModifier::tr("Selecting particles as molecules requires a simulation cell.")
                                : TransportModifier::tr("This transport calculation requires a simulation cell."));
        if(!referenceCell->isAxisAligned())
            throw Exception(useMoleculeEntities && !exactPyLATMode
                                ? TransportModifier::tr("Selecting particles as molecules currently requires an axis-aligned simulation cell.")
                                : TransportModifier::tr("This transport calculation currently requires an axis-aligned simulation cell."));

        result.dimensionality = referenceCell->is2D() ? 2 : 3;
        result.fixedCellLengthsRaw = Vector3(referenceCell->cellVector1().length(),
                                             referenceCell->cellVector2().length(),
                                             referenceCell->is2D() ? 0.0 : referenceCell->cellVector3().length());
        result.fixedHalfCellLengthsRaw = result.fixedCellLengthsRaw / FloatType(2);
        result.fixedVolumeRaw = referenceCell->is2D() ? referenceCell->volume2D() : referenceCell->volume3D();
        result.fixedVolumeSI = result.fixedVolumeRaw * std::pow(lengthScaleToSI, result.dimensionality);
        if(exactPyLATMode && useOnlySelectedParticles && !selectAsMolecules) {
            result.selectedAtomsAnalyzedDirectlyInPyLATMode = true;

            std::vector<size_t> referenceIndices;
            referenceIndices.reserve(referenceCount);
            for(size_t atomIndex = 0; atomIndex < referenceCount; ++atomIndex) {
                if(atomPresentInAllFrames[atomIndex] && atomSelectedInAllFrames[atomIndex])
                    referenceIndices.push_back(atomIndex);
            }

            if(referenceIndices.empty()) {
                throw Exception(TransportModifier::tr(
                    "The selected transport-analysis subset is empty after intersecting the sampled frames. Selected atoms are analyzed directly in this configuration, so only atoms that remain selected in every sampled frame are kept."));
            }

            analyzedEntityCount = referenceIndices.size();
            result.chargesRaw.reserve(referenceIndices.size());
            for(size_t index : referenceIndices)
                result.chargesRaw.push_back(chargeValues[index]);

            if(computePerType && referenceTypeProperty) {
                BufferReadAccess<int32_t> typeAcc(referenceTypeProperty);
                std::unordered_map<int, size_t> groupIndexByType;
                for(size_t localIndex = 0; localIndex < referenceIndices.size(); ++localIndex) {
                    const int typeId = typeAcc[referenceIndices[localIndex]];
                    auto [iter, inserted] = groupIndexByType.emplace(typeId, result.groups.size());
                    if(inserted) {
                        GroupInfo group;
                        group.typeId = typeId;
                        if(const ElementType* elementType = referenceTypeProperty->elementType(typeId); elementType && !elementType->name().isEmpty())
                            group.name = elementType->name();
                        else
                            group.name = QString::number(typeId);
                        group.representativeChargeRaw = result.chargesRaw[localIndex];
                        result.groups.push_back(std::move(group));
                    }
                    result.groups[iter->second].memberIndices.push_back(localIndex);
                }
                result.hasPerTypeGroups = !result.groups.empty();
            }

            for(size_t frameIndex = 0; frameIndex < sampleStates.size(); ++frameIndex) {
                const PipelineFlowState& sampleState = sampleStates[frameIndex];
                const Particles* particles = sampleState.expectObject<Particles>();
                particles->verifyIntegrity();
                BufferReadAccess<Point3> positions(particles->expectProperty(Particles::PositionProperty));
                BufferReadAccess<Vector3> velocities(storedVelocitiesAvailable ? particles->expectProperty(Particles::VelocityProperty) : nullptr);

                if(const SimulationCell* cell = sampleState.getObject<SimulationCell>(); cell && !cell->isAxisAligned())
                    throw Exception(useMoleculeEntities && !exactPyLATMode
                                        ? TransportModifier::tr("Selecting particles as molecules currently requires an axis-aligned simulation cell in every sampled frame.")
                                        : TransportModifier::tr("This transport calculation currently requires an axis-aligned simulation cell in every sampled frame."));

                FrameSample& frame = result.frames[frameIndex];
                frame.positions.reserve(referenceIndices.size());
                if(storedVelocitiesAvailable)
                    frame.velocities.reserve(referenceIndices.size());
                appendTimeAndVolume(frame, sampleState, frameIndex);

                for(size_t referenceIndex : referenceIndices) {
                    const size_t mappedIndex = mappedIndicesByReference[referenceIndex][frameIndex];
                    frame.positions.push_back(positions[mappedIndex]);
                    if(storedVelocitiesAvailable)
                        frame.velocities.push_back(velocities[mappedIndex]);
                }
            }
        }
        else {
            const std::vector<FloatType> referenceMoleculeTypeValues =
                referenceMoleculeTypeProperty ? readScalarProperty(referenceMoleculeTypeProperty) : std::vector<FloatType>{};

            std::vector<MoleculeInfo> allMolecules;
            if(manualMoleculeOverridesActive) {
                std::vector<int32_t> referenceParticleTypeValues(referenceCount, 0);
                for(size_t atomIndex = 0; atomIndex < referenceCount; ++atomIndex)
                    referenceParticleTypeValues[atomIndex] = referenceParticleTypes[atomIndex];
                allMolecules = buildMoleculesFromManualTemplates(referenceParticleTypeValues,
                                                                parseManualMoleculeTemplates(manualMoleculeDefinitions,
                                                                                             referenceTypeProperty,
                                                                                             referenceParticleTypes));
                result.manualMoleculeDefinitionsApplied = true;
            }
            else {
                result.usedFallbackMoleculeIds = !referenceMoleculeProperty;

                BufferReadAccess<IdentifierIntType> referenceMoleculeIds(referenceMoleculeProperty);
                std::unordered_map<IdentifierIntType, size_t> moleculeLookup;
                moleculeLookup.reserve(referenceCount);
                for(size_t atomIndex = 0; atomIndex < referenceCount; ++atomIndex) {
                    const IdentifierIntType moleculeId =
                        referenceMoleculeIds ? referenceMoleculeIds[atomIndex]
                                             : (useIdentifiers ? referenceIds[atomIndex] : static_cast<IdentifierIntType>(atomIndex + 1));
                    auto [iter, inserted] = moleculeLookup.emplace(moleculeId, allMolecules.size());
                    if(inserted) {
                        MoleculeInfo molecule;
                        molecule.id = moleculeId;
                        allMolecules.push_back(std::move(molecule));
                    }
                    allMolecules[iter->second].referenceAtomIndices.push_back(atomIndex);
                }
            }

            std::vector<MoleculeInfo> includedMolecules;
            includedMolecules.reserve(allMolecules.size());
            std::unordered_map<QString, size_t> groupLookup;
            size_t moleculesIncludedBySelectionPromotion = 0;
            const bool sumChargesAcrossAtoms = static_cast<bool>(referenceChargeProperty) || result.manualTypeChargesApplied;

            for(MoleculeInfo& molecule : allMolecules) {
                const bool isPresentInAllFrames = std::ranges::all_of(molecule.referenceAtomIndices, [&](size_t atomIndex) {
                    return atomPresentInAllFrames[atomIndex];
                });
                if(!isPresentInAllFrames)
                    continue;

                bool includeMolecule = true;
                if(useOnlySelectedParticles) {
                    const bool allAtomsPersistentlySelected = std::ranges::all_of(molecule.referenceAtomIndices, [&](size_t atomIndex) {
                        return atomSelectedInAllFrames[atomIndex];
                    });
                    const bool anyAtomPersistentlySelected = allAtomsPersistentlySelected || std::ranges::any_of(molecule.referenceAtomIndices, [&](size_t atomIndex) {
                        return atomSelectedInAllFrames[atomIndex];
                    });
                    includeMolecule = anyAtomPersistentlySelected;
                    if(anyAtomPersistentlySelected && !allAtomsPersistentlySelected)
                        ++moleculesIncludedBySelectionPromotion;
                }

                if(!includeMolecule)
                    continue;

                for(size_t atomIndex : molecule.referenceAtomIndices) {
                    double atomMass = massValues[atomIndex];
                    if(atomMass <= 0) {
                        atomMass = 1.0;
                        result.usedUnitMasses = true;
                    }
                    molecule.totalMassRaw += atomMass;
                    if(sumChargesAcrossAtoms)
                        molecule.chargeRaw += chargeValues[atomIndex];
                }

                if(!sumChargesAcrossAtoms)
                    molecule.chargeRaw = 1.0;

                if(computePerType) {
                    if(result.manualMoleculeDefinitionsApplied && !molecule.groupName.isEmpty()) {
                        // Preserve the molecule-type label from the manual override template.
                    }
                    else if(referenceMoleculeTypeProperty && !referenceMoleculeTypeValues.empty()) {
                        const int moleculeTypeId = static_cast<int>(std::llround(referenceMoleculeTypeValues[molecule.referenceAtomIndices.front()]));
                        if(const ElementType* elementType = referenceMoleculeTypeProperty->elementType(moleculeTypeId); elementType && !elementType->name().isEmpty())
                            molecule.groupName = elementType->name();
                        else
                            molecule.groupName = QString::number(moleculeTypeId);
                    }
                    else if(referenceTypeProperty) {
                        std::map<int, size_t> composition;
                        for(size_t atomIndex : molecule.referenceAtomIndices)
                            composition[referenceParticleTypes[atomIndex]]++;
                        if(composition.size() == 1) {
                            const int typeId = composition.begin()->first;
                            if(const ElementType* elementType = referenceTypeProperty->elementType(typeId); elementType && !elementType->name().isEmpty())
                                molecule.groupName = elementType->name();
                            else
                                molecule.groupName = QString::number(typeId);
                        }
                        else {
                            result.usedFallbackMoleculeTypes = true;
                            QStringList signatureParts;
                            for(const auto& [typeId, count] : composition) {
                                QString label;
                                if(const ElementType* elementType = referenceTypeProperty->elementType(typeId); elementType && !elementType->name().isEmpty())
                                    label = elementType->name();
                                else
                                    label = QString::number(typeId);
                                signatureParts.push_back(QStringLiteral("%1x%2").arg(label).arg(count));
                            }
                            molecule.groupName = signatureParts.join(QStringLiteral(" + "));
                        }
                    }
                    else {
                        result.usedFallbackMoleculeTypes = true;
                        molecule.groupName = TransportModifier::tr("All molecules");
                    }
                }

                const size_t moleculeIndex = includedMolecules.size();
                includedMolecules.push_back(molecule);
                result.chargesRaw.push_back(molecule.chargeRaw);

                if(computePerType) {
                    auto [groupIter, inserted] = groupLookup.emplace(molecule.groupName, result.groups.size());
                    if(inserted) {
                        GroupInfo group;
                        group.typeId = static_cast<int>(result.groups.size() + 1);
                        group.name = molecule.groupName;
                        group.representativeChargeRaw = molecule.chargeRaw;
                        result.groups.push_back(std::move(group));
                    }
                    result.groups[groupIter->second].memberIndices.push_back(moleculeIndex);
                }
            }

            result.selectionPromotedToWholeMolecules = useOnlySelectedParticles && moleculesIncludedBySelectionPromotion > 0;
            result.hasPerTypeGroups = computePerType && !result.groups.empty();
            analyzedEntityCount = includedMolecules.size();

            if(includedMolecules.empty())
                throw Exception(useOnlySelectedParticles
                                    ? TransportModifier::tr("The selected transport-analysis subset is empty after promoting the persistent selected atoms to whole molecules.")
                                    : TransportModifier::tr("No persistent molecules were found for the current Transport configuration."));

            for(size_t frameIndex = 0; frameIndex < sampleStates.size(); ++frameIndex) {
                const PipelineFlowState& sampleState = sampleStates[frameIndex];
                const Particles* particles = sampleState.expectObject<Particles>();
                particles->verifyIntegrity();
                BufferReadAccess<Point3> positions(particles->expectProperty(Particles::PositionProperty));
                BufferReadAccess<Vector3> velocities(storedVelocitiesAvailable ? particles->expectProperty(Particles::VelocityProperty) : nullptr);

                if(const SimulationCell* cell = sampleState.getObject<SimulationCell>(); cell && !cell->isAxisAligned())
                    throw Exception(useMoleculeEntities && !exactPyLATMode
                                        ? TransportModifier::tr("Selecting particles as molecules currently requires an axis-aligned simulation cell in every sampled frame.")
                                        : TransportModifier::tr("This transport calculation currently requires an axis-aligned simulation cell in every sampled frame."));

                FrameSample& frame = result.frames[frameIndex];
                frame.positions.reserve(includedMolecules.size());
                if(storedVelocitiesAvailable)
                    frame.velocities.reserve(includedMolecules.size());
                appendTimeAndVolume(frame, sampleState, frameIndex);

                for(const MoleculeInfo& molecule : includedMolecules) {
                    const size_t firstAtomIndex = molecule.referenceAtomIndices.front();
                    const size_t mappedFirstAtom = mappedIndicesByReference[firstAtomIndex][frameIndex];
                    const Point3 referencePosition = positions[mappedFirstAtom];

                    Vector3 weightedPositionSum = Vector3::Zero();
                    Vector3 weightedVelocitySum = Vector3::Zero();

                    for(size_t atomIndex : molecule.referenceAtomIndices) {
                        const size_t mappedAtomIndex = mappedIndicesByReference[atomIndex][frameIndex];
                        Point3 adjustedPosition = positions[mappedAtomIndex];

                        if(atomIndex != firstAtomIndex) {
                            if(result.fixedCellLengthsRaw.x() > 0) {
                                if((adjustedPosition.x() - referencePosition.x()) > result.fixedHalfCellLengthsRaw.x())
                                    adjustedPosition += Vector3(-result.fixedCellLengthsRaw.x(), 0, 0);
                                else if((referencePosition.x() - adjustedPosition.x()) > result.fixedHalfCellLengthsRaw.x())
                                    adjustedPosition += Vector3(result.fixedCellLengthsRaw.x(), 0, 0);
                            }
                            if(result.fixedCellLengthsRaw.y() > 0) {
                                if((adjustedPosition.y() - referencePosition.y()) > result.fixedHalfCellLengthsRaw.y())
                                    adjustedPosition += Vector3(0, -result.fixedCellLengthsRaw.y(), 0);
                                else if((referencePosition.y() - adjustedPosition.y()) > result.fixedHalfCellLengthsRaw.y())
                                    adjustedPosition += Vector3(0, result.fixedCellLengthsRaw.y(), 0);
                            }
                            if(result.dimensionality == 3 && result.fixedCellLengthsRaw.z() > 0) {
                                if((adjustedPosition.z() - referencePosition.z()) > result.fixedHalfCellLengthsRaw.z())
                                    adjustedPosition += Vector3(0, 0, -result.fixedCellLengthsRaw.z());
                                else if((referencePosition.z() - adjustedPosition.z()) > result.fixedHalfCellLengthsRaw.z())
                                    adjustedPosition += Vector3(0, 0, result.fixedCellLengthsRaw.z());
                            }
                        }

                        double atomMass = massValues[atomIndex];
                        if(atomMass <= 0)
                            atomMass = 1.0;

                        weightedPositionSum += (adjustedPosition - Point3::Origin()) * atomMass;
                        if(storedVelocitiesAvailable)
                            weightedVelocitySum += velocities[mappedAtomIndex] * atomMass;
                    }

                    frame.positions.push_back(Point3::Origin() + weightedPositionSum / molecule.totalMassRaw);
                    if(storedVelocitiesAvailable)
                        frame.velocities.push_back(weightedVelocitySum / molecule.totalMassRaw);
                }
            }

            for(size_t frameIndex = 1; frameIndex < result.frames.size(); ++frameIndex) {
                for(size_t moleculeIndex = 0; moleculeIndex < result.frames[frameIndex].positions.size(); ++moleculeIndex) {
                    Point3& currentPosition = result.frames[frameIndex].positions[moleculeIndex];
                    const Point3& previousPosition = result.frames[frameIndex - 1].positions[moleculeIndex];
                    if(result.fixedCellLengthsRaw.x() > 0) {
                        while((currentPosition.x() - previousPosition.x()) > result.fixedHalfCellLengthsRaw.x())
                            currentPosition += Vector3(-result.fixedCellLengthsRaw.x(), 0, 0);
                        while((currentPosition.x() - previousPosition.x()) < -result.fixedHalfCellLengthsRaw.x())
                            currentPosition += Vector3(result.fixedCellLengthsRaw.x(), 0, 0);
                    }
                    if(result.fixedCellLengthsRaw.y() > 0) {
                        while((currentPosition.y() - previousPosition.y()) > result.fixedHalfCellLengthsRaw.y())
                            currentPosition += Vector3(0, -result.fixedCellLengthsRaw.y(), 0);
                        while((currentPosition.y() - previousPosition.y()) < -result.fixedHalfCellLengthsRaw.y())
                            currentPosition += Vector3(0, result.fixedCellLengthsRaw.y(), 0);
                    }
                    if(result.dimensionality == 3 && result.fixedCellLengthsRaw.z() > 0) {
                        while((currentPosition.z() - previousPosition.z()) > result.fixedHalfCellLengthsRaw.z())
                            currentPosition += Vector3(0, 0, -result.fixedCellLengthsRaw.z());
                        while((currentPosition.z() - previousPosition.z()) < -result.fixedHalfCellLengthsRaw.z())
                            currentPosition += Vector3(0, 0, result.fixedCellLengthsRaw.z());
                    }
                }
            }
        }
    }
    else {
        std::vector<size_t> referenceIndices;
        referenceIndices.reserve(referenceCount);
        for(size_t atomIndex = 0; atomIndex < referenceCount; ++atomIndex) {
            if(atomPresentInAllFrames[atomIndex] && (!useOnlySelectedParticles || atomSelectedInAllFrames[atomIndex]))
                referenceIndices.push_back(atomIndex);
        }

        if(referenceIndices.empty())
            throw Exception(TransportModifier::tr("The selected transport-analysis subset is empty after intersecting the sampled frames."));

        analyzedEntityCount = referenceIndices.size();
        result.chargesRaw.reserve(referenceIndices.size());
        for(size_t index : referenceIndices)
            result.chargesRaw.push_back(chargeValues[index]);

        if(computePerType && referenceTypeProperty) {
            BufferReadAccess<int32_t> typeAcc(referenceTypeProperty);
            std::unordered_map<int, size_t> groupIndexByType;
            for(size_t localIndex = 0; localIndex < referenceIndices.size(); ++localIndex) {
                const int typeId = typeAcc[referenceIndices[localIndex]];
                auto [iter, inserted] = groupIndexByType.emplace(typeId, result.groups.size());
                if(inserted) {
                    GroupInfo group;
                    group.typeId = typeId;
                    if(const ElementType* elementType = referenceTypeProperty->elementType(typeId); elementType && !elementType->name().isEmpty())
                        group.name = elementType->name();
                    else
                        group.name = QString::number(typeId);
                    group.representativeChargeRaw = result.chargesRaw[localIndex];
                    result.groups.push_back(std::move(group));
                }
                result.groups[iter->second].memberIndices.push_back(localIndex);
            }
            result.hasPerTypeGroups = !result.groups.empty();
        }

        if(referenceCell)
            result.dimensionality = referenceCell->is2D() ? 2 : 3;

        for(size_t frameIndex = 0; frameIndex < sampleStates.size(); ++frameIndex) {
            const PipelineFlowState& sampleState = sampleStates[frameIndex];
            const Particles* particles = sampleState.expectObject<Particles>();
            particles->verifyIntegrity();
            BufferReadAccess<Point3> positions(particles->expectProperty(Particles::PositionProperty));

            FrameSample& frame = result.frames[frameIndex];
            frame.positions.reserve(referenceIndices.size());
            for(size_t referenceIndex : referenceIndices)
                frame.positions.push_back(positions[mappedIndicesByReference[referenceIndex][frameIndex]]);
            appendTimeAndVolume(frame, sampleState, frameIndex);

            if(storedVelocitiesAvailable) {
                BufferReadAccess<Vector3> velocities(particles->expectProperty(Particles::VelocityProperty));
                frame.velocities.reserve(referenceIndices.size());
                for(size_t referenceIndex : referenceIndices)
                    frame.velocities.push_back(velocities[mappedIndicesByReference[referenceIndex][frameIndex]]);
            }
        }
    }

    if(referenceCell) {
        result.averageVolumeRaw = std::accumulate(result.frames.begin(), result.frames.end(), 0.0, [](double sum, const FrameSample& frame) { return sum + frame.volumeRaw; }) / result.frames.size();
        result.averageVolumeSI = std::accumulate(result.frames.begin(), result.frames.end(), 0.0, [](double sum, const FrameSample& frame) { return sum + frame.volumeSI; }) / result.frames.size();
        if(!exactPyLATMode) {
            result.fixedVolumeRaw = result.averageVolumeRaw;
            result.fixedVolumeSI = result.averageVolumeSI;
        }
    }

    if(result.usedFiniteDifferenceVelocities) {
        if(result.frames.size() < 3) {
            result.velocityAnalysisAvailable = false;
        }
        else {
            for(size_t frameIndex = 0; frameIndex < result.frames.size(); ++frameIndex)
                result.frames[frameIndex].velocities.assign(analyzedEntityCount, Vector3::Zero());
            for(size_t frameIndex = 1; frameIndex + 1 < result.frames.size(); ++frameIndex) {
                const double dtRaw = result.frames[frameIndex + 1].timeRaw - result.frames[frameIndex - 1].timeRaw;
                if(dtRaw <= 0)
                    throw Exception(TransportModifier::tr("Encountered a non-positive timestep while deriving velocities from particle positions."));
                for(size_t particleIndex = 0; particleIndex < analyzedEntityCount; ++particleIndex) {
                    const Vector3 displacement = result.frames[frameIndex + 1].positions[particleIndex] - result.frames[frameIndex - 1].positions[particleIndex];
                    result.frames[frameIndex].velocities[particleIndex] = displacement / static_cast<FloatType>(dtRaw);
                }
            }
        }
    }

    if(exactPyLATMode)
        result.conductivityVelocityAnalysisAvailable = storedVelocitiesAvailable;
    else
        result.conductivityVelocityAnalysisAvailable = result.velocityAnalysisAvailable;

    QStringList warnings;
    if(manualMoleculeOverridesActive && !result.manualMoleculeDefinitionsApplied)
        warnings.push_back(TransportModifier::tr("Manual molecule definitions are currently inactive because this Transport configuration analyzes particles directly. Enable 'Select as molecules' for selected atoms to apply the molecule templates."));
    if(result.manualMoleculeDefinitionsApplied)
        warnings.push_back(TransportModifier::tr("Manual molecule definitions overrode Molecule Identifier and reconstructed molecules from contiguous particle-type blocks in the reference-frame ordering."));
    if(result.usedFallbackMoleculeIds)
        warnings.push_back(exactPyLATMode
                               ? TransportModifier::tr("No Molecule Identifier property was found. This transport calculation treated each persistent atom as its own molecule.")
                               : TransportModifier::tr("No Molecule Identifier property was found. The molecule-based Transport analysis treated each persistent atom as its own molecule. Provide Molecule Identifier or manual molecule definitions to analyze whole molecules."));
    if(result.usedUnitCharges)
        warnings.push_back(exactPyLATMode
                               ? TransportModifier::tr("No Charge property was found. Unit charges (q = 1) were used for every analyzed transport entity. This preserves q^2-based terms such as Nernst-Einstein conductivity for unit-valence ions, but signed-charge terms such as correlated Einstein conductivity and Green-Kubo conductivity will be inaccurate for mixed-sign systems. Import the matching data file together with the trajectory or provide an explicit Charge property.")
                               : TransportModifier::tr("No Charge property was found. Unit charges (q = 1) were used for all particles."));
    if(result.manualTypeChargesApplied)
        warnings.push_back(result.manualTypeChargesPartiallyApplied
                               ? TransportModifier::tr("Manual type-charge overrides were applied to the listed particle types. Unlisted particle types kept their trajectory charges or unit charges (q = 1) when no Charge property was available.")
                               : TransportModifier::tr("Manual type-charge overrides replaced the trajectory/default charges for all listed particle types."));
    if(result.usedUnitMasses)
        warnings.push_back(TransportModifier::tr("Some analyzed atoms did not have a positive mass. Unit masses were used for the affected molecules, which changes the center-of-mass calculation."));
    if(result.selectedAtomsAnalyzedDirectlyInPyLATMode)
        warnings.push_back(TransportModifier::tr("The persistent selected-atom subset was analyzed directly instead of being promoted to whole molecules. The filtered results therefore track the selected atoms, not molecule center-of-mass transport, and can differ from full-molecule transport for multi-atom molecules."));
    if(result.selectionPromotedToWholeMolecules)
        warnings.push_back(exactPyLATMode
                               ? TransportModifier::tr("The selected-atom filter was promoted to full molecules. Any molecule with at least one persistently selected atom was included as a whole molecule.")
                               : TransportModifier::tr("The selected-atom filter was promoted to full molecules. Any molecule with at least one persistently selected atom was included as a whole molecule."));
    if(computePerType && !result.hasPerTypeGroups)
        warnings.push_back(exactPyLATMode
                               ? (result.selectedAtomsAnalyzedDirectlyInPyLATMode
                                      ? TransportModifier::tr("No Particle Type property was found, so the selected-atom transport analysis generated aggregate transport curves only.")
                                      : TransportModifier::tr("No molecule/species type property was found, so the transport analysis generated aggregate transport curves only."))
                               : (useMoleculeEntities
                                      ? TransportModifier::tr("No molecule/species type property was found, so the molecule-based Transport analysis generated aggregate transport curves only.")
                                      : TransportModifier::tr("No Particle Type property was found, so only all-particle transport curves were generated.")));
    if(result.usedFallbackMoleculeTypes)
        warnings.push_back(exactPyLATMode
                               ? TransportModifier::tr("Molecule species were inferred from particle-type compositions because no explicit Molecule Type property was available.")
                               : TransportModifier::tr("The molecule-based Transport analysis inferred molecule species from particle-type compositions because no explicit Molecule Type property was available."));
    if(result.usedFiniteDifferenceVelocities && result.velocityAnalysisAvailable) {
        if(exactPyLATMode)
            warnings.push_back(result.selectedAtomsAnalyzedDirectlyInPyLATMode
                                   ? TransportModifier::tr("VACF used centered finite-difference velocities derived from selected atom positions. Green-Kubo conductivity remained tied to explicit stored velocities only.")
                                   : TransportModifier::tr("VACF used centered finite-difference velocities derived from molecule COM positions. Green-Kubo conductivity remained tied to explicit stored velocities only."));
        else if(useMoleculeEntities)
            warnings.push_back(TransportModifier::tr("VACF and Green-Kubo conductivity used centered finite-difference velocities derived from molecule COM positions."));
        else
            warnings.push_back(TransportModifier::tr("VACF and Green-Kubo conductivity used centered finite-difference velocities derived from particle positions."));
    }
    if(needVelocityAnalysis && allowFiniteDifferenceVelocities && !result.velocityAnalysisAvailable)
        warnings.push_back(TransportModifier::tr("At least three sampled trajectory frames are required for VACF/current correlations if explicit particle velocities are unavailable."));
    if(exactPyLATMode && needVelocityAnalysis && !result.conductivityVelocityAnalysisAvailable)
        warnings.push_back(TransportModifier::tr("Green-Kubo conductivity requires explicit velocity data in every sampled frame and was skipped."));
    warningText = warnings.join(QLatin1String(" "));

    return result;
}

}   // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(TransportModifier);
OVITO_CLASSINFO(TransportModifier, "DisplayName", "Transport");
OVITO_CLASSINFO(TransportModifier, "Description", "Compute MSD, VACF, diffusion, and ionic conductivity from particle trajectories.");
OVITO_CLASSINFO(TransportModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(TransportModifier, computeMSD);
DEFINE_PROPERTY_FIELD(TransportModifier, computeVACF);
DEFINE_PROPERTY_FIELD(TransportModifier, computeConductivity);
DEFINE_PROPERTY_FIELD(TransportModifier, useOnlySelectedParticles);
DEFINE_PROPERTY_FIELD(TransportModifier, selectAsMolecules);
DEFINE_PROPERTY_FIELD(TransportModifier, computePerType);
DEFINE_PROPERTY_FIELD(TransportModifier, includeVACFCrossTerms);
DEFINE_PROPERTY_FIELD(TransportModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(TransportModifier, intervalStart);
DEFINE_PROPERTY_FIELD(TransportModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(TransportModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(TransportModifier, maxLag);
DEFINE_PROPERTY_FIELD(TransportModifier, summaryWindowStartLag);
DEFINE_PROPERTY_FIELD(TransportModifier, summaryWindowEndLag);
DEFINE_PROPERTY_FIELD(TransportModifier, usePyLATCompatibility);
DEFINE_PROPERTY_FIELD(TransportModifier, pyLatDiffusivityTolerance);
DEFINE_PROPERTY_FIELD(TransportModifier, pyLatConductivityTolerance);
DEFINE_PROPERTY_FIELD(TransportModifier, deltaT);
DEFINE_PROPERTY_FIELD(TransportModifier, temperature);
DEFINE_PROPERTY_FIELD(TransportModifier, timeUnit);
DEFINE_PROPERTY_FIELD(TransportModifier, lengthUnit);
DEFINE_PROPERTY_FIELD(TransportModifier, chargeUnit);
DEFINE_PROPERTY_FIELD(TransportModifier, useManualMoleculeDefinitions);
DEFINE_PROPERTY_FIELD(TransportModifier, manualMoleculeDefinitions);
DEFINE_PROPERTY_FIELD(TransportModifier, useManualTypeCharges);
DEFINE_PROPERTY_FIELD(TransportModifier, manualTypeCharges);
DEFINE_PROPERTY_FIELD(TransportModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(TransportModifier, computeMSD, "Compute MSD");
SET_PROPERTY_FIELD_LABEL(TransportModifier, computeVACF, "Compute VACF");
SET_PROPERTY_FIELD_LABEL(TransportModifier, computeConductivity, "Compute ionic conductivity");
SET_PROPERTY_FIELD_LABEL(TransportModifier, useOnlySelectedParticles, "Use only selected atoms");
SET_PROPERTY_FIELD_LABEL(TransportModifier, selectAsMolecules, "Select as molecules");
SET_PROPERTY_FIELD_LABEL(TransportModifier, computePerType, "Compute per-type curves");
SET_PROPERTY_FIELD_LABEL(TransportModifier, includeVACFCrossTerms, "Include collective and cross VACF");
SET_PROPERTY_FIELD_LABEL(TransportModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(TransportModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(TransportModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(TransportModifier, samplingFrequency, "Sampling frequency");
SET_PROPERTY_FIELD_LABEL(TransportModifier, maxLag, "Maximum lag");
SET_PROPERTY_FIELD_LABEL(TransportModifier, summaryWindowStartLag, "Summary start lag");
SET_PROPERTY_FIELD_LABEL(TransportModifier, summaryWindowEndLag, "Summary end lag");
SET_PROPERTY_FIELD_LABEL(TransportModifier, usePyLATCompatibility, "Use fitted transport workflow");
SET_PROPERTY_FIELD_LABEL(TransportModifier, pyLatDiffusivityTolerance, "Diffusivity fit tolerance");
SET_PROPERTY_FIELD_LABEL(TransportModifier, pyLatConductivityTolerance, "Conductivity plateau tolerance");
SET_PROPERTY_FIELD_LABEL(TransportModifier, deltaT, "Delta t");
SET_PROPERTY_FIELD_LABEL(TransportModifier, temperature, "Temperature");
SET_PROPERTY_FIELD_LABEL(TransportModifier, timeUnit, "Time unit");
SET_PROPERTY_FIELD_LABEL(TransportModifier, lengthUnit, "Length unit");
SET_PROPERTY_FIELD_LABEL(TransportModifier, chargeUnit, "Charge unit");
SET_PROPERTY_FIELD_LABEL(TransportModifier, useManualMoleculeDefinitions, "Use manual molecule definitions");
SET_PROPERTY_FIELD_LABEL(TransportModifier, manualMoleculeDefinitions, "Manual molecule definitions");
SET_PROPERTY_FIELD_LABEL(TransportModifier, useManualTypeCharges, "Use manual type charges");
SET_PROPERTY_FIELD_LABEL(TransportModifier, manualTypeCharges, "Manual type charges");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, summaryWindowStartLag, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, summaryWindowEndLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, pyLatDiffusivityTolerance, FloatParameterUnit, 0, std::numeric_limits<FloatType>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TransportModifier, pyLatConductivityTolerance, FloatParameterUnit, 0, std::numeric_limits<FloatType>::max());
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(TransportModifier, deltaT, FloatParameterUnit, 1e-12);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(TransportModifier, temperature, FloatParameterUnit, 0);

IMPLEMENT_CREATABLE_OVITO_CLASS(TransportModificationNode);
DEFINE_REFERENCE_FIELD(TransportModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(TransportModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(TransportModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(TransportModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(TransportModifier, TransportModificationNode);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool TransportModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void TransportModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
    setUsePyLATCompatibility(true);
    setUseManualMoleculeDefinitions(false);
    setManualMoleculeDefinitions(QString{});
    setUseManualTypeCharges(false);
    setManualTypeCharges(QString{});
}

/******************************************************************************
 * Determines the list of sampled frames.
 ******************************************************************************/
std::vector<int> TransportModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for transport analysis."));

    const int stride = 1;
    int firstFrame = 0;
    int lastFrame = numFrames - 1;
    if(useCustomFrameInterval()) {
        firstFrame = std::clamp(intervalStart(), 0, numFrames - 1);
        lastFrame = std::clamp(intervalEnd(), 0, numFrames - 1);
        if(firstFrame > lastFrame)
            std::swap(firstFrame, lastFrame);
    }

    std::vector<int> frames;
    frames.reserve(((lastFrame - firstFrame) / stride) + 1);
    for(int frame = firstFrame; frame <= lastFrame; frame += stride)
        frames.push_back(frame);

    if(frames.empty())
        throw Exception(tr("The selected transport-analysis interval does not contain any sampled frames."));

    return frames;
}

/******************************************************************************
 * Asks the modifier for the set of animation time intervals that should be cached.
 ******************************************************************************/
void TransportModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    if(request.modificationNode()->numberOfSourceFrames() > 0) {
        const std::vector<int> frames = sampledFrames(request.modificationNode());
        if(!frames.empty()) {
            request.mutableCachingIntervals().add(TimeInterval(
                request.modificationNode()->sourceFrameToAnimationTime(frames.front()),
                request.modificationNode()->sourceFrameToAnimationTime(frames.back())));
        }
    }

    Modifier::inputCachingHints(request);
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void TransportModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                            PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                            TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
    Q_UNUSED(validityInterval);
}

/******************************************************************************
 * Lets the modifier adjust the validity interval.
 ******************************************************************************/
void TransportModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> TransportModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(TransportModificationNode* modNode = dynamic_object_cast<TransportModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr("Transport analysis is idle. Open the Run section and click 'Run transport analysis' to compute the selected observables.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr("Transport analysis is queued. Click 'Run transport analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeTransportData(request, std::move(state));
}

/******************************************************************************
 * Computes the cached transport data by traversing the sampled trajectory.
 ******************************************************************************/
Future<PipelineFlowState> TransportModifier::computeTransportData(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const int cacheGenerationId = dynamic_object_cast<TransportModificationNode>(request.modificationNode())
        ? dynamic_object_cast<TransportModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;
    std::vector<SharedFuture<PipelineFlowState>> sampleFutures;
    sampleFutures.reserve(frames.size());
    for(const int frame : frames) {
        ModifierEvaluationRequest frameRequest(request);
        frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
        sampleFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
    }

    return when_all_futures(std::move(sampleFutures))
        .then(DeferredObjectExecutor(this), [this, request, state = std::move(state), cacheGenerationId](std::vector<SharedFuture<PipelineFlowState>> futures) mutable -> Future<PipelineFlowState> {
            std::vector<PipelineFlowState> sampleStates;
            sampleStates.reserve(futures.size());
            for(SharedFuture<PipelineFlowState>& future : futures)
                sampleStates.push_back(future.result());

            const int completedRunRequestId = runRequestId();

            return asyncLaunch([this,
                                self = OORef<TransportModifier>(this),
                                request = ModifierEvaluationRequest(request),
                                state = std::move(state),
                                sampleStates = std::move(sampleStates),
                                completedRunRequestId,
                                cacheGenerationId]() mutable {
                TransportComputationResult computationResult{std::move(state)};
                TransportModificationNode* modNode = dynamic_object_cast<TransportModificationNode>(request.modificationNode());
                if(!modNode)
                    return computationResult;

                PipelineFlowState state = std::move(computationResult.state);

                if(!computeMSD() && !computeVACF() && !computeConductivity())
                    throw Exception(tr("Enable at least one transport observable (MSD, VACF, or conductivity)."));
                if(deltaT() <= 0)
                    throw Exception(tr("The transport analysis requires a positive delta t value."));
                if(temperature() <= 0 && computeConductivity())
                    throw Exception(tr("The conductivity calculation requires a positive temperature."));

                const double timeScaleToSI = timeUnitToSeconds(timeUnit());
                const double lengthScaleToSI = lengthUnitToMeters(lengthUnit());
                const double chargeScaleToSI = chargeUnitToCoulombs(chargeUnit());
                const QString timeLabel = timeUnitLabel(timeUnit());
                const QString lengthLabel = lengthUnitLabel(lengthUnit());
                const QString chargeLabel = chargeUnitLabel(chargeUnit());
                const bool needMSDOutput = computeMSD();
                const bool needVACFOutput = computeVACF();
                const bool needConductivityOutput = computeConductivity();
                const bool needVelocitySamples = needVACFOutput || needConductivityOutput;
                const bool allowFiniteDifferenceVelocities = needVACFOutput;

                QString warningText;
                PreparedData prepared = prepareTransportData(sampleStates,
                                                            useOnlySelectedParticles(),
                                                            selectAsMolecules(),
                                                            computePerType(),
                                                            needVelocitySamples,
                                                            allowFiniteDifferenceVelocities,
                                                            true,
                                                            useManualMoleculeDefinitions(),
                                                            manualMoleculeDefinitions(),
                                                            useManualTypeCharges(),
                                                            manualTypeCharges(),
                                                            deltaT(),
                                                            timeScaleToSI,
                                                            lengthScaleToSI,
                                                            warningText);

                const size_t frameCount = prepared.frames.size();
                const size_t particleCount = prepared.frames.front().positions.size();
                const int dimensionality = prepared.dimensionality;

            const PyLATMSDCurves pyLatMSD = (needMSDOutput || needConductivityOutput) ? computePyLATMSDCurves(prepared) : PyLATMSDCurves{};
            const PyLATChargeTransportCurves pyLatChargeTransport = needConductivityOutput ? computePyLATChargeTransportCurves(prepared) : PyLATChargeTransportCurves{};

            std::vector<double> msdTimesRaw = pyLatMSD.timesRaw;
            std::vector<double> msdTimesSI = pyLatMSD.timesSI;
            const std::vector<double>& msdOverallTrimmed = pyLatMSD.overall;
            const std::vector<std::vector<double>>& msdPerTypeTrimmed = pyLatMSD.perType;

            std::vector<double> msdOverallSI;
            std::vector<double> diffusionFromMSDRaw;
            std::vector<double> diffusionFromMSDSI;
            std::vector<std::vector<double>> msdPerTypeSI;
            std::vector<std::vector<double>> diffusionPerTypeMSDRaw;
            std::vector<std::vector<double>> diffusionPerTypeMSDSI;
            if(needMSDOutput) {
                msdOverallSI.resize(msdOverallTrimmed.size(), 0.0);
                std::ranges::transform(msdOverallTrimmed, msdOverallSI.begin(), [lengthScaleToSI](double value) { return value * lengthScaleToSI * lengthScaleToSI; });

                diffusionFromMSDRaw.assign(msdOverallTrimmed.size(), 0.0);
                diffusionFromMSDSI.assign(msdOverallTrimmed.size(), 0.0);
                for(size_t i = 1; i < msdOverallTrimmed.size(); ++i) {
                    if(msdTimesRaw[i] > 0)
                        diffusionFromMSDRaw[i] = msdOverallTrimmed[i] / (2.0 * dimensionality * msdTimesRaw[i]);
                    if(msdTimesSI[i] > 0)
                        diffusionFromMSDSI[i] = msdOverallSI[i] / (2.0 * dimensionality * msdTimesSI[i]);
                }

                msdPerTypeSI.reserve(msdPerTypeTrimmed.size());
                diffusionPerTypeMSDRaw.reserve(msdPerTypeTrimmed.size());
                diffusionPerTypeMSDSI.reserve(msdPerTypeTrimmed.size());
                for(const std::vector<double>& curve : msdPerTypeTrimmed) {
                    std::vector<double> curveSI(curve.size(), 0.0);
                    std::ranges::transform(curve, curveSI.begin(),
                                           [lengthScaleToSI](double value) { return value * lengthScaleToSI * lengthScaleToSI; });
                    std::vector<double> diffRaw(curve.size(), 0.0);
                    std::vector<double> diffSI(curve.size(), 0.0);
                    for(size_t i = 1; i < curve.size(); ++i) {
                        if(msdTimesRaw[i] > 0)
                            diffRaw[i] = curve[i] / (2.0 * dimensionality * msdTimesRaw[i]);
                        if(msdTimesSI[i] > 0)
                            diffSI[i] = curveSI[i] / (2.0 * dimensionality * msdTimesSI[i]);
                    }
                    msdPerTypeSI.push_back(std::move(curveSI));
                    diffusionPerTypeMSDRaw.push_back(std::move(diffRaw));
                    diffusionPerTypeMSDSI.push_back(std::move(diffSI));
                }
            }

            if(msdTimesRaw.empty() && !pyLatChargeTransport.timesRaw.empty()) {
                msdTimesRaw = pyLatChargeTransport.timesRaw;
                msdTimesSI = pyLatChargeTransport.timesSI;
            }

            std::vector<double> chargeDisplacementTrimmed = pyLatChargeTransport.correlatedChargeDisplacement;
            std::vector<double> nernstEinsteinTrimmed = pyLatChargeTransport.nernstEinsteinNumerator;
            const size_t conductivityCurveSize = std::min({msdTimesSI.size(), chargeDisplacementTrimmed.size(), nernstEinsteinTrimmed.size()});
            msdTimesRaw = trimVector(msdTimesRaw, conductivityCurveSize == 0 ? msdTimesRaw.size() : conductivityCurveSize);
            msdTimesSI = trimVector(msdTimesSI, conductivityCurveSize == 0 ? msdTimesSI.size() : conductivityCurveSize);
            chargeDisplacementTrimmed = trimVector(chargeDisplacementTrimmed, conductivityCurveSize);
            nernstEinsteinTrimmed = trimVector(nernstEinsteinTrimmed, conductivityCurveSize);

            std::vector<double> vacfSelf;
            std::vector<double> vacfTotal;
            std::vector<double> vacfCross;
            std::vector<std::vector<double>> vacfPerType;
            std::vector<double> vacfTimesRaw;
            std::vector<double> vacfTimesSI;
            std::vector<double> diffusionFromVACFRaw;
            std::vector<double> diffusionFromVACFSI;
            std::vector<std::vector<double>> diffusionPerTypeVACFRaw;
            std::vector<std::vector<double>> diffusionPerTypeVACFSI;
            std::vector<double> currentCorrelationRaw;
            std::vector<double> conductivityGKAverageVolumeSI;
            std::vector<double> conductivityGKLagTimesRaw;
            std::vector<double> conductivityGKLagTimesSI;

            if(needVACFOutput && prepared.velocityAnalysisAvailable) {
                const size_t firstValidVelocityFrame = prepared.usedFiniteDifferenceVelocities ? 1 : 0;
                const size_t lastValidVelocityFrame = prepared.usedFiniteDifferenceVelocities ? frameCount - 2 : frameCount - 1;
                const size_t maxLagVelocity =
                    std::min<size_t>((maxLag() > 0 ? static_cast<size_t>(maxLag()) : lastValidVelocityFrame - firstValidVelocityFrame),
                                     lastValidVelocityFrame - firstValidVelocityFrame);

                std::vector<size_t> vacfCounts(maxLagVelocity + 1, 0);
                std::vector<double> vacfLagTimesRaw(maxLagVelocity + 1, 0.0);
                std::vector<double> vacfLagTimesSI(maxLagVelocity + 1, 0.0);
                vacfSelf.assign(maxLagVelocity + 1, 0.0);
                vacfTotal.assign(maxLagVelocity + 1, 0.0);
                vacfPerType.assign(prepared.groups.size(), std::vector<double>(maxLagVelocity + 1, 0.0));

                for(size_t origin = firstValidVelocityFrame; origin <= lastValidVelocityFrame; ++origin) {
                    this_task::throwIfCanceled();
                    for(size_t lag = 0; lag <= maxLagVelocity && origin + lag <= lastValidVelocityFrame; ++lag) {
                        const size_t target = origin + lag;
                        const double dtRaw = prepared.frames[target].timeRaw - prepared.frames[origin].timeRaw;
                        const double dtSI = prepared.frames[target].timeSI - prepared.frames[origin].timeSI;

                        double selfDot = 0.0;
                        std::vector<double> selfDotPerType(prepared.groups.size(), 0.0);
                        Vector3 totalVelocityOrigin = Vector3::Zero();
                        Vector3 totalVelocityTarget = Vector3::Zero();
                        Vector3 totalCurrentOrigin = Vector3::Zero();
                        Vector3 totalCurrentTarget = Vector3::Zero();

                        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                            const Vector3& v0 = prepared.frames[origin].velocities[particleIndex];
                            const Vector3& v1 = prepared.frames[target].velocities[particleIndex];
                            const double dot = v0.dot(v1);
                            selfDot += dot;
                            totalVelocityOrigin += v0;
                            totalVelocityTarget += v1;
                            totalCurrentOrigin += v0 * static_cast<FloatType>(prepared.chargesRaw[particleIndex]);
                            totalCurrentTarget += v1 * static_cast<FloatType>(prepared.chargesRaw[particleIndex]);
                        }

                        for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                            for(size_t particleIndex : prepared.groups[groupIndex].memberIndices) {
                                selfDotPerType[groupIndex] += prepared.frames[origin].velocities[particleIndex].dot(
                                    prepared.frames[target].velocities[particleIndex]);
                            }
                        }

                        vacfSelf[lag] += selfDot / (static_cast<double>(particleCount) * dimensionality);
                        vacfTotal[lag] += totalVelocityOrigin.dot(totalVelocityTarget) / (static_cast<double>(particleCount) * dimensionality);
                        for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                            if(!prepared.groups[groupIndex].memberIndices.empty()) {
                                vacfPerType[groupIndex][lag] += selfDotPerType[groupIndex] /
                                                               (static_cast<double>(prepared.groups[groupIndex].memberIndices.size()) * dimensionality);
                            }
                        }

                        vacfLagTimesRaw[lag] += dtRaw;
                        vacfLagTimesSI[lag] += dtSI;
                        vacfCounts[lag] += 1;
                    }
                }

                for(size_t lag = 0; lag < vacfSelf.size(); ++lag) {
                    if(vacfCounts[lag] == 0)
                        continue;
                    const double normalization = static_cast<double>(vacfCounts[lag]);
                    vacfSelf[lag] /= normalization;
                    vacfTotal[lag] /= normalization;
                    vacfLagTimesRaw[lag] /= normalization;
                    vacfLagTimesSI[lag] /= normalization;
                    for(std::vector<double>& curve : vacfPerType)
                        curve[lag] /= normalization;
                }

                const size_t vacfEffectiveSize = effectiveSizeFromCounts(vacfCounts);
                vacfSelf = trimVector(vacfSelf, vacfEffectiveSize);
                vacfTotal = trimVector(vacfTotal, vacfEffectiveSize);
                vacfTimesRaw = trimVector(vacfLagTimesRaw, vacfEffectiveSize);
                vacfTimesSI = trimVector(vacfLagTimesSI, vacfEffectiveSize);
                vacfCross.resize(vacfSelf.size(), 0.0);
                for(size_t i = 0; i < vacfSelf.size(); ++i)
                    vacfCross[i] = vacfTotal[i] - vacfSelf[i];
                for(std::vector<double>& curve : vacfPerType)
                    curve = trimVector(curve, vacfEffectiveSize);

                diffusionFromVACFRaw = cumulativeTrapezoid(vacfTimesRaw, vacfSelf);
                diffusionFromVACFSI.resize(diffusionFromVACFRaw.size(), 0.0);
                const double diffusionScaleToSI = lengthScaleToSI * lengthScaleToSI / timeScaleToSI;
                std::ranges::transform(diffusionFromVACFRaw, diffusionFromVACFSI.begin(), [diffusionScaleToSI](double value) { return value * diffusionScaleToSI; });

                diffusionPerTypeVACFRaw.reserve(vacfPerType.size());
                diffusionPerTypeVACFSI.reserve(vacfPerType.size());
                for(const std::vector<double>& curve : vacfPerType) {
                    std::vector<double> diffusionCurveRaw = cumulativeTrapezoid(vacfTimesRaw, curve);
                    std::vector<double> diffusionCurveSI(diffusionCurveRaw.size(), 0.0);
                    std::ranges::transform(diffusionCurveRaw, diffusionCurveSI.begin(), [diffusionScaleToSI](double value) { return value * diffusionScaleToSI; });
                    diffusionPerTypeVACFRaw.push_back(std::move(diffusionCurveRaw));
                    diffusionPerTypeVACFSI.push_back(std::move(diffusionCurveSI));
                }
            }

            const size_t conductivitySize = chargeDisplacementTrimmed.size();
            std::vector<double> conductivityCorrelatedAverageVolumeSI(conductivitySize, 0.0);
            std::vector<double> conductivityNernstEinsteinAverageVolumeSI(conductivitySize, 0.0);

            const double chargeDisplacementScaleToSI = chargeScaleToSI * chargeScaleToSI * lengthScaleToSI * lengthScaleToSI;
            for(size_t i = 1; i < conductivitySize; ++i) {
                if(msdTimesSI[i] <= 0)
                    continue;
                if(prepared.fixedVolumeSI > 0) {
                    conductivityCorrelatedAverageVolumeSI[i] = (chargeDisplacementTrimmed[i] * chargeDisplacementScaleToSI) /
                                                               (2.0 * dimensionality * BoltzmannConstant * temperature() * prepared.fixedVolumeSI * msdTimesSI[i]);
                    conductivityNernstEinsteinAverageVolumeSI[i] = (nernstEinsteinTrimmed[i] * chargeDisplacementScaleToSI) /
                                                                    (2.0 * dimensionality * BoltzmannConstant * temperature() * prepared.fixedVolumeSI * msdTimesSI[i]);
                }
            }

            const double conductivityScaleToSI = chargeScaleToSI * chargeScaleToSI /
                                                 (timeScaleToSI * std::pow(lengthScaleToSI, dimensionality - 2.0));
            std::vector<double> conductivityCorrelatedAverageVolumeRaw(conductivityCorrelatedAverageVolumeSI.size(), 0.0);
            std::vector<double> conductivityNernstEinsteinAverageVolumeRaw(conductivityNernstEinsteinAverageVolumeSI.size(), 0.0);
            std::vector<double> conductivityGKAverageVolumeRaw(conductivityGKAverageVolumeSI.size(), 0.0);
            if(conductivityScaleToSI > 0) {
                std::ranges::transform(conductivityCorrelatedAverageVolumeSI, conductivityCorrelatedAverageVolumeRaw.begin(),
                                       [conductivityScaleToSI](double value) { return value / conductivityScaleToSI; });
                std::ranges::transform(conductivityNernstEinsteinAverageVolumeSI, conductivityNernstEinsteinAverageVolumeRaw.begin(),
                                       [conductivityScaleToSI](double value) { return value / conductivityScaleToSI; });
                std::ranges::transform(conductivityGKAverageVolumeSI, conductivityGKAverageVolumeRaw.begin(),
                                       [conductivityScaleToSI](double value) { return value / conductivityScaleToSI; });
            }

            const bool pyLatCompatibilityEnabled = true;
            if(pyLatCompatibilityEnabled && needConductivityOutput) {
                const PyLATGreenKuboCurves pyLatGK =
                    computePyLATGreenKuboCurves(prepared, timeScaleToSI, lengthScaleToSI, chargeScaleToSI, conductivityScaleToSI, temperature());
                if(!pyLatGK.totalCurrentCorrelationRaw.empty()) {
                    const size_t conductivitySize = msdTimesRaw.size();
                    currentCorrelationRaw = pyLatGK.totalCurrentCorrelationRaw;
                    conductivityGKAverageVolumeRaw = trimVector(pyLatGK.totalConductivityRaw, conductivitySize);
                    conductivityGKAverageVolumeSI = trimVector(pyLatGK.totalConductivitySI, conductivitySize);
                    conductivityGKLagTimesRaw = pyLatGK.timesRaw;
                    conductivityGKLagTimesSI = pyLatGK.timesSI;
                }
            }

                DataOORef<DataCollection> results = DataOORef<DataCollection>::create();
                const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();

            const QString aggregateCurveLabel = useOnlySelectedParticles()
                ? tr("Selected subset atoms (persisted across all sampled frames)")
                : tr("All atoms");

            QStringList msdComponentNames{aggregateCurveLabel};
            std::vector<std::vector<double>> msdColumns{msdOverallTrimmed};
            std::vector<std::vector<double>> msdColumnsSI{msdOverallSI};
            for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                msdComponentNames.push_back(prepared.groups[groupIndex].name);
                msdColumns.push_back(msdPerTypeTrimmed[groupIndex]);
                msdColumnsSI.push_back(msdPerTypeSI[groupIndex]);
            }

            if(computeMSD()) {
                createLineTable(results, MSDTableId, tr("Mean squared displacement"), msdTimesRaw, msdColumns, msdComponentNames,
                                tr("Time (%1)").arg(timeLabel), tr("MSD (%1)").arg(msdUnitLabel(lengthLabel)), createdByNode);

                QStringList diffusionComponentNames{aggregateCurveLabel};
                std::vector<std::vector<double>> diffusionColumns{diffusionFromMSDRaw};
                std::vector<std::vector<double>> diffusionColumnsSI{diffusionFromMSDSI};
                for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                    diffusionComponentNames.push_back(prepared.groups[groupIndex].name);
                    diffusionColumns.push_back(diffusionPerTypeMSDRaw[groupIndex]);
                    diffusionColumnsSI.push_back(diffusionPerTypeMSDSI[groupIndex]);
                }

                createLineTable(results, DiffusionMSDTableId, tr("Diffusion coefficient from MSD"), msdTimesRaw, diffusionColumns, diffusionComponentNames,
                                tr("Time (%1)").arg(timeLabel), tr("Diffusion (%1)").arg(diffusionUnitLabel(lengthLabel, timeLabel)), createdByNode);
            }

            if(computeVACF() && prepared.velocityAnalysisAvailable) {
                QStringList vacfComponentNames{QStringLiteral("Self")};
                std::vector<std::vector<double>> vacfColumns{vacfSelf};
                std::vector<std::vector<double>> vacfColumnsSI(1, std::vector<double>(vacfSelf.size(), 0.0));
                const double vacfScaleToSI = lengthScaleToSI * lengthScaleToSI / (timeScaleToSI * timeScaleToSI);
                std::ranges::transform(vacfSelf, vacfColumnsSI.front().begin(), [vacfScaleToSI](double value) { return value * vacfScaleToSI; });
                for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                    vacfComponentNames.push_back(QStringLiteral("Self (%1)").arg(prepared.groups[groupIndex].name));
                    vacfColumns.push_back(vacfPerType[groupIndex]);
                    std::vector<double> curveSI(vacfPerType[groupIndex].size(), 0.0);
                    std::ranges::transform(vacfPerType[groupIndex], curveSI.begin(), [vacfScaleToSI](double value) { return value * vacfScaleToSI; });
                    vacfColumnsSI.push_back(std::move(curveSI));
                }
                if(includeVACFCrossTerms()) {
                    vacfComponentNames.push_back(QStringLiteral("Collective"));
                    vacfComponentNames.push_back(QStringLiteral("Cross"));
                    vacfColumns.push_back(vacfTotal);
                    vacfColumns.push_back(vacfCross);
                    std::vector<double> totalSI(vacfTotal.size(), 0.0);
                    std::vector<double> crossSI(vacfCross.size(), 0.0);
                    std::ranges::transform(vacfTotal, totalSI.begin(), [vacfScaleToSI](double value) { return value * vacfScaleToSI; });
                    std::ranges::transform(vacfCross, crossSI.begin(), [vacfScaleToSI](double value) { return value * vacfScaleToSI; });
                    vacfColumnsSI.push_back(std::move(totalSI));
                    vacfColumnsSI.push_back(std::move(crossSI));
                }

                createLineTable(results, VACFTableId, tr("Velocity autocorrelation function"), vacfTimesRaw, vacfColumns, vacfComponentNames,
                                tr("Time (%1)").arg(timeLabel), tr("VACF (%1)").arg(vacfUnitLabel(lengthLabel, timeLabel)), createdByNode);

                QStringList diffusionComponentNames{aggregateCurveLabel};
                std::vector<std::vector<double>> diffusionColumns{diffusionFromVACFRaw};
                std::vector<std::vector<double>> diffusionColumnsSI{diffusionFromVACFSI};
                for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                    diffusionComponentNames.push_back(prepared.groups[groupIndex].name);
                    diffusionColumns.push_back(diffusionPerTypeVACFRaw[groupIndex]);
                    diffusionColumnsSI.push_back(diffusionPerTypeVACFSI[groupIndex]);
                }

                createLineTable(results, DiffusionVACFTableId, tr("Diffusion coefficient from VACF"), vacfTimesRaw, diffusionColumns, diffusionComponentNames,
                                tr("Time (%1)").arg(timeLabel), tr("Diffusion (%1)").arg(diffusionUnitLabel(lengthLabel, timeLabel)), createdByNode);
            }

            if(computeConductivity()) {
                createLineTable(results, ChargeDisplacementTableId, tr("Charge displacement correlations"), msdTimesRaw,
                                {chargeDisplacementTrimmed, nernstEinsteinTrimmed},
                                {QStringLiteral("Correlated"), QStringLiteral("Nernst-Einstein")},
                                tr("Time (%1)").arg(timeLabel),
                                tr("Charge-displacement (%1^2*%2^2)").arg(chargeLabel, lengthLabel),
                                createdByNode);

                if(prepared.conductivityVelocityAnalysisAvailable && !currentCorrelationRaw.empty()) {
                    createLineTable(results, CurrentCorrelationTableId, tr("Current autocorrelation"), conductivityGKLagTimesRaw, {currentCorrelationRaw},
                                    {QStringLiteral("Current autocorrelation")},
                                    tr("Time (%1)").arg(timeLabel),
                                    tr("Current correlation (%1)").arg(currentCorrelationUnitLabel(chargeLabel, lengthLabel, timeLabel)),
                                    createdByNode);
                }

                QStringList conductivityNames{
                    QStringLiteral("Correlated Einstein (V)"),
                    QStringLiteral("Nernst-Einstein (V)")
                };
                std::vector<std::vector<double>> conductivityColumnsRaw{
                    conductivityCorrelatedAverageVolumeRaw,
                    conductivityNernstEinsteinAverageVolumeRaw
                };
                std::vector<std::vector<double>> conductivityColumns{
                    conductivityCorrelatedAverageVolumeSI,
                    conductivityNernstEinsteinAverageVolumeSI
                };
                if(prepared.conductivityVelocityAnalysisAvailable && !conductivityGKAverageVolumeSI.empty()) {
                    conductivityNames.push_back(QStringLiteral("Green-Kubo (V)"));
                    conductivityColumnsRaw.push_back(conductivityGKAverageVolumeRaw);
                    conductivityColumns.push_back(conductivityGKAverageVolumeSI);
                }

                createLineTable(results, ConductivityTableId, tr("Ionic conductivity"), msdTimesRaw, conductivityColumnsRaw, conductivityNames,
                                tr("Time (%1)").arg(timeLabel),
                                tr("Conductivity (%1)").arg(conductivityRawUnitLabel(chargeLabel, timeLabel, lengthLabel, dimensionality)),
                                createdByNode);
            }

            double summaryDMSDRaw = std::numeric_limits<double>::quiet_NaN();
            double summaryDMSDSI = std::numeric_limits<double>::quiet_NaN();
            double summaryDVACFRaw = (needVACFOutput && prepared.velocityAnalysisAvailable) ? lastFiniteValue(diffusionFromVACFRaw) : std::numeric_limits<double>::quiet_NaN();
            double summaryDVACFSI = (needVACFOutput && prepared.velocityAnalysisAvailable) ? lastFiniteValue(diffusionFromVACFSI) : std::numeric_limits<double>::quiet_NaN();

            double summarySigmaCorrVavgRaw = std::numeric_limits<double>::quiet_NaN();
            double summarySigmaCorrVavgSI = std::numeric_limits<double>::quiet_NaN();
            double summarySigmaNEVavgRaw = std::numeric_limits<double>::quiet_NaN();
            double summarySigmaNEVavgSI = std::numeric_limits<double>::quiet_NaN();
            double summarySigmaGKVavgRaw = std::numeric_limits<double>::quiet_NaN();
            double summarySigmaGKVavgSI = std::numeric_limits<double>::quiet_NaN();
            int pyLatMsdFitStartLag = 0;
            int pyLatGkFitStartLag = 0;
            int pyLatGkFitEndLag = 0;

            if(pyLatCompatibilityEnabled) {
                if(pyLatMSD.overall.size() >= 2) {
                    const PyLATDiffusivityFit overallFit =
                        fitPyLATDiffusivity(pyLatMSD.timesRaw, pyLatMSD.overall, dimensionality, deltaT(), timeScaleToSI, lengthScaleToSI, pyLatDiffusivityTolerance());
                    pyLatMsdFitStartLag = overallFit.fitStartLag;
                    summaryDMSDRaw = overallFit.diffusionRaw;
                    summaryDMSDSI = overallFit.diffusionSI;

                    if(prepared.fixedVolumeSI > 0 && msdTimesSI.size() == chargeDisplacementTrimmed.size()) {
                        std::vector<double> chargeDisplacementSI(chargeDisplacementTrimmed.size(), 0.0);
                        std::ranges::transform(chargeDisplacementTrimmed, chargeDisplacementSI.begin(),
                                               [chargeDisplacementScaleToSI](double value) { return value * chargeDisplacementScaleToSI; });
                        const double sigmaCorrSlopeSI = linearRegressionSlope(msdTimesSI, chargeDisplacementSI, pyLatMsdFitStartLag);
                        if(std::isfinite(sigmaCorrSlopeSI)) {
                            summarySigmaCorrVavgSI = sigmaCorrSlopeSI /
                                                     (2.0 * dimensionality * BoltzmannConstant * temperature() * prepared.fixedVolumeSI);
                            if(conductivityScaleToSI > 0)
                                summarySigmaCorrVavgRaw = summarySigmaCorrVavgSI / conductivityScaleToSI;
                        }
                    }

                    double sigmaNEPyLatSI = std::numeric_limits<double>::quiet_NaN();
                    if(prepared.fixedVolumeSI > 0) {
                        double weightedDiffusionSum = 0.0;
                        bool hasFiniteContribution = false;
                        if(!prepared.groups.empty() && pyLatMSD.perType.size() == prepared.groups.size()) {
                            for(size_t groupIndex = 0; groupIndex < prepared.groups.size(); ++groupIndex) {
                                const PyLATDiffusivityFit groupFit =
                                    fitPyLATDiffusivity(pyLatMSD.timesRaw, pyLatMSD.perType[groupIndex], dimensionality, deltaT(), timeScaleToSI, lengthScaleToSI, pyLatDiffusivityTolerance());
                                if(!std::isfinite(groupFit.diffusionSI))
                                    continue;

                                const double charge = prepared.groups[groupIndex].representativeChargeRaw;
                                weightedDiffusionSum += static_cast<double>(prepared.groups[groupIndex].memberIndices.size()) * charge * charge * groupFit.diffusionSI;
                                hasFiniteContribution = true;
                            }
                        }
                        else if(std::isfinite(overallFit.diffusionSI)) {
                            double chargeSquaredSum = 0.0;
                            for(const double charge : prepared.chargesRaw)
                                chargeSquaredSum += charge * charge;
                            weightedDiffusionSum = chargeSquaredSum * overallFit.diffusionSI;
                            hasFiniteContribution = true;
                        }

                        if(hasFiniteContribution) {
                            sigmaNEPyLatSI = weightedDiffusionSum * chargeScaleToSI * chargeScaleToSI /
                                             (BoltzmannConstant * temperature() * prepared.fixedVolumeSI);
                        }
                    }

                    if(std::isfinite(sigmaNEPyLatSI)) {
                        summarySigmaNEVavgSI = sigmaNEPyLatSI;
                        if(conductivityScaleToSI > 0)
                            summarySigmaNEVavgRaw = sigmaNEPyLatSI / conductivityScaleToSI;
                    }
                }

                if(prepared.conductivityVelocityAnalysisAvailable && !currentCorrelationRaw.empty() && !conductivityGKAverageVolumeSI.empty()) {
                    const PyLATConvergenceWindow gkWindow = findPyLATConvergenceWindow(currentCorrelationRaw, pyLatConductivityTolerance());
                    pyLatGkFitStartLag = gkWindow.beginLag;
                    pyLatGkFitEndLag = gkWindow.endLag;
                    summarySigmaGKVavgRaw = averagePyLATRange(conductivityGKAverageVolumeRaw, gkWindow.beginLag, gkWindow.endLag);
                    summarySigmaGKVavgSI = averagePyLATRange(conductivityGKAverageVolumeSI, gkWindow.beginLag, gkWindow.endLag);
                }
            }

            setSummaryAttribute(results, QStringLiteral("Transport.sampled_particle_count"), static_cast<double>(particleCount), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.sampled_frame_count"), static_cast<double>(frameCount), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.dimensionality"), static_cast<double>(dimensionality), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.pylat_compatibility_enabled"), 1.0, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.pylat_selected_atoms_direct"), prepared.selectedAtomsAnalyzedDirectlyInPyLATMode ? 1.0 : 0.0, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.pylat_msd_fit_start_lag"), static_cast<double>(pyLatMsdFitStartLag), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.pylat_gk_fit_start_lag"), static_cast<double>(pyLatGkFitStartLag), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.pylat_gk_fit_end_lag"), static_cast<double>(pyLatGkFitEndLag), createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.D_MSD"), summaryDMSDRaw, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.D_MSD_SI"), summaryDMSDSI, createdByNode);
            if(needVACFOutput && prepared.velocityAnalysisAvailable) {
                setSummaryAttribute(results, QStringLiteral("Transport.D_VACF"), summaryDVACFRaw, createdByNode);
                setSummaryAttribute(results, QStringLiteral("Transport.D_VACF_SI"), summaryDVACFSI, createdByNode);
            }
            if(prepared.conductivityVelocityAnalysisAvailable) {
                setSummaryAttribute(results, QStringLiteral("Transport.sigma_green_kubo_vavg_raw"), summarySigmaGKVavgRaw, createdByNode);
                setSummaryAttribute(results, QStringLiteral("Transport.sigma_green_kubo_vavg"), summarySigmaGKVavgSI, createdByNode);
            }
            setSummaryAttribute(results, QStringLiteral("Transport.sigma_correlated_einstein_vavg_raw"), summarySigmaCorrVavgRaw, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.sigma_correlated_einstein_vavg"), summarySigmaCorrVavgSI, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.sigma_nernst_einstein_vavg_raw"), summarySigmaNEVavgRaw, createdByNode);
            setSummaryAttribute(results, QStringLiteral("Transport.sigma_nernst_einstein_vavg"), summarySigmaNEVavgSI, createdByNode);

            const double sigmaCorrelatedVavg = summarySigmaCorrVavgSI;
            const double sigmaNernstEinsteinVavg = summarySigmaNEVavgSI;
            if(std::isfinite(sigmaCorrelatedVavg) && std::isfinite(sigmaNernstEinsteinVavg) && sigmaNernstEinsteinVavg != 0)
                setSummaryAttribute(results, QStringLiteral("Transport.haven_ratio_vavg"), sigmaCorrelatedVavg / sigmaNernstEinsteinVavg, createdByNode);

                computationResult.state = std::move(state);
                computationResult.results = std::move(results);
                computationResult.warningText = std::move(warningText);
                computationResult.completedRunRequestId = completedRunRequestId;
                computationResult.cacheGenerationId = cacheGenerationId;
                return computationResult;
            }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](TransportComputationResult computationResult) mutable {
                TransportModificationNode* modNode = dynamic_object_cast<TransportModificationNode>(request.modificationNode());
                if(!modNode || !computationResult.results)
                    return std::move(computationResult.state);

                if(modNode->cacheGenerationId() != computationResult.cacheGenerationId || runRequestId() != computationResult.completedRunRequestId)
                    return std::move(computationResult.state);

                modNode->setCachedResults(computationResult.results);
                modNode->setCachedWarningText(computationResult.warningText);
                modNode->setCompletedRunRequestId(computationResult.completedRunRequestId);
                return applyCachedResults(request, std::move(computationResult.state));
            });
        });
}

/******************************************************************************
 * Applies the cached transport data to the current pipeline state.
 ******************************************************************************/
PipelineFlowState TransportModifier::applyCachedResults(const ModifierEvaluationRequest& request, PipelineFlowState state) const
{
    TransportModificationNode* modNode = dynamic_object_cast<TransportModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

/******************************************************************************
 * Clears all cached transport data.
 ******************************************************************************/
void TransportModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

/******************************************************************************
 * Is called when a referenced target generated an event.
 ******************************************************************************/
bool TransportModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}   // End of namespace
