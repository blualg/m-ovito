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

IF(MSVC)
    # Precompiled headers require more compiler memory.
    ADD_COMPILE_OPTIONS(-Zm150)
ENDIF()

OVITO_FIND_PACKAGE(Python3 REQUIRED COMPONENTS Interpreter Development)

IF(WIN32)
    SET(_ovito_active_python_runtime_dll "")
    SET(_ovito_active_python3_dll "")

    IF(DEFINED ENV{PYTHONHOME})
        SET(_ovito_pythonhome_candidate "$ENV{PYTHONHOME}")
        IF(EXISTS "${_ovito_pythonhome_candidate}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
            SET(_ovito_active_python_runtime_dll "${_ovito_pythonhome_candidate}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
            IF(EXISTS "${_ovito_pythonhome_candidate}/python3.dll")
                SET(_ovito_active_python3_dll "${_ovito_pythonhome_candidate}/python3.dll")
            ENDIF()
        ENDIF()
    ENDIF()

    IF(NOT _ovito_active_python_runtime_dll)
        SET(_ovito_local_python_candidate "$ENV{LOCALAPPDATA}/Programs/Python/Python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}")
        IF(EXISTS "${_ovito_local_python_candidate}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
            SET(_ovito_active_python_runtime_dll "${_ovito_local_python_candidate}/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
            IF(EXISTS "${_ovito_local_python_candidate}/python3.dll")
                SET(_ovito_active_python3_dll "${_ovito_local_python_candidate}/python3.dll")
            ENDIF()
        ENDIF()
    ENDIF()

    SET(OVITO_ACTIVE_PYTHON_RUNTIME_DLL "${_ovito_active_python_runtime_dll}" CACHE INTERNAL "Preferred embedded Python runtime DLL")
    SET(OVITO_ACTIVE_PYTHON3_DLL "${_ovito_active_python3_dll}" CACHE INTERNAL "Preferred embedded Python abi3 helper DLL")
ENDIF()

OVITO_STANDARD_PLUGIN(PyScript
    SOURCES
        PyScript.cpp
        engine/ScriptAutostarter.cpp
        engine/ScriptEngine.cpp
        binding/PythonInterface.cpp
        binding/SceneBinding.cpp
        binding/ModifierRuntimeModule.cpp
        binding/ModifierRuntimeBinding.cpp
        binding/AppBinding.cpp
        binding/TypeCasting.cpp
        extensions/PythonScriptModifier.cpp
    LIB_DEPENDENCIES
        OpenGLRenderer
        StdObj
        Particles
        Python3::Python
        pybind11::headers
    PYTHON_WRAPPERS
        "${CMAKE_CURRENT_SOURCE_DIR}/python/"
    PRECOMPILED_HEADERS
        PyScript.h
)

IF(WIN32 AND OVITO_ACTIVE_PYTHON_RUNTIME_DLL)
    ADD_CUSTOM_COMMAND(TARGET PyScript POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON_RUNTIME_DLL}" "$<TARGET_FILE_DIR:PyScript>/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
        COMMENT "Syncing embedded Python runtime for PyScript")
    IF(OVITO_ACTIVE_PYTHON3_DLL)
        ADD_CUSTOM_COMMAND(TARGET PyScript POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON3_DLL}" "$<TARGET_FILE_DIR:PyScript>/python3.dll")
    ENDIF()
ENDIF()

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/python/ovito/plugins/__init__.embedded.py"
    "${OVITO_PYTHON_DIRECTORY}/ovito/plugins/__init__.py"
    COPYONLY
)
INSTALL(
    FILES "${OVITO_PYTHON_DIRECTORY}/ovito/plugins/__init__.py"
    DESTINATION "${OVITO_RELATIVE_PYTHON_DIRECTORY}/ovito/plugins/"
)

IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(gui)
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
