#######################################################################################
#
#  GUI plugin for the OSPRay renderer.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(OSPRayRendererGui
    SOURCES
        OSPRayRendererEditor.cpp
    PLUGIN_DEPENDENCIES
        OSPRayRenderer
    GUI_PLUGIN
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
