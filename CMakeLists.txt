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

# Make sure we have a recent version of CMake.
CMAKE_MINIMUM_REQUIRED(VERSION 3.12.0 FATAL_ERROR)
CMAKE_POLICY(VERSION 3.12.0)

# Choose CMake policies.
IF(NOT CMAKE_VERSION VERSION_LESS "3.17")
    CMAKE_POLICY(SET CMP0100 NEW)   # Let AUTOMOC and AUTOUIC process .hh files.
    CMAKE_POLICY(SET CMP0099 NEW)   # Link properties are transitive over private dependency on static libraries.
ENDIF()
IF(NOT CMAKE_VERSION VERSION_LESS "3.13")
    CMAKE_POLICY(SET CMP0079 NEW)   # Allows use of target_link_libraries() with targets defined in other directories.
ENDIF()

PROJECT(Ovito)

# This is to enable target debugging within Visual Studio Code.
# It is ignored outside of Visual Studio Code/CMake tools.
INCLUDE(CMakeToolsHelpers OPTIONAL)

IF(EMSCRIPTEN)
    INCLUDE(cmake/wasm.cmake)
ENDIF()

SET(OVITO_SOURCE_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
LIST(APPEND CMAKE_MODULE_PATH "${OVITO_SOURCE_BASE_DIR}/cmake")
INCLUDE(cmake/Version.cmake)
INCLUDE(cmake/Plugins.cmake)

# Define build options.
OPTION(OVITO_DOUBLE_PRECISION_FP "Use double-precision floating-point numbers." "ON")
OPTION(OVITO_BUILD_APP "Build the OVITO desktop application." "ON")
OPTION(OVITO_QML_GUI "Use an alternative (experimental) graphical user interface, which is based on Qt's QML module instead of Widgets." "OFF")
OPTION(OVITO_BUILD_MONOLITHIC "Build the application as a monolithic executable, statically linking in all plugins." "OFF")
OPTION(OVITO_REDISTRIBUTABLE_PACKAGE "Create a redistributable program package that includes third-party libraries." "OFF")
OPTION(OVITO_BUILD_APPSTORE_VERSION "Build binaries for the Apple App Store and Windows Store." "OFF")
OPTION(OVITO_RUN_CLANG_TIDY "Run the clang-tidy tool to check code." "OFF")
OPTION(OVITO_USE_ADDRESS_SANITIZER "Builds the code with AddressSanitizer support to detect memory errors." "OFF")
OPTION(OVITO_USE_PRECOMPILED_HEADERS "Use precompiled C++ headers to speed up build (requires CMake 3.16)." "ON")
OPTION(OVITO_USE_UNITY_BUILD "Use unity builds to speed up compilation (requires CMake 3.16)." "OFF")
OPTION(OVITO_DISABLE_THREADING "Disable multi-threading code (meant only for development purposes)." "OFF")
OPTION(OVITO_USE_GIT_REVISION_NUMBER "Include Git revision information in OVITO version string." "OFF")
OPTION(OVITO_BUILD_SSH_CLIENT "Build the SSH client for remote file access (requires libssh)." "ON")
OPTION(OVITO_LIBSSH_RUNTIME_LINKING "Load libssh at runtime" "OFF")
OPTION(OVITO_USE_SYCL "Enable SYCL build" "OFF")

# Ovito needs Qt>=6.2.
SET(OVITO_MINIMUM_REQUIRED_QT_VERSION 6.2)
FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core REQUIRED)

# Look for optional Vulkan SDK (requires version 1.1 or above).
FIND_PACKAGE(Vulkan)

