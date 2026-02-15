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

# CMake configuration for 3rdparty libraries used by OVITO.

# Build zstd compression library from source for static linking.
IF(NOT OVITO_BUILD_CONDA)
    # Configure and build zstd using FetchContent.
    # This provides cleaner integration without polluting the CMake namespace with zstd options.
    INCLUDE(FetchContent)
    SET(ZSTD_LEGACY_SUPPORT OFF)
    SET(ZSTD_BUILD_PROGRAMS OFF)
    SET(ZSTD_BUILD_TESTS OFF)
    SET(ZSTD_BUILD_CONTRIB OFF)
    IF(OVITO_ZSTD_USE_SHARED_LIB)
        SET(ZSTD_BUILD_STATIC OFF)
        SET(ZSTD_BUILD_SHARED ON)
    ELSE()
        SET(ZSTD_BUILD_STATIC ON)
        SET(ZSTD_BUILD_SHARED OFF)
    ENDIF()

    # Declare zstd using the local git submodule. Set EXCLUDE_FROM_ALL to avoid installation of zstd build artifacts together with the main OVITO build.
    FetchContent_Declare(zstd SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zstd/build/cmake EXCLUDE_FROM_ALL)

    # Populate zstd (this processes its CMakeLists.txt with the options set above)
    FetchContent_MakeAvailable(zstd)

    # Create an alias for compatibility with existing code in core/CMakeLists.txt that references zstd::libzstd_static.
    IF(OVITO_ZSTD_USE_SHARED_LIB)
        ADD_LIBRARY(zstd::libzstd_shared ALIAS libzstd_shared)
        ADD_LIBRARY(zstd::libzstd ALIAS libzstd_shared)
    ELSE()
        ADD_LIBRARY(zstd::libzstd_static ALIAS libzstd_static)
    ENDIF()

    # Get the full version from the zstd sub-project.
    LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/zstd/build/cmake/CMakeModules")
    INCLUDE(GetZstdLibraryVersion)
    GetZstdLibraryVersion("zstd/lib/zstd.h" zstd_VERSION_MAJOR zstd_VERSION_MINOR zstd_VERSION_PATCH)
    # Register zstd package in SBOM.
    OVITO_REGISTER_PACKAGE_FOR_SBOM("zstd" "${zstd_VERSION_MAJOR}.${zstd_VERSION_MINOR}.${zstd_VERSION_PATCH}")
ENDIF()

ADD_SUBDIRECTORY(function2)
IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(qwt)
ENDIF()
IF(OVITO_BUILD_PLUGIN_STDOBJ OR OVITO_BUILD_PLUGIN_PARTICLES)
    ADD_SUBDIRECTORY(muparser)
ENDIF()
IF(OVITO_BUILD_PLUGIN_PARTICLES)
    ADD_SUBDIRECTORY(voro++)
    ADD_SUBDIRECTORY(ptm)
    ADD_SUBDIRECTORY(mwm_csp)
    ADD_SUBDIRECTORY(xdrfile)
    ADD_SUBDIRECTORY(gsd)

    # Build gemmi from source.
    ADD_LIBRARY(gemmi STATIC
                gemmi/src/align.cpp gemmi/src/assembly.cpp gemmi/src/calculate.cpp gemmi/src/ccp4.cpp
                gemmi/src/crd.cpp gemmi/src/ddl.cpp gemmi/src/eig3.cpp gemmi/src/fprime.cpp # gemmi/src/gz.cpp
                gemmi/src/intensit.cpp gemmi/src/json.cpp gemmi/src/mmcif.cpp # gemmi/src/mmread_gz.cpp
                gemmi/src/monlib.cpp gemmi/src/mtz.cpp gemmi/src/mtz2cif.cpp
                gemmi/src/pdb.cpp gemmi/src/polyheur.cpp gemmi/src/read_cif.cpp
                gemmi/src/resinfo.cpp gemmi/src/riding_h.cpp
                gemmi/src/select.cpp gemmi/src/sprintf.cpp gemmi/src/dssp.cpp gemmi/src/symmetry.cpp
                gemmi/src/to_json.cpp gemmi/src/to_mmcif.cpp gemmi/src/to_pdb.cpp gemmi/src/topo.cpp
                gemmi/src/xds_ascii.cpp)
    SET_PROPERTY(TARGET gemmi PROPERTY POSITION_INDEPENDENT_CODE ON)
    TARGET_COMPILE_DEFINITIONS(gemmi PRIVATE GEMMI_BUILD)
    TARGET_INCLUDE_DIRECTORIES(gemmi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/gemmi/include")
    TARGET_INCLUDE_DIRECTORIES(gemmi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/gemmi/third_party")
    IF(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        TARGET_COMPILE_OPTIONS(gemmi PRIVATE /wd4267) # conversion from 'size_t' to 'unsigned int', possible loss of data
        TARGET_COMPILE_DEFINITIONS(gemmi PRIVATE _CRT_SECURE_NO_WARNINGS)
    ENDIF()
ENDIF()
IF(OVITO_BUILD_PLUGIN_PARTICLES)
    ADD_SUBDIRECTORY(geogram)
ENDIF()
IF(OVITO_BUILD_PLUGIN_NETCDFPLUGIN)
    ADD_SUBDIRECTORY(netcdf_integration)
ENDIF()
IF(OVITO_BUILD_PLUGIN_CORRELATION)
    ADD_SUBDIRECTORY(kissfft)
ENDIF()
