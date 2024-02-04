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

# Enables SYCL support for a CMake target.
MACRO(OVITO_ADD_SYCL_TO_TARGET target_name)

    # Enable SYCL.
    IF(OVITO_USE_SYCL STREQUAL AdaptiveCpp)
        FIND_PACKAGE(AdaptiveCpp CONFIG REQUIRED)
        ADD_SYCL_TO_TARGET(TARGET ${target_name})
        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC HIPSYCL_DEBUG_LEVEL=${ADAPTIVECPP_DEBUG_LEVEL})
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "$<$<CONFIG:Debug>:-O0>") # To silcense acpp warning: No optimization flag was given, optimizations are disabled by default.
    ELSEIF(OVITO_USE_SYCL STREQUAL DPC++)
        #ADD_SYCL_TO_TARGET(TARGET ${target_name})
        TARGET_LINK_LIBRARIES(${target_name} PUBLIC IntelSYCL::SYCL_CXX)
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
        TARGET_LINK_OPTIONS(${target_name} PUBLIC "-fsycl-targets=nvptx64-nvidia-cuda")
        TARGET_COMPILE_OPTIONS(${target_name} PUBLIC "-Wno-undefined-var-template")
    ELSEIF(NOT OVITO_USE_SYCL STREQUAL None AND NOT OVITO_USE_SYCL STREQUAL OFF)
        MESSAGE(FATAL_ERROR "Invalid OVITO_USE_SYCL setting. Must be one of [None, AdaptiveCpp, DPC++].")
    ENDIF()

ENDMACRO()
