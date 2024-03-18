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
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdmod/modifiers/ColorByTypeModifier.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/core/app/Application.h>
#include "StructureIdentificationModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(StructureIdentificationModifier);
DEFINE_VECTOR_REFERENCE_FIELD(StructureIdentificationModifier, structureTypes);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, colorByType);
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, structureTypes, "Structure types");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, colorByType, "Color particles by type");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
StructureIdentificationModifier::StructureIdentificationModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _onlySelectedParticles(false),
    _colorByType(true)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool StructureIdentificationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Create an instance of the ParticleType class to represent a structure type.
******************************************************************************/
ElementType* StructureIdentificationModifier::createStructureType(int id, ParticleType::PredefinedStructureType predefType)
{
    DataOORef<ElementType> stype = DataOORef<ElementType>::create();
    stype->setNumericId(id);
    stype->setName(ParticleType::getPredefinedStructureTypeName(predefType));
    stype->initializeType(ParticlePropertyReference(Particles::StructureTypeProperty));
    addStructureType(stype);
    return stype;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void StructureIdentificationModifier::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    Modifier::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x02);
    // For future use.
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void StructureIdentificationModifier::loadFromStream(ObjectLoadStream& stream)
{
    Modifier::loadFromStream(stream);
    stream.expectChunkRange(0, 2);
    // For future use.
    stream.closeChunk();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
bool StructureIdentificationModifier::preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        result.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Interactive);
    else
        result.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Noninteractive);

    return true;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> StructureIdentificationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            Particles* particles = state.expectMutableObject<Particles>();
            particles->verifyIntegrity();
            reuseCachedState(request, particles, state, cachedState);
        }
        return std::move(state);
    }

    // Phase I: Perform the structure identification. The results are cached in the node's partial cache.
    auto identificationFuture = request.modificationNode()->partialResultsCache().getOrCompute(state.data(), [&]() {

        // Get the input particles.
        DataOORef<const Particles> particles = state.expectObject<Particles>();
        particles->verifyIntegrity();

        // Get input particle selection.
        ConstPropertyPtr selection = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

        // Create the output structure property.
        PropertyPtr structures = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::StructureTypeProperty);

        // Create deep copies of the structure elements types, because data objects owned by the modifier should
        // not be passed to the data pipeline.
        for(const ElementType* type : structureTypes()) {
            OVITO_ASSERT(type && type->numericId() == structures->elementTypes().size());

            // Attach structure types to output particle property.
            structures->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
        }

        // Ask the subclass to create the engine that will perform the structure identification.
        std::shared_ptr<Algorithm> algorithm = createAlgorithm(request, state, std::move(structures));

        // Get the simulation cell (optional).
        DataOORef<const SimulationCell> simulationCell = state.getObject<SimulationCell>();

        // Perform the structure identification in a separate thread.
        return AsynchronousTask<std::shared_ptr<Algorithm>>::runAsync([
                algorithm = std::move(algorithm),
                particles = std::move(particles),
                simulationCell = std::move(simulationCell),
                selection = std::move(selection)]() mutable
        {
            // Run the algorithm.
            algorithm->identifyStructures(particles, simulationCell, selection);
            return std::move(algorithm);
        }, true);
    });

    // Phase II: Compute structure statistics.
    return identificationFuture.then(*this, [this, state = std::move(state), createdByNode = request.modificationNode()](std::shared_ptr<const Algorithm> algorithm) {

        // Perform the structure identification in a separate thread.
        return AsynchronousTask<PipelineFlowState>::runAsync([
                state = std::move(state),
                modifierParameters = algorithm->getModifierParameters(this),
                algorithm = std::move(algorithm),
                colorByType = colorByType(),
                createdByNode = std::move(createdByNode)]() mutable
        {
            // Post-process computed structure classifications.
            PropertyPtr structures = algorithm->postProcessStructureTypes(algorithm->structures(), modifierParameters);
            this_task::throwIfCanceled();

            // Add output property to the particles.
            Particles* particles = state.expectMutableObject<Particles>();
            particles->createProperty(structures);

            // Color particles based on their structural type (if requested).
            if(colorByType) {
                ColorByTypeModifier::colorByType(structures, particles);
                this_task::throwIfCanceled();
            }

            // Compute the structure identification statistics.
            algorithm->computeStructureStatistics(structures, state, createdByNode, modifierParameters);

            return std::move(state);
        });
    });
}

