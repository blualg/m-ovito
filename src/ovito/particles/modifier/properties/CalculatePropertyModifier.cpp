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
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include "CalculatePropertyModifier.h"

#include <array>
#include <limits>
#include <memory>
#include <numeric>
#include <unordered_map>
#include <QRegularExpression>

namespace Ovito {

namespace {

struct MoleculeGroup
{
    std::vector<size_t> indices;
    bool anySelected = false;
};

struct ReductionAccumulator
{
    size_t count = 0;
    double sum = 0.0;
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();

    void add(double value)
    {
        count++;
        sum += value;
        min = std::min(min, value);
        max = std::max(max, value);
    }

    [[nodiscard]] bool valid() const
    {
        return count > 0;
    }

    [[nodiscard]] double mean() const
    {
        OVITO_ASSERT(count > 0);
        return sum / static_cast<double>(count);
    }
};

struct VectorReductionAccumulator
{
    std::array<ReductionAccumulator, 3> components;

    void add(const Vector3& value)
    {
        components[0].add(static_cast<double>(value.x()));
        components[1].add(static_cast<double>(value.y()));
        components[2].add(static_cast<double>(value.z()));
    }

    [[nodiscard]] bool valid() const
    {
        return components[0].valid();
    }
};

struct ParsedScript
{
    bool pairMode = false;
    bool vectorMode = false;
    QString scalarExpression;
    std::array<QString, 3> vectorExpressions;
};

std::vector<MoleculeGroup> buildMoleculeGroups(const BufferReadAccess<IdentifierIntType>& moleculeIds,
                                               const BufferReadAccess<SelectionIntType>& selection)
{
    std::unordered_map<IdentifierIntType, size_t> groupLookup;
    groupLookup.reserve(moleculeIds.size());
    std::vector<MoleculeGroup> molecules;
    molecules.reserve(moleculeIds.size());

    for(size_t particleIndex = 0; particleIndex < moleculeIds.size(); ++particleIndex) {
        const IdentifierIntType moleculeId = moleculeIds[particleIndex];
        auto [iter, inserted] = groupLookup.try_emplace(moleculeId, molecules.size());
        if(inserted)
            molecules.emplace_back();

        MoleculeGroup& group = molecules[iter->second];
        group.indices.push_back(particleIndex);
        if(selection && selection[particleIndex])
            group.anySelected = true;
    }

    return molecules;
}

class PairParticleExpressionEvaluator : public PropertyExpressionEvaluator
{
public:
    PairParticleExpressionEvaluator(BufferReadAccess<Point3> positions, const SimulationCell* cell) :
        _positions(std::move(positions)),
        _cell(cell)
    {
        setIndexVarName(QStringLiteral("PairIndex"));
    }

    void setCurrentPair(size_t sourceIndex, size_t targetIndex)
    {
        _currentSourceIndex = sourceIndex;
        _currentTargetIndex = targetIndex;
    }

protected:
    virtual void createInputVariables(const std::vector<ConstPropertyPtr>& inputProperties,
                                      const SimulationCell* simCell,
                                      const QVariantMap& attributes,
                                      int animationFrame) override
    {
        registerPropertyVariables(inputProperties, 1, _T("@i."));
        registerPropertyVariables(inputProperties, 2, _T("@j."));

        registerGlobalParameter("N", elementCount(), tr("total number of particles"));
        registerGlobalParameter("Frame", animationFrame, tr("animation frame number"));
        for(auto entry = attributes.constBegin(); entry != attributes.constEnd(); ++entry) {
            if(entry.value().canConvert<double>())
                registerGlobalParameter(entry.key(), entry.value().toDouble());
            else if(entry.value().canConvert<long>())
                registerGlobalParameter(entry.key(), entry.value().value<long>());
        }

        if(simCell) {
            registerGlobalParameter("CellVolume", simCell->is2D() ? simCell->volume2D() : simCell->volume3D(), tr("simulation cell volume"));
            registerGlobalParameter("CellSize.X", std::abs(simCell->cellMatrix().column(0).x()), tr("size along X"));
            registerGlobalParameter("CellSize.Y", std::abs(simCell->cellMatrix().column(1).y()), tr("size along Y"));
            registerGlobalParameter("CellSize.Z", std::abs(simCell->cellMatrix().column(2).z()), tr("size along Z"));
        }

        registerConstant("pi", M_PI, QStringLiteral("%1...").arg(M_PI));
        if(std::numeric_limits<FloatType>::has_infinity)
            registerConstant("inf", std::numeric_limits<FloatType>::infinity(), QStringLiteral("∞"));

        registerComputedVariable("Distance", [this](size_t) -> double {
            return currentDelta().length();
        }, tr("minimum-image pair distance"));
        registerComputedVariable("DistanceSquared", [this](size_t) -> double {
            return static_cast<double>(currentDelta().squaredLength());
        }, tr("squared minimum-image pair distance"));
        registerComputedVariable("Delta.X", [this](size_t) -> double {
            return static_cast<double>(currentDelta().x());
        }, tr("x_j - x_i"));
        registerComputedVariable("Delta.Y", [this](size_t) -> double {
            return static_cast<double>(currentDelta().y());
        }, tr("y_j - y_i"));
        registerComputedVariable("Delta.Z", [this](size_t) -> double {
            return static_cast<double>(currentDelta().z());
        }, tr("z_j - z_i"));
        registerComputedVariable("dx", [this](size_t) -> double {
            return static_cast<double>(currentDelta().x());
        }, tr("x_j - x_i"));
        registerComputedVariable("dy", [this](size_t) -> double {
            return static_cast<double>(currentDelta().y());
        }, tr("y_j - y_i"));
        registerComputedVariable("dz", [this](size_t) -> double {
            return static_cast<double>(currentDelta().z());
        }, tr("z_j - z_i"));
    }

    virtual void updateVariables(Worker& worker, size_t elementIndex) override
    {
        worker.updateVariables(0, elementIndex);
        worker.updateVariables(1, _currentSourceIndex);
        worker.updateVariables(2, _currentTargetIndex);
    }

private:
    Vector3 currentDelta() const
    {
        Vector3 delta = _positions[_currentTargetIndex] - _positions[_currentSourceIndex];
        if(_cell)
            delta = _cell->wrapVector(delta);
        return delta;
    }

