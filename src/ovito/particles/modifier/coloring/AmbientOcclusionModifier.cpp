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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/opengl/OffscreenOpenGLRenderingJob.h>
#include <ovito/opengl/OpenGLRenderBuffer.h>
#include "AmbientOcclusionModifier.h"

#ifdef Q_OS_MACOS
#include <ovito/core/utilities/concurrent/ForEach.h>
#endif

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
        OORef<OffscreenOpenGLRenderingJob> renderingJob = OORef<OffscreenOpenGLRenderingJob>::create(std::make_shared<RendererResourceCache>(), nullptr);

        // Perform the AO computation in a separate thread.
        return asyncLaunch([
                self = OORef<AmbientOcclusionModifier>(this),
                renderingJobTemp = std::move(renderingJob),
                bufferResolution = bufferResolution(),
                samplingCount = std::max(1, samplingCount()),
                particles = DataOORef<const Particles>(particles)]() mutable
        {
            auto progress = std::make_unique<TaskProgress>(this_task::ui());
            progress->setText(tr("Ambient occlusion"));

            // Move this object into a function scope variable to make sure it gets destroyed in the current thread.
            // That's because the QOpenGLContext and other resources managed by the rendering job are tied to a specific thread.
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
            OORef<FrameGraph> frameGraph = OORef<FrameGraph>::create(
                renderingJob->visCache()->acquireResourceFrame(),
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
            frameGraph->addCommandGroup(FrameGraph::SceneLayer).addPrimitive(std::move(particleBuffer), AffineTransformation::Identity(), boundingBox, OORef<const SceneNode>{});
            OVITO_ASSERT(frameGraph->commandGroups().size() == 1);
            OVITO_ASSERT(frameGraph->commandGroups().front().commands().size() == 1);
            OVITO_ASSERT(frameGraph->commandGroups().front().commands().front().skipInPickingPass() == false);
            renderingJob->postprocessFrameGraph(*frameGraph);
            this_task::throwIfCanceled();

            // Release data that is no longer needed to reduce memory footprint.
            particles.reset();

            // Create a special object picking map to extract the particle indices from the frame buffer.
            std::shared_ptr<OpenGLPickingMap> objectIdentifierMap = std::make_shared<OpenGLPickingMap>();

#ifndef Q_OS_MACOS
            // Create an offscreen framebuffer for rendering.
            OORef<OpenGLRenderBuffer> renderBuffer = static_object_cast<OpenGLRenderBuffer>(renderingJob->createOffscreenRenderBuffer(frameBufferRect, frameBuffer));

            progress->setMaximum(samplingCount);
            for(int sample = 0; sample < samplingCount; sample++) {
                progress->setValue(sample);
#else
            return std::make_tuple(std::move(progress), std::move(renderingJob), std::move(objectIdentifierMap), std::move(frameBuffer), std::move(frameGraph), std::move(brightness), std::move(radii), samplingCount, resolution, boundingBox);
        }).then(DeferredObjectExecutor(request.modificationNode()), [node=request.modificationNode()]<typename... Args>(std::tuple<Args...>&& inputs) {
            auto [progress, renderingJob, objectIdentifierMap, frameBuffer, frameGraph, brightness, radii, samplingCount, resolution, boundingBox] = std::move(inputs);

            // Create an offscreen framebuffer for rendering.
            OORef<OpenGLRenderBuffer> renderBuffer = static_object_cast<OpenGLRenderBuffer>(renderingJob->createOffscreenRenderBuffer(QRect(QPoint(0,0), frameBuffer->size())));

            // Perform the evaluation for all requested animation frames.
            progress->setMaximum(samplingCount);
            return for_each_sequential(
                boost::irange(0, samplingCount),
                DeferredObjectExecutor(node),
                [=, progress=std::move(progress)](int sample) -> Future<void> {
                    progress->setValue(sample);
#endif
                // Generate lighting direction on unit sphere using "Fibonacci sphere algorithm".
                // https://stackoverflow.com/a/26127012
                FloatType y = FloatType(1) - (sample / FloatType(samplingCount - 1)) * 2; // y goes from 1 to -1
                FloatType r = std::sqrt(FloatType(1) - y * y); // radius at y
                FloatType phi = (FloatType)sample * FLOATTYPE_PI * (FloatType(3) - std::sqrt(FloatType(5)));
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
                objectIdentifierMap->reset();
                auto future = renderingJob->renderFrame(frameGraph, renderBuffer, frameBuffer, objectIdentifierMap);
                OVITO_ASSERT(future && future.isFinished() && !future.isCanceled());
#ifdef Q_OS_MACOS
                return static_cast<Future<void>&&>(future); // Note: for_each_sequential cannot deal with SCFuture yet, but we know it's finished
            },
            [=](int sample) {
#endif
                // Extract brightness values from rendered image.
                const QImage& image = frameBuffer->image();
                OVITO_ASSERT(!image.isNull());
                BufferWriteAccess<FloatType, access_mode::read_write> brightnessValues(brightness);
                for(int y = 0; y < resolution; y++) {
                    const QRgb* pixel = reinterpret_cast<const QRgb*>(image.scanLine(y));
                    for(int x = 0; x < resolution; x++, ++pixel) {
                        uint32_t red = qRed(*pixel);
                        uint32_t green = qGreen(*pixel);
                        uint32_t blue = qBlue(*pixel);
                        uint32_t alpha = qAlpha(*pixel);
                        uint32_t id = red + (green << 8) + (blue << 16) + (alpha << 24);
                        if(id == 0)
                            continue;
                        uint32_t particleIndex = id - 1; // Note: frame buffer object IDs start at 1.
                        OVITO_ASSERT(particleIndex < brightnessValues.size());
                        brightnessValues[particleIndex] += 1;
                    }
                }
#ifndef Q_OS_MACOS
            }

            progress->setValue(samplingCount);
#else
            },
            std::make_tuple(radii, brightness));
        }).then(DeferredObjectExecutor(request.modificationNode()), []<typename... Args2>(std::tuple<Args2...>&& inputs) {
            auto [radii, brightness] = std::move(inputs);
#endif
            // Normalize brightness values by particle area.
            BufferReadAccess<GraphicsFloatType> radiusArray(radii);
            BufferWriteAccess<FloatType, access_mode::read_write> brightnessValues(brightness);
            auto r = radiusArray.cbegin();
            FloatType maxBrightness = 0;
            for(FloatType& b : brightnessValues) {
                if(*r != 0)
                    b /= (*r) * (*r);
                if(b > maxBrightness)
                    maxBrightness = b;
                ++r;
            }
            this_task::throwIfCanceled();

            // Normalize brightness values by global maximum.
            if(maxBrightness != 0) {
                for(FloatType& b : brightnessValues) {
                    b /= maxBrightness;
                }
            }

            return brightness;
        });
    });

    // Phase II: Modulate input particle colors with the computed brightness values.
    return brightnessFuture.then(ObjectExecutor(this), [this, state = std::move(state)](ConstDataBufferPtr brightness) {

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
