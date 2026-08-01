////////////////////////////////////////////////////////////////////////////////////////
//
//  OSPRay renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "OSPRayRenderer.h"
#include "OSPRayRenderingJob.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OSPRayRenderer);
OVITO_CLASSINFO(OSPRayRenderer, "DisplayName", "OSPRay");
OVITO_CLASSINFO(OSPRayRenderer, "Description", "CPU-based ray-tracing renderer using Intel OSPRay when the OSPRay runtime is available.");
DEFINE_PROPERTY_FIELD(OSPRayRenderer, libraryPath);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, rendererType);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, samplesPerPixel);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, directLight);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, directLightIntensity);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, ambientLight);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, ambientLightIntensity);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, ambientOcclusion);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, ambientOcclusionSamples);
DEFINE_PROPERTY_FIELD(OSPRayRenderer, shadows);
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, libraryPath, "OSPRay library path");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, rendererType, "Renderer type");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, samplesPerPixel, "Samples per pixel");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, directLight, "Direct light");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, directLightIntensity, "Intensity");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, ambientLight, "Ambient light");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, ambientLightIntensity, "Intensity");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, ambientOcclusion, "Ambient occlusion");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, ambientOcclusionSamples, "AO sample count");
SET_PROPERTY_FIELD_LABEL(OSPRayRenderer, shadows, "Cast shadows");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OSPRayRenderer, samplesPerPixel, IntegerParameterUnit, 1, 1024);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OSPRayRenderer, directLightIntensity, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OSPRayRenderer, ambientLightIntensity, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OSPRayRenderer, ambientOcclusionSamples, IntegerParameterUnit, 0, 256);

/******************************************************************************
* Constructor.
******************************************************************************/
void OSPRayRenderer::initializeObject(ObjectInitializationFlags flags)
{
    SceneRenderer::initializeObject(flags);
}

/******************************************************************************
* Creates a new renderer-specific rendering job for offscreen rendering.
******************************************************************************/
OORef<RenderingJob> OSPRayRenderer::createOffscreenRenderingJob()
{
    return OORef<OSPRayRenderingJob>::create(this_task::ui()->datasetContainer().visCache(), this);
}

}   // End of namespace
