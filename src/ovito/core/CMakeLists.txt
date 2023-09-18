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

# Find ffmpeg libraries for video encoding support.
IF(NOT EMSCRIPTEN)
    FIND_PACKAGE(FFMPEG)
ENDIF()

# Find the zlib library, needed for reading/writing compressed data files.
IF(NOT EMSCRIPTEN)
    FIND_PACKAGE(ZLIB)
ENDIF()

# Locate the libssh library, needed for the built-in SSH client.
IF(OVITO_BUILD_SSH_CLIENT AND NOT EMSCRIPTEN)
    FIND_PACKAGE(libssh QUIET)
ENDIF()

# Locate the Boost library.
SET(_Boost_IMPORTED_TARGETS TRUE) # Workaround for bug in CMake 3.5, which doesn't define the Boost::boost imported target.
FIND_PACKAGE(Boost REQUIRED)
IF(NOT TARGET Boost::boost)
    MESSAGE(FATAL_ERROR "The Boost library was not found on your system. Please make sure it is available.")
ENDIF()

SET(SourceFiles
    utilities/Exception.cpp
    utilities/linalg/AffineDecomposition.cpp
    utilities/io/SaveStream.cpp
    utilities/io/LoadStream.cpp
    app/Application.cpp
    app/ApplicationService.cpp
    app/PluginManager.cpp
    app/StandaloneApplication.cpp
    app/UserInterface.cpp
    app/undo/UndoableOperation.cpp
    app/undo/UndoStack.cpp
    oo/CloneHelper.cpp
    oo/OvitoClass.cpp
    oo/OvitoObject.cpp
    oo/PropertyFieldDescriptor.cpp
    oo/PropertyField.cpp
    oo/RefMaker.cpp
    oo/RefMakerClass.cpp
    oo/RefTarget.cpp
    oo/RefTargetListener.cpp
    dataset/animation/AnimationSettings.cpp
    dataset/animation/controller/Controller.cpp
    dataset/animation/controller/ConstantControllers.cpp
    dataset/animation/controller/KeyframeController.cpp
    dataset/animation/controller/LinearInterpolationControllers.cpp
    dataset/animation/controller/SplineInterpolationControllers.cpp
    dataset/animation/controller/TCBInterpolationControllers.cpp
    dataset/animation/controller/AnimationKeys.cpp
    dataset/animation/controller/LookAtController.cpp
    dataset/animation/controller/PRSTransformationController.cpp
    dataset/DataSet.cpp
    dataset/DataSetContainer.cpp
    dataset/io/FileImporter.cpp
    dataset/io/FileExporter.cpp
    dataset/io/FileSourceImporter.cpp
    dataset/io/FileSource.cpp
    dataset/io/AttributeFileExporter.cpp
    dataset/scene/SceneNode.cpp
    dataset/scene/PipelineSceneNode.cpp
    dataset/scene/Scene.cpp
    dataset/scene/ScenePreparation.cpp
    dataset/scene/SceneAnimationPlayback.cpp
    dataset/scene/SelectionSet.cpp
    dataset/data/DataVis.cpp
    dataset/data/DataObject.cpp
    dataset/data/DataBuffer.cpp
    dataset/data/DataCollection.cpp
    dataset/data/TransformingDataVis.cpp
    dataset/data/TransformedDataObject.cpp
    dataset/data/camera/AbstractCameraObject.cpp
    dataset/data/AttributeDataObject.cpp
    dataset/data/mesh/TriMeshObject.cpp
    dataset/data/mesh/TriMeshVis.cpp
    dataset/pipeline/ActiveObject.cpp
    dataset/pipeline/PipelineFlowState.cpp
    dataset/pipeline/PipelineCache.cpp
    dataset/pipeline/PipelineEvaluation.cpp
    dataset/pipeline/PipelineObject.cpp
    dataset/pipeline/PipelineStatus.cpp
    dataset/pipeline/CachingPipelineObject.cpp
    dataset/pipeline/AsynchronousModifier.cpp
    dataset/pipeline/Modifier.cpp
    dataset/pipeline/ModifierApplication.cpp
    dataset/pipeline/ModifierGroup.cpp
    dataset/pipeline/ModifierTemplates.cpp
    dataset/pipeline/AsynchronousModifierApplication.cpp
    dataset/pipeline/AsynchronousDelegatingModifier.cpp
    dataset/pipeline/StaticSource.cpp
    dataset/pipeline/BasePipelineSource.cpp
    dataset/pipeline/DelegatingModifier.cpp
    utilities/units/UnitsManager.cpp
    utilities/io/ObjectSaveStream.cpp
    utilities/io/ObjectLoadStream.cpp
    utilities/io/FileManager.cpp
    utilities/io/RemoteFileJob.cpp
    utilities/io/CompressedTextReader.cpp
    utilities/io/CompressedTextWriter.cpp
    utilities/concurrent/TaskWatcher.cpp
    utilities/concurrent/Task.cpp
    utilities/concurrent/ProgressingTask.cpp
    utilities/concurrent/AsynchronousTask.cpp
    utilities/concurrent/TaskManager.cpp
    utilities/concurrent/MainThreadOperation.cpp
    utilities/concurrent/ExecutionContext.cpp
    utilities/io/ssh/SshConnection.cpp
    utilities/io/ssh/openssh/OpensshConnection.cpp
    utilities/io/ssh/openssh/SshRequest.cpp
    utilities/io/ssh/openssh/DownloadRequest.cpp
    utilities/io/ssh/openssh/FileListingRequest.cpp
    rendering/SceneRenderer.cpp
    rendering/ParticlePrimitive.cpp
    rendering/MeshPrimitive.cpp
    rendering/MarkerPrimitive.cpp
    rendering/LinePrimitive.cpp
    rendering/CylinderPrimitive.cpp
    rendering/ImagePrimitive.cpp
    rendering/TextPrimitive.cpp
    rendering/RenderSettings.cpp
    rendering/FrameBuffer.cpp
    rendering/StandardSceneRenderer.cpp
    rendering/ColorCodingGradient.cpp
    viewport/Viewport.cpp
    viewport/ViewportWindowInterface.cpp
    viewport/ViewportConfiguration.cpp
    viewport/ViewportLayout.cpp
    viewport/ViewportSettings.cpp
    viewport/overlays/ViewportOverlay.cpp
    viewport/overlays/CoordinateTripodOverlay.cpp
    viewport/overlays/TextLabelOverlay.cpp
)

