////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticleBondMap.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/dataset/DataSet.h>
#include "ParticlesComputePropertyModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesComputePropertyModifierDelegate);
OVITO_CLASSINFO(ParticlesComputePropertyModifierDelegate, "DisplayName", "Particles");
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, neighborExpressions);
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, neighborMode);
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, cutoff);
DEFINE_PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate, useMultilineFields);
SET_PROPERTY_FIELD_LABEL(ParticlesComputePropertyModifierDelegate, neighborExpressions, "Neighbor expressions");
SET_PROPERTY_FIELD_LABEL(ParticlesComputePropertyModifierDelegate, neighborMode, "Neighbor mode");
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
    // To update the list of vector components in the UI.
    notifyDependents(ReferenceEvent::ObjectStatusChanged);
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
* Sets up the visual element(s) associated with the new property.
******************************************************************************/
void ParticlesComputePropertyModifierDelegate::setupVisualElements(Property* outputProperty, ComputePropertyModificationNode* modNode)
{
    // Automatically create a VectorVis element for user-defined vector properties with 3 components named X,Y,Z.
    if(outputProperty->typeId() == Property::GenericUserProperty && outputProperty->componentCount() == 3 && outputProperty->dataType() == Property::Float64) {
        const QStringList& names = outputProperty->componentNames();
        if(names.size() == 3 && names[0].compare("x", Qt::CaseInsensitive) == 0 && names[1].compare("y", Qt::CaseInsensitive) == 0 && names[2].compare("z", Qt::CaseInsensitive) == 0) {
            OORef<VectorVis> vis = OORef<VectorVis>::create();
            vis->setObjectTitle(outputProperty->name());
            vis->setEnabled(false);
            vis->setReverseArrowDirection(false);
            vis->setArrowPosition(VectorVis::Base);
            vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title), SHADOW_PROPERTY_FIELD(ActiveObject::isEnabled), SHADOW_PROPERTY_FIELD(VectorVis::reverseArrowDirection), SHADOW_PROPERTY_FIELD(VectorVis::arrowPosition)});
            outputProperty->addVisElement(std::move(vis));
        }
    }

    ComputePropertyModifierDelegate::setupVisualElements(outputProperty, modNode);
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
    bool visitNeighbors = false;
    for(QString& expr : neighborExpressions) {
        if(expr.trimmed().isEmpty())
            expr = QStringLiteral("0");
        else if(expr.trimmed() != QStringLiteral("0"))
            visitNeighbors = true;
    }

    evaluator->registerGlobalParameter("Cutoff", neighborMode() == Cutoff ? cutoff() : 0);
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
        visitNeighbors = true;

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
    variableTable.append(QStringLiteral("<li>@... (<i style=\"color: #555;\">center particle properties</i>)</li>"));
    variableTable.append(QStringLiteral("</ul></p>"));
    modNode->setInputVariableTable(std::move(variableTable));

    // Notify the UI component that the list of variables should be refreshed.
    modifier->notifyDependents(ReferenceEvent::ObjectStatusChanged);
    modNode->notifyDependents(ReferenceEvent::ObjectStatusChanged);

    // Get the input particle positions.
    ConstPropertyPtr positions = particles->expectProperty(Particles::PositionProperty);

    // Make sure bonds are present if we are in bonded neighbor mode.
    if(neighborMode() == Bonded) {
        particles->expectBonds()->verifyIntegrity();
    }

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            outputProperty = std::move(outputProperty),
            selectionProperty = std::move(selectionProperty),
            evaluator = std::move(evaluator),
            neighborEvaluator = std::move(neighborEvaluator),
            positions = std::move(positions),
            visitNeighbors,
            bonds = DataOORef<const Bonds>(neighborMode() == Bonded ? particles->bonds() : nullptr),
            cutoff = cutoff()]() mutable
    {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Computing property '%1'").arg(outputProperty->name()));

        // Prepare the neighbor finder (only used when cutoff neighbor mode is active).
        CutoffNeighborFinder neighborFinder;
        if(visitNeighbors && !bonds)
            neighborFinder.prepare(cutoff, positions, neighborEvaluator->simCell(), {});

        // Prepare bonds enumerator (only used when bonded neighbor mode is active).
        std::optional<ParticleBondMap> bondsMap;
        if(visitNeighbors && bonds)
            bondsMap.emplace(*bonds);

        RawBufferAccess<access_mode::write> outputAccessor(outputProperty, selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        BufferReadAccess<SelectionIntType> selectionAccessor(selectionProperty);
        BufferReadAccess<ParticleIndexPair> bondTopology(bonds ? bonds->getTopology() : nullptr);
        BufferReadAccess<Point3> positionAccess(positions);
        DataOORef<const SimulationCell> simCell = state.getObject<SimulationCell>();
        BufferReadAccess<Vector3I> periodicImages((bonds && simCell) ? bonds->getProperty(Bonds::PeriodicImageProperty) : nullptr);

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

        size_t particleCount = outputProperty->size();
        parallelForInnerOuter(particleCount, 4096, progress, [&](auto&& iterate) {
            WorkerData& wd = workerData.create(*evaluator, *neighborEvaluator, visitNeighbors);
            iterate([&](size_t i) {

                // Skip unselected particles if requested.
                if(selectionAccessor && !selectionAccessor[i])
                    return;

                if(wd.selfNumNeighbors) {
                    // Determine number of neighbors (only if this value is being referenced in the expressions).
                    int nneigh = 0;
                    if(!bonds) {
                        // Count neighbors within cutoff radius.
                        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next())
                            nneigh++;
                    }
                    else if(bondsMap) {
                        // Count bonded neighbors.
                        for(size_t neighborBondIndex : bondsMap->bondIndicesOfParticle(i)) {
                            OVITO_ASSERT(bondTopology[neighborBondIndex][0] == i || bondTopology[neighborBondIndex][1] == i);
                            nneigh++;
                        }
                    }
                    *wd.selfNumNeighbors = *wd.neighNumNeighbors = nneigh;
                }

                // Update neighbor expression variables that provide access to the properties of the central particle.
                if(visitNeighbors)
                    wd.neighborWorker.updateVariables(1, i);

                for(size_t component = 0; component < componentCount; component++) {
                    // Compute central term.
                    FloatType value = wd.worker.evaluate(i, component);

                    if(visitNeighbors) {
                        // Compute and add neighbor terms.
                        if(!bonds) {
                            for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next()) {
                                *wd.distanceVar = neighQuery.distance();
                                *wd.deltaX = neighQuery.delta().x();
                                *wd.deltaY = neighQuery.delta().y();
                                *wd.deltaZ = neighQuery.delta().z();
                                value += wd.neighborWorker.evaluate(neighQuery.current(), component);
                            }
                        }
                        else if(bondsMap && bondTopology) {
                            for(size_t bondIndex : bondsMap->bondIndicesOfParticle(i)) {
                                size_t index1 = bondTopology[bondIndex][0];
                                size_t index2 = bondTopology[bondIndex][1];
                                OVITO_ASSERT(index1 == i || index2 == i);
                                if(index1 >= particleCount || index2 >= particleCount)
                                    throw Exception(tr("Invalid bond topology. A particle index of bond %1 is out of range.").arg(bondIndex));
                                const Point3& p1 = positionAccess[index1];
                                const Point3& p2 = positionAccess[index2];
                                Vector3 delta = p2 - p1;
                                if(periodicImages) {
                                    if(int dx = periodicImages[bondIndex][0]) delta += simCell->matrix().column(0) * (FloatType)dx;
                                    if(int dy = periodicImages[bondIndex][1]) delta += simCell->matrix().column(1) * (FloatType)dy;
                                    if(int dz = periodicImages[bondIndex][2]) delta += simCell->matrix().column(2) * (FloatType)dz;
                                }
                                if(index2 == i) {
                                    delta = -delta;
                                    index2 = index1;
                                }
                                *wd.distanceVar = delta.length();
                                *wd.deltaX = delta.x();
                                *wd.deltaY = delta.y();
                                *wd.deltaZ = delta.z();
                                value += wd.neighborWorker.evaluate(index2, component);
                            }
                        }
                    }

                    // Store results in output property.
                    outputAccessor.set(i, component, value);
                }
            });
        });

        return std::move(state);
    });
}

}   // End of namespace