    BufferReadAccess<Point3> _positions;
    const SimulationCell* _cell = nullptr;
    size_t _currentSourceIndex = 0;
    size_t _currentTargetIndex = 0;
};

QString propertyTypeLabel(CalculatePropertyModifier::PropertyType propertyType)
{
    switch(propertyType) {
    case CalculatePropertyModifier::DipoleDirection:
        return CalculatePropertyModifier::tr("dipole direction");
    case CalculatePropertyModifier::ManualMolecularDirection:
        return CalculatePropertyModifier::tr("molecular direction");
    case CalculatePropertyModifier::KineticEnergy:
        return CalculatePropertyModifier::tr("kinetic energy");
    case CalculatePropertyModifier::ParticleExpression:
        return CalculatePropertyModifier::tr("particle expression");
    case CalculatePropertyModifier::VectorExpression:
        return CalculatePropertyModifier::tr("vector expression");
    case CalculatePropertyModifier::PairExpression:
        return CalculatePropertyModifier::tr("pair expression");
    case CalculatePropertyModifier::PairDistances:
        return CalculatePropertyModifier::tr("pair distances");
    }
    OVITO_ASSERT(false);
    return {};
}

QString defaultOutputPropertyName(CalculatePropertyModifier::PropertyType propertyType)
{
    switch(propertyType) {
    case CalculatePropertyModifier::DipoleDirection:
        return CalculatePropertyModifier::tr("Dipole Orientation");
    case CalculatePropertyModifier::ManualMolecularDirection:
        return CalculatePropertyModifier::tr("Molecular Direction");
    case CalculatePropertyModifier::KineticEnergy:
        return CalculatePropertyModifier::tr("Kinetic Energy");
    case CalculatePropertyModifier::ParticleExpression:
        return CalculatePropertyModifier::tr("Calculated Property");
    case CalculatePropertyModifier::VectorExpression:
        return CalculatePropertyModifier::tr("Calculated Vector");
    case CalculatePropertyModifier::PairExpression:
        return CalculatePropertyModifier::tr("Pair Property");
    case CalculatePropertyModifier::PairDistances:
        return CalculatePropertyModifier::tr("Pair Distances");
    }
    OVITO_ASSERT(false);
    return {};
}

QString groupingModeLabel(CalculatePropertyModifier::GroupingMode groupingMode)
{
    switch(groupingMode) {
    case CalculatePropertyModifier::NoGrouping:
        return CalculatePropertyModifier::tr("none");
    case CalculatePropertyModifier::GroupByMolecule:
        return CalculatePropertyModifier::tr("molecule");
    }
    OVITO_ASSERT(false);
    return {};
}

QString reductionLabel(CalculatePropertyModifier::ReductionOperation operation)
{
    switch(operation) {
    case CalculatePropertyModifier::NoReduction:
        return CalculatePropertyModifier::tr("none");
    case CalculatePropertyModifier::SumReduction:
        return CalculatePropertyModifier::tr("sum");
    case CalculatePropertyModifier::MeanReduction:
        return CalculatePropertyModifier::tr("mean");
    case CalculatePropertyModifier::MinReduction:
        return CalculatePropertyModifier::tr("min");
    case CalculatePropertyModifier::MaxReduction:
        return CalculatePropertyModifier::tr("max");
    }
    OVITO_ASSERT(false);
    return {};
}

double reducedValue(const ReductionAccumulator& accumulator, CalculatePropertyModifier::ReductionOperation operation)
{
    switch(operation) {
    case CalculatePropertyModifier::NoReduction:
        OVITO_ASSERT(false);
        return 0.0;
    case CalculatePropertyModifier::SumReduction:
        return accumulator.sum;
    case CalculatePropertyModifier::MeanReduction:
        return accumulator.mean();
    case CalculatePropertyModifier::MinReduction:
        return accumulator.min;
    case CalculatePropertyModifier::MaxReduction:
        return accumulator.max;
    }
    OVITO_ASSERT(false);
    return 0.0;
}

PropertyPtr createScalarProperty(const QString& propertyName, size_t elementCount)
{
    return PropertyPtr::create(DataBuffer::Initialized, elementCount, Property::FloatDefault, 1, propertyName);
}

PropertyPtr createVectorProperty(const QString& propertyName, size_t elementCount)
{
    PropertyPtr property = PropertyPtr::create(DataBuffer::Initialized, elementCount, Property::FloatDefault, 3,
                                               propertyName, 0, QStringList() << "X" << "Y" << "Z");
    if(property->visElements().empty()) {
        OORef<VectorVis> vis = OORef<VectorVis>::create();
        vis->setObjectTitle(propertyName);
        vis->setEnabled(false);
        vis->setReverseArrowDirection(false);
        vis->setArrowPosition(VectorVis::Center);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
        property->addVisElement(std::move(vis));
    }
    return property;
}

void finalizeScalarReductionOutput(PipelineFlowState& state,
                                   const OOWeakRef<const PipelineNode>& createdByNode,
                                   const QString& outputName,
                                   CalculatePropertyModifier::ReductionOperation operation,
                                   const ReductionAccumulator& accumulator)
{
    if(operation == CalculatePropertyModifier::NoReduction || !accumulator.valid())
        return;

    const double value = reducedValue(accumulator, operation);
    const QString reductionName = reductionLabel(operation);
    const QString attributeKey = QStringLiteral("CalculateProperty.%1.%2").arg(outputName, reductionName);
    state.setAttribute(attributeKey, QVariant::fromValue(value), createdByNode);

    DataTable* table = state.createObject<DataTable>(
        QStringLiteral("calculate-property-reduction[%1]").arg(outputName),
        createdByNode,
        DataTable::BarChart,
        CalculatePropertyModifier::tr("%1 (%2)").arg(outputName, reductionName));
    table->setAxisLabelX(CalculatePropertyModifier::tr("Reduction"));
    table->setAxisLabelY(outputName);
    table->setElementCount(1);

    PropertyPtr xValues = PropertyPtr::create(DataBuffer::Initialized, 1, DataBuffer::Int64, 1,
                                              CalculatePropertyModifier::tr("Index"));
    BufferWriteAccess<int64_t, access_mode::discard_write> xAcc(xValues);
    xAcc[0] = 0;

    PropertyPtr yValues = PropertyPtr::create(DataBuffer::Initialized, 1, Property::FloatDefault, 1, outputName);
    BufferWriteAccess<FloatType, access_mode::discard_write> yAcc(yValues);
    yAcc[0] = static_cast<FloatType>(value);

    table->setX(std::move(xValues));
    table->setY(std::move(yValues));
}

void finalizePairScalarOutput(PipelineFlowState& state,
                              const OOWeakRef<const PipelineNode>& createdByNode,
                              const QString& outputName,
                              const std::vector<IdentifierIntType>& sourceIds,
                              const std::vector<IdentifierIntType>& targetIds,
                              const std::vector<double>& values,
                              const ReductionAccumulator& summary)
{
    DataTable* table = state.createObject<DataTable>(
        QStringLiteral("calculate-property-pair-values[%1]").arg(outputName),
        createdByNode,
        DataTable::Scatter,
        outputName);
    table->setAxisLabelX(CalculatePropertyModifier::tr("Pair index"));
    table->setAxisLabelY(outputName);
    table->setElementCount(values.size());

    PropertyPtr xValues = PropertyPtr::create(DataBuffer::Initialized, values.size(), DataBuffer::Int64, 1,
                                              CalculatePropertyModifier::tr("Pair Index"));
    BufferWriteAccess<int64_t, access_mode::discard_write> xAcc(xValues);
    for(size_t index = 0; index < values.size(); ++index)
        xAcc[index] = static_cast<int64_t>(index);

    PropertyPtr yValues = PropertyPtr::create(DataBuffer::Initialized, values.size(), Property::FloatDefault, 1,
                                              outputName);
    BufferWriteAccess<FloatType, access_mode::discard_write> yAcc(yValues);
    for(size_t index = 0; index < values.size(); ++index)
        yAcc[index] = static_cast<FloatType>(values[index]);

    PropertyPtr sourceIdProperty = PropertyPtr::create(DataBuffer::Initialized, values.size(), DataBuffer::Int64, 1,
                                                       CalculatePropertyModifier::tr("Source Particle"));
    BufferWriteAccess<int64_t, access_mode::discard_write> sourceIdAcc(sourceIdProperty);
    for(size_t index = 0; index < values.size(); ++index)
        sourceIdAcc[index] = static_cast<int64_t>(sourceIds[index]);

    PropertyPtr targetIdProperty = PropertyPtr::create(DataBuffer::Initialized, values.size(), DataBuffer::Int64, 1,
                                                       CalculatePropertyModifier::tr("Target Particle"));
    BufferWriteAccess<int64_t, access_mode::discard_write> targetIdAcc(targetIdProperty);
    for(size_t index = 0; index < values.size(); ++index)
        targetIdAcc[index] = static_cast<int64_t>(targetIds[index]);

    table->setX(std::move(xValues));
    table->setY(std::move(yValues));
    table->addProperty(std::move(sourceIdProperty));
    table->addProperty(std::move(targetIdProperty));

    state.setAttribute(QStringLiteral("CalculateProperty.%1.Count").arg(outputName), QVariant::fromValue(static_cast<qlonglong>(values.size())), createdByNode);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.Min").arg(outputName), QVariant::fromValue(summary.min), createdByNode);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.Mean").arg(outputName), QVariant::fromValue(summary.mean()), createdByNode);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.Max").arg(outputName), QVariant::fromValue(summary.max), createdByNode);
}