# Include video encoder interface code.
IF(FFMPEG_FOUND)
    LIST(APPEND SourceFiles utilities/io/video/VideoEncoder.cpp)
ELSE()
    MESSAGE("ffmpeg video encoding library not found. OVITO will be built without support for video output.")
ENDIF()

# Include optional SSH client code.
IF(OVITO_BUILD_SSH_CLIENT)
    IF(libssh_FOUND)
        MESSAGE(STATUS "Found libssh. Will build integrated SSH client.")
        LIST(APPEND SourceFiles
            utilities/io/ssh/libssh/LibsshConnection.cpp
            utilities/io/ssh/libssh/SshChannel.cpp
            utilities/io/ssh/libssh/ProcessChannel.cpp
            utilities/io/ssh/libssh/ScpChannel.cpp
            utilities/io/ssh/libssh/LsChannel.cpp
            utilities/io/ssh/libssh/LibsshWrapper.cpp
        )
    ELSE()
        MESSAGE("libssh not found. OVITO will be built without integrated SSH client.")
    ENDIF()
ENDIF()

# Build the askpass utility, which is needed for the external OpenSSH-based client.
IF(OVITO_BUILD_APP AND NOT EMSCRIPTEN)
    ADD_SUBDIRECTORY(utilities/io/ssh/openssh/askpass)
ENDIF()

# Make use of the zlib library provided by Emscripten.
IF(EMSCRIPTEN)
    MESSAGE("Using Emscripten port of zlib library.")
    ADD_LIBRARY(ZLIB::ZLIB INTERFACE IMPORTED)
    TARGET_COMPILE_OPTIONS(ZLIB::ZLIB INTERFACE "SHELL:-s USE_ZLIB=1")
    SET(ZLIB_FOUND TRUE)
ENDIF()

# Include optional Gzip file support.
IF(ZLIB_FOUND)
    LIST(APPEND SourceFiles
        utilities/io/gzdevice/GzipIODevice.cpp
    )
ELSE()
    MESSAGE("zlib library not found. OVITO will be built without I/O support for gzip compressed data files.")
ENDIF()

# Define library target.
OVITO_STANDARD_PLUGIN(Core
    SOURCES ${SourceFiles} resources/core.qrc
    LIB_DEPENDENCIES Boost::boost function2::function2
    PRECOMPILED_HEADERS Core.h
)