/******************************************************************************
* Computes the structure identification statistics.
******************************************************************************/
std::vector<int64_t> StructureIdentificationModifier::Algorithm::computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const
{
    // Count the number of particles of each identified type.
    int maxTypeId = 0;
    for(const ElementType* stype : structures->elementTypes()) {
        OVITO_ASSERT(stype->numericId() >= 0);
        maxTypeId = std::max(maxTypeId, stype->numericId());
    }

    std::vector<int64_t> counts(maxTypeId + 1);

#ifdef OVITO_USE_SYCL
    if(!counts.empty() && structures->size() != 0) {
        sycl::buffer<int64_t> countsBuf(counts.data(), counts.size());
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            auto countsAcc = countsBuf.get_access(cgh, sycl::write_only);
            SyclBufferAccess<int32_t, access_mode::read> typeAcc(structures, cgh);
            OVITO_SYCL_PARALLEL_FOR(cgh, StructureIdentificationModifier_countTypes)(sycl::range(typeAcc.size()), [=](size_t i) {
                auto t = typeAcc[i];
                if(t >= 0 && t < countsAcc.size())
                    sycl::atomic_ref<int64_t, sycl::memory_order::relaxed, sycl::memory_scope::device>(countsAcc[t]).fetch_add((int64_t)1);
            });
        });
    }
#else
    boost::fill(counts, 0);
    for(auto t : BufferReadAccess<int32_t>(structures)) {
        if(t >= 0 && t <= maxTypeId)
            counts[t]++;
    }
#endif

    // Create the property arrays for the bar chart.
    PropertyPtr typeCounts = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int64, 1, tr("Count"));
    boost::copy(counts, BufferWriteAccess<int64_t, access_mode::discard_write>(typeCounts).begin());
    PropertyPtr typeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int32, 1, tr("Structure type"));
    boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(typeIds).begin(), 0, typeIds->size());

    // Use the structure types as labels for the output bar chart.
    for(const ElementType* type : structures->elementTypes()) {
        if(type->enabled())
            typeIds->addElementType(type);
    }

    // Output a bar chart with the type counts.
    state.createObject<DataTable>(QStringLiteral("structures"), createdByNode, DataTable::BarChart, tr("Structure counts"), std::move(typeCounts), std::move(typeIds));

    return counts;
}

/******************************************************************************
* Adopts existing computation results for an interactive pipeline evaluation.
******************************************************************************/
void StructureIdentificationModifier::reuseCachedState(const ModifierEvaluationRequest& request, Particles* particles, PipelineFlowState& output, const PipelineFlowState& cachedState)
{
    // Adopt the structure property from the cached state.
    if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
        if(cachedParticles->elementCount() == particles->elementCount()) {
            if(const Property* cachedStructures = cachedParticles->getProperty(Particles::StructureTypeProperty)) {
                particles->createProperty(cachedStructures);
            }
            if(colorByType()) {
                if(const Property* cachedColors = cachedParticles->getProperty(Particles::ColorProperty)) {
                    particles->createProperty(cachedColors);
                }
            }
        }
    }

    // Adopt the structure count data table from the cached state.
    if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("structures"))) {
        output.addObject(cachedTable);
    }

    // Adopt all global attributes computed by the modifier from the cached state.
    for(const DataObject* obj : cachedState.data()->objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(attribute->createdByNode() == request.modificationNode()) {
                output.addObject(attribute);
            }
        }
    }
}

}   // End of namespace
