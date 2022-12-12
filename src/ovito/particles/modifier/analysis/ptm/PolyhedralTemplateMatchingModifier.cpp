////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include "PolyhedralTemplateMatchingModifier.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(PolyhedralTemplateMatchingModifier);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, rmsdCutoff);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputRmsd);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputInteratomicDistance);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputOrientation);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputDeformationGradient);
DEFINE_PROPERTY_FIELD(PolyhedralTemplateMatchingModifier, outputOrderingTypes);
DEFINE_VECTOR_REFERENCE_FIELD(PolyhedralTemplateMatchingModifier, orderingTypes);
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, rmsdCutoff, "RMSD cutoff");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputRmsd, "Output RMSD values");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputInteratomicDistance, "Output interatomic distance");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputOrientation, "Output lattice orientations");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputDeformationGradient, "Output deformation gradients");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, outputOrderingTypes, "Output ordering types");
SET_PROPERTY_FIELD_LABEL(PolyhedralTemplateMatchingModifier, orderingTypes, "Ordering types");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PolyhedralTemplateMatchingModifier, rmsdCutoff, FloatParameterUnit, 0);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
PolyhedralTemplateMatchingModifier::PolyhedralTemplateMatchingModifier(ObjectCreationParams params) : StructureIdentificationModifier(params),
		_rmsdCutoff(0.1),
		_outputRmsd(false),
		_outputInteratomicDistance(false),
		_outputOrientation(false),
		_outputDeformationGradient(false),
		_outputOrderingTypes(false)
{
	if(params.createSubObjects()) {
		// Define the structure types.
		createStructureType(PTMAlgorithm::OTHER, ParticleType::PredefinedStructureType::OTHER, params);
		createStructureType(PTMAlgorithm::FCC, ParticleType::PredefinedStructureType::FCC, params);
		createStructureType(PTMAlgorithm::HCP, ParticleType::PredefinedStructureType::HCP, params);
		createStructureType(PTMAlgorithm::BCC, ParticleType::PredefinedStructureType::BCC, params);
		createStructureType(PTMAlgorithm::ICO, ParticleType::PredefinedStructureType::ICO, params)->setEnabled(false);
		createStructureType(PTMAlgorithm::SC, ParticleType::PredefinedStructureType::SC, params)->setEnabled(false);
		createStructureType(PTMAlgorithm::CUBIC_DIAMOND, ParticleType::PredefinedStructureType::CUBIC_DIAMOND, params)->setEnabled(false);
		createStructureType(PTMAlgorithm::HEX_DIAMOND, ParticleType::PredefinedStructureType::HEX_DIAMOND, params)->setEnabled(false);
		createStructureType(PTMAlgorithm::GRAPHENE, ParticleType::PredefinedStructureType::GRAPHENE, params)->setEnabled(false);

		// Define the ordering types.
		for(int id = 0; id < PTMAlgorithm::NUM_ORDERING_TYPES; id++) {
			OORef<ParticleType> otype = OORef<ParticleType>::create(params);
			otype->setNumericId(id);
			otype->initializeType(ParticlePropertyReference(QStringLiteral("Ordering Type")), params.loadUserDefaults());
			otype->setColor({0.75f, 0.75f, 0.75f});
			_orderingTypes.push_back(this, PROPERTY_FIELD(orderingTypes), std::move(otype));
		}
		orderingTypes()[PTMAlgorithm::ORDERING_NONE]->setColor({0.95f, 0.95f, 0.95f});
		orderingTypes()[PTMAlgorithm::ORDERING_NONE]->setName(tr("Other"));
		orderingTypes()[PTMAlgorithm::ORDERING_PURE]->setName(tr("Pure"));
		orderingTypes()[PTMAlgorithm::ORDERING_L10]->setName(tr("L10"));
		orderingTypes()[PTMAlgorithm::ORDERING_L12_A]->setName(tr("L12 (A-site)"));
		orderingTypes()[PTMAlgorithm::ORDERING_L12_B]->setName(tr("L12 (B-site)"));
		orderingTypes()[PTMAlgorithm::ORDERING_B2]->setName(tr("B2"));
		orderingTypes()[PTMAlgorithm::ORDERING_ZINCBLENDE_WURTZITE]->setName(tr("Zincblende/Wurtzite"));
		orderingTypes()[PTMAlgorithm::ORDERING_BORON_NITRIDE]->setName(tr("Boron/Nitride"));
	}
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void PolyhedralTemplateMatchingModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(rmsdCutoff)) {
		// Immediately update viewports when RMSD cutoff has been changed by the user.
		notifyDependents(ReferenceEvent::PreliminaryStateAvailable);
	}
	StructureIdentificationModifier::propertyChanged(field);
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the modifier's results.
******************************************************************************/
Future<AsynchronousModifier::EnginePtr> PolyhedralTemplateMatchingModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
	// Get modifier input.
	const ParticlesObject* particles = input.expectObject<ParticlesObject>();
	particles->verifyIntegrity();
	const PropertyObject* posProperty = particles->expectProperty(ParticlesObject::PositionProperty);
	const SimulationCellObject* simCell = input.expectObject<SimulationCellObject>();
	if(simCell->is2D())
		throw Exception(tr("The PTM modifier does not support 2D simulation cells."));

	// Get particle selection.
	const PropertyObject* selectionProperty = onlySelectedParticles() ? particles->expectProperty(ParticlesObject::SelectionProperty) : nullptr;

	// Get particle types if needed.
	const PropertyObject* typeProperty = outputOrderingTypes() ? particles->expectProperty(ParticlesObject::TypeProperty) : nullptr;

	return std::make_shared<PTMEngine>(request, posProperty, particles, typeProperty, simCell,
			structureTypes(), orderingTypes(), selectionProperty,
			outputInteratomicDistance(), outputOrientation(), outputDeformationGradient());
}

