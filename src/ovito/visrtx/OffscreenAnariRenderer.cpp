////////////////////////////////////////////////////////////////////////////////////////
//
//  VisRTX renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "AnariRenderingJob.h"
#include "OffscreenAnariRenderer.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OffscreenAnariRenderer);
OVITO_CLASSINFO(OffscreenAnariRenderer, "DisplayName", "VisRTX");
OVITO_CLASSINFO(OffscreenAnariRenderer, "Description", "NVIDIA GPU ray-tracing renderer using VisRTX through the ANARI runtime when available.");
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, anariRuntimePath);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, anariLibraryName);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, deviceSubtype);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, rendererSubtype);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, samplesPerPixel);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, directLight);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, directLightIrradiance);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, ambientRadiance);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, ambientOcclusion);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, ambientSamples);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, denoise);
DEFINE_PROPERTY_FIELD(OffscreenAnariRenderer, maxRayDepth);
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, anariRuntimePath, "ANARI runtime path");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, anariLibraryName, "ANARI library name");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, deviceSubtype, "Device subtype");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, rendererSubtype, "Renderer subtype");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, samplesPerPixel, "Samples per pixel");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, directLight, "Direct light");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, directLightIrradiance, "Irradiance");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, ambientRadiance, "Ambient radiance");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, ambientOcclusion, "Ambient occlusion");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, ambientSamples, "AO sample count");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, denoise, "Denoise");
SET_PROPERTY_FIELD_LABEL(OffscreenAnariRenderer, maxRayDepth, "Max ray depth");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OffscreenAnariRenderer, samplesPerPixel, IntegerParameterUnit, 1, 4096);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OffscreenAnariRenderer, directLightIrradiance, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OffscreenAnariRenderer, ambientRadiance, FloatParameterUnit, 0, 100);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OffscreenAnariRenderer, ambientSamples, IntegerParameterUnit, 0, 1024);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(OffscreenAnariRenderer, maxRayDepth, IntegerParameterUnit, 1, 128);

/******************************************************************************
* Constructor.
******************************************************************************/
void OffscreenAnariRenderer::initializeObject(ObjectInitializationFlags flags)
{
    SceneRenderer::initializeObject(flags);
}

/******************************************************************************
* Creates a new renderer-specific rendering job for offscreen rendering.
******************************************************************************/
OORef<RenderingJob> OffscreenAnariRenderer::createOffscreenRenderingJob()
{
    return OORef<AnariRenderingJob>::create(this_task::ui()->datasetContainer().visCache(), this);
}

}   // End of namespace
