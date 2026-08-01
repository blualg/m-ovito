#######################################################################################
#
#  Tachyon renderer plugin for OVITO.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(TachyonRenderer
    SOURCES
        TachyonRenderBuffer.cpp
        TachyonRenderer.cpp
        TachyonRenderingJob.cpp
    PRIVATE_LIB_DEPENDENCIES
        TachyonRayTracer
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