/******************************************************************************
* Compute engine constructor.
******************************************************************************/
PolyhedralTemplateMatchingModifier::PTMEngine::PTMEngine(const ModifierEvaluationRequest& request, ConstPropertyPtr positions, ParticleOrderingFingerprint fingerprint, ConstPropertyPtr particleTypes, const SimulationCellObject* simCell,
		const OORefVector<ElementType>& structureTypes, const OORefVector<ElementType>& orderingTypes, ConstPropertyPtr selection,
		bool outputInteratomicDistance, bool outputOrientation, bool outputDeformationGradient) :
	StructureIdentificationEngine(request, std::move(fingerprint), positions, simCell, structureTypes, std::move(selection)),
	_rmsd(ParticlesObject::OOClass().createUserProperty(positions->size(), PropertyObject::Float, 1, QStringLiteral("RMSD"))),
	_interatomicDistances(outputInteratomicDistance ? ParticlesObject::OOClass().createUserProperty(positions->size(), PropertyObject::Float, 1, QStringLiteral("Interatomic Distance"), DataBuffer::InitializeMemory) : nullptr),
	_orientations(outputOrientation ? ParticlesObject::OOClass().createStandardProperty(positions->size(), ParticlesObject::OrientationProperty, DataBuffer::InitializeMemory) : nullptr),
	_deformationGradients(outputDeformationGradient ? ParticlesObject::OOClass().createStandardProperty(positions->size(), ParticlesObject::ElasticDeformationGradientProperty, DataBuffer::InitializeMemory) : nullptr),
	_orderingTypes(particleTypes ? ParticlesObject::OOClass().createUserProperty(positions->size(), PropertyObject::Int, 1, QStringLiteral("Ordering Type"), DataBuffer::InitializeMemory) : nullptr),
	_correspondences(outputOrientation ? ParticlesObject::OOClass().createUserProperty(positions->size(), PropertyObject::Int64, 1, QStringLiteral("Correspondences"), DataBuffer::InitializeMemory) : nullptr),	// only output correspondences if orientations are selected
	_rmsdHistogram(DataTable::OOClass().createUserProperty(100, PropertyObject::Int64, 1, tr("Count"), DataBuffer::InitializeMemory))
{
	_algorithm.emplace();
	_algorithm->setCalculateDefGradient(outputDeformationGradient);
	_algorithm->setIdentifyOrdering(particleTypes);
	_algorithm->setRmsdCutoff(0.0); // Note: We do our own RMSD threshold filtering in postProcessStructureTypes().

	// Attach ordering types to output particle property.
	if(_orderingTypes) {
		// Create deep copies of the elements types, because data objects owned by the modifier should
		// not be passed to the data pipeline.
		for(const ElementType* type : orderingTypes) {
			// Attach element type to output particle property.
			_orderingTypes->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
		}
	}
}

