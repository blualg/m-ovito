////////////////////////////////////////////////////////////////////////////////////////
//
//  Tachyon renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

class OVITO_TACHYONRENDERER_EXPORT TachyonRenderer : public SceneRenderer
{
    OVITO_CLASS(TachyonRenderer)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Creates a new renderer-specific rendering job for offscreen rendering.
    virtual OORef<RenderingJob> createOffscreenRenderingJob() override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, enableAntialiasing, setEnableAntialiasing, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{4}, antialiasingSamples, setAntialiasingSamples, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, directLight, setDirectLight, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.75}, directLightBrightness, setDirectLightBrightness, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, shadows, setShadows, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, ambientOcclusion, setAmbientOcclusion, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.8}, ambientOcclusionBrightness, setAmbientOcclusionBrightness, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{12}, ambientOcclusionSamples, setAmbientOcclusionSamples, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, depthOfField, setDepthOfField, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{40}, focalLength, setFocalLength, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.01}, aperture, setAperture, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{24}, maxRayRecursion, setMaxRayRecursion, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{50}, maxTransparentSurfaces, setMaxTransparentSurfaces, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.35}, materialAmbient, setMaterialAmbient, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.55}, materialDiffuse, setMaterialDiffuse, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.08}, materialSpecular, setMaterialSpecular, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, maxThreads, setMaxThreads, PROPERTY_FIELD_RESETTABLE);
};

}   // End of namespace
