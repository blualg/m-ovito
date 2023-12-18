////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/util/ParticleOrderingFingerprint.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/pipeline/AsynchronousModifier.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * \brief Calculates ambient occlusion lighting for particles.
 */
class OVITO_PARTICLES_EXPORT AmbientOcclusionModifier : public AsynchronousModifier
{
    /// Give this modifier class its own metaclass.
    class AmbientOcclusionModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(AmbientOcclusionModifier, AmbientOcclusionModifierClass)

    OVITO_CLASSINFO("DisplayName", "Ambient occlusion");
    OVITO_CLASSINFO("Description", "Perform an ambient occlusion calculation to shade particles.");
    OVITO_CLASSINFO("ModifierCategory", "Coloring");

public:

    enum { MAX_AO_RENDER_BUFFER_RESOLUTION = 4 };

    /// Computes the modifier's results.
    class AmbientOcclusionEngine : public Engine
    {
    public:

        /// Constructor.
        AmbientOcclusionEngine(const ModifierEvaluationRequest& request, const TimeInterval& validityInterval, ParticleOrderingFingerprint fingerprint, int resolution, int samplingCount, ConstPropertyPtr positions,
            ConstPropertyPtr particleRadii, const Box3& boundingBox, OORef<SceneRenderer> renderer);

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        /// This method is called by the system whenever a parameter of the modifier changes.
        /// The method can be overridden by subclasses to indicate to the caller whether the engine object should be
        /// discarded (false) or may be kept in the cache, because the computation results are not affected by the changing parameter (true).
        virtual bool modifierChanged(const PropertyFieldEvent& event) override {
            // Avoid a recomputation if the user changes just the intensity parameter.
            if(event.field() == PROPERTY_FIELD(intensity))
                return true;
            return Engine::modifierChanged(event);
        }

        /// Returns the property storage that contains the computed per-particle brightness values.
        const DataBufferPtr& brightness() const { return _brightness; }

        /// Returns the data buffer containing the input particle positions.
        const ConstPropertyPtr& positions() const { return _positions; }

        /// Returns the data buffer containing the input particle radii.
        const ConstPropertyPtr& particleRadii() const { return _particleRadii; }

    private:

        OORef<SceneRenderer> _renderer;
        const int _resolution;
        const int _samplingCount;
        ConstPropertyPtr _positions;
        ConstPropertyPtr _particleRadii;
        const Box3 _boundingBox;
        DataBufferPtr _brightness;
        ParticleOrderingFingerprint _inputFingerprint;
    };

public:

    /// Constructor.
    explicit AmbientOcclusionModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

private:

    /// This controls the intensity of the shading effect.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, intensity, setIntensity);

    /// Controls the quality of the lighting computation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, samplingCount, setSamplingCount);

    /// Controls the resolution of the offscreen rendering buffer.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, bufferResolution, setBufferResolution);
};

}   // End of namespace