/******************************************************************************
* Performs the actual analysis. This method is executed in a worker thread.
******************************************************************************/
void PolyhedralTemplateMatchingModifier::PTMEngine::perform()
{
//TODO: separate pre-calculation of neighbor ordering, so that we don't have to call it again unless the pipeline has changed.
//	i.e.: if user adds the option "Output RMSD", the old neighbor ordering is still valid.

	if(cell() && cell()->is2D())
		throw Exception(tr("The PTM modifier does not support 2D simulation cells."));

	// Specify the structure types the PTM should look for.
	for(int i = 0; i < PTMAlgorithm::NUM_STRUCTURE_TYPES; i++) {
		_algorithm->setStructureTypeIdentification(static_cast<PTMAlgorithm::StructureType>(i), typeIdentificationEnabled(i));
	}

	// Initialize the algorithm object.
	if(!_algorithm->prepare(positions(), cell(), selection()))
		return;

	// Get access to the particle selection flags.
	ConstPropertyAccess<int> selectionData(selection());

	setProgressMaximum(positions()->size());
	setProgressText(tr("Pre-calculating neighbor ordering"));

	// Pre-order neighbors of each particle.
	std::vector<uint64_t> cachedNeighbors(positions()->size());
	parallelForChunks(positions()->size(), *this, [&](size_t startIndex, size_t count, ProgressingTask& operation) {
		// Create a thread-local kernel for the PTM algorithm.
		PTMAlgorithm::Kernel kernel(*_algorithm);

		// Loop over input particles.
		size_t endIndex = startIndex + count;
		for(size_t index = startIndex; index < endIndex; index++) {

			// Update progress indicator.
			if((index % 256) == 0)
				operation.incrementProgressValue(256);

			// Break out of loop when operation was canceled.
			if(operation.isCanceled())
				break;

			// Skip particles that are not included in the analysis.
			if(selectionData && !selectionData[index])
				continue;

			// Calculate ordering of neighbors
			kernel.cacheNeighbors(index, &cachedNeighbors[index]);
		}
	});
	if(isCanceled())
		return;

	setProgressValue(0);
	setProgressText(tr("Performing polyhedral template matching"));

	// Get access to the output buffers that will receive the identified particle types and other data.
	PropertyAccess<int> outputStructureArray(structures());
	PropertyAccess<FloatType> rmsdArray(rmsd());
	PropertyAccess<FloatType> interatomicDistancesArray(interatomicDistances());
	PropertyAccess<Quaternion> orientationsArray(orientations());
	PropertyAccess<Matrix3> deformationGradientsArray(deformationGradients());
	PropertyAccess<int> orderingTypesArray(orderingTypes());
	PropertyAccess<qlonglong> correspondencesArray(correspondences());

	// Perform analysis on each particle.
	parallelForChunks(positions()->size(), *this, [&](size_t startIndex, size_t count, ProgressingTask& operation) {

		// Create a thread-local kernel for the PTM algorithm.
		PTMAlgorithm::Kernel kernel(*_algorithm);

		// Loop over input particles.
		size_t endIndex = startIndex + count;
		for(size_t index = startIndex; index < endIndex; index++) {

			// Update progress indicator.
			if((index % 256) == 0)
				operation.incrementProgressValue(256);

			// Break out of loop when operation was canceled.
			if(operation.isCanceled())
				break;

			// Skip particles that are not included in the analysis.
			if(selectionData && !selectionData[index]) {
				outputStructureArray[index] = PTMAlgorithm::OTHER;
				rmsdArray[index] = 0;
				continue;
			}

			// Perform the PTM analysis for the current particle.
			PTMAlgorithm::StructureType type = kernel.identifyStructure(index, cachedNeighbors);

			// Store results in the output arrays.
			outputStructureArray[index] = type;
			rmsdArray[index] = kernel.rmsd();
			if(type != PTMAlgorithm::OTHER) {
				if(interatomicDistancesArray) interatomicDistancesArray[index] = kernel.interatomicDistance();
				if(deformationGradientsArray) deformationGradientsArray[index] = kernel.deformationGradient();
				if(orientationsArray) orientationsArray[index] = kernel.orientation();
				if(orderingTypesArray) orderingTypesArray[index] = kernel.orderingType();
				if(correspondencesArray) correspondencesArray[index] = kernel.correspondence();
			}
		}
	});
	if(isCanceled())
		return;

	// Determine histogram bin size based on maximum RMSD value.
	const size_t numHistogramBins = _rmsdHistogram->size();
	FloatType rmsdHistogramBinSize = (rmsdArray.size() != 0) ? (FloatType(1.01) * *boost::max_element(rmsdArray) / numHistogramBins) : 0;
	if(rmsdHistogramBinSize <= 0) rmsdHistogramBinSize = 1;
	_rmsdHistogramRange = rmsdHistogramBinSize * numHistogramBins;

	// Perform binning of RMSD values.
	if(outputStructureArray.size() != 0) {
		PropertyAccess<qlonglong> histogramCounts(_rmsdHistogram);
		const int* structureType = outputStructureArray.cbegin();
		for(FloatType rmsdValue : rmsdArray) {
			if(*structureType++ != PTMAlgorithm::OTHER) {
				OVITO_ASSERT(rmsdValue >= 0);
				int binIndex = rmsdValue / rmsdHistogramBinSize;
				if(binIndex < numHistogramBins)
					histogramCounts[binIndex]++;
			}
		}
	}

	// Release data that is no longer needed.
	releaseWorkingData();
	_algorithm.reset();
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
PropertyPtr PolyhedralTemplateMatchingModifier::PTMEngine::postProcessStructureTypes(const ModifierEvaluationRequest& request, const PropertyPtr& structures)
{
	PolyhedralTemplateMatchingModifier* modifier = static_object_cast<PolyhedralTemplateMatchingModifier>(request.modifier());
	OVITO_ASSERT(modifier);

	// Enforce RMSD cutoff.
	FloatType rmsdCutoff = modifier->rmsdCutoff();
	if(rmsdCutoff > 0 && rmsd()) {

		// Start off with the original particle classifications and make a copy.
		PropertyPtr finalStructureTypes = structures.makeCopy();

		// Mark those particles whose RMSD exceeds the cutoff as 'OTHER'.
		ConstPropertyAccess<FloatType> rmdsArray(rmsd());
		PropertyAccess<int> structureTypesArray(finalStructureTypes);
		const FloatType* rmsdValue = rmdsArray.cbegin();
		for(int& type : structureTypesArray) {
			if(*rmsdValue++ > rmsdCutoff)
				type = PTMAlgorithm::OTHER;
		}

		// Replace old classifications with updated ones.
		return finalStructureTypes;
	}
	else {
		return structures;
	}
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void PolyhedralTemplateMatchingModifier::PTMEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	StructureIdentificationEngine::applyResults(request, state);

	// Also output structure type counts, which have been computed by the base class.
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.OTHER"), QVariant::fromValue(getTypeCount(PTMAlgorithm::OTHER)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.FCC"), QVariant::fromValue(getTypeCount(PTMAlgorithm::FCC)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.HCP"), QVariant::fromValue(getTypeCount(PTMAlgorithm::HCP)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.BCC"), QVariant::fromValue(getTypeCount(PTMAlgorithm::BCC)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.ICO"), QVariant::fromValue(getTypeCount(PTMAlgorithm::ICO)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.SC"), QVariant::fromValue(getTypeCount(PTMAlgorithm::SC)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.CUBIC_DIAMOND"), QVariant::fromValue(getTypeCount(PTMAlgorithm::CUBIC_DIAMOND)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.HEX_DIAMOND"), QVariant::fromValue(getTypeCount(PTMAlgorithm::HEX_DIAMOND)), request.modApp());
	state.addAttribute(QStringLiteral("PolyhedralTemplateMatching.counts.GRAPHENE"), QVariant::fromValue(getTypeCount(PTMAlgorithm::GRAPHENE)), request.modApp());

	PolyhedralTemplateMatchingModifier* modifier = static_object_cast<PolyhedralTemplateMatchingModifier>(request.modifier());
	OVITO_ASSERT(modifier);
	ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();

	// Output per-particle properties.
	if(rmsd() && modifier->outputRmsd()) {
		particles->createProperty(rmsd());
	}
	if(interatomicDistances() && modifier->outputInteratomicDistance()) {
		particles->createProperty(interatomicDistances());
	}
	if(modifier->outputOrientation()) {
		if(orientations()) {
			particles->createProperty(orientations());
		}
		if(correspondences()) {
			particles->createProperty(correspondences());
		}
	}
	if(deformationGradients() && modifier->outputDeformationGradient()) {
		particles->createProperty(deformationGradients());
	}
	if(orderingTypes() && modifier->outputOrderingTypes()) {
		particles->createProperty(orderingTypes());
	}

	// Output RMSD histogram.
	DataTable* table = state.createObject<DataTable>(QStringLiteral("ptm-rmsd"), request.modApp(), DataTable::Line, tr("RMSD distribution"), rmsdHistogram());
	table->setAxisLabelX(tr("RMSD"));
	table->setIntervalStart(0);
	table->setIntervalEnd(rmsdHistogramRange());
}

}	// End of namespace
