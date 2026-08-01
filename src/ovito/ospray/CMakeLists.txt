#######################################################################################
#
#  OSPRay renderer plugin for OVITO.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(OSPRayRenderer
    SOURCES
        OSPRayRenderBuffer.cpp
        OSPRayRenderer.cpp
        OSPRayRenderingJob.cpp
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
