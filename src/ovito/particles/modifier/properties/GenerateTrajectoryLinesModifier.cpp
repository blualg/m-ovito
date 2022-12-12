////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/TrajectoryObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "GenerateTrajectoryLinesModifier.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(GenerateTrajectoryLinesModifier);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, useCustomInterval);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, customIntervalStart);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, customIntervalEnd);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, everyNthFrame);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, unwrapTrajectories);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, transferParticleProperties);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, particleProperty);
DEFINE_REFERENCE_FIELD(GenerateTrajectoryLinesModifier, trajectoryVis);
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, onlySelectedParticles, "Only selected particles");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, useCustomInterval, "Custom time interval");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, customIntervalStart, "Custom interval start");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, customIntervalEnd, "Custom interval end");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, unwrapTrajectories, "Unwrap trajectories");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, transferParticleProperties, "Sample particle property");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, particleProperty, "Particle property");
SET_PROPERTY_FIELD_UNITS(GenerateTrajectoryLinesModifier, customIntervalStart, TimeParameterUnit);
SET_PROPERTY_FIELD_UNITS(GenerateTrajectoryLinesModifier, customIntervalEnd, TimeParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(GenerateTrajectoryLinesModifier, everyNthFrame, IntegerParameterUnit, 1);

IMPLEMENT_OVITO_CLASS(GenerateTrajectoryLinesModifierApplication);
DEFINE_REFERENCE_FIELD(GenerateTrajectoryLinesModifierApplication, trajectoryData);
SET_MODIFIER_APPLICATION_TYPE(GenerateTrajectoryLinesModifier, GenerateTrajectoryLinesModifierApplication);

/******************************************************************************
* Constructor.
******************************************************************************/
GenerateTrajectoryLinesModifier::GenerateTrajectoryLinesModifier(ObjectCreationParams params) : Modifier(params),
	_onlySelectedParticles(true),
	_useCustomInterval(false),
	_customIntervalStart(0),
	_customIntervalEnd(0),
	_everyNthFrame(1),
	_unwrapTrajectories(true),
	_transferParticleProperties(false)
{
	if(params.createSubObjects()) {
		// Create the vis element for rendering the trajectories created by the modifier.
		setTrajectoryVis(OORef<TrajectoryVis>::create(params));
	}
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted into a pipeline.
******************************************************************************/
void GenerateTrajectoryLinesModifier::initializeModifier(const ModifierInitializationRequest& request)
{
	Modifier::initializeModifier(request);

	if(ExecutionContext::isInteractive()) {
		auto [firstFrame, lastFrame] = ExecutionContext::current().ui().datasetContainer().currentAnimationInterval();
		setCustomIntervalStart(firstFrame);
		setCustomIntervalEnd(lastFrame);
	}
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool GenerateTrajectoryLinesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
	return input.containsObject<ParticlesObject>();
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void GenerateTrajectoryLinesModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	// Inject the precomputed trajectory lines, which are stored in the modifier application, into the pipeline.
	if(GenerateTrajectoryLinesModifierApplication* myModApp = dynamic_object_cast<GenerateTrajectoryLinesModifierApplication>(request.modApp())) {
		if(myModApp->trajectoryData()) {
			state.addObject(myModApp->trajectoryData());
		}
	}
}

/******************************************************************************
* Updates the stored trajectories from the source particle object.
******************************************************************************/
bool GenerateTrajectoryLinesModifier::generateTrajectories(AnimationTime currentTime, MainThreadOperation& operation)
{
	OVITO_ASSERT(operation.isCurrent());
	
	for(ModifierApplication* modApp : modifierApplications()) {
		GenerateTrajectoryLinesModifierApplication* myModApp = dynamic_object_cast<GenerateTrajectoryLinesModifierApplication>(modApp);
		if(!myModApp) continue;

		// Get input particles.
		SharedFuture<PipelineFlowState> stateFuture = myModApp->evaluateInput(PipelineEvaluationRequest(currentTime));
		if(!stateFuture.waitForFinished())
			return false;

		const PipelineFlowState& state = stateFuture.result();
		const ParticlesObject* particles = state.getObject<ParticlesObject>();
		if(!particles)
			throw Exception(tr("Cannot generate trajectory lines. The pipeline data contains no particles."));
		particles->verifyIntegrity();

		// Determine set of input particles in the current frame.
		std::vector<size_t> selectedIndices;
		std::set<qlonglong> selectedIdentifiers;
		if(onlySelectedParticles()) {
			if(ConstPropertyAccess<int> selectionProperty = particles->getProperty(ParticlesObject::SelectionProperty)) {
				ConstPropertyAccess<qlonglong> identifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty);
				if(identifierProperty && identifierProperty.size() == selectionProperty.size()) {
					const int* s = selectionProperty.cbegin();
					for(auto id : identifierProperty)
						if(*s++) selectedIdentifiers.insert(id);
				}
				else {
					const int* s = selectionProperty.cbegin();
					for(size_t index = 0; index < selectionProperty.size(); index++)
						if(*s++) selectedIndices.push_back(index);
				}
			}
			if(selectedIndices.empty() && selectedIdentifiers.empty())
				throw Exception(tr("Cannot generate trajectory lines for selected particles. Particle selection has not been defined or selection set is empty."));
		}

		// Determine time interval over which trajectories should be generated.
		int startFrame, endFrame;
		if(useCustomInterval()) {
			startFrame = customIntervalStart();
			endFrame = customIntervalEnd();
		}
		else {
			startFrame = 0;
			endFrame = myModApp->numberOfSourceFrames() - 1;
		}
		if(startFrame >= endFrame)
			throw Exception(tr("The current simulation sequence consists only of a single frame. Thus, no trajectory lines were created."));

		// Generate list of animation times at which particle positions should be sampled.
		std::vector<int> sampleFrames;
		for(int frame = startFrame; frame <= endFrame; frame += everyNthFrame()) {
			sampleFrames.push_back(frame);
		}
		operation.setProgressMaximum(sampleFrames.size());

		// Collect particle positions to generate trajectory line vertices.
		std::vector<Point3> pointData;
		std::vector<int> timeData;
		std::vector<qlonglong> idData;
		std::vector<uint8_t> samplingPropertyData;
		std::vector<DataOORef<const SimulationCellObject>> cells;
		int timeIndex = 0;
		for(int frame : sampleFrames) {
			operation.setProgressText(tr("Generating trajectory lines (frame %1 of %2)").arg(operation.progressValue()+1).arg(operation.progressMaximum()));

			SharedFuture<PipelineFlowState> stateFuture = myModApp->evaluateInput(PipelineEvaluationRequest(myModApp->sourceFrameToAnimationTime(frame)));
			if(!stateFuture.waitForFinished())
				return false;

			const PipelineFlowState& state = stateFuture.result();
			const ParticlesObject* particles = state.getObject<ParticlesObject>();
			if(!particles)
				throw Exception(tr("Input data contains no particles at frame %1.").arg(frame));
			particles->verifyIntegrity();
			ConstPropertyAccess<Point3> posProperty = particles->expectProperty(ParticlesObject::PositionProperty);

			// Get the particle property to be sampled.
			ConstPropertyAccess<void,true> particleSamplingProperty;
			if(transferParticleProperties()) {
				if(particleProperty().isNull())
					throw Exception(tr("Please select a particle property to be sampled."));
				particleSamplingProperty = particleProperty().findInContainer(particles);
				if(!particleSamplingProperty)
					throw Exception(tr("The particle property '%1' to be sampled and transferred to the trajectory lines does not exist (at frame %2). "
						"Perhaps you need to restrict the sampling time interval to those times where the property is available.").arg(particleProperty().name()).arg(frame));
			}

			if(onlySelectedParticles()) {
				if(!selectedIdentifiers.empty()) {
					ConstPropertyAccess<qlonglong> identifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty);
					if(!identifierProperty || identifierProperty.size() != posProperty.size())
						throw Exception(tr("Input particles do not possess identifiers at frame %1.").arg(frame));

					// Create a mapping from IDs to indices.
					std::map<qlonglong,size_t> idmap;
					size_t index = 0;
					for(auto id : identifierProperty)
						idmap.insert(std::make_pair(id, index++));

					for(auto id : selectedIdentifiers) {
						if(auto entry = idmap.find(id); entry != idmap.end()) {
							pointData.push_back(posProperty[entry->second]);
							timeData.push_back(timeIndex);
							idData.push_back(id);
							if(particleSamplingProperty) {
								const uint8_t* dataBegin = particleSamplingProperty.cdata(entry->second, 0);
								samplingPropertyData.insert(samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
							}
						}
					}
				}
				else {
					// Add coordinates of selected particles by index.
					for(auto index : selectedIndices) {
						if(index < posProperty.size()) {
							pointData.push_back(posProperty[index]);
							timeData.push_back(timeIndex);
							idData.push_back(index);
							if(particleSamplingProperty) {
								const uint8_t* dataBegin = particleSamplingProperty.cdata(index, 0);
								samplingPropertyData.insert(samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
							}
						}
					}
				}
			}
			else {
				// Add coordinates of all particles.
				pointData.insert(pointData.end(), posProperty.cbegin(), posProperty.cend());
				ConstPropertyAccess<qlonglong> identifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty);
				if(identifierProperty && identifierProperty.size() == posProperty.size()) {
					// Particles with IDs.
					idData.insert(idData.end(), identifierProperty.cbegin(), identifierProperty.cend());
				}
				else {
					// Particles without IDs.
					idData.resize(idData.size() + posProperty.size());
					std::iota(idData.begin() + timeData.size(), idData.end(), 0);
				}
				timeData.resize(timeData.size() + posProperty.size(), timeIndex);
				if(particleSamplingProperty)
					samplingPropertyData.insert(samplingPropertyData.end(), particleSamplingProperty.cdata(), particleSamplingProperty.cdata() + particleSamplingProperty.size() * particleSamplingProperty.stride());
			}

			// Onbtain the simulation cell geometry at the current animation time.
			if(unwrapTrajectories()) {
				if(const SimulationCellObject* simCellObj = state.getObject<SimulationCellObject>()) {
					cells.push_back(simCellObj);
				}
				else {
					cells.push_back({});
				}
			}

			operation.incrementProgressValue(1);
			if(operation.isCanceled())
				return false;
			timeIndex++;
		}

		// Sort vertex data to obtain continuous trajectory lines.
		operation.setProgressMaximum(0);
		operation.setProgressText(tr("Sorting trajectory data"));
		std::vector<size_t> permutation(pointData.size());
		std::iota(permutation.begin(), permutation.end(), (size_t)0);
		std::sort(permutation.begin(), permutation.end(), [&idData, &timeData](size_t a, size_t b) {
			if(idData[a] < idData[b]) return true;
			if(idData[a] > idData[b]) return false;
			return timeData[a] < timeData[b];
		});
		if(operation.isCanceled())
			return false;

		// Do not create undo records while computing the trajectories.
		DataOORef<TrajectoryObject> trajObj = DataOORef<TrajectoryObject>::create();
		{
			UndoSuspender noUndo;
			
			// Copy re-ordered trajectory points.
			trajObj->setElementCount(pointData.size());
			PropertyAccess<Point3> trajPosProperty = trajObj->createProperty(TrajectoryObject::PositionProperty);
			auto piter = permutation.cbegin();
			for(Point3& p : trajPosProperty) {
				p = pointData[*piter++];
			}

			// Copy re-ordered trajectory time stamps.
			PropertyAccess<int> trajTimeProperty = trajObj->createProperty(TrajectoryObject::SampleTimeProperty);
			piter = permutation.cbegin();
			for(int& t : trajTimeProperty) {
				t = sampleFrames[timeData[*piter++]];
			}

			// Copy re-ordered trajectory ids.
			PropertyAccess<qlonglong> trajIdProperty = trajObj->createProperty(TrajectoryObject::ParticleIdentifierProperty);
			piter = permutation.cbegin();
			for(qlonglong& id : trajIdProperty) {
				id = idData[*piter++];
			}

			// Create the trajectory line property receiving the sampled particle property values.
			if(transferParticleProperties() && particleProperty() && particleProperty().type() != ParticlesObject::PositionProperty) {
				if(const PropertyObject* inputProperty = particleProperty().findInContainer(particles)) {
					OVITO_ASSERT(samplingPropertyData.size() == inputProperty->stride() * trajObj->elementCount());
					if(samplingPropertyData.size() != inputProperty->stride() * trajObj->elementCount())
						throw Exception(tr("Sampling buffer size mismatch. Sampled particle property '%1' seems to have a varying component count.").arg(inputProperty->name()));

					// Create a corresponding output property of the trajectory lines.
					PropertyAccess<void,true> samplingProperty;
					if(TrajectoryObject::OOClass().isValidStandardPropertyId(inputProperty->type())) {
						// Input particle property is also a standard property for trajectory lines.
						samplingProperty = trajObj->createProperty(inputProperty->type());
						OVITO_ASSERT(samplingProperty.dataType() == inputProperty->dataType());
						OVITO_ASSERT(samplingProperty.stride() == inputProperty->stride());
					}
					else if(TrajectoryObject::OOClass().standardPropertyTypeId(inputProperty->name()) != 0) {
						// Input property name is that of a standard property for trajectory lines.
						// Must rename the property to avoid naming conflict, because user properties may not have a standard property name.
						QString newPropertyName = inputProperty->name() + tr("_particles");
						samplingProperty = trajObj->createProperty(newPropertyName, inputProperty->dataType(), inputProperty->componentCount(), DataBuffer::NoFlags, inputProperty->componentNames());
					}
					else {
						// Input property is a user property for trajectory lines.
						samplingProperty = trajObj->createProperty(inputProperty->name(), inputProperty->dataType(), inputProperty->componentCount(), DataBuffer::NoFlags, inputProperty->componentNames());
					}

					// Copy property values from temporary sampling buffer to destination trajectory line property.
					const uint8_t* src = samplingPropertyData.data();
					uint8_t* dst = samplingProperty.data();
					size_t stride = samplingProperty.stride();
					piter = permutation.cbegin();
					for(size_t mapping : permutation) {
						OVITO_ASSERT(stride * (mapping + 1) <= samplingPropertyData.size());
						std::memcpy(dst, src + stride * mapping, stride);
						dst += stride;
					}
				}
			}

			if(operation.isCanceled())
				return false;

			// Unwrap trajectory vertices at periodic boundaries of the simulation cell.
			if(unwrapTrajectories() && pointData.size() >= 2 && !cells.empty() && cells.front() && cells.front()->hasPbcCorrected()) {
				operation.setProgressText(tr("Unwrapping trajectory lines"));
				operation.setProgressMaximum(trajPosProperty.size() - 1);
				Point3* pos = trajPosProperty.begin();
				piter = permutation.cbegin();
				const qlonglong* id = trajIdProperty.cbegin();
				for(auto pos_end = pos + trajPosProperty.size() - 1; pos != pos_end; ++pos, ++piter, ++id) {
					if(!operation.incrementProgressValue())
						return false;
					if(id[0] == id[1]) {
						const SimulationCellObject* cell1 = cells[timeData[piter[0]]];
						const SimulationCellObject* cell2 = cells[timeData[piter[1]]];
						if(cell1 && cell2) {
							const Point3& p1 = pos[0];
							Point3 p2 = pos[1];
							for(size_t dim = 0; dim < 3; dim++) {
								if(cell1->hasPbcCorrected(dim)) {
									FloatType reduced1 = cell1->inverseMatrix().prodrow(p1, dim);
									FloatType reduced2 = cell2->inverseMatrix().prodrow(p2, dim);
									FloatType delta = reduced2 - reduced1;
									FloatType shift = std::floor(delta + FloatType(0.5));
									if(shift != 0) {
										pos[1] -= cell2->matrix().column(dim) * shift;
									}
								}
							}
						}
					}
				}
			}

			trajObj->setVisElement(trajectoryVis());

			// Enable undo recording again from here on, because the trajectory line generation should be an undoable operation.
		}

		// Store generated trajectory lines in the ModifierApplication.
		myModApp->setTrajectoryData(std::move(trajObj));
	}
	return true;
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void GenerateTrajectoryLinesModifier::loadFromStreamComplete(ObjectLoadStream& stream)
{
	Modifier::loadFromStreamComplete(stream);

	// For backward compatibility with OVITO 3.7: 
	// Convert legacy time values from ticks to frames. This requires access to the AnimationSettings object, which is stored in the scene.
	if(stream.formatVersion() <= 30008) {
		if(ModifierApplication* modApp = someModifierApplication()) {
			QSet<PipelineSceneNode*> pipelines = modApp->pipelines(true);
			if(!pipelines.empty()) {
				if(Scene* scene = (*pipelines.begin())->scene()) {
					if(scene->animationSettings()) {
						int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
						setCustomIntervalStart(customIntervalStart() / ticksPerFrame);
						setCustomIntervalEnd(customIntervalEnd() / ticksPerFrame);
					}
				}
			}
		}
	}
}

}	// End of namespace
