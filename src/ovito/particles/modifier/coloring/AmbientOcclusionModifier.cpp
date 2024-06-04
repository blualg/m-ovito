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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/ObjectPickingIdentifierMap.h>
#include <ovito/opengl/OffscreenOpenGLRenderingJob.h>
#include "AmbientOcclusionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AmbientOcclusionModifier);
OVITO_CLASSINFO(AmbientOcclusionModifier, "DisplayName", "Ambient occlusion");
OVITO_CLASSINFO(AmbientOcclusionModifier, "Description", "Perform an ambient occlusion calculation to shade particles.");
OVITO_CLASSINFO(AmbientOcclusionModifier, "ModifierCategory", "Coloring");
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, intensity);
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, samplingCount);
DEFINE_PROPERTY_FIELD(AmbientOcclusionModifier, bufferResolution);
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, intensity, "Shading intensity");
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, samplingCount, "Number of exposure samples");
SET_PROPERTY_FIELD_LABEL(AmbientOcclusionModifier, bufferResolution, "Render buffer resolution");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, intensity, PercentParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, samplingCount, IntegerParameterUnit, 3, 2000);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(AmbientOcclusionModifier, bufferResolution, IntegerParameterUnit, 1, AmbientOcclusionModifier::MAX_AO_RENDER_BUFFER_RESOLUTION);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool AmbientOcclusionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void AmbientOcclusionModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> AmbientOcclusionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(DataOORef<const Particles> cachedParticles = cachedState.getObject<Particles>()) {
                Particles* particles = state.expectMutableObject<Particles>();
                particles->verifyIntegrity();
                const Property* cachedColors = cachedParticles->getProperty(Particles::ColorProperty);
                return asyncLaunch([state = std::move(state), particles, cachedColors, cachedParticles = std::move(cachedParticles)]() mutable {
                    particles->tryToAdoptProperties(cachedParticles, {cachedColors}, {particles});
                    return std::move(state);
                });
            }
        }
        return std::move(state);
    }

    // Special case handling: there are no particles to shade.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();
    if(particles->elementCount() == 0) {
        state.makeMutable(particles)->createProperty(DataBuffer::Initialized, Particles::ColorProperty, {particles});
        return std::move(state);
    }

    // Phase I: Perform the particle occlusion calculation. The results are cached in the node's partial cache.
    auto brightnessFuture = request.modificationNode()->partialResultsCache().getOrCompute(state.data(), [&]() {

        // Create the OpenGL offscreen rendering job. This needs to be done in the main thread.
        OORef<OffscreenOpenGLRenderingJob> renderingJob = OORef<OffscreenOpenGLRenderingJob>::create(std::make_shared<RendererResourceCache>(), 1, false);

        // Perform the AO computation in a separate thread.
        return asyncLaunch(
                [renderingJobTemp = std::move(renderingJob),
                bufferResolution = bufferResolution(),
                samplingCount = std::max(1, samplingCount()),
                particles = DataOORef<const Particles>(particles)]() mutable
        {
            this_task::setProgressText(tr("Ambient occlusion"));

            // Move this object into a function scope variable to make sure it gets destroyed in the current thread.
            // That's because the QOpenGLContext and other resources managed by the rendering job is tied to a specific thread.
            auto renderingJob = std::move(renderingJobTemp);

            // Get particle radii.
            ConstPropertyPtr radii = particles->inputParticleRadii();
            this_task::throwIfCanceled();

            // Compute bounding box of input particles (and include particle radii).
            const Property* positions = particles->expectProperty(Particles::PositionProperty);
            Box3 boundingBox;
            boundingBox.addPoints(BufferReadAccess<Point3>(positions));
            OVITO_ASSERT(!boundingBox.isEmpty());
            boundingBox = boundingBox.padBox(std::max(FloatType(0), radii->minMax().second));
            this_task::throwIfCanceled();

            // The render buffer resolution.
            int res = qBound(0, bufferResolution, (int)MAX_AO_RENDER_BUFFER_RESOLUTION);
            int resolution = (128 << res);

            // Create output array.
            DataBufferPtr brightness = DataBufferPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 1);

            // Create the rendering frame buffer that receives the rendered image of the particles.
            std::shared_ptr<FrameBuffer> frameBuffer = std::make_shared<FrameBuffer>(resolution, resolution);
            QRect frameBufferRect(QPoint(0,0), frameBuffer->size());

            // Create a frame graph.
            std::shared_ptr<FrameGraph> frameGraph = std::make_shared<FrameGraph>(
                std::make_shared<RendererResourceCache>()->acquireResourceFrame(),
                AnimationTime(0), ViewProjectionParameters{}, frameBufferRect.size(), false, false, false,
                renderingJob->preferredImageFormat(), 1.0);
            frameGraph->setClearColor(ColorA(0,0,0,0));
            this_task::throwIfCanceled();

            // Add the particles to the frame graph.
            std::unique_ptr<ParticlePrimitive> particleBuffer = std::make_unique<ParticlePrimitive>();
            particleBuffer->setShadingMode(ParticlePrimitive::FlatShading);
            particleBuffer->setRenderingQuality(ParticlePrimitive::LowQuality);
            particleBuffer->setPositions(positions);
            particleBuffer->setRadii(radii);
            auto pickingGroup = frameGraph->addPickingGroup(nullptr);
            OVITO_ASSERT(pickingGroup == 1);
            frameGraph->setCurrentRenderLayer(FrameGraph::RenderLayer::SceneLayer);
            frameGraph->addPrimitive(std::move(particleBuffer), AffineTransformation::Identity(), pickingGroup, boundingBox);
            renderingJob->postprocessFrameGraph(*frameGraph);
            this_task::throwIfCanceled();

            // Create a special object picking map to extract the particle indices from the frame buffer.
            std::shared_ptr<ObjectPickingIdentifierMap> objectIdentifierMap = std::make_shared<ObjectPickingIdentifierMap>();

            // Create an offscreen framebuffer for rendering.
            OORef<AbstractRenderingFrameBuffer> renderBuffer = renderingJob->createOffscreenFrameBuffer(frameBufferRect, frameBuffer);

            this_task::setProgressMaximum(samplingCount);
            for(int sample = 0; sample < samplingCount; sample++) {
                this_task::setProgressValue(sample);

                // Generate lighting direction on unit sphere using "Fibonacci sphere algorithm".
                // https://stackoverflow.com/a/26127012
                FloatType y = FloatType(1) - (sample / FloatType(samplingCount - 1)) * 2; // y goes from 1 to -1
                FloatType r = std::sqrt(FloatType(1) - y * y); // radius at y
                FloatType phi = (FloatType)sample * FLOATTYPE_PI * (FloatType(3) - sqrt(FloatType(5)));
                Vector3 dir(std::cos(phi)*r, y, std::sin(phi)*r);
                OVITO_ASSERT(std::abs(dir.length() - 1.0) < FLOATTYPE_EPSILON);

                // Set up view projection.
                ViewProjectionParameters projParams;
                projParams.viewMatrix = AffineTransformation::lookAlong(boundingBox.center(), dir, Vector3(0,0,1));

                // Transform bounding box to camera space.
                Box3 bb = boundingBox.transformed(projParams.viewMatrix).centerScale(FloatType(1.01));

                // Complete projection parameters.
                projParams.aspectRatio = 1;
                projParams.isPerspective = false;
                projParams.inverseViewMatrix = projParams.viewMatrix.inverse();
                projParams.fieldOfView = FloatType(0.5) * boundingBox.size().length();
                projParams.znear = -bb.maxc.z();
                projParams.zfar  = std::max(-bb.minc.z(), projParams.znear + FloatType(1));
                projParams.projectionMatrix = Matrix4::ortho(-projParams.fieldOfView, projParams.fieldOfView,
                                    -projParams.fieldOfView, projParams.fieldOfView,
                                    projParams.znear, projParams.zfar);
                projParams.inverseProjectionMatrix = projParams.projectionMatrix.inverse();
                projParams.validityInterval = TimeInterval::infinite();
                frameGraph->setProjectionParams(projParams);

                // Discard the existing image in the frame buffer so that
                // OffscreenOpenGLRenderer::renderFrame() can just return the unmodified
                // frame buffer contents.
                frameBuffer->image() = QImage();

                // Render the current view to the frame buffer
                auto future = renderingJob->renderFrame(frameGraph, renderBuffer, objectIdentifierMap);
                OVITO_ASSERT(future.isValid() && future.isFinished() && !future.isCanceled());

                // Extract brightness values from rendered image.
                const QImage& image = frameBuffer->image();
                OVITO_ASSERT(!image.isNull());
                BufferWriteAccess<FloatType, access_mode::read_write> brightnessValues(brightness);
                for(int y = 0; y < resolution; y++) {
                    const QRgb* pixel = reinterpret_cast<const QRgb*>(image.scanLine(y));
                    for(int x = 0; x < resolution; x++, ++pixel) {
                        quint32 red = qRed(*pixel);
                        quint32 green = qGreen(*pixel);
                        quint32 blue = qBlue(*pixel);
                        quint32 alpha = qAlpha(*pixel);
                        quint32 id = red + (green << 8) + (blue << 16) + (alpha << 24);
                        if(id == 0)
                            continue;
                        quint32 particleIndex = id - 1; // Note: frame buffer object IDs start at 1.
                        OVITO_ASSERT(particleIndex < brightnessValues.size());
                        brightnessValues[particleIndex] += 1;
                    }
                }
            }

            this_task::setProgressValue(samplingCount);

            // Normalize brightness values by particle area.
            BufferReadAccess<GraphicsFloatType> radiusArray(radii);
            BufferWriteAccess<FloatType, access_mode::read_write> brightnessValues(brightness);
            auto r = radiusArray.cbegin();
            for(FloatType& b : brightnessValues) {
                if(*r != 0)
                    b /= (*r) * (*r);
                ++r;
            }
            this_task::throwIfCanceled();

            // Normalize brightness values by global maximum.
            FloatType maxBrightness = *boost::max_element(brightnessValues);
            if(maxBrightness != 0) {
                for(FloatType& b : brightnessValues) {
                    b /= maxBrightness;
                }
            }

            // Release data that is no longer needed to reduce memory footprint.
            particles.reset();

            return brightness;
        });
    });

    // Phase II: Modulate input particle colors with the computed brightness values.
    return brightnessFuture.then(*this, [this, state = std::move(state)](ConstDataBufferPtr brightness) {

        // Perform work in a separate thread.
        return asyncLaunch([state = std::move(state), brightness = std::move(brightness), intensity = intensity()]() mutable {

            Particles* particles = state.expectMutableObject<Particles>();
            OVITO_ASSERT(brightness && particles->elementCount() == brightness->size());

            GraphicsFloatType effIntensity = qBound(GraphicsFloatType(0), static_cast<GraphicsFloatType>(intensity), GraphicsFloatType(1));

            BufferReadAccess<FloatType> brightnessAcc(brightness);
            BufferWriteAccess<ColorG, access_mode::read_write> colorAcc = particles->createProperty(DataBuffer::Initialized, Particles::ColorProperty, {particles});

            const FloatType* __restrict b = brightnessAcc.cbegin();
            for(ColorG& c : colorAcc) {
                GraphicsFloatType factor = std::min(GraphicsFloatType(1) - effIntensity + static_cast<GraphicsFloatType>(*b), GraphicsFloatType(1));
                c = c * factor;
                ++b;
            }

            return std::move(state);
        });
    });
}

}   // End of namespace
