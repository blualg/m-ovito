#######################################################################################
#
#  Copyright 2023 OVITO GmbH, Germany
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

# Find the required Qt module.
FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS OpenGL REQUIRED)

# Define plugin module.
OVITO_STANDARD_PLUGIN(OpenGLRenderer
    SOURCES
        OpenGLLinePrimitive.cpp
        OpenGLParticlePrimitive.cpp
        OpenGLImagePrimitive.cpp
        OpenGLCylinderPrimitive.cpp
        OpenGLMeshPrimitive.cpp
        OpenGLMarkerPrimitive.cpp
        OpenGLResourceManager.cpp
        OpenGLShaderHelper.cpp
        OpenGLSceneRenderer.cpp
        OffscreenOpenGLSceneRenderer.cpp
        OffscreenInteractiveOpenGLSceneRenderer.cpp
        PickingOpenGLSceneRenderer.cpp
        OpenGLDepthTextureBlitter.cpp
        resources/opengl.qrc
    LIB_DEPENDENCIES
        Qt6::OpenGL
)

IF(NOT OVITO_QML_GUI AND NOT OVITO_BUILD_CONDA)
    # Find OpenGL library.
    FIND_PACKAGE(OpenGL REQUIRED)

    # Define a CMake imported target for the OpenGL libraries and headers.
    # Newer versions of CMake do that by themselves.
    IF(NOT TARGET OpenGL::GL AND OPENGL_FOUND)
        ADD_LIBRARY(OpenGL::GL UNKNOWN IMPORTED)
        SET_TARGET_PROPERTIES(OpenGL::GL PROPERTIES
            IMPORTED_LOCATION "${OPENGL_gl_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENGL_INCLUDE_DIR}"
        )
    ENDIF()

    # Link to OpenGL library.
    TARGET_LINK_LIBRARIES(OpenGLRenderer PRIVATE OpenGL::GL)

    IF(APPLE)
        # OpenGL functions have been marked as deprecated since macOS 10.14. Silence the compiler warnings.
        TARGET_COMPILE_DEFINITIONS(OpenGLRenderer PRIVATE "GL_SILENCE_DEPRECATION")
    ENDIF()

ENDIF()

# Build corresponding GUI plugin.
ADD_SUBDIRECTORY(gui)

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
