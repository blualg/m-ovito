#######################################################################################
#
#  Copyright 2026 OVITO GmbH, Germany
#
#  This file is part of OVITO (Open Visualization Tool).
#
#  OVITO is free software; you can redistribute it and/or modify it either under the
#  terms of the GNU General Public License version 3 as published by the Free Software
#  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
#  If you do not alter this notice, a recipient may use your version of this
#  file under either the GPL or the MIT License.
#
#  You should have received a copy of the GPL along with this program in a
#  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
#  with this program in a file LICENSE.MIT.txt
#
#  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
#  either express or implied. See the GPL or the MIT License for the specific language
#  governing rights and limitations.
#
#######################################################################################

OVITO_STANDARD_PLUGIN(PyScriptGui
    SOURCES
        PythonScriptModifierEditor.cpp
        ObjectScriptEditor.cpp
    PLUGIN_DEPENDENCIES
        PyScript
    GUI_PLUGIN
)

IF(WIN32 AND OVITO_ACTIVE_PYTHON_RUNTIME_DLL)
    ADD_CUSTOM_COMMAND(TARGET PyScriptGui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PyScriptGui>/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
        COMMENT "Syncing embedded Python runtime for PyScriptGui")
    IF(OVITO_ACTIVE_PYTHON3_DLL)
        ADD_CUSTOM_COMMAND(TARGET PyScriptGui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON3_DLL}" "$<TARGET_FILE_DIR:PyScriptGui>/python3.dll")
    ENDIF()
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
