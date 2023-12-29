#######################################################################################
#
#  Copyright 2024 OVITO GmbH, Germany
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

IF(WIN32)
    # Always build this module as a static library under Windows, because gsd does not explicitly export any symbols.
    # Building as a DLL would require tagging all symbols to be exported with __declspec(dllexport) in the source  code.
    SET(BUILD_SHARED_LIBS OFF)
ENDIF()

# Build library.
ADD_LIBRARY(gsd
    gsd.c
)

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(gsd PROPERTIES OUTPUT_NAME "ovito_gsd")

# gsd library does not support unity build. Need to turn it off.
SET_TARGET_PROPERTIES(gsd PROPERTIES UNITY_BUILD OFF)

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(gsd INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

IF(MSVC)
    # Do not warn about use of unsafe CRT Library functions.
    TARGET_COMPILE_DEFINITIONS(gsd PRIVATE "_CRT_SECURE_NO_WARNINGS=")
    # Suppress warning on conversion from size_t to int, possible loss of data.
    TARGET_COMPILE_OPTIONS(gsd PRIVATE "/wd4267" "/wd4244")
    # Suppress warning on signed/unsigned mismatch.
    TARGET_COMPILE_OPTIONS(gsd PRIVATE "/wd4018")
ENDIF()

IF(NOT BUILD_SHARED_LIBS AND OVITO_BUILD_PYPI)
    # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
    SET_PROPERTY(TARGET gsd PROPERTY POSITION_INDEPENDENT_CODE ON)
ENDIF()

IF(EMSCRIPTEN)
    # Adopt the Emscript compiler flags from Qt's platform target.
    # That's because all libraries linked into the static WASM module need be built with the same flags.
    TARGET_LINK_LIBRARIES(gsd PRIVATE Qt::Platform)
ENDIF()

# Export this target.
INSTALL(TARGETS gsd EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
