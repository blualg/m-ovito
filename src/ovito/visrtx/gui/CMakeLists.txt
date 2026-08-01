#######################################################################################
#
#  GUI plugin for the VisRTX renderer.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(VisRTXRendererGui
    SOURCES
        OffscreenAnariRendererEditor.cpp
    PLUGIN_DEPENDENCIES
        VisRTXRenderer
    GUI_PLUGIN
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
