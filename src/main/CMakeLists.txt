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

# Find the required Qt modules.
OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core Gui REQUIRED)

# Include the resource in the Win32 executable.
IF(WIN32)
    SET(WINDOWS_RESOURCES resources/ovito.rc)
ENDIF(WIN32)

IF(CMAKE_BUILD_TYPE STREQUAL "Release")
    # This will create a GUI application on Windows platform.
    SET(OVITO_WIN32_EXECUTABLE "WIN32")
ELSE()
    # This will create a console application on Windows platform.
    SET(OVITO_WIN32_EXECUTABLE "")
ENDIF()

# Builds the main executable of the application.
IF(NOT EMSCRIPTEN)
    ADD_EXECUTABLE(Ovito ${OVITO_WIN32_EXECUTABLE} Main.cpp ${WINDOWS_RESOURCES})
ELSE()
    QT_ADD_EXECUTABLE(Ovito Main.cpp ${WINDOWS_RESOURCES})
ENDIF()

# The executable depends on the core module, of course.
TARGET_LINK_LIBRARIES(Ovito PRIVATE Core)

# The executable depends on the gui module.
TARGET_LINK_LIBRARIES(Ovito PRIVATE Gui)
TARGET_LINK_LIBRARIES(Ovito PRIVATE
    Qt6::Core
    Qt6::Gui)

# Support SYCL.
OVITO_ADD_SYCL_TO_TARGET(Ovito)

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(Ovito)

# Place the executable into the right installation directory.
IF(NOT OVITO_BUILD_CONDA)
    IF(APPLE)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}")
        INSTALL(TARGETS Ovito DESTINATION ".")
    ELSEIF(LINUX)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}/bin")
        INSTALL(TARGETS Ovito DESTINATION "bin/")
    ELSEIF(WIN32)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}")
        INSTALL(TARGETS Ovito DESTINATION ".")
    ENDIF()
ELSE()
    IF(APPLE OR LINUX)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}/bin")
        INSTALL(TARGETS Ovito DESTINATION "bin/")
    ELSEIF(WIN32)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
        INSTALL(TARGETS Ovito DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}")
    ENDIF()
ENDIF()

# Set name of executable.
SET_TARGET_PROPERTIES(Ovito PROPERTIES OUTPUT_NAME "ovito")

IF(WIN32 AND OVITO_ACTIVE_PYTHON_RUNTIME_DLL)
    ADD_CUSTOM_COMMAND(TARGET Ovito POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON_RUNTIME_DLL}" "$<TARGET_FILE_DIR:Ovito>/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
        COMMENT "Syncing embedded Python runtime for Ovito")
    IF(OVITO_ACTIVE_PYTHON3_DLL)
        ADD_CUSTOM_COMMAND(TARGET Ovito POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON3_DLL}" "$<TARGET_FILE_DIR:Ovito>/python3.dll")
    ENDIF()
ENDIF()

