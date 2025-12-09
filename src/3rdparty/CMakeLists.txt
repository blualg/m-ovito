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


IF(OVITO_BUILD_CONDA)
    # In case of using shared library version, we rely on the system-installed zstd package.
    FIND_PACKAGE(ZSTD REQUIRED)
ELSE()
    # Configure and build zstd from source for static linking.
    SET(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "Disable legacy support in zstd")
    SET(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Disable building zstd programs")
    SET(ZSTD_BUILD_TESTS OFF CACHE BOOL "Disable building zstd tests")
    SET(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "Disable building zstd contrib")
    SET(ZSTD_BUILD_STATIC ON CACHE BOOL "Enable building zstd static library")
    SET(ZSTD_BUILD_SHARED OFF CACHE BOOL "Disable building zstd shared library")
    ADD_SUBDIRECTORY(zstd/build/cmake zstd EXCLUDE_FROM_ALL)
    ADD_LIBRARY(zstd::libzstd_static ALIAS libzstd_static) # Target zstd::libzstd_static is consumed by OVITO's Core module.
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
    # Disable tests for gemmi
    SET(_build_testing_backup ${BUILD_TESTING})
    SET(BUILD_TESTING OFF)
    ADD_SUBDIRECTORY(gemmi EXCLUDE_FROM_ALL)
    SET(BUILD_TESTING ${_build_testing_backup})
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
