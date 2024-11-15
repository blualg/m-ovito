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
        OpenGLShaderHelper.cpp
        OpenGLRenderer.cpp
        OpenGLRenderingJob.cpp
        OpenGLRenderingFrameBuffer.cpp
        OpenGLPickingMap.cpp
        OffscreenOpenGLRenderingJob.cpp
        resources/opengl.qrc
    LIB_DEPENDENCIES
        Qt6::OpenGL
)

IF(NOT OVITO_BUILD_CONDA)
    IF(APPLE)
        # OpenGL functions have been marked as deprecated since macOS 10.14. Silence the compiler warnings.
        TARGET_COMPILE_DEFINITIONS(OpenGLRenderer PRIVATE "GL_SILENCE_DEPRECATION")
    ENDIF()
ENDIF()

# Build module that containing the interactive viewport widget.
ADD_SUBDIRECTORY(window)

# Build corresponding GUI plugin.
IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(gui)
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
