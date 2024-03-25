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
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include "AmbientOcclusionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AmbientOcclusionModifier);
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
* Constructor.
******************************************************************************/
AmbientOcclusionModifier::AmbientOcclusionModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _intensity(0.7),
    _samplingCount(40),
    _bufferResolution(3)
{
}

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
bool AmbientOcclusionModifier::preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const
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
Future<PipelineFlowState> AmbientOcclusionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    throw Exception(tr("This modifier is not yet implemented for the new pipeline system."));

    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            Particles* particles = state.expectMutableObject<Particles>();
            particles->verifyIntegrity();
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                if(cachedParticles->elementCount() == particles->elementCount()) {
                    if(const Property* cachedColors = cachedParticles->getProperty(Particles::ColorProperty)) {
                        state.expectMutableObject<Particles>()->createProperty(cachedColors);
                    }
                }
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

        // Create the offscreen renderer implementation.
        OvitoClassPtr rendererClass = PluginManager::instance().findClass("OpenGLRenderer", "OffscreenOpenGLSceneRenderer");
        if(!rendererClass)
            throw Exception(tr("The OffscreenOpenGLSceneRenderer class is not available. Please make sure the OpenGLRenderer plugin is installed correctly."));
        OORef<SceneRenderer> renderer = static_object_cast<SceneRenderer>(rendererClass->createInstance());

        // Perform the AO computation in a separate thread.
        return AsynchronousTask<ConstDataBufferPtr>::runAsync([
                renderer = std::move(renderer),
                bufferResolution = bufferResolution(),
                samplingCount = std::max(1, samplingCount()),
                particles = DataOORef<const Particles>(particles)]() mutable
        {
            this_task::setProgressText(tr("Ambient occlusion"));
            const Property* posProperty = particles->expectProperty(Particles::PositionProperty);

            // Get particle radii.
            ConstPropertyPtr radii = particles->inputParticleRadii();
            this_task::throwIfCanceled();

            // Compute bounding box of input particles (including particle radii).
            Box3 boundingBox;
            boundingBox.addPoints(BufferReadAccess<Point3>(posProperty));
            OVITO_ASSERT(!boundingBox.isEmpty());
            boundingBox = boundingBox.padBox(std::max(FloatType(0), radii->minMax().second));
            this_task::throwIfCanceled();

            // The render buffer resolution.
            int res = qBound(0, bufferResolution, (int)MAX_AO_RENDER_BUFFER_RESOLUTION);
            int resolution = (128 << res);

            // Create output array.
            DataBufferPtr brightness = DataBufferPtr::create(DataBuffer::Initialized, particles->elementCount(), Property::FloatDefault, 1);

            // Create the rendering frame buffer that receives the rendered image of the particles.
            FrameBuffer frameBuffer(resolution, resolution);
            QRect frameBufferRect(QPoint(0,0), frameBuffer.size());

            // Create a local vis cache, because we are not in the main thread.
            // But we assume that this cache is not being used much anyway.
            auto visCache = std::make_shared<RendererResourceCache>();

            // Initialize the renderer.
            renderer->startRender(frameBufferRect.size());
            try {
                // The buffered particle geometry used for rendering the particles.
                ParticlePrimitive particleBuffer;

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

                    // Discard the existing image in the frame buffer so that
                    // OffscreenOpenGLSceneRenderer::endFrame() can just return the unmodified
                    // frame buffer contents.
                    frameBuffer.image() = QImage();

                    // Create a frame graph.
                    std::unique_ptr<FrameGraph> frameGraph = std::make_unique<FrameGraph>(
                        visCache->acquireResourceFrame(),
                        AnimationTime(0), projParams, frameBufferRect.size(), true, false, false,
                        renderer->preferredImageFormat(), 1.0);

#if 0 // TODO
                    _renderer->beginFrame(AnimationTime(0), nullptr, projParams, nullptr, frameBufferRect, &frameBuffer);
                    _renderer->setWorldTransform(AffineTransformation::Identity());
                    _renderer->resetPickingBuffer();
                    try {
                        // Create particle buffer.
                        if(!particleBuffer.positions()) {
                            particleBuffer.setShadingMode(ParticlePrimitive::FlatShading);
                            particleBuffer.setRenderingQuality(ParticlePrimitive::LowQuality);
                            particleBuffer.setPositions(positions());
                            particleBuffer.setRadii(particleRadii());
                        }
                        _renderer->renderParticles(particleBuffer);
                    }
                    catch(...) {
                        _renderer->endFrame(false, frameBufferRect);
                        throw;
                    }

                    // Retrieve the frame buffer contents.
                    _renderer->endFrame(true, frameBufferRect);
#endif

                    // Extract brightness values from rendered image.
                    const QImage& image = frameBuffer.image();
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
                            // Subtracting base 1 from ID, because that's how SceneRenderer::registerSubObjectIDs() is implemented.
                            quint32 particleIndex = id - 1;
                            OVITO_ASSERT(particleIndex < brightnessValues.size());
                            brightnessValues[particleIndex] += 1;
                        }
                    }
                }
            }
            catch(...) {
                renderer->endRender();
                throw;
            }
            renderer->endRender();

            // The vis cache should remain unused when rendering just a bunch of spherical particles.
            OVITO_ASSERT(visCache->empty());

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
            brightnessValues.reset();
            renderer.reset();

            return std::move(brightness);
        }, true);
    });

    // Phase II: Module input particle colors with the computed brightness values.
    return brightnessFuture.then(*this, [this, state = std::move(state)](ConstDataBufferPtr brightness) {

        // Perform work in a separate thread.
        return AsynchronousTask<PipelineFlowState>::runAsync([state = std::move(state), brightness = std::move(brightness), intensity = intensity()]() mutable {

            Particles* particles = state.expectMutableObject<Particles>();
            OVITO_ASSERT(brightness && particles->elementCount() == brightness->size());

            GraphicsFloatType effIntensity = qBound(GraphicsFloatType(0), static_cast<GraphicsFloatType>(intensity), GraphicsFloatType(1));

            BufferReadAccess<FloatType> brightnessAcc(brightness);
            BufferWriteAccess<ColorG, access_mode::read_write> colorAcc = particles->createProperty(DataBuffer::Initialized, Particles::ColorProperty, {particles});

            const FloatType* b = brightnessAcc.cbegin();
            for(ColorG& c : colorAcc) {
                GraphicsFloatType factor = GraphicsFloatType(1) - effIntensity + static_cast<GraphicsFloatType>(*b);
                if(factor < GraphicsFloatType(1))
                    c = c * factor;
                ++b;
            }

            return std::move(state);
        });
    });
}

}   // End of namespace
