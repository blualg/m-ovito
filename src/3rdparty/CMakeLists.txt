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
