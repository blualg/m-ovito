////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/dataset/DataSet.h>
#include "ParticlesComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesComputePropertyModifierDelegate);
OVITO_CLASSINFO(ParticlesComputePropertyModifierDelegate, "DisplayName", "Particles");
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, neighborExpressions);
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, cutoff);
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, useMultilineFields);
SET_PROPERTY_FIELD_LABEL(ParticlesComputePropertyModifierDelegate, neighborExpressions, "Neighbor expressions");
SET_PROPERTY_FIELD_LABEL(ParticlesComputePropertyModifierDelegate, cutoff, "Cutoff radius");
SET_PROPERTY_FIELD_LABEL(ParticlesComputePropertyModifierDelegate, useMultilineFields, "Expand field(s)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticlesComputePropertyModifierDelegate, cutoff, WorldParameterUnit, 0);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesComputePropertyModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Constructs a new instance of this class.
******************************************************************************/
ParticlesComputePropertyModifierDelegate::ParticlesComputePropertyModifierDelegate(ObjectInitializationFlags flags) : ComputePropertyModifierDelegate(flags),
    _cutoff(3),
    _useMultilineFields(false)
{
}

/******************************************************************************
* Sets the number of vector components of the property to compute.
******************************************************************************/
void ParticlesComputePropertyModifierDelegate::setComponentCount(int componentCount)
{
    if(componentCount < neighborExpressions().size()) {
        setNeighborExpressions(neighborExpressions().mid(0, componentCount));
    }
    else if(componentCount > neighborExpressions().size()) {
        QStringList newList = neighborExpressions();
        while(newList.size() < componentCount)
            newList.append(QString());
        setNeighborExpressions(newList);
    }
}

/******************************************************************************
 * Checks if math expressions are time-dependent, i.e. whether they involve the animation frame number.
 ******************************************************************************/
bool ParticlesComputePropertyModifierDelegate::isExpressionTimeDependent(ComputePropertyModifier* modifier) const
{
    if(ComputePropertyModifierDelegate::isExpressionTimeDependent(modifier))
        return true;

    for(const QString& expression : neighborExpressions()) {
        // This is a very simple check for the presence of the word "Frame" in the expression.
        // It's not perfect, but should catch all relevant cases (maybe more).
        if(expression.contains(QLatin1String("Frame")))
            return true;
    }

    return false;
}

/******************************************************************************
 * Launches the actual computations.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesComputePropertyModifierDelegate::performComputation(
    const ComputePropertyModifier* modifier,
    ComputePropertyModificationNode* modNode,
    PipelineFlowState state,
    const PipelineFlowState& originalState,
    PropertyPtr outputProperty,
    ConstPropertyPtr selectionProperty,
    int frame) const
{
    if(!neighborExpressions().empty() && neighborExpressions().size() != outputProperty->componentCount() && (neighborExpressions().size() != 1 || !neighborExpressions().front().isEmpty()))
        throw Exception(tr("Number of neighbor expressions that have been specified (%1) does not match the number of components per particle (%2) of the output property '%3'.")
            .arg(neighborExpressions().size()).arg(outputProperty->componentCount()).arg(outputProperty->name()));

    const Particles* particles = originalState.expectObject<Particles>();

    // Initialize expression evaluator.
    auto evaluator = std::make_unique<ParticleExpressionEvaluator>();
    evaluator->initialize(modifier->expressions(), originalState, originalState.expectObject(inputContainerRef()), frame);

    // Make sure we have the right number of expression strings.
    QStringList neighborExpressions = this->neighborExpressions();
    while((size_t)neighborExpressions.size() < outputProperty->componentCount())
        neighborExpressions.append(QString());
    while((size_t)neighborExpressions.size() > outputProperty->componentCount())
        neighborExpressions.pop_back();

    // Determine whether any neighbor expressions are present.
    bool neighborMode = false;
    for(QString& expr : neighborExpressions) {
        if(expr.trimmed().isEmpty())
            expr = QStringLiteral("0");
        else if(expr.trimmed() != QStringLiteral("0"))
            neighborMode = true;
    }

    evaluator->registerGlobalParameter("Cutoff", cutoff());
    evaluator->registerGlobalParameter("NumNeighbors", 0);

    auto neighborEvaluator = std::make_unique<ParticleExpressionEvaluator>();
    neighborEvaluator->initialize(neighborExpressions, originalState, originalState.expectObject(inputContainerRef()), frame);
    neighborEvaluator->registerGlobalParameter("Cutoff", _cutoff);
    neighborEvaluator->registerGlobalParameter("NumNeighbors", 0);
    neighborEvaluator->registerGlobalParameter("Distance", 0);
    neighborEvaluator->registerGlobalParameter("Delta.X", 0);
    neighborEvaluator->registerGlobalParameter("Delta.Y", 0);
    neighborEvaluator->registerGlobalParameter("Delta.Z", 0);
    neighborEvaluator->registerIndexVariable(QStringLiteral("@") + neighborEvaluator->indexVarName(), 1);

    // Build list of properties that will be made available as expression variables.
    std::vector<ConstPropertyPtr> inputProperties;
    for(const Property* prop : particles->properties()) {
        inputProperties.push_back(prop);
    }
    neighborEvaluator->registerPropertyVariables(inputProperties, 1, _T("@"));

    // Activate neighbor mode if NumNeighbors variable is referenced in the central particle expression(s).
    if(evaluator->isVariableUsed(_T("NumNeighbors")))
        neighborMode = true;

    // Store the list of input variables in the ModificationNode so that the UI component can display it to the user.
    modNode->setInputVariableNames(evaluator->inputVariableNames());
    modNode->setDelegateInputVariableNames(neighborEvaluator->inputVariableNames());
    QString variableTable = evaluator->inputVariableTable();
    variableTable.append(QStringLiteral("<p><b>Neighbor expression variables:</b><ul>"));
    variableTable.append(QStringLiteral("<li>Cutoff (<i style=\"color: #555;\">radius</i>)</li>"));
    variableTable.append(QStringLiteral("<li>NumNeighbors (<i style=\"color: #555;\">of central particle</i>)</li>"));
    variableTable.append(QStringLiteral("<li>Distance (<i style=\"color: #555;\">from central particle</i>)</li>"));
    variableTable.append(QStringLiteral("<li>Delta.X (<i style=\"color: #555;\">neighbor vector component</i>)</li>"));
    variableTable.append(QStringLiteral("<li>Delta.Y (<i style=\"color: #555;\">neighbor vector component</i>)</li>"));
    variableTable.append(QStringLiteral("<li>Delta.Z (<i style=\"color: #555;\">neighbor vector component</i>)</li>"));
    variableTable.append(QStringLiteral("<li>@... (<i style=\"color: #555;\">central particle properties</i>)</li>"));
    variableTable.append(QStringLiteral("</ul></p>"));
    modNode->setInputVariableTable(std::move(variableTable));

    // Notify the UI component that the list of variables should be refreshed.
    modifier->notifyDependents(ReferenceEvent::ObjectStatusChanged);
    modNode->notifyDependents(ReferenceEvent::ObjectStatusChanged);

    // Get the input particle positions.
    ConstPropertyPtr positions = particles->expectProperty(Particles::PositionProperty);

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            outputProperty = std::move(outputProperty),
            selectionProperty = std::move(selectionProperty),
            evaluator = std::move(evaluator),
            neighborEvaluator = std::move(neighborEvaluator),
            positions = std::move(positions),
            neighborMode,
            cutoff = cutoff()]() mutable
    {
        this_task::setProgressText(tr("Computing property '%1'").arg(outputProperty->name()));

        // Prepare the neighbor finder (only used when neighbor mode is active).
        CutoffNeighborFinder neighborFinder;
        if(neighborMode)
            neighborFinder.prepare(cutoff, positions, neighborEvaluator->simCell(), {});

        RawBufferAccess<access_mode::write> outputAccessor(outputProperty, selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        BufferReadAccess<SelectionIntType> selectionAccessor(selectionProperty);

        // Per-worker data structure.
        struct WorkerData {
            ParticleExpressionEvaluator::Worker worker;
            ParticleExpressionEvaluator::Worker neighborWorker;
            double* distanceVar;
            double* deltaX;
            double* deltaY;
            double* deltaZ;
            double* selfNumNeighbors = nullptr;
            double* neighNumNeighbors = nullptr;

            WorkerData(ParticleExpressionEvaluator& evaluator, ParticleExpressionEvaluator& neighborEvaluator, bool neighborMode) : worker(evaluator), neighborWorker(neighborEvaluator)
            {
                if(neighborMode) {
                    distanceVar = neighborWorker.variableAddress(_T("Distance"));
                    deltaX = neighborWorker.variableAddress(_T("Delta.X"));
                    deltaY = neighborWorker.variableAddress(_T("Delta.Y"));
                    deltaZ = neighborWorker.variableAddress(_T("Delta.Z"));
                    selfNumNeighbors = worker.variableAddress(_T("NumNeighbors"));
                    neighNumNeighbors = neighborWorker.variableAddress(_T("NumNeighbors"));
                    if(!worker.isVariableUsed(_T("NumNeighbors")) && !neighborWorker.isVariableUsed(_T("NumNeighbors")))
                        selfNumNeighbors = neighNumNeighbors = nullptr;
                }
            }
        };

        EnumerableThreadSpecific<WorkerData> workerData;
        size_t componentCount = outputAccessor.componentCount();

        parallelForInnerOuter(outputProperty->size(), 4096, [&](auto&& iterate) {
            WorkerData& wd = workerData.create(*evaluator, *neighborEvaluator, neighborMode);
            iterate([&](size_t i) {

                // Skip unselected particles if requested.
                if(selectionAccessor && !selectionAccessor[i])
                    return;

                if(wd.selfNumNeighbors) {
                    // Determine number of neighbors (only if this value is being referenced in the expressions).
                    int nneigh = 0;
                    for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next())
                        nneigh++;
                    *wd.selfNumNeighbors = *wd.neighNumNeighbors = nneigh;
                }

                // Update neighbor expression variables that provide access to the properties of the central particle.
                if(neighborMode)
                    wd.neighborWorker.updateVariables(1, i);

                for(size_t component = 0; component < componentCount; component++) {
                    // Compute central term.
                    FloatType value = wd.worker.evaluate(i, component);

                    if(neighborMode) {
                        // Compute and add neighbor terms.
                        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next()) {
                            *wd.distanceVar = neighQuery.distance();
                            *wd.deltaX = neighQuery.delta().x();
                            *wd.deltaY = neighQuery.delta().y();
                            *wd.deltaZ = neighQuery.delta().z();
                            value += wd.neighborWorker.evaluate(neighQuery.current(), component);
                        }
                    }

                    // Store results in output property.
                    outputAccessor.set(i, component, value);
                }
            });
        });

        return std::move(state);
    }, true);
}

}   // End of namespace