# Disable Qt AUTOMOC processing of the header OvitoClass.h.
# This is necessary, because the header contains the definition of the OVITO_CLASS macro, which
# is added to the AUTOMOC_MACRO_NAMES list.
SET_PROPERTY(SOURCE oo/OvitoClass PROPERTY SKIP_AUTOMOC ON)

IF(NOT EMSCRIPTEN)
    # Qt Network module is used by the Core module, except in the wasm build.
    FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Network REQUIRED)
    TARGET_LINK_LIBRARIES(Core PUBLIC Qt6::Network)
ENDIF()

# Link to zlib.
IF(ZLIB_FOUND)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_ZLIB_SUPPORT)
    TARGET_LINK_LIBRARIES(Core PUBLIC ZLIB::ZLIB)
ENDIF()

# Link to Libssh.
IF(OVITO_BUILD_SSH_CLIENT AND libssh_FOUND)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_SSH_CLIENT)
    IF(OVITO_LIBSSH_RUNTIME_LINKING)
        TARGET_COMPILE_DEFINITIONS(Core PRIVATE OVITO_LIBSSH_RUNTIME_LINKING)
        TARGET_INCLUDE_DIRECTORIES(Core PRIVATE "${LIBSSH_INCLUDE_DIR}")
        FILE(RELATIVE_PATH OVITO_LIBSSH_RELATIVE_PATH "${OVITO_BINARY_DIRECTORY}" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}")
        TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_LIBSSH_RELATIVE_PATH=\"${OVITO_LIBSSH_RELATIVE_PATH}\"")
    ELSE()
        TARGET_LINK_LIBRARIES(Core PRIVATE ssh)
    ENDIF()
ENDIF()

# Link to video encoding lib.
IF(FFMPEG_FOUND)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_VIDEO_OUTPUT_SUPPORT)
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
    TARGET_LINK_LIBRARIES(Core PRIVATE "${FFMPEG_LIBRARIES}")
    TARGET_INCLUDE_DIRECTORIES(Core BEFORE PRIVATE "${FFMPEG_INCLUDE_DIRS}")
ENDIF()

# Define the OVITO_DEBUG macro in debug builds.
TARGET_COMPILE_DEFINITIONS(Core PUBLIC "$<$<CONFIG:Debug>:OVITO_DEBUG>")
# Define QT_NO_DEBUG_OUTPUT in release builds.
TARGET_COMPILE_DEFINITIONS(Core PUBLIC "$<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT=1>")

# Turn off Clang compiler warnings regarding undefined instantiation of static variable of class templates.
IF(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    TARGET_COMPILE_OPTIONS(Core PUBLIC "-Wno-undefined-var-template")
ENDIF()

IF(MSVC AND OVITO_BUILD_CONDA)
    # Silence deprecation warnings in conda-forge's 'qt-main' package (qpdfwriter.h and qpagedpaintdevice.h).
    TARGET_COMPILE_OPTIONS(Core PUBLIC "/wd4996")
ENDIF()

# Enable SYCL in source code.
IF(OVITO_USE_SYCL STREQUAL OpenSYCL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_OPENSYCL)
ELSEIF(OVITO_USE_SYCL STREQUAL DPC++)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_SYCL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC OVITO_USE_DPCPP)
ENDIF()

# Compute the relative path from the OVITO executable to where the plugin libraries get installed.
FILE(RELATIVE_PATH OVITO_PLUGINS_RELATIVE_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_PLUGINS_DIRECTORY}")
TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PLUGINS_RELATIVE_PATH=\"${OVITO_PLUGINS_RELATIVE_PATH}\"")

IF(NOT OVITO_BUILD_CONDA)
    # Compute the relative path from the OVITO executable to where the Python module layer gets installed.
    FILE(RELATIVE_PATH OVITO_PYTHON_LAYER_PATH "${OVITO_BINARY_DIRECTORY}" "${OVITO_PYTHON_DIRECTORY}")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PYTHON_LAYER_PATH=\"${OVITO_PYTHON_LAYER_PATH}\"")
ELSE()
    # Compute the relative path from the Conda prefix to where the Python module layer gets installed.
    FILE(RELATIVE_PATH OVITO_PYTHON_LAYER_PATH "$ENV{PREFIX}" "$ENV{SP_DIR}")
    TARGET_COMPILE_DEFINITIONS(Core PRIVATE "OVITO_PYTHON_LAYER_PATH=\"${OVITO_PYTHON_LAYER_PATH}\"")
    MESSAGE(STATUS "PREFIX: $ENV{PREFIX}")
    MESSAGE(STATUS "SP_DIR: $ENV{SP_DIR}")
    MESSAGE(STATUS "OVITO_PYTHON_LAYER_PATH: ${OVITO_PYTHON_LAYER_PATH}")