# Small developer utility for headless transport regression checks against
# external references such as PyLAT on local Windows builds.
IF(WIN32)
    ADD_EXECUTABLE(OvitoTransportHarness transport_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoTransportHarness PRIVATE
        Core
        Gui
        Particles
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoTransportHarness)
    SET_TARGET_PROPERTIES(OvitoTransportHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_transport_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoTransportHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoTransportHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoPythonModifierHarness python_modifier_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoPythonModifierHarness PRIVATE
        Core
        Gui
        Particles
        PyScript
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoPythonModifierHarness)
    SET_TARGET_PROPERTIES(OvitoPythonModifierHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_python_modifier_harness")
    IF(OVITO_ACTIVE_PYTHON_RUNTIME_DLL)
        ADD_CUSTOM_COMMAND(TARGET OvitoPythonModifierHarness POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON_RUNTIME_DLL}" "$<TARGET_FILE_DIR:OvitoPythonModifierHarness>/python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll"
            COMMENT "Syncing embedded Python runtime for the Python modifier harness")
        IF(OVITO_ACTIVE_PYTHON3_DLL)
            ADD_CUSTOM_COMMAND(TARGET OvitoPythonModifierHarness POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OVITO_ACTIVE_PYTHON3_DLL}" "$<TARGET_FILE_DIR:OvitoPythonModifierHarness>/python3.dll")
        ENDIF()
    ENDIF()

    ADD_EXECUTABLE(OvitoInterfacialHarness interfacial_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoInterfacialHarness PRIVATE
        Core
        Gui
        Particles
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoInterfacialHarness)
    SET_TARGET_PROPERTIES(OvitoInterfacialHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_interfacial_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoInterfacialHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoInterfacialHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoWillardChandlerHarness willard_chandler_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoWillardChandlerHarness PRIVATE
        Core
        Gui
        Particles
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoWillardChandlerHarness)
    SET_TARGET_PROPERTIES(OvitoWillardChandlerHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_willard_chandler_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoWillardChandlerHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoWillardChandlerHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoMoleculeComHarness molecule_com_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoMoleculeComHarness PRIVATE
        Core
        Gui
        Particles
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoMoleculeComHarness)
    SET_TARGET_PROPERTIES(OvitoMoleculeComHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_molecule_com_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoMoleculeComHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoMoleculeComHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoTimeSeriesHarness time_series_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoTimeSeriesHarness PRIVATE
        Core
        Gui
        Particles
        StdMod
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoTimeSeriesHarness)
    SET_TARGET_PROPERTIES(OvitoTimeSeriesHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_time_series_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoTimeSeriesHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoTimeSeriesHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoTimeAveragingHarness time_averaging_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoTimeAveragingHarness PRIVATE
        Core
        Gui
        Particles
        StdMod
        StdObj
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoTimeAveragingHarness)
    SET_TARGET_PROPERTIES(OvitoTimeAveragingHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_time_averaging_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoTimeAveragingHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoTimeAveragingHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoImportProgressHarness import_progress_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoImportProgressHarness PRIVATE
        Core
        Gui
        Particles
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoImportProgressHarness)
    SET_TARGET_PROPERTIES(OvitoImportProgressHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_import_progress_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoImportProgressHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoImportProgressHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()

    ADD_EXECUTABLE(OvitoFullImportHarness full_import_harness.cpp)
    TARGET_LINK_LIBRARIES(OvitoFullImportHarness PRIVATE
        Core
        Gui
        Particles
        Qt6::Core
        Qt6::Gui)
    OVITO_ADD_STANDARD_COMPILE_OPTIONS(OvitoFullImportHarness)
    SET_TARGET_PROPERTIES(OvitoFullImportHarness PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${Ovito_BINARY_DIR}"
        OUTPUT_NAME "ovito_full_import_harness")
    FOREACH(plugin ${OVITO_PLUGIN_LIST})
        IF(BUILD_SHARED_LIBS)
            ADD_DEPENDENCIES(OvitoFullImportHarness ${plugin})
        ELSE()
            TARGET_LINK_LIBRARIES(OvitoFullImportHarness PRIVATE ${plugin})
        ENDIF()
    ENDFOREACH()
ENDIF()

# Copy in HTML/JS launch files.
IF(EMSCRIPTEN)
    GET_TARGET_PROPERTY(APPNAME Ovito OUTPUT_NAME)
    CONFIGURE_FILE("${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/plugins/platforms/qtloader.js" "${OVITO_BINARY_DIRECTORY}/qtloader.js" COPYONLY)
    CONFIGURE_FILE("wasm/wasm_shell.html" "${OVITO_BINARY_DIRECTORY}/wasm_shell.html")
    CONFIGURE_FILE("wasm/index.html" "${OVITO_BINARY_DIRECTORY}/index.html" COPYONLY)
    CONFIGURE_FILE("../ovito/gui/base/resources/mainwin/window_icon_256.png" "${OVITO_BINARY_DIRECTORY}/ovito_logo.png" COPYONLY)
ENDIF()

# Under Linux, Qt's Xcb platform plugin will dynamically load the DBus Qt module.
# To avoid the dynamic linker pulling the wrong versions of the Qt module from the
# system path, we make the executable preload the module on application startup.
IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)
    OVITO_FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS DBus REQUIRED)
    TARGET_LINK_LIBRARIES(Ovito PRIVATE Qt6::DBus)
ENDIF()

SET_TARGET_PROPERTIES(Ovito PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
IF(APPLE)
    IF(NOT OVITO_BUILD_CONDA)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES MACOSX_BUNDLE TRUE)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES INSTALL_RPATH "@executable_path/;@executable_path/../Frameworks/;@executable_path/../PlugIns/")
    ELSE()
        SET_TARGET_PROPERTIES(Ovito PROPERTIES MACOSX_BUNDLE FALSE)
        SET_TARGET_PROPERTIES(Ovito PROPERTIES INSTALL_RPATH "@loader_path/;@loader_path/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}/")
    ENDIF()
ELSEIF(LINUX)
    SET_TARGET_PROPERTIES(Ovito PROPERTIES INSTALL_RPATH "$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/:$ORIGIN/../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/:$ORIGIN/../${OVITO_RELATIVE_PLUGINS_DIRECTORY}/")
ENDIF()

# Build all plugins first before building the main executable.
FOREACH(plugin ${OVITO_PLUGIN_LIST})
    IF(BUILD_SHARED_LIBS)
        ADD_DEPENDENCIES(Ovito ${plugin})
    ELSE()
        TARGET_LINK_LIBRARIES(Ovito PRIVATE ${plugin})
    ENDIF()
ENDFOREACH()