# Define user options that control the building of OVITO's standard plugins.
OPTION(OVITO_BUILD_PLUGIN_STDOBJ "Build the standard objects plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_STDMOD "Build the standard modifiers plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_PARTICLES "Build the plugin for particle-related functions." "ON")
OPTION(OVITO_BUILD_PLUGIN_MESH "Build the Mesh plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_GRID "Build the Grid plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_CRYSTALANALYSIS "Build the CrystalAnalysis plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_NETCDFPLUGIN "Build the NetCDF plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_CORRELATION "Build the spatial correlation function modifier plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_VOROTOP "Build the VoroTop modifier plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_GALAMOST "Build the GALAMOST I/O plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_OXDNA "Build the oxDNA I/O plugin." "ON")
OPTION(OVITO_BUILD_PLUGIN_VULKAN "Build the Vulkan renderer plugin." "${Vulkan_FOUND}")

# This is a global list of plugin targets that will be built.
# It will get populated by the OVITO_PLUGIN function.
SET(OVITO_PLUGINS_LIST "")

# Enable software testing framework.
ENABLE_TESTING()

# Activate C++17 language standard.
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set visibility of symbols in libraries to hidden by default, except those exported in the source code.
SET(CMAKE_CXX_VISIBILITY_PRESET "hidden")
SET(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Enable link time optimization (LTO)
IF(WIN32 OR APPLE) # On Linux, LTO currently breaks the executable due to missing symbols at runtime.
    IF(${CMAKE_BUILD_TYPE} MATCHES Release)
        INCLUDE(CheckIPOSupported)
        check_ipo_supported(RESULT result OUTPUT output)
        IF(result)
            MESSAGE(STATUS "Using link-time optimization (LTO)")
            SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        ENDIF()
    ENDIF()
ENDIF()

IF(CYGWIN AND CMAKE_COMPILER_IS_GNUCXX)
    # Linking fails without -O3
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
ENDIF()

IF(APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-invalid-offsetof")
ENDIF()

IF(OVITO_USE_SYCL AND NOT OPENSYCL_DEBUG_LEVEL)
    IF(CMAKE_BUILD_TYPE MATCHES "Debug")
        SET(OPENSYCL_DEBUG_LEVEL 3 CACHE INTEGER "Choose the debug level, options are: 0 (no debug), 1 (print errors), 2 (also print warnings), 3 (also print general information)" FORCE)
    ELSE()
        SET(OPENSYCL_DEBUG_LEVEL 2 CACHE INTEGER "Choose the debug level, options are: 0 (no debug), 1 (print errors), 2 (also print warnings), 3 (also print general information)" FORCE)
    ENDIF()
ENDIF()

# Enable build with AddressSanitizer support if requested.
IF(OVITO_USE_ADDRESS_SANITIZER)
    IF(MSVC)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
    ELSE()
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fsanitize=address")
    ENDIF()
ENDIF()

# Set the name of the macOS application bundle to generate.
SET(MACOSX_BUNDLE_NAME "Ovito")
SET(MACOSX_BUNDLE_BUNDLE_NAME "${MACOSX_BUNDLE_NAME}")

IF(NOT OVITO_BUILD_PYPI)
    IF(UNIX AND NOT APPLE)
        # The directory where the main executable goes to.
        SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
        # The directory where the main libraries go to.
        SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "lib/ovito")
        # The directory where the third-party libraries go to.
        SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "lib/ovito")
        # The directory where the auxiliary files go to.
        SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
        # The directory where the compiled plugins go to.
        SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins")
        # The directory where the Python source modules go to.
        IF(NOT OVITO_BUILD_CONDA)
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "${OVITO_RELATIVE_PLUGINS_DIRECTORY}/python")
        ELSE()
            # For Conda package builds:
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "$ENV{SP_DIR}")
        ENDIF()
    ELSEIF(APPLE)
        IF(NOT OVITO_BUILD_CONDA)
            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/MacOS")
            # The directory where the main libraries of Ovito go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/MacOS")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Frameworks")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Resources")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/PlugIns")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "${MACOSX_BUNDLE_NAME}.app/Contents/Resources/python")
        ELSE()
            # For Conda package builds:

            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "lib/ovito")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "$ENV{SP_DIR}")
        ENDIF()
    ELSEIF(WIN32)
        IF(NOT OVITO_BUILD_CONDA)
            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY ".")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY ".")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY ".")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY ".")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the Python source modules go to.
            SET(OVITO_RELATIVE_PYTHON_DIRECTORY "./plugins/python")
        ELSE()
            # For Conda package builds:

            # The directory where the main executable goes to.
            SET(OVITO_RELATIVE_BINARY_DIRECTORY "bin")
            # The directory where the main libraries go to.
            SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "bin")
            # The directory where the third-party libraries go to.
            SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the auxiliary files go to.
            SET(OVITO_RELATIVE_SHARE_DIRECTORY "share/ovito")
            # The directory where the compiled plugins go to.
            SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
            # The directory where the Python source modules go to.
            FILE(TO_CMAKE_PATH "$ENV{SP_DIR}" OVITO_RELATIVE_PYTHON_DIRECTORY)
        ENDIF()
    ENDIF()
