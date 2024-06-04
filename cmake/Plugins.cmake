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

# This macro creates an OVITO plugin module.
MACRO(OVITO_STANDARD_PLUGIN target_name)

    # Parse macro arguments.
    SET(options GUI_PLUGIN HAS_NO_EXPORTS)
    SET(multiValueArgs SOURCES LIB_DEPENDENCIES PRIVATE_LIB_DEPENDENCIES PLUGIN_DEPENDENCIES OPTIONAL_PLUGIN_DEPENDENCIES PRECOMPILED_HEADERS)
    CMAKE_PARSE_ARGUMENTS(ARG
        "${options}" # options
        ""  # one-value keywords
        "${multiValueArgs}" # multi-value keywords
        ${ARGN}) # strings to parse

    # Validate argument values.
    IF(ARG_UNPARSED_ARGUMENTS)
        MESSAGE(FATAL_ERROR "Bad macro arguments: ${ARG_UNPARSED_ARGUMENTS}")
    ENDIF()
    SET(plugin_sources ${ARG_SOURCES})
    SET(lib_dependencies ${ARG_LIB_DEPENDENCIES})
    SET(private_lib_dependencies ${ARG_PRIVATE_LIB_DEPENDENCIES})
    SET(plugin_dependencies ${ARG_PLUGIN_DEPENDENCIES})
    SET(optional_plugin_dependencies ${ARG_OPTIONAL_PLUGIN_DEPENDENCIES})
    SET(precompiled_headers ${ARG_PRECOMPILED_HEADERS})

    # Determine the type of library target to build.
    SET(plugin_library_type "")
    IF(OVITO_BUILD_MONOLITHIC)
        # When building a static executable, create a CMake object library for each plugin.
        SET(plugin_library_type "OBJECT")
    ELSEIF(BUILD_SHARED_LIBS AND ${ARG_HAS_NO_EXPORTS})
        # Define the library as a module if it doesn't export any symbols.
        SET(plugin_library_type "MODULE")
    ELSEIF(NOT BUILD_SHARED_LIBS AND EMSCRIPTEN)
        # When building a static executable for WASM, create a CMake object library for each plugin.
        SET(plugin_library_type "OBJECT")
    ENDIF()

    # Create the library target for the plugin.
    ADD_LIBRARY(${target_name} ${plugin_library_type} ${plugin_sources})

    # Set default include directory.
    TARGET_INCLUDE_DIRECTORIES(${target_name} PUBLIC
        "$<BUILD_INTERFACE:${OVITO_SOURCE_BASE_DIR}/src>")

    # Speed up compilation by using precompiled headers.
    IF(OVITO_USE_PRECOMPILED_HEADERS)
        FOREACH(precompiled_header ${precompiled_headers})
            TARGET_PRECOMPILE_HEADERS(${target_name} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/${precompiled_header}>")
        ENDFOREACH()
    ENDIF()

    # Speed up compilation by using unity build.
    IF(OVITO_USE_UNITY_BUILD)
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES UNITY_BUILD ON)
    ENDIF()

    IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # Turn off certain Microsoft compiler warnings.
        TARGET_COMPILE_OPTIONS(${target_name}
            PUBLIC "/wd4267" # Suppress warning on conversion from size_t to int, possible loss of data.
            PUBLIC "/bigobj" # Compiling template code leads to large object files.
        )

        # Do not warn about use of unsafe CRT Library functions.
        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC "_CRT_SECURE_NO_WARNINGS=1")
        # Workaround for deprecation warnings in Visual Studio 2022 due to Qt's QVarLength Array:
        # "warning C4996: 'stdext::checked_array_iterator<Ovito::TimeInterval *>': warning STL4043: stdext::checked_array_iterator, stdext::unchecked_array_iterator, and related factory functions are non-Standard extensions and will be removed in the future. std::span (since C++20) and gsl::span can be used instead."
        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING=1")

        IF(OVITO_BUILD_CONDA)
            # Silence deprecation warnings in conda-forge's 'qt-main' package (qpdfwriter.h and qpagedpaintdevice.h).
            TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "/wd4996")
        ENDIF()

        # Activate newer lambda function processor of MSVC (needed for correct copy-of-this captures).
        # (https://docs.microsoft.com/en-us/cpp/build/reference/zc-lambda?view=msvc-170)
        #IF(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29)
        #   TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "/Zc:lambda")
        #ELSEIF(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.23)
        #   TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "/experimental:newLambdaProcessor")
        #ENDIF()
    ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Turn off Clang compiler warnings regarding undefined instantiation of static variable of class templates.
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-undefined-var-template")
        # We are using MPL compile-time string literals in our code. Disable the resulting compiler warning ("warning: multi-character character constant [-Wmultichar]").
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-multichar")
    ELSEIF(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
        # Silence Intel DPC++ compiler warning: "explicit comparison with NaN in fast floating point mode" on every use of std::isnan()
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-tautological-constant-compare")
        # Silence Intel DPC++ compiler warning: "field XXX will be initialized after field YYY"
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-reorder-ctor")
        # Silence Intel DPC++ compiler warning: "destructor called on non-final XXX that has virtual functions but non-virtual destructor"
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-delete-non-abstract-non-virtual-dtor")
        # Silence Intel DPC++ compiler warning: "known but unsupported action 'shared' for '#pragma section' - ignored"
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-ignored-pragmas")
        # Silence Intel DPC++ compiler warning: "warning: 'QImage::setPixelColor' redeclared inline; 'dllimport' attribute ignored"
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-ignored-attributes")
        # Silence Intel DPC++ compiler remark: "Note that use of '-g' without any optimization-level option will turn off most compiler optimizations similar to use of '-O0'"
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Rno-debug-disables-optimization")
    ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # We are using MPL compile-time string literals in our code. Disable the resulting compiler warning ("warning: multi-character character constant [-Wmultichar]").
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-multichar")
        IF(CYGWIN)
            # Linking fails without -O3
            TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-O3")
        ENDIF()
    ENDIF()

    # Make the name of current plugin available to the source code.
    TARGET_COMPILE_DEFINITIONS(${target_name} PRIVATE "OVITO_PLUGIN_NAME=\"${target_name}\"")

    IF(WIN32 AND NOT OVITO_BUILD_MONOLITHIC)
        # Add a suffix to the shared library filename to avoid name clashes with other libraries in the installation directory.
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES OUTPUT_NAME "${target_name}.ovito")
    ENDIF()

    # Link to OVITO's core module (unless it's the core plugin itself we are defining).
    IF(NOT ${target_name} STREQUAL "Core")
        TARGET_LINK_LIBRARIES(${target_name} PUBLIC Core)
    ENDIF()

    # Link to OVITO's desktop GUI module when the plugin provides a GUI.
    IF(${ARG_GUI_PLUGIN})
        IF(OVITO_BUILD_APP)
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC Gui)
            FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Widgets REQUIRED)
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC Qt6::Widgets)
        ELSE()
            MESSAGE(FATAL_ERROR "Cannot build plugin ${target_name} marked as GUI_PLUGIN if building the GUI has been completely disabled.")
        ENDIF()
    ENDIF()

    # Link to Qt framework.
    FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core Gui REQUIRED)
    TARGET_LINK_LIBRARIES(${target_name} PUBLIC Qt6::Core Qt6::Gui)

    # Link to other third-party libraries needed by this specific plugin.
    TARGET_LINK_LIBRARIES(${target_name} PUBLIC ${lib_dependencies})

    # Link to other third-party libraries needed by this specific plugin, which should not be visible to dependent plugins.
    TARGET_LINK_LIBRARIES(${target_name} PRIVATE ${private_lib_dependencies})

    # Enable SYCL.
    OVITO_ADD_SYCL_TO_TARGET(${target_name})

    # Link to other plugin modules that are dependencies of this plugin.
    FOREACH(plugin_name ${plugin_dependencies})
        IF(NOT TARGET ${plugin_name})
            STRING(TOUPPER "${plugin_name}" uppercase_plugin_name)
            IF(DEFINED OVITO_BUILD_PLUGIN_${uppercase_plugin_name} AND NOT OVITO_BUILD_PLUGIN_${uppercase_plugin_name})
                MESSAGE(FATAL_ERROR "To build the ${target_name} plugin, the ${plugin_name} plugin needs to be enabled too. Please set the OVITO_BUILD_PLUGIN_${uppercase_plugin_name} option to ON.")
            ELSE()
                MESSAGE(FATAL_ERROR "To build the ${target_name} plugin, the ${plugin_name} plugin needs to be built too. Please set the necessary CMake option(s).")
            ENDIF()
        ENDIF()
        TARGET_LINK_LIBRARIES(${target_name} PUBLIC ${plugin_name})
    ENDFOREACH()

    # Link to other plugin modules that are optional dependencies of this plugin.
    FOREACH(plugin_name ${optional_plugin_dependencies})
        STRING(TOUPPER "${plugin_name}" uppercase_plugin_name)
        IF(TARGET ${plugin_name})
            TARGET_LINK_LIBRARIES(${target_name} PUBLIC ${plugin_name})
        ENDIF()
    ENDFOREACH()

    IF(NOT EMSCRIPTEN)
        # Set prefix and suffix of library name.
        # This is needed so that the Python interpreter can load OVITO plugins as modules.
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES PREFIX "" SUFFIX "${OVITO_PLUGIN_LIBRARY_SUFFIX}")
    ENDIF()

    # Tell CMake to run Qt moc on source files added to the target.
    SET_TARGET_PROPERTIES(${target_name} PROPERTIES AUTOMOC ON)
    # Tell CMake to run the Qt resource compiler on all .qrc files added to a target.
    SET_TARGET_PROPERTIES(${target_name} PROPERTIES AUTORCC ON)

    # Define macro for symbol export from shared library.
    STRING(TOUPPER "${target_name}" _uppercase_plugin_name)
    IF(BUILD_SHARED_LIBS)
        TARGET_COMPILE_DEFINITIONS(${target_name} PRIVATE "OVITO_${_uppercase_plugin_name}_EXPORT=Q_DECL_EXPORT")
        TARGET_COMPILE_DEFINITIONS(${target_name} INTERFACE "OVITO_${_uppercase_plugin_name}_EXPORT=Q_DECL_IMPORT")
    ELSE()
        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC "OVITO_${_uppercase_plugin_name}_EXPORT=")
    ENDIF()

    IF(APPLE)
        # This is required to avoid error by install_name_tool.
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES LINK_FLAGS "-headerpad_max_install_names")
    ELSEIF(UNIX)
        # Tell linker to detect missing references already at link time (and not at runtime).
        # This check must NOT be performed when building Python extension modules, because they deliberately do not
        # link to the Python library at build time, only at runtime. That's because the Python library is assumed to be already
        # loaded into the process once the extension module gets loaded.
        # Here we assume that all OVITO modules that depend on the PyScript module, and the PyScript module itself, are Python
        # extension modules. The link-time check will not be enabled for these modules.
        GET_PROPERTY(_link_libs TARGET ${target_name} PROPERTY LINK_LIBRARIES)
        IF(NOT ${target_name} STREQUAL "PyScript" AND NOT "PyScript" IN_LIST _link_libs)
            TARGET_LINK_OPTIONS(${target_name} PRIVATE "LINKER:--no-undefined" "LINKER:--no-allow-shlib-undefined")
        ENDIF()
    ENDIF()

    IF(NOT OVITO_BUILD_PYPI)
        IF(APPLE)
            IF(NOT OVITO_BUILD_CONDA)
                SET_TARGET_PROPERTIES(${target_name} PROPERTIES INSTALL_RPATH "@loader_path/;@executable_path/;@loader_path/../MacOS/;@executable_path/../Frameworks/")
            ELSE()
                # Look for other shared libraries in the parent directory ("lib/ovito/") and in the plugins directory ("lib/ovito/plugins/")
                SET_TARGET_PROPERTIES(${target_name} PROPERTIES INSTALL_RPATH "@loader_path/;@loader_path/../")
            ENDIF()
            # The build tree target should have rpath of install tree target.
            SET_TARGET_PROPERTIES(${target_name} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
        ELSEIF(UNIX)
            # Look for other shared libraries in the parent directory ("lib/ovito/") and in the plugins directory ("lib/ovito/plugins/")
            SET_TARGET_PROPERTIES(${target_name} PROPERTIES INSTALL_RPATH "$ORIGIN:$ORIGIN/..")
        ENDIF()
    ELSE()
        IF(APPLE)
            # Use @loader_path on macOS when building the Python package.
            SET_TARGET_PROPERTIES(${target_name} PROPERTIES INSTALL_RPATH "@loader_path/")
        ELSEIF(UNIX)
            # Look for other shared libraries in the same directory.
            SET_TARGET_PROPERTIES(${target_name} PROPERTIES INSTALL_RPATH "$ORIGIN")
        ENDIF()

        IF(NOT BUILD_SHARED_LIBS)
            # Since we will link this library into the dynamically loaded Python extension module, we need to use the fPIC flag.
            SET_PROPERTY(TARGET ${target_name} PROPERTY POSITION_INDEPENDENT_CODE ON)
        ENDIF()
    ENDIF()

    # Make this module part of the installation package.
    IF(WIN32 AND (${target_name} STREQUAL "Core" OR ${target_name} STREQUAL "Gui" OR ${target_name} STREQUAL "GuiBase"))
        # On Windows, the Core and Gui DLLs need to be placed in the same directory
        # as the Ovito executable, because Windows won't find them if they are in the
        # plugins subdirectory.
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_BINARY_DIRECTORY}")
        INSTALL(TARGETS ${target_name} EXPORT OVITO
            RUNTIME DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}"
            LIBRARY DESTINATION "${OVITO_RELATIVE_BINARY_DIRECTORY}"
            ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
    ELSE()
        # Install all plugins into the plugins directory.
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${OVITO_PLUGINS_DIRECTORY}")
        SET_TARGET_PROPERTIES(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${OVITO_PLUGINS_DIRECTORY}")
        INSTALL(TARGETS ${target_name} EXPORT OVITO
            RUNTIME DESTINATION "${OVITO_RELATIVE_PLUGINS_DIRECTORY}"
            LIBRARY DESTINATION "${OVITO_RELATIVE_PLUGINS_DIRECTORY}"
            ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
    ENDIF()

    # Maintain the list of all plugins.
    LIST(APPEND OVITO_PLUGIN_LIST ${target_name})
    SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)

ENDMACRO()