IF(EMSCRIPTEN)
    # Link to missing Qt libraries (the order does matter).
    IF(NOT OVITO_DISABLE_THREADING)
        TARGET_LINK_LIBRARIES(Ovito PRIVATE "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/WorkerScript/libworkerscriptplugin.a") # QtQml.WorkerScript
    ENDIF()
    TARGET_LINK_LIBRARIES(Ovito PRIVATE
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/libqmlplugin.a"               # QtQml
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/Models/libmodelsplugin.a"     # QtQml.Models
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/libqtquick2plugin.a"        # QtQuick
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/libqtquickcontrols2plugin.a" # QtQuick.Controls
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/impl/libqtquickcontrols2implplugin.a" # QtQuick.Controls.impl
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.a" # QtQuick.Controls.Basic
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/libqtquickcontrols2universalstyleplugin.a" # QtQuick.Controls.Universal
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/impl/libqtquickcontrols2universalstyleimplplugin.a" # QtQuick.Controls.Universal.impl
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Layouts/libqquicklayoutsplugin.a" # QtQuick.Layouts
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Templates/libqtquicktemplates2plugin.a" # QtQuick.Templates
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Window/libquickwindow.a" # QtQuick.Window
        "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/Qt/labs/qmlmodels/liblabsmodelsplugin.a" # Qt.labs.qmlmodels
#       "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/plugins/iconengines/libqsvgicon.a"      # SVG Icon engine plugin
#       "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/plugins/imageformats/libqsvg.a"         # SVG image format plugin
    )
    # When built without exception support, Qt6 passes DISABLE_EXCEPTION_CATCHING=1 to the Emscripten linker (see QtWasmHelpers.cmake).
    # However, OVITO requires exception support in its own code. The following commands patch the INTERFACE_LINK_OPTIONS property
    # of the Qt platform module to override the linker flag value.
    get_target_property(_opt_old Qt6::Platform INTERFACE_LINK_OPTIONS)
    string(REPLACE "DISABLE_EXCEPTION_CATCHING=1" "DISABLE_EXCEPTION_CATCHING=0" _opt_new "${_opt_old}")
    set_target_properties(Qt6::Platform PROPERTIES INTERFACE_LINK_OPTIONS "${_opt_new}")
ENDIF()

# Install fonts required by 'minimal' platform plugin on Linux
IF(LINUX AND NOT OVITO_BUILD_CONDA)
    INSTALL(DIRECTORY "resources/fonts" DESTINATION "${OVITO_RELATIVE_SHARE_DIRECTORY}/")
ENDIF()

IF(LINUX AND OVITO_REDISTRIBUTABLE_PACKAGE)

    # Create a qt.conf file.
    # This is required to let Qt search for its plugin modules in the program directory.
    FILE(WRITE "${OVITO_BINARY_DIRECTORY}/qt.conf" "[Paths]\nPlugins = ../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
    INSTALL(CODE "
        FILE(WRITE \"\${CMAKE_INSTALL_PREFIX}/${OVITO_RELATIVE_BINARY_DIRECTORY}/qt.conf\" \"[Paths]\\nPlugins = ../${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/\")
        ")

ELSEIF(WIN32 AND NOT OVITO_BUILD_CONDA)

    # Create a qt.conf file.
    # This is required to let Qt search for its plugin modules in the program directory.
    FILE(WRITE "${OVITO_BINARY_DIRECTORY}/qt.conf" "[Paths]\nPlugins = plugins/")
    INSTALL(CODE "
        FILE(WRITE \"\${CMAKE_INSTALL_PREFIX}/${OVITO_RELATIVE_BINARY_DIRECTORY}/qt.conf\" \"[Paths]\\nPlugins = plugins/\")
        ")

ELSEIF(APPLE AND NOT OVITO_BUILD_CONDA)

    # Install the Info.plist file in the app bundle.
    CONFIGURE_FILE("resources/Info.plist" "${Ovito_BINARY_DIR}/${MACOSX_BUNDLE_NAME}.app/Contents/Info.plist")
    SET_TARGET_PROPERTIES(Ovito PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${Ovito_BINARY_DIR}/${MACOSX_BUNDLE_NAME}.app/Contents/Info.plist")

    # Copy the application icon into the resource directory.
    INSTALL(FILES "resources/ovito.icns" DESTINATION "${OVITO_RELATIVE_SHARE_DIRECTORY}")

    # Fix Mac app bundle.
    INCLUDE("${Ovito_SOURCE_DIR}/cmake/FixupMacBundle.cmake")

ENDIF()

# Special steps needed to build OVITO with Python scripting support.
IF(OVITO_BUILD_PLUGIN_PYSCRIPT)

    # Make sure the Python library gets loaded at program startup. We need to do it
    # this way, because the OVITO scripting plugins do not explicitly link the
    # Python interpreter library. They rely on it already being loaded into the
    # process memory (as is the case if the OVITO Python modules are imported by an external interpreter).
    OVITO_FIND_PACKAGE(Python3 REQUIRED COMPONENTS Interpreter Development)
    TARGET_LINK_LIBRARIES(Ovito PRIVATE pybind11::headers Python3::Python)
    TARGET_COMPILE_DEFINITIONS(Ovito PRIVATE "OVITO_BUILD_PLUGIN_PYSCRIPT=1")
    TARGET_INCLUDE_DIRECTORIES(Ovito PRIVATE "${CMAKE_SOURCE_DIR}/proprietary")

    IF(OVITO_REDISTRIBUTABLE_PACKAGE AND LINUX)
        # Uninstall Python packages from the embedded interpreter that should not be part of the official distribution.
        INSTALL(CODE "${OVITO_UNINSTALL_UNUSED_PYTHON_MODULES_CODE}")
    ENDIF()
ENDIF()
