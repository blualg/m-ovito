#######################################################################################
#
#  VisRTX renderer plugin for OVITO.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(VisRTXRenderer
    SOURCES
        AnariRenderingJob.cpp
        OffscreenAnariRenderer.cpp
        VisRTXRenderBuffer.cpp
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
