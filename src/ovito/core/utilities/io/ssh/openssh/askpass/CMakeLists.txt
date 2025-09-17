#######################################################################################
#
#  Copyright 2025 OVITO GmbH, Germany
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
FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core Gui Widgets REQUIRED)

# Builds the main executable of the application.
ADD_EXECUTABLE(ssh_askpass Main.cpp)

# Link Qt framework
TARGET_LINK_LIBRARIES(ssh_askpass PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Under Linux, Qt's Xcb platform plugin will dynamically load the DBus Qt module.
# To avoid the dynamic linker pulling the wrong versions of the Qt module from the
# system path, we make the executable preload the module on application startup.
IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)
    FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS DBus REQUIRED)
    TARGET_LINK_LIBRARIES(ssh_askpass PRIVATE Qt6::DBus)
ENDIF()

# Put the executable into the right directory.
IF(APPLE AND NOT OVITO_BUILD_CONDA)
    SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
    INSTALL(TARGETS ssh_askpass DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")
ELSEIF(UNIX)
    SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
    INSTALL(TARGETS ssh_askpass DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")
ELSEIF(WIN32)
    IF(NOT OVITO_BUILD_CONDA)
        SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
        INSTALL(TARGETS ssh_askpass DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")
    ELSE()
        SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
        INSTALL(TARGETS ssh_askpass DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")
    ENDIF()
ENDIF()

# Inherit INSTALL_RPATH target property from main Ovito executable target.
SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
SET_TARGET_PROPERTIES(ssh_askpass PROPERTIES INSTALL_RPATH "$<TARGET_PROPERTY:Ovito,INSTALL_RPATH>")