ENDIF()

# Link to the pthread library on Linux platform.
IF(UNIX AND NOT APPLE)
    SET(THREADS_PREFER_PTHREAD_FLAG ON)
    FIND_PACKAGE(Threads REQUIRED)
    TARGET_LINK_LIBRARIES(Core PUBLIC Threads::Threads)
ENDIF()

IF(NOT OVITO_DOUBLE_PRECISION_FP)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "FLOATTYPE_FLOAT")
ENDIF()
IF(OVITO_BUILD_APPSTORE_VERSION)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_APPSTORE_VERSION")
ENDIF()
IF(OVITO_BUILD_MONOLITHIC)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_MONOLITHIC")
ENDIF()
IF(OVITO_DISABLE_THREADING)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_DISABLE_THREADING")
ENDIF()
IF(OVITO_QML_GUI)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_QML_GUI")
ENDIF()
IF(OVITO_BUILD_PROFESSIONAL)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_PROFESSIONAL")
ENDIF()
IF(OVITO_BUILD_BASIC)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_BASIC")
ENDIF()
IF(OVITO_BUILD_CONDA)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_BUILD_CONDA")
ENDIF()
IF(EMSCRIPTEN)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_DISABLE_QSETTINGS")
ENDIF()
IF(OVITO_NO_EVENT_LOOP)
    TARGET_COMPILE_DEFINITIONS(Core PUBLIC "OVITO_NO_EVENT_LOOP")
ENDIF()

# Inject program release version into the source code.
SET_PROPERTY(SOURCE app/Application.cpp app/StandaloneApplication.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_VERSION_MAJOR=${OVITO_VERSION_MAJOR}"
    "OVITO_VERSION_MINOR=${OVITO_VERSION_MINOR}"
    "OVITO_VERSION_REVISION=${OVITO_VERSION_REVISION}"
    "OVITO_VERSION_STRING=\"${OVITO_VERSION_STRING}\""
    "OVITO_APPLICATION_NAME=\"${OVITO_APPLICATION_NAME}\"")
SET_PROPERTY(SOURCE utilities/io/SaveStream.cpp utilities/io/LoadStream.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS
    "OVITO_FILE_FORMAT_VERSION=${OVITO_FILE_FORMAT_VERSION}")

# Run clang-tidy to check C++ code and perform static analysis.
IF(OVITO_CLANG_TIDY_CMD)
    SET_TARGET_PROPERTIES(Core PROPERTIES CXX_CLANG_TIDY "${OVITO_CLANG_TIDY_CMD}")
ENDIF()

# Install ffmpeg video encoding libraries.
IF(FFMPEG_FOUND AND NOT EMSCRIPTEN)
    FOREACH(lib ${FFMPEG_LIBRARIES})
        GET_FILENAME_COMPONENT(base_name "${lib}" NAME_WE)
        GET_FILENAME_COMPONENT(lib_path "${lib}" PATH)
        IF(WIN32 AND NOT OVITO_BUILD_CONDA)
            FILE(GLOB lib_dll "${lib_path}/${base_name}-*.dll")
            IF(NOT lib_dll)
                MESSAGE(FATAL_ERROR "Could not find corresponding DLL for ffmpeg library '${lib}' in same directory.")
            ENDIF()
            FOREACH(dll ${lib_dll})
                OVITO_INSTALL_SHARED_LIB("${dll}")
            ENDFOREACH()
        ELSEIF(UNIX AND NOT APPLE AND (OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI))
            FILE(GLOB lib_versions "${lib}*")
            FOREACH(lib_version ${lib_versions})
                OVITO_INSTALL_SHARED_LIB("${lib_version}")
            ENDFOREACH()
        ENDIF()
    ENDFOREACH()
ENDIF()

IF(ZLIB_FOUND AND NOT EMSCRIPTEN)
    IF(WIN32 AND NOT OVITO_BUILD_CONDA)
        # Deploy the zlib DLL.
        GET_TARGET_PROPERTY(ZLIB_LIBRARY_LOCATION ZLIB::ZLIB LOCATION)
        GET_FILENAME_COMPONENT(ZLIB_LIBRARY_PATH "${ZLIB_LIBRARY_LOCATION}" PATH)
        OVITO_INSTALL_SHARED_LIB("${ZLIB_LIBRARY_PATH}/../bin/zlib.dll")
    ENDIF()
