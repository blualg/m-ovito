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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticlesVis.h>
#include "ParticlePickingHelper.h"

namespace Ovito {

/******************************************************************************
* Finds the particle under the mouse cursor.
******************************************************************************/
bool ParticlePickingHelper::pickParticle(ViewportWindow* vpwin, const QPoint& clickPoint, PickResult& result)
{
    // Check if user has clicked on something.
    if(std::optional<ViewportWindow::PickResult> vpPickResult = vpwin->pick(clickPoint)) {

        // Check if that was a particle.
        ParticlePickInfo* pickInfo = dynamic_object_cast<ParticlePickInfo>(vpPickResult->pickInfo());
        if(pickInfo) {
            const Particles* particles = pickInfo->particles();
            BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
            size_t particleIndex = pickInfo->particleIndexFromSubObjectID(vpPickResult->subobjectId());
            if(posProperty && particleIndex < posProperty.size()) {
                // Save reference to the selected particle.
                TimeInterval iv;
                AnimationTime time = vpwin->viewport()->scene()->animationSettings()->currentTime();
                result.pipeline = vpPickResult->pipeline();
                result.particleIndex = particleIndex;
                result.localPos = posProperty[result.particleIndex];
                result.worldPos = result.pipeline->getWorldTransform(time, iv) * result.localPos;

                // Determine particle ID.
                BufferReadAccess<IdentifierIntType> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                if(identifierProperty && result.particleIndex < identifierProperty.size()) {
                    result.particleId = identifierProperty[result.particleIndex];
                }
                else result.particleId = -1;

                return true;
            }
        }
    }

    result.pipeline = nullptr;
    return false;
}

/******************************************************************************
* Renders the particle selection overlay in a viewport.
******************************************************************************/
void ParticlePickingHelper::renderSelectionMarker(Viewport* vp, FrameGraph& frameGraph, const PickResult& pickRecord)
{
    if(!pickRecord.pipeline)
        return;

    PipelineEvaluationRequest request(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive());
    const PipelineFlowState flowState = pickRecord.pipeline->evaluatePipeline(request).result();
    const Particles* particles = flowState.getObject<Particles>();
    if(!particles)
        return;

    // If particle selection is based on ID, find particle with the given ID.
    size_t particleIndex = pickRecord.particleIndex;
    if(pickRecord.particleId >= 0) {
        if(BufferReadAccess<IdentifierIntType> identifierProperty = particles->getProperty(Particles::IdentifierProperty)) {
            if(particleIndex >= identifierProperty.size() || identifierProperty[particleIndex] != pickRecord.particleId) {
                auto iter = boost::find(identifierProperty, pickRecord.particleId);
                if(iter == identifierProperty.cend())
                    return;
                particleIndex = (iter - identifierProperty.cbegin());
            }
        }
    }

    // Get the particle vis element, which is attached to the position property object.
    ParticlesVis* particleVis = particles->visElement<ParticlesVis>();
    if(!particleVis)
        return;

    // Render highlight marker.
    particleVis->highlightParticle(particleIndex, particles, frameGraph, pickRecord.pipeline);
}

}   // End of namespace
