////////////////////////////////////////////////////////////////////////////////////////
//
//  VisRTX renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

class OVITO_VISRTXRENDERER_EXPORT OffscreenAnariRenderer : public SceneRenderer
{
    OVITO_CLASS(OffscreenAnariRenderer)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Creates a new renderer-specific rendering job for offscreen rendering.
    virtual OORef<RenderingJob> createOffscreenRenderingJob() override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, anariRuntimePath, setAnariRuntimePath, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{QStringLiteral("visrtx")}, anariLibraryName, setAnariLibraryName, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{QStringLiteral("default")}, deviceSubtype, setDeviceSubtype, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{QStringLiteral("default")}, rendererSubtype, setRendererSubtype, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{8}, samplesPerPixel, setSamplesPerPixel, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, directLight, setDirectLight, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.5}, directLightIrradiance, setDirectLightIrradiance, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, ambientRadiance, setAmbientRadiance, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, ambientOcclusion, setAmbientOcclusion, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, ambientSamples, setAmbientSamples, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, denoise, setDenoise, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{4}, maxRayDepth, setMaxRayDepth, PROPERTY_FIELD_RESETTABLE);
};

}   // End of namespace
