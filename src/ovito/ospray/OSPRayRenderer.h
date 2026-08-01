////////////////////////////////////////////////////////////////////////////////////////
//
//  OSPRay renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

class OVITO_OSPRAYRENDERER_EXPORT OSPRayRenderer : public SceneRenderer
{
    OVITO_CLASS(OSPRayRenderer)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Creates a new renderer-specific rendering job for offscreen rendering.
    virtual OORef<RenderingJob> createOffscreenRenderingJob() override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, libraryPath, setLibraryPath, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{QStringLiteral("scivis")}, rendererType, setRendererType, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{4}, samplesPerPixel, setSamplesPerPixel, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, directLight, setDirectLight, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, directLightIntensity, setDirectLightIntensity, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, ambientLight, setAmbientLight, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.7}, ambientLightIntensity, setAmbientLightIntensity, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, ambientOcclusion, setAmbientOcclusion, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, ambientOcclusionSamples, setAmbientOcclusionSamples, PROPERTY_FIELD_RESETTABLE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, shadows, setShadows, PROPERTY_FIELD_RESETTABLE);
};

}   // End of namespace