void finalizeVectorReductionOutput(PipelineFlowState& state,
                                   const OOWeakRef<const PipelineNode>& createdByNode,
                                   const QString& outputName,
                                   CalculatePropertyModifier::ReductionOperation operation,
                                   const VectorReductionAccumulator& accumulator)
{
    if(operation == CalculatePropertyModifier::NoReduction || !accumulator.valid())
        return;

    const std::array<double, 3> reduced = {
        reducedValue(accumulator.components[0], operation),
        reducedValue(accumulator.components[1], operation),
        reducedValue(accumulator.components[2], operation)
    };

    const QString reductionName = reductionLabel(operation);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.X.%2").arg(outputName, reductionName), QVariant::fromValue(reduced[0]), createdByNode);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.Y.%2").arg(outputName, reductionName), QVariant::fromValue(reduced[1]), createdByNode);
    state.setAttribute(QStringLiteral("CalculateProperty.%1.Z.%2").arg(outputName, reductionName), QVariant::fromValue(reduced[2]), createdByNode);

    DataTable* table = state.createObject<DataTable>(
        QStringLiteral("calculate-property-vector-reduction[%1]").arg(outputName),
        createdByNode,
        DataTable::BarChart,
        CalculatePropertyModifier::tr("%1 (%2)").arg(outputName, reductionName));
    table->setAxisLabelX(CalculatePropertyModifier::tr("Component"));
    table->setAxisLabelY(outputName);
    table->setElementCount(3);

    PropertyPtr xValues = PropertyPtr::create(DataBuffer::Initialized, 3, DataBuffer::Int64, 1,
                                              CalculatePropertyModifier::tr("Component Index"));
    BufferWriteAccess<int64_t, access_mode::discard_write> xAcc(xValues);
    xAcc[0] = 0;
    xAcc[1] = 1;
    xAcc[2] = 2;

    PropertyPtr yValues = PropertyPtr::create(DataBuffer::Initialized, 3, Property::FloatDefault, 1, outputName);
    BufferWriteAccess<FloatType, access_mode::discard_write> yAcc(yValues);
    for(size_t component = 0; component < 3; ++component)
        yAcc[component] = static_cast<FloatType>(reduced[component]);

    table->setX(std::move(xValues));
    table->setY(std::move(yValues));
}

QString formatVector(const Vector3& value)
{
    return QStringLiteral("(%1, %2, %3)")
        .arg(value.x())
        .arg(value.y())
        .arg(value.z());
}

QString substituteScriptVariable(QString expression, const QString& variableName, const QString& replacement)
{
    const QRegularExpression pattern(QStringLiteral("(?<![A-Za-z0-9_])%1(?![A-Za-z0-9_])")
        .arg(QRegularExpression::escape(variableName)));
    return expression.replace(pattern, QStringLiteral("(%1)").arg(replacement));
}