ELSE()
    # For PyPI wheel builds:

    # The directory where the main executable goes to.
    SET(OVITO_RELATIVE_BINARY_DIRECTORY "ovito")
    # The directory where the main libraries go to.
    SET(OVITO_RELATIVE_LIBRARY_DIRECTORY "ovito/plugins")
    # The directory where the third-party libraries go to.
    SET(OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    # The directory where the auxiliary files go to.
    SET(OVITO_RELATIVE_SHARE_DIRECTORY "ovito")
    # The directory where the compiled plugins go to.
    SET(OVITO_RELATIVE_PLUGINS_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    # The directory where the Python source modules go to.
    SET(OVITO_RELATIVE_PYTHON_DIRECTORY ".")
ENDIF()

# The directory where the main executable goes to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_BINARY_DIRECTORY}")
    SET(OVITO_BINARY_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_BINARY_DIRECTORY}")
ELSE()
    SET(OVITO_BINARY_DIRECTORY "${OVITO_RELATIVE_BINARY_DIRECTORY}")
ENDIF()
# The directory where the main libraries go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
    SET(OVITO_LIBRARY_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
ELSE()
    SET(OVITO_LIBRARY_DIRECTORY "${OVITO_RELATIVE_LIBRARY_DIRECTORY}")
ENDIF()
# The directory where the compiled plugins go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
    SET(OVITO_PLUGINS_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
ELSE()
    SET(OVITO_PLUGINS_DIRECTORY "${OVITO_RELATIVE_PLUGINS_DIRECTORY}")
ENDIF()
# The directory where the Python source files go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_PYTHON_DIRECTORY}")
    SET(OVITO_PYTHON_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_PYTHON_DIRECTORY}")
ELSE()
    SET(OVITO_PYTHON_DIRECTORY "${OVITO_RELATIVE_PYTHON_DIRECTORY}")
ENDIF()
# The directory where the auxiliary files go to.
IF(NOT IS_ABSOLUTE "${OVITO_RELATIVE_SHARE_DIRECTORY}")
    SET(OVITO_SHARE_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_SHARE_DIRECTORY}")
ELSE()
    SET(OVITO_SHARE_DIRECTORY "${OVITO_RELATIVE_SHARE_DIRECTORY}")
ENDIF()

IF(APPLE OR WIN32 OR NOT OVITO_REDISTRIBUTABLE_PACKAGE)
    # Add the automatically determined parts of the RPATH,
    # which point to directories outside the build tree to the install RPATH.
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
ENDIF()

# Select linkage type (shared or static) for plugin libraries.
IF(OVITO_BUILD_MONOLITHIC)
    SET(BUILD_SHARED_LIBS OFF)
    SET(OVITO_PLUGIN_LIBRARY_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
ELSE()
    SET(BUILD_SHARED_LIBS ON)
    # Define name suffix used for generating plugin libraries.
    IF(APPLE)
        # On macOS, we use the .so extension instead of the standard .dylib to be compatible
        # with the Python interpreter, which only finds modules having a .so suffix.
        SET(OVITO_PLUGIN_LIBRARY_SUFFIX ".so")
    ELSE()
        SET(OVITO_PLUGIN_LIBRARY_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    ENDIF()
ENDIF()

# Helper functions for finding and deploying third-party prerequisites.
INCLUDE(cmake/Prerequisites.cmake)

# Determine the command for running the clang-tidy tool.
IF(OVITO_RUN_CLANG_TIDY)
    FIND_PROGRAM(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
    SET(OVITO_CLANG_TIDY_CMD "${CLANG_TIDY_EXE}") # List of checks will be read from the .clang-tidy file.
ENDIF()

# Generate the build targets that are part of Ovito.
ADD_SUBDIRECTORY(src)

# This allows external CMake projects to inject their customization code into the CMake project:
IF(OVITO_CMAKE_FILE_INJECTION_PROJECT)
    INCLUDE("${OVITO_CMAKE_FILE_INJECTION_PROJECT}")
ENDIF()

# Generate the user manual.
ADD_SUBDIRECTORY(doc/manual)
