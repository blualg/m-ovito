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

# Enables CUDA support for a CMake target.
MACRO(OVITO_ADD_CUDA_TO_TARGET target_name)
    IF(OVITO_USE_CUDA)
        FIND_PACKAGE(CUDAToolkit REQUIRED)

        TARGET_COMPILE_DEFINITIONS(${target_name} PUBLIC OVITO_USE_CUDA)
        TARGET_LINK_LIBRARIES(${target_name} PUBLIC CUDA::cudart_static)

        IF(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CUDA_COMPILER_ID STREQUAL "NVIDIA")
            TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=\"/Zc:__cplusplus\">)
        ENDIF()

        TARGET_COMPILE_OPTIONS(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
            --use_fast_math
            --relocatable-device-code=true
            --expt-relaxed-constexpr
            $<$<CONFIG:Debug>:--generate-line-info>
        >)
    ENDIF()
ENDMACRO()