ENDIF()

# Workaround for failing Linux conda builds (may become obsolete in the future once the issue in qt6-main has been fixed):
#   "warning: libEGL.so.1, needed by libQt6Gui.so.6.5.0, not found (try using -rpath or -rpath-link"
IF(OVITO_BUILD_CONDA AND UNIX AND NOT APPLE)
    # Find and link against EGL library (optional).
    FIND_PACKAGE(OpenGL COMPONENTS EGL)
    IF(TARGET OpenGL::EGL)
        TARGET_LINK_LIBRARIES(Core PUBLIC OpenGL::EGL)
    ENDIF()
ENDIF()

# Deploy the libssh library and its dependencies.
IF(OVITO_BUILD_SSH_CLIENT AND libssh_FOUND AND NOT EMSCRIPTEN AND NOT OVITO_BUILD_CONDA)
    IF(WIN32)
        # Install libssh.dylib
        GET_TARGET_PROPERTY(LIBSSH_LIBRARY_LOCATION ssh LOCATION)
        GET_FILENAME_COMPONENT(LIBSSH_LIBRARY_DIR "${LIBSSH_LIBRARY_LOCATION}" PATH)
        GET_FILENAME_COMPONENT(LIBSSH_LIBRARY_NAME "${LIBSSH_LIBRARY_LOCATION}" NAME_WE)
        OVITO_INSTALL_SHARED_LIB("${LIBSSH_LIBRARY_DIR}/../bin/${LIBSSH_LIBRARY_NAME}.dll")

        # Also install the OpenSSL DLLs needed by libssh.
        SET(OPENSSL_ROOT_DIR "" CACHE PATH "Location of the OpenSSL library installation")
        IF(NOT OPENSSL_ROOT_DIR)
            MESSAGE(FATAL_ERROR "Please specify the location of the OpenSSL library by setting the OPENSSL_ROOT_DIR variable.")
        ENDIF()
        STRING(REPLACE "\\" "/" _OPENSSL_ROOT_DIR "${OPENSSL_ROOT_DIR}")
        FILE(GLOB openssl_dlls "${_OPENSSL_ROOT_DIR}/bin/*.dll")
        IF(NOT openssl_dlls)
            MESSAGE(FATAL_ERROR "Could not find any OpenSSL DLLs in \"${_OPENSSL_ROOT_DIR}/bin/\". Please check value of OPENSSL_ROOT_DIR path variable.")
        ENDIF()
        FOREACH(openssl_dll ${openssl_dlls})
            OVITO_INSTALL_SHARED_LIB("${openssl_dll}")
        ENDFOREACH()

    ELSEIF(APPLE)

        IF(OVITO_LIBSSH_RUNTIME_LINKING)
            # Create a symlink to libssh in the build directory to make it available at runtime.
            GET_TARGET_PROPERTY(LIBSSH_LIBRARY_LOCATION ssh LOCATION)
            FILE(MAKE_DIRECTORY "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}")
            EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${LIBSSH_LIBRARY_LOCATION}" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/libssh.dylib")
            # Install libssh in the installation directory to ship it with OVITO.
            INSTALL(FILES "${LIBSSH_LIBRARY_LOCATION}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/")
        ENDIF()

    ELSEIF(UNIX AND OVITO_REDISTRIBUTABLE_PACKAGE)

        # Install libssh.so
        GET_TARGET_PROPERTY(LIBSSH_LIBRARY_LOCATION ssh LOCATION)
        OVITO_INSTALL_SHARED_LIB("${LIBSSH_LIBRARY_LOCATION}")

        # Install OpenSSL (libcrypto.so and libssl.so) needed by libssh.
        FOREACH(lib libcrypto.so libssl.so)
            FIND_LIBRARY(openssl_lib_location NAMES ${lib} PATHS /usr/lib /usr/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH NO_CACHE)
            IF(NOT openssl_lib_location)
                MESSAGE(FATAL_ERROR "Could not find shared library ${lib} in system path, which is required by libssh.")
            ENDIF()
            OVITO_INSTALL_SHARED_LIB("${openssl_lib_location}")
            UNSET(openssl_lib_location)
        ENDFOREACH()
    ENDIF()
ENDIF()
