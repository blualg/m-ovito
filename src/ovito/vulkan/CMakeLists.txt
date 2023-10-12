#######################################################################################
#
#  Copyright 2023 OVITO GmbH, Germany
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

# Find Vulkan library.
FIND_PACKAGE(Vulkan REQUIRED)

function(add_vulkan_shader target shaderfile)

    # Locate glslc shader compiler.
    # CMake 3.19 and newer define the Vulkan::glslc imported target for the shader compiler.
    IF(TARGET Vulkan::glslc)
        SET(glslc_executable "$<TARGET_FILE:Vulkan::glslc>")
    ELSE()
        FIND_PROGRAM(glslc_executable
            NAMES glslc
            HINTS "$ENV{VULKAN_SDK}/bin")
    ENDIF()

    SET(current-shader-path ${CMAKE_CURRENT_SOURCE_DIR}/${shaderfile})
    SET(current-output-path ${CMAKE_CURRENT_SOURCE_DIR}/${shaderfile}.spv)

    # Add a custom command to compile GLSL to SPIR-V.
    ADD_CUSTOM_COMMAND(
           OUTPUT "${current-output-path}"
           COMMAND "${glslc_executable}" -o "${current-output-path}" "${current-shader-path}"
           DEPENDS "${current-shader-path}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/resources/picking.glsl"
                   "${CMAKE_CURRENT_SOURCE_DIR}/resources/shading.glsl"
                   "${CMAKE_CURRENT_SOURCE_DIR}/resources/global_uniforms.glsl"
           IMPLICIT_DEPENDS CXX "${current-shader-path}"
           COMMENT "Compiling Vulkan shader file ${shaderfile}"
           VERBATIM)

    # Make sure our build depends on this output.
    SET_SOURCE_FILES_PROPERTIES("${current-output-path}" PROPERTIES GENERATED TRUE)
    TARGET_SOURCES(${target} PRIVATE "${current-output-path}")
endfunction()

# Define plugin module.
OVITO_STANDARD_PLUGIN(VulkanRenderer
    SOURCES
        VulkanContext.cpp
        VulkanPipeline.cpp
        VulkanSceneRenderer.cpp
        OffscreenVulkanSceneRenderer.cpp
        VulkanLinePrimitive.cpp
        VulkanImagePrimitive.cpp
        VulkanParticlePrimitive.cpp
        VulkanCylinderPrimitive.cpp
        VulkanMeshPrimitive.cpp
        vma/vk_mem_alloc.cpp
        resources/vulkan.qrc
)

# Make sure the Vulkan headers are available.
# Note that linking to the Vulkan library is not necessary, because Qt will load it at runtime.
TARGET_INCLUDE_DIRECTORIES(VulkanRenderer PUBLIC "${Vulkan_INCLUDE_DIRS}")

# Recompile the Vulkan shader source files if the shader compiler is available.
IF(NOT OVITO_DONT_RECOMPILE_VULKAN_SHADERS AND TARGET Vulkan::glslc)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_with_colors.frag)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_with_colors.vert)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_uniform_color.frag)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_uniform_color.vert)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/lines/thin_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/image/image.frag)
    add_vulkan_shader(VulkanRenderer resources/image/image.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/cube/cube.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/cube/cube.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/cube/cube_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/cube/cube_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/sphere/sphere.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/sphere/sphere.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/sphere/sphere_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/sphere/sphere_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/square/square.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/square/square.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/square/square_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/square/square_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/circle/circle.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/circle/circle.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/circle/circle_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/circle/circle_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/imposter/imposter.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/imposter/imposter.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/imposter/imposter_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/imposter/imposter_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/box/box.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/box/box.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/box/box_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/box/box_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/ellipsoid/ellipsoid.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/ellipsoid/ellipsoid.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/ellipsoid/ellipsoid_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/ellipsoid/ellipsoid_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/superquadric/superquadric.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/superquadric/superquadric.vert)
    add_vulkan_shader(VulkanRenderer resources/particles/superquadric/superquadric_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/particles/superquadric/superquadric_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_flat.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_flat.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_flat_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/cylinder_flat_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_head.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_head.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_head_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_head_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_tail.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_tail.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_tail_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_tail_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_flat.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_flat.vert)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_flat_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/cylinder/arrow_flat_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_wireframe.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_wireframe.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_wireframe_instanced.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_wireframe_instanced.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced_picking.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced_picking.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced_with_colors.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_instanced_with_colors.vert)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_color_mapping.frag)
    add_vulkan_shader(VulkanRenderer resources/mesh/mesh_color_mapping.vert)
ENDIF()

# Build corresponding GUI plugin.
IF(OVITO_BUILD_APP)
    ADD_SUBDIRECTORY(gui)
ENDIF()

# On macOS, deploy Vulkan loader and MoltenVK library as part of the app bundle.
IF(APPLE AND NOT OVITO_BUILD_CONDA AND OVITO_BUILD_APP)
    GET_TARGET_PROPERTY(VULKAN_LIBRARY_LOCATION Vulkan::Vulkan LOCATION)
    GET_FILENAME_COMPONENT(VULKAN_LIBRARY_PATH "${VULKAN_LIBRARY_LOCATION}" PATH)
    OVITO_INSTALL_SHARED_LIB("${VULKAN_LIBRARY_LOCATION}")
    OVITO_INSTALL_SHARED_LIB("${VULKAN_LIBRARY_PATH}/libMoltenVK.dylib")

    # The macOS Vulkan loader looks in the application's bundle for ICD and layer JSON files.
    # It looks in the bundle's Resources/vulkan/icd.d/ directory for ICD JSON files and in the bundle's
    # Resources/vulkan/explicit_layer.d/ directory for layer JSON files.
    CONFIGURE_FILE("resources/MoltenVK_icd.json" "${Ovito_BINARY_DIR}/${MACOSX_BUNDLE_NAME}.app/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json")
    INSTALL(FILES "resources/MoltenVK_icd.json" DESTINATION "${MACOSX_BUNDLE_NAME}.app/Contents/Resources/vulkan/icd.d/")
ENDIF()

# Propagate list of plugins to parent scope.
SET(OVITO_PLUGIN_LIST ${OVITO_PLUGIN_LIST} PARENT_SCOPE)
