////////////////////////////////////////////////////////////////////////////////////////
//
//  Tachyon renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TachyonRenderer.h"
#include "TachyonRenderingJob.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TachyonRenderer);
OVITO_CLASSINFO(TachyonRenderer, "DisplayName", "Tachyon");
OVITO_CLASSINFO(TachyonRenderer, "Description", "Software-based ray-tracing renderer with support for anti-aliasing, shadows, and ambient occlusion.");
DEFINE_PROPERTY_FIELD(TachyonRenderer, enableAntialiasing);
DEFINE_PROPERTY_FIELD(TachyonRenderer, antialiasingSamples);
DEFINE_PROPERTY_FIELD(TachyonRenderer, directLight);
DEFINE_PROPERTY_FIELD(TachyonRenderer, directLightBrightness);
DEFINE_PROPERTY_FIELD(TachyonRenderer, shadows);
DEFINE_PROPERTY_FIELD(TachyonRenderer, ambientOcclusion);
DEFINE_PROPERTY_FIELD(TachyonRenderer, ambientOcclusionBrightness);
DEFINE_PROPERTY_FIELD(TachyonRenderer, ambientOcclusionSamples);
DEFINE_PROPERTY_FIELD(TachyonRenderer, depthOfField);
DEFINE_PROPERTY_FIELD(TachyonRenderer, focalLength);
DEFINE_PROPERTY_FIELD(TachyonRenderer, aperture);
DEFINE_PROPERTY_FIELD(TachyonRenderer, maxRayRecursion);
DEFINE_PROPERTY_FIELD(TachyonRenderer, maxTransparentSurfaces);
DEFINE_PROPERTY_FIELD(TachyonRenderer, materialAmbient);
DEFINE_PROPERTY_FIELD(TachyonRenderer, materialDiffuse);
DEFINE_PROPERTY_FIELD(TachyonRenderer, materialSpecular);
DEFINE_PROPERTY_FIELD(TachyonRenderer, maxThreads);
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, enableAntialiasing, "Enable anti-aliasing");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, antialiasingSamples, "Anti-aliasing samples");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, directLight, "Direct light");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, directLightBrightness, "Brightness");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, shadows, "Cast shadows");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, ambientOcclusion, "Ambient occlusion");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, ambientOcclusionBrightness, "Brightness");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, ambientOcclusionSamples, "Sample count");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, depthOfField, "Depth of field");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, focalLength, "Focal length");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, aperture, "Aperture");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, maxRayRecursion, "Max ray recursion");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, maxTransparentSurfaces, "Max transparent surfaces");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, materialAmbient, "Material ambient");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, materialDiffuse, "Material diffuse");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, materialSpecular, "Material highlight");
SET_PROPERTY_FIELD_LABEL(TachyonRenderer, maxThreads, "Maximum threads");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, antialiasingSamples, IntegerParameterUnit, 0, 64);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, directLightBrightness, FloatParameterUnit, 0, 4);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, ambientOcclusionBrightness, FloatParameterUnit, 0, 4);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, ambientOcclusionSamples, IntegerParameterUnit, 0, 256);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, focalLength, FloatParameterUnit, 0, 1000000);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, aperture, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, maxRayRecursion, IntegerParameterUnit, 1, 512);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, maxTransparentSurfaces, IntegerParameterUnit, 1, 512);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, materialAmbient, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, materialDiffuse, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, materialSpecular, FloatParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TachyonRenderer, maxThreads, IntegerParameterUnit, 0, 4096);

/******************************************************************************
* Constructor.
******************************************************************************/
void TachyonRenderer::initializeObject(ObjectInitializationFlags flags)
{
    SceneRenderer::initializeObject(flags);
}

/******************************************************************************
* Creates a new renderer-specific rendering job for offscreen rendering.
******************************************************************************/
OORef<RenderingJob> TachyonRenderer::createOffscreenRenderingJob()
{
    return OORef<TachyonRenderingJob>::create(this_task::ui()->datasetContainer().visCache(), this);
}

}   // End of namespace