bool scriptUsesPairVariables(const QString& expression)
{
    return expression.contains(QStringLiteral("@i."))
        || expression.contains(QStringLiteral("@j."))
        || expression.contains(QRegularExpression(QStringLiteral("(?<![A-Za-z0-9_])(Distance|DistanceSquared|dx|dy|dz)(?![A-Za-z0-9_])")))
        || expression.contains(QStringLiteral("Delta.X"))
        || expression.contains(QStringLiteral("Delta.Y"))
        || expression.contains(QStringLiteral("Delta.Z"));
}

ParsedScript parseCalculationScript(const QString& scriptText)
{
    ParsedScript parsed;
    QHash<QString, QString> temporaries;
    QString scalarResult;
    std::array<QString, 3> vectorResults;
    bool hasScalarResult = false;
    bool hasVectorResult = false;

    const QStringList lines = scriptText.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    int lineNumber = 0;
    for(QString line : lines) {
        lineNumber++;
        line = line.trimmed();
        if(line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QStringLiteral("//")))
            continue;

        const int equalsPos = line.indexOf(QLatin1Char('='));
        if(equalsPos <= 0)
            throw Exception(CalculatePropertyModifier::tr("Invalid script line %1. Each non-empty line must be an assignment of the form <code>name = expression</code>.").arg(lineNumber));

        const QString lhs = line.left(equalsPos).trimmed();
        QString rhs = line.mid(equalsPos + 1).trimmed();
        if(lhs.isEmpty() || rhs.isEmpty())
            throw Exception(CalculatePropertyModifier::tr("Invalid script line %1. Each assignment needs both a left-hand side and a right-hand expression.").arg(lineNumber));
        if(!QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")).match(lhs).hasMatch())
            throw Exception(CalculatePropertyModifier::tr("Invalid variable name '%1' on script line %2. Use letters, digits, and underscores only.").arg(lhs).arg(lineNumber));

        for(auto iter = temporaries.constBegin(); iter != temporaries.constEnd(); ++iter)
            rhs = substituteScriptVariable(rhs, iter.key(), iter.value());

        if(lhs == QStringLiteral("result")) {
            scalarResult = rhs;
            hasScalarResult = true;
        }
        else if(lhs == QStringLiteral("X")) {
            vectorResults[0] = rhs;
            hasVectorResult = true;
        }
        else if(lhs == QStringLiteral("Y")) {
            vectorResults[1] = rhs;
            hasVectorResult = true;
        }
        else if(lhs == QStringLiteral("Z")) {
            vectorResults[2] = rhs;
            hasVectorResult = true;
        }
        else {
            temporaries.insert(lhs, rhs);
        }
    }

    if(hasScalarResult && hasVectorResult)
        throw Exception(CalculatePropertyModifier::tr("The script cannot define both <code>result</code> and vector outputs <code>X</code>, <code>Y</code>, <code>Z</code>. Choose either a scalar result or a vector result."));

    if(hasVectorResult) {
        if(vectorResults[0].isEmpty() || vectorResults[1].isEmpty() || vectorResults[2].isEmpty())
            throw Exception(CalculatePropertyModifier::tr("Vector scripts must assign all three components <code>X</code>, <code>Y</code>, and <code>Z</code>."));
        parsed.vectorMode = true;
        parsed.vectorExpressions = vectorResults;
        parsed.pairMode = scriptUsesPairVariables(vectorResults[0]) || scriptUsesPairVariables(vectorResults[1]) || scriptUsesPairVariables(vectorResults[2]);
        if(parsed.pairMode)
            throw Exception(CalculatePropertyModifier::tr("Pair scripts currently support only a scalar <code>result</code>. Vector pair outputs are not supported yet."));
        return parsed;
    }

    if(!hasScalarResult)
        throw Exception(CalculatePropertyModifier::tr("The script must assign either <code>result = ...</code> for a scalar output or all three of <code>X</code>, <code>Y</code>, and <code>Z</code> for a vector output."));

    parsed.scalarExpression = scalarResult;
    parsed.pairMode = scriptUsesPairVariables(scalarResult);
    return parsed;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(CalculatePropertyModifier);
OVITO_CLASSINFO(CalculatePropertyModifier, "DisplayName", "Calculate property");
OVITO_CLASSINFO(CalculatePropertyModifier, "Description", "Calculate predefined derived particle properties.");
OVITO_CLASSINFO(CalculatePropertyModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, propertyType);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, groupingMode);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, reductionOperation);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, fromTypes);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, fromExpression);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, toTypes);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, toExpression);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, expression);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, script);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, expressionX);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, expressionY);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, expressionZ);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, outputPropertyName);
DEFINE_PROPERTY_FIELD(CalculatePropertyModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, propertyType, "Property");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, groupingMode, "Group by");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, reductionOperation, "Reduction");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, fromTypes, "From atom type(s)");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, fromExpression, "From selector override");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, toTypes, "To atom type(s)");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, toExpression, "To selector override");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, expression, "Expression");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, script, "Script");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, expressionX, "X expression");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, expressionY, "Y expression");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, expressionZ, "Z expression");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, outputPropertyName, "Output property name");
SET_PROPERTY_FIELD_LABEL(CalculatePropertyModifier, onlySelectedParticles, "Use only selected particles");

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool CalculatePropertyModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> CalculatePropertyModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    const PropertyType recipe = propertyType();
    const GroupingMode grouping = groupingMode();
    const ReductionOperation reduction = reductionOperation();
    const bool selectedOnly = onlySelectedParticles();
    const QString selectorFromTypes = fromTypes();
    const QString selectorFromExpression = fromExpression();
    const QString selectorToTypes = toTypes();
    const QString selectorToExpression = toExpression();
    const QString expressionText = expression().trimmed();
    const QString scriptText = script().trimmed();
    const QString expressionXText = expressionX().trimmed();
    const QString expressionYText = expressionY().trimmed();
    const QString expressionZText = expressionZ().trimmed();
    const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();

    PropertyType effectiveRecipe = recipe;
    QString scalarExpressionText = expressionText;
    std::array<QString, 3> vectorExpressionTexts = {expressionXText, expressionYText, expressionZText};
    QString computedModeLabel = propertyTypeLabel(recipe);
    if(!scriptText.isEmpty()) {
        const ParsedScript parsedScript = parseCalculationScript(scriptText);
        if(parsedScript.vectorMode) {
            effectiveRecipe = VectorExpression;
            vectorExpressionTexts = parsedScript.vectorExpressions;
        }
        else if(parsedScript.pairMode) {
            effectiveRecipe = PairExpression;
            scalarExpressionText = parsedScript.scalarExpression;
        }
        else {
            effectiveRecipe = ParticleExpression;
            scalarExpressionText = parsedScript.scalarExpression;
        }
        computedModeLabel = tr("script");
    }
    const QString outputName = outputPropertyName().trimmed().isEmpty() ? defaultOutputPropertyName(effectiveRecipe) : outputPropertyName().trimmed();

    if(effectiveRecipe == PairExpression && grouping != NoGrouping)
        throw Exception(tr("Pair scripts do not support grouping yet. Set 'Group by' to 'None' for pair calculations."));

    BufferReadAccess<SelectionIntType> selection(selectedOnly ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(selectedOnly && !selection) {
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection. Add a selection modifier upstream or disable this option."));
    }

    if(effectiveRecipe == DipoleDirection || effectiveRecipe == ManualMolecularDirection) {
        BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
        BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
        if(!moleculeIds) {
            throw Exception(tr("The selected property calculation requires the particle property 'Molecule Identifier'. Load molecular topology or create that property upstream first."));
        }

        BufferReadAccess<FloatType> charges(effectiveRecipe == DipoleDirection ? particles->getProperty(Particles::ChargeProperty) : nullptr);
        if(effectiveRecipe == DipoleDirection && !charges) {
            throw Exception(tr("The selected property calculation requires the particle property 'Charge'."));
        }

        const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
        BufferReadAccess<int32_t> particleTypes(typeProperty ? typeProperty : nullptr);

        std::vector<uint8_t> fromMask;
        std::vector<uint8_t> toMask;
        if(effectiveRecipe == ManualMolecularDirection) {
            if(!typeProperty && selectorFromExpression.trimmed().isEmpty() && selectorToExpression.trimmed().isEmpty())
                throw Exception(tr("The manual molecular direction mode requires the particle property 'Particle Type' or expression overrides."));

            if(canonicalizeParticleSelector(selectorFromTypes, selectorFromExpression)
                == canonicalizeParticleSelector(selectorToTypes, selectorToExpression)) {
                throw Exception(tr("The manual molecular direction mode requires different source and target selectors."));
            }

            fromMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes,
                                                selectorFromTypes, selectorFromExpression,
                                                tr("source selector"),
                                                tr("Manual molecular direction"));
            toMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes,
                                              selectorToTypes, selectorToExpression,
                                              tr("target selector"),
                                              tr("Manual molecular direction"));
        }

        PropertyPtr vectorProperty;
        PropertyPtr magnitudeProperty;
        if(effectiveRecipe == DipoleDirection) {
            vectorProperty =
                Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleOrientationProperty);
            magnitudeProperty =
                Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particles->elementCount(), Particles::DipoleMagnitudeProperty);
        }
        else {
            vectorProperty = PropertyPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 3,
                                                 QStringLiteral("Molecular Direction"), 0, QStringList() << "X" << "Y" << "Z");
            if(vectorProperty->visElements().empty()) {
                OORef<VectorVis> vis = OORef<VectorVis>::create();
                vis->setObjectTitle(tr("Molecular Direction"));
                vis->setEnabled(false);
                vis->setReverseArrowDirection(false);
                vis->setArrowPosition(VectorVis::Center);
                vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
                vectorProperty->addVisElement(std::move(vis));
            }

            magnitudeProperty = PropertyPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 1,
                                                    QStringLiteral("Molecular Direction Magnitude"));
        }

        const SimulationCell* cell = state.getObject<SimulationCell>();
        return asyncLaunch([
                state = std::move(state),
                positions = std::move(positions),
                charges = std::move(charges),
                moleculeIds = std::move(moleculeIds),
                selection = std::move(selection),
                vectorProperty = std::move(vectorProperty),
                magnitudeProperty = std::move(magnitudeProperty),
                fromMask = std::move(fromMask),
                toMask = std::move(toMask),
                cell,
                recipe = effectiveRecipe,
                selectedOnly]() mutable
        {
            std::unordered_map<IdentifierIntType, size_t> groupLookup;
            groupLookup.reserve(positions.size());
            std::vector<MoleculeGroup> molecules;
            molecules.reserve(positions.size());

            for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
                const IdentifierIntType moleculeId = moleculeIds[particleIndex];
                auto [iter, inserted] = groupLookup.try_emplace(moleculeId, molecules.size());
                if(inserted)
                    molecules.emplace_back();

                MoleculeGroup& group = molecules[iter->second];
                group.indices.push_back(particleIndex);
                if(selection && selection[particleIndex])
                    group.anySelected = true;
            }

            BufferWriteAccess<Vector3, access_mode::discard_write> directionAcc(vectorProperty);
            BufferWriteAccess<FloatType, access_mode::discard_write> magnitudeAcc(magnitudeProperty);

            std::vector<Point3> moleculePositions;
            size_t processedMoleculeCount = 0;
            size_t zeroMagnitudeCount = 0;
            size_t skippedMissingTypeCount = 0;

            for(const MoleculeGroup& group : molecules) {
                if(group.indices.empty())
                    continue;
                if(selectedOnly && !group.anySelected)
                    continue;

                moleculePositions.clear();
                moleculePositions.reserve(group.indices.size());

                const Point3 reference = positions[group.indices.front()];
                moleculePositions.push_back(reference);
                for(size_t atomListIndex = 1; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const Point3 current = positions[group.indices[atomListIndex]];
                    Vector3 delta = current - reference;
                    if(cell)
                        delta = cell->wrapVector(delta);
                    moleculePositions.push_back(reference + delta);
                }

                Vector3 centerOffsetSum = Vector3::Zero();
                for(const Point3& position : moleculePositions)
                    centerOffsetSum += (position - reference);
                const Point3 center = reference + centerOffsetSum / static_cast<FloatType>(moleculePositions.size());

                Vector3 orientationVector = Vector3::Zero();
                switch(recipe) {
                case DipoleDirection:
                    for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex)
                        orientationVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - center);
                    break;
                case ManualMolecularDirection: {
                    Vector3 fromCentroidOffset = Vector3::Zero();
                    Vector3 toCentroidOffset = Vector3::Zero();
                    size_t fromCount = 0;
                    size_t toCount = 0;
                    for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                        const size_t particleIndex = group.indices[atomListIndex];
                        const Point3& position = moleculePositions[atomListIndex];
                        if(fromMask[particleIndex]) {
                            fromCentroidOffset += (position - reference);
                            fromCount++;
                        }
                        if(toMask[particleIndex]) {
                            toCentroidOffset += (position - reference);
                            toCount++;
                        }
                    }
                    if(fromCount == 0 || toCount == 0) {
                        skippedMissingTypeCount++;
                        continue;
                    }
                    const Point3 fromCentroid = reference + fromCentroidOffset / static_cast<FloatType>(fromCount);
                    const Point3 toCentroid = reference + toCentroidOffset / static_cast<FloatType>(toCount);
                    orientationVector = toCentroid - fromCentroid;
                    break;
                }
                default:
                    OVITO_ASSERT(false);
                    break;
                }

                const FloatType orientationMagnitude = orientationVector.length();
                const Vector3 orientationDirection = (orientationMagnitude > FloatType(0)) ? (orientationVector / orientationMagnitude) : Vector3::Zero();
                if(orientationMagnitude <= FloatType(0))
                    zeroMagnitudeCount++;

                for(size_t particleIndex : group.indices) {
                    directionAcc[particleIndex] = orientationDirection;
                    magnitudeAcc[particleIndex] = orientationMagnitude;
                }

                processedMoleculeCount++;
            }

            if(selectedOnly && processedMoleculeCount == 0) {
                if(recipe == ManualMolecularDirection && skippedMissingTypeCount > 0) {
                    throw Exception(tr("The selected molecules do not contain both requested selectors. The manual molecular direction uses the centroid of the chosen source selector and the centroid of the chosen target selector within each molecule."));
                }
                throw Exception(tr("No molecules were selected. This modifier promotes the particle selection to whole molecules, so at least one particle in a molecule must be selected."));
            }
            if(recipe == ManualMolecularDirection && processedMoleculeCount == 0)
                throw Exception(tr("No molecules contained both selected selectors. The manual molecular direction uses the centroid of the chosen source selector and the centroid of the chosen target selector within each molecule."));

            Particles* outputParticles = state.expectMutableObject<Particles>();
            outputParticles->createProperty(std::move(vectorProperty));
            outputParticles->createProperty(std::move(magnitudeProperty));

            QString statusText;
            switch(recipe) {
            case DipoleDirection:
                statusText = selectedOnly
                    ? tr("Computed %1 for %2 selected molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount)
                    : tr("Computed %1 for %2 molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
                if(zeroMagnitudeCount > 0)
                    statusText += tr(" %1 molecules had zero dipole magnitude.").arg(zeroMagnitudeCount);
                break;
            case ManualMolecularDirection:
                statusText = selectedOnly
                    ? tr("Computed %1 for %2 selected molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount)
                    : tr("Computed %1 for %2 molecules.").arg(propertyTypeLabel(recipe)).arg(processedMoleculeCount);
                if(skippedMissingTypeCount > 0)
                    statusText += tr(" %1 molecules were skipped because they did not contain both selected selectors.").arg(skippedMissingTypeCount);
                if(zeroMagnitudeCount > 0)
                    statusText += tr(" %1 molecules had zero direction magnitude.").arg(zeroMagnitudeCount);
                break;
            default:
                OVITO_ASSERT(false);
                break;
            }
            state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(processedMoleculeCount)));
            return std::move(state);
        });
    }

    if(effectiveRecipe == KineticEnergy || effectiveRecipe == ParticleExpression || effectiveRecipe == VectorExpression) {
        const bool vectorMode = (effectiveRecipe == VectorExpression);
        BufferReadAccess<IdentifierIntType> moleculeIds(grouping == GroupByMolecule ? particles->getProperty(Particles::MoleculeProperty) : nullptr);
        std::vector<MoleculeGroup> moleculeGroups;
        if(grouping == GroupByMolecule) {
            if(!moleculeIds)
                throw Exception(tr("Grouping by molecule requires the particle property 'Molecule Identifier'."));
            moleculeGroups = buildMoleculeGroups(moleculeIds, selection);
        }

        ConstDataObjectPath containerPath = state.data()->expectObject(DataObjectReference(&Particles::OOClass()));
        const int animationFrame = state.data() ? std::max(0, state.data()->sourceFrame()) : 0;

        std::unique_ptr<ParticleExpressionEvaluator> evaluator;
        std::unique_ptr<PropertyExpressionEvaluator::Worker> worker;
        BufferReadAccess<Vector3> velocities;
        BufferReadAccess<FloatType> masses;
        if(effectiveRecipe == KineticEnergy) {
            velocities = particles->getProperty(Particles::VelocityProperty);
            masses = particles->getProperty(Particles::MassProperty);
            if(!velocities)
                throw Exception(tr("The kinetic energy mode requires the particle property 'Velocity'."));
            if(!masses)
                throw Exception(tr("The kinetic energy mode requires the particle property 'Mass'."));
        }
        if(effectiveRecipe == ParticleExpression) {
            if(scalarExpressionText.isEmpty())
                throw Exception(tr("Please enter an expression to evaluate."));
            evaluator = std::make_unique<ParticleExpressionEvaluator>();
            evaluator->initialize(QStringList(scalarExpressionText), state, containerPath, animationFrame);
            worker = std::make_unique<PropertyExpressionEvaluator::Worker>(*evaluator);
        }
        else if(effectiveRecipe == VectorExpression) {
            if(vectorExpressionTexts[0].isEmpty() || vectorExpressionTexts[1].isEmpty() || vectorExpressionTexts[2].isEmpty())
                throw Exception(tr("Please enter expressions for the X, Y, and Z components."));
            evaluator = std::make_unique<ParticleExpressionEvaluator>();
            evaluator->initialize(QStringList() << vectorExpressionTexts[0] << vectorExpressionTexts[1] << vectorExpressionTexts[2],
                                  state, containerPath, animationFrame);
            worker = std::make_unique<PropertyExpressionEvaluator::Worker>(*evaluator);
        }

        auto scalarContribution = [&](size_t particleIndex) -> double {
            if(effectiveRecipe == KineticEnergy) {
                const Vector3& velocity = velocities[particleIndex];
                return 0.5 * static_cast<double>(masses[particleIndex]) * static_cast<double>(velocity.squaredLength());
            }
            OVITO_ASSERT(worker);
            return worker->evaluate(particleIndex, 0);
        };
        auto vectorContribution = [&](size_t particleIndex) -> Vector3 {
            OVITO_ASSERT(worker);
            return Vector3(static_cast<FloatType>(worker->evaluate(particleIndex, 0)),
                           static_cast<FloatType>(worker->evaluate(particleIndex, 1)),
                           static_cast<FloatType>(worker->evaluate(particleIndex, 2)));
        };

        size_t includedEntityCount = 0;
        size_t contributingParticleCount = 0;

        if(!vectorMode) {
            std::vector<double> values(particles->elementCount(), 0.0);
            ReductionAccumulator accumulator;

            if(grouping == GroupByMolecule) {
                for(const MoleculeGroup& group : moleculeGroups) {
                    double groupValue = 0.0;
                    size_t groupContributorCount = 0;
                    for(size_t particleIndex : group.indices) {
                        if(selectedOnly && !selection[particleIndex])
                            continue;
                        groupValue += scalarContribution(particleIndex);
                        groupContributorCount++;
                    }
                    if(groupContributorCount == 0)
                        continue;
                    for(size_t particleIndex : group.indices)
                        values[particleIndex] = groupValue;
                    accumulator.add(groupValue);
                    includedEntityCount++;
                    contributingParticleCount += groupContributorCount;
                }
            }
            else {
                for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
                    if(selectedOnly && !selection[particleIndex])
                        continue;
                    const double value = scalarContribution(particleIndex);
                    values[particleIndex] = value;
                    accumulator.add(value);
                    includedEntityCount++;
                }
                contributingParticleCount = includedEntityCount;
            }

            if(includedEntityCount == 0) {
                if(selectedOnly)
                    throw Exception(grouping == GroupByMolecule
                        ? tr("No selected particles produced any molecule groups for the requested calculation.")
                        : tr("No selected particles were available for the requested calculation."));
                throw Exception(tr("The requested calculation did not produce any values."));
            }

            PropertyPtr outputProperty = createScalarProperty(outputName, particles->elementCount());
            BufferWriteAccess<FloatType, access_mode::discard_write> outputAcc(outputProperty);
            for(size_t particleIndex = 0; particleIndex < values.size(); ++particleIndex)
                outputAcc[particleIndex] = static_cast<FloatType>(values[particleIndex]);

            Particles* outputParticles = state.expectMutableObject<Particles>();
            outputParticles->createProperty(std::move(outputProperty));

            finalizeScalarReductionOutput(state, createdByNode, outputName, reduction, accumulator);

            const QString entityLabel = (grouping == GroupByMolecule) ? tr("molecules") : tr("particles");
            QString statusText = selectedOnly
                ? tr("Computed %1 for %2 selected %3.")
                    .arg(computedModeLabel)
                    .arg(includedEntityCount)
                    .arg(entityLabel)
                : tr("Computed %1 for %2 %3.")
                    .arg(computedModeLabel)
                    .arg(includedEntityCount)
                    .arg(entityLabel);
            if(grouping == GroupByMolecule)
                statusText += tr(" Molecule values are sums of the per-particle contributions.");
            if(reduction != NoReduction) {
                statusText += tr(" %1 = %2.")
                    .arg(reductionLabel(reduction))
                    .arg(reducedValue(accumulator, reduction));
            }
            state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(contributingParticleCount)));
            return Future<PipelineFlowState>::createImmediate(std::move(state));
        }

        std::vector<Vector3> values(particles->elementCount(), Vector3::Zero());
        VectorReductionAccumulator accumulator;

        if(grouping == GroupByMolecule) {
            for(const MoleculeGroup& group : moleculeGroups) {
                Vector3 groupValue = Vector3::Zero();
                size_t groupContributorCount = 0;
                for(size_t particleIndex : group.indices) {
                    if(selectedOnly && !selection[particleIndex])
                        continue;
                    groupValue += vectorContribution(particleIndex);
                    groupContributorCount++;
                }
                if(groupContributorCount == 0)
                    continue;
                for(size_t particleIndex : group.indices)
                    values[particleIndex] = groupValue;
                accumulator.add(groupValue);
                includedEntityCount++;
                contributingParticleCount += groupContributorCount;
            }
        }
        else {
            for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
                if(selectedOnly && !selection[particleIndex])
                    continue;
                const Vector3 value = vectorContribution(particleIndex);
                values[particleIndex] = value;
                accumulator.add(value);
                includedEntityCount++;
            }
            contributingParticleCount = includedEntityCount;
        }

        if(includedEntityCount == 0) {
            if(selectedOnly)
                throw Exception(grouping == GroupByMolecule
                    ? tr("No selected particles produced any molecule groups for the requested calculation.")
                    : tr("No selected particles were available for the requested calculation."));
            throw Exception(tr("The requested calculation did not produce any values."));
        }

        PropertyPtr outputProperty = createVectorProperty(outputName, particles->elementCount());
        BufferWriteAccess<Vector3, access_mode::discard_write> outputAcc(outputProperty);
        for(size_t particleIndex = 0; particleIndex < values.size(); ++particleIndex)
            outputAcc[particleIndex] = values[particleIndex];

        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->createProperty(std::move(outputProperty));

        finalizeVectorReductionOutput(state, createdByNode, outputName, reduction, accumulator);

        const QString entityLabel = (grouping == GroupByMolecule) ? tr("molecules") : tr("particles");
        QString statusText = selectedOnly
            ? tr("Computed %1 for %2 selected %3.")
                .arg(computedModeLabel)
                .arg(includedEntityCount)
                .arg(entityLabel)
            : tr("Computed %1 for %2 %3.")
                .arg(computedModeLabel)
                .arg(includedEntityCount)
                .arg(entityLabel);
        if(grouping == GroupByMolecule)
            statusText += tr(" Molecule vectors are sums of the per-particle vector contributions.");
        if(reduction != NoReduction) {
            const Vector3 reducedVector(static_cast<FloatType>(reducedValue(accumulator.components[0], reduction)),
                                        static_cast<FloatType>(reducedValue(accumulator.components[1], reduction)),
                                        static_cast<FloatType>(reducedValue(accumulator.components[2], reduction)));
            statusText += tr(" %1 = %2.").arg(reductionLabel(reduction), formatVector(reducedVector));
        }
        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(contributingParticleCount)));
        return Future<PipelineFlowState>::createImmediate(std::move(state));
    }

    OVITO_ASSERT(effectiveRecipe == PairDistances || effectiveRecipe == PairExpression);
    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<int32_t> particleTypes(typeProperty ? typeProperty : nullptr);
    std::vector<uint8_t> fromMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes,
                                                             selectorFromTypes, selectorFromExpression,
                                                             tr("source selector"),
                                                             tr("Pair calculation"));
    std::vector<uint8_t> toMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes,
                                                           selectorToTypes, selectorToExpression,
                                                           tr("target selector"),
                                                           tr("Pair calculation"));

    if(selectedOnly) {
        for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
            if(!selection[particleIndex]) {
                fromMask[particleIndex] = 0;
                toMask[particleIndex] = 0;
            }
        }
    }

    const QString canonicalFromSelector = canonicalizeParticleSelector(selectorFromTypes, selectorFromExpression);
    const QString canonicalToSelector = canonicalizeParticleSelector(selectorToTypes, selectorToExpression);
    const bool identicalSelectors = (canonicalFromSelector == canonicalToSelector);
    const SimulationCell* cell = state.getObject<SimulationCell>();

    std::vector<size_t> sourceIndices;
    std::vector<size_t> targetIndices;
    sourceIndices.reserve(particles->elementCount());
    targetIndices.reserve(particles->elementCount());
    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
        if(fromMask[particleIndex])
            sourceIndices.push_back(particleIndex);
        if(toMask[particleIndex])
            targetIndices.push_back(particleIndex);
    }

    if(sourceIndices.empty())
        throw Exception(tr("The source selector did not match any particles."));
    if(targetIndices.empty())
        throw Exception(tr("The target selector did not match any particles."));

    std::vector<IdentifierIntType> pairSourceIds;
    std::vector<IdentifierIntType> pairTargetIds;
    std::vector<double> pairValues;
    ReductionAccumulator pairSummary;

    std::unique_ptr<PairParticleExpressionEvaluator> pairEvaluator;
    std::unique_ptr<PropertyExpressionEvaluator::Worker> pairWorker;
    size_t pairOrdinal = 0;
    if(effectiveRecipe == PairExpression) {
        if(scalarExpressionText.isEmpty())
            throw Exception(tr("Please enter a pair expression to evaluate."));
        pairEvaluator = std::make_unique<PairParticleExpressionEvaluator>(positions, cell);
        ConstDataObjectPath containerPath = state.data()->expectObject(DataObjectReference(&Particles::OOClass()));
        const int animationFrame = state.data() ? std::max(0, state.data()->sourceFrame()) : 0;
        pairEvaluator->initialize(QStringList(scalarExpressionText), state, containerPath, animationFrame);
        pairWorker = std::make_unique<PropertyExpressionEvaluator::Worker>(*pairEvaluator);
    }

    if(identicalSelectors) {
        const size_t pairCountEstimate = sourceIndices.size() > 1 ? (sourceIndices.size() * (sourceIndices.size() - 1)) / 2 : 0;
        pairSourceIds.reserve(pairCountEstimate);
        pairTargetIds.reserve(pairCountEstimate);
        pairValues.reserve(pairCountEstimate);
        for(size_t i = 0; i < sourceIndices.size(); ++i) {
            for(size_t j = i + 1; j < sourceIndices.size(); ++j) {
                double pairValue = 0.0;
                if(effectiveRecipe == PairDistances) {
                    Vector3 delta = positions[sourceIndices[j]] - positions[sourceIndices[i]];
                    if(cell)
                        delta = cell->wrapVector(delta);
                    pairValue = delta.length();
                }
                else {
                    pairEvaluator->setCurrentPair(sourceIndices[i], sourceIndices[j]);
                    pairValue = pairWorker->evaluate(pairOrdinal++, 0);
                }
                pairSourceIds.push_back(static_cast<IdentifierIntType>(sourceIndices[i]));
                pairTargetIds.push_back(static_cast<IdentifierIntType>(sourceIndices[j]));
                pairValues.push_back(pairValue);
                pairSummary.add(pairValue);
            }
        }
    }
    else {
        const size_t pairCountEstimate = sourceIndices.size() * targetIndices.size();
        pairSourceIds.reserve(pairCountEstimate);
        pairTargetIds.reserve(pairCountEstimate);
        pairValues.reserve(pairCountEstimate);
        for(size_t sourceIndex : sourceIndices) {
            for(size_t targetIndex : targetIndices) {
                if(sourceIndex == targetIndex)
                    continue;
                double pairValue = 0.0;
                if(effectiveRecipe == PairDistances) {
                    Vector3 delta = positions[targetIndex] - positions[sourceIndex];
                    if(cell)
                        delta = cell->wrapVector(delta);
                    pairValue = delta.length();
                }
                else {
                    pairEvaluator->setCurrentPair(sourceIndex, targetIndex);
                    pairValue = pairWorker->evaluate(pairOrdinal++, 0);
                }
                pairSourceIds.push_back(static_cast<IdentifierIntType>(sourceIndex));
                pairTargetIds.push_back(static_cast<IdentifierIntType>(targetIndex));
                pairValues.push_back(pairValue);
                pairSummary.add(pairValue);
            }
        }
    }

    if(!pairSummary.valid())
        throw Exception(tr("No valid particle pairs matched the selected source and target selectors."));

    finalizePairScalarOutput(state, createdByNode, outputName, pairSourceIds, pairTargetIds, pairValues, pairSummary);
    finalizeScalarReductionOutput(state, createdByNode, outputName, reduction, pairSummary);
    QString statusText = tr("Computed %1 for %2 pairs. min=%3, mean=%4, max=%5.")
        .arg(computedModeLabel)
        .arg(pairValues.size())
        .arg(pairSummary.min)
        .arg(pairSummary.mean())
        .arg(pairSummary.max);
    if(reduction != NoReduction) {
        statusText += tr(" %1 = %2.").arg(reductionLabel(reduction)).arg(reducedValue(pairSummary, reduction));
    }
    state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(pairValues.size())));
    return Future<PipelineFlowState>::createImmediate(std::move(state));
}

}  // namespace Ovito
