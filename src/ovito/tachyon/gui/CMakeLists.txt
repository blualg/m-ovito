#######################################################################################
#
#  GUI plugin for the Tachyon renderer.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(TachyonRendererGui
    SOURCES
        TachyonRendererEditor.cpp
    PLUGIN_DEPENDENCIES
        TachyonRenderer
    GUI_PLUGIN
)

SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
