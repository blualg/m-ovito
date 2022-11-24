#######################################################################################
#
#  Copyright 2022 OVITO GmbH, Germany
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

# Find the required Qt modules.
FIND_PACKAGE(${OVITO_QT_MAJOR_VERSION} ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Widgets REQUIRED)

# Define the GUI component of the OpenGLRenderer plugin.
OVITO_STANDARD_PLUGIN(OpenGLRendererGui
	SOURCES
		OpenGLViewportWindow.cpp
#		OpenGLOffscreenViewportWindow.cpp
	PLUGIN_DEPENDENCIES
		OpenGLRenderer
		GuiBase
	LIB_DEPENDENCIES
		${OVITO_QT_MAJOR_VERSION}::Widgets 
)

# QOpenGLWidget class has been moved from the Qt Widgets to the OpenGLWidgets module in Qt 6.
# Add OpenGLWidgets as an extra dependency to the Ovito plugin module.
IF(OVITO_QT_MAJOR_VERSION STREQUAL "Qt6")
	FIND_PACKAGE(${OVITO_QT_MAJOR_VERSION} ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS OpenGLWidgets REQUIRED)
	TARGET_LINK_LIBRARIES(OpenGLRendererGui PUBLIC ${OVITO_QT_MAJOR_VERSION}::OpenGLWidgets)
ENDIF()