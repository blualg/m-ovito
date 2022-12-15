#######################################################################################
#
#  Copyright 2021 OVITO GmbH, Germany
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

# This function installs a third-party shared library/DLL in the OVITO program directory
# so that it can be distributed together with the program.
# On Unix/Linux based platforms it takes care of installing symbolic links as well.
# This macro creates an OVITO plugin module.
MACRO(OVITO_INSTALL_SHARED_LIB shared_lib)

    # Parse macro arguments.
    CMAKE_PARSE_ARGUMENTS(ARG 
		"" # options
		"DESTINATION"  # one-value keywords
		"" # multi-value keywords
		${ARGN}) # strings to parse

	# Validate argument values.
	IF(ARG_UNPARSED_ARGUMENTS)
		MESSAGE(FATAL_ERROR "Bad macro arguments: ${ARG_UNPARSED_ARGUMENTS}")
	ENDIF()
	SET(destination_dir ${ARG_DESTINATION})

	# Install libs in third-party library directory by default.
	IF(NOT destination_dir)
		SET(destination_dir ".")
	ENDIF()

	IF(WIN32 OR OVITO_REDISTRIBUTABLE_PACKAGE OR OVITO_BUILD_PYPI)
		# Replace backslashes in the path with regular slashes.
		STRING(REGEX REPLACE "\\\\" "/" _shared_lib ${shared_lib}) 
		# Make sure the destination directory exists.
		SET(_abs_dest_dir "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}")
		FILE(MAKE_DIRECTORY "${_abs_dest_dir}")
		# Strip version number from shared lib filename.
		GET_FILENAME_COMPONENT(shared_lib_ext "${_shared_lib}" EXT)
		STRING(REPLACE ${shared_lib_ext} "" lib_base_name "${_shared_lib}")

		# Find all files/symlinks in the same directory having the same base name. 
		FILE(GLOB lib_versions LIST_DIRECTORIES FALSE "${_shared_lib}" "${lib_base_name}.*${CMAKE_SHARED_LIBRARY_SUFFIX}" "${lib_base_name}${CMAKE_SHARED_LIBRARY_SUFFIX}.*")
		IF(NOT lib_versions)
			MESSAGE(FATAL_ERROR "Did not find any library files that match the file path ${_shared_lib} (globbing patterns: ${lib_base_name}.*${CMAKE_SHARED_LIBRARY_SUFFIX}; ${lib_base_name}${CMAKE_SHARED_LIBRARY_SUFFIX}.*)")
		ENDIF()

		# Find all variants of the shared library name, including symbolic links.
		UNSET(lib_files)
		FOREACH(lib_version ${lib_versions})
			WHILE(IS_SYMLINK ${lib_version})
				GET_FILENAME_COMPONENT(symlink_target "${lib_version}" REALPATH)
				GET_FILENAME_COMPONENT(symlink_target_name "${symlink_target}" NAME)
				GET_FILENAME_COMPONENT(lib_version_name "${lib_version}" NAME)
				IF(NOT lib_version_name STREQUAL symlink_target_name AND NOT OVITO_BUILD_PYPI)
					MESSAGE("Installing symlink ${lib_version_name} to ${symlink_target_name}")
					EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink ${symlink_target_name} "${_abs_dest_dir}/${lib_version_name}")
					IF(NOT APPLE)
						INSTALL(FILES "${_abs_dest_dir}/${lib_version_name}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
					ENDIF()
				ENDIF()
				SET(lib_version "${symlink_target}")
			ENDWHILE()
			IF(NOT IS_SYMLINK ${lib_version})
				LIST(APPEND lib_files "${lib_version}")
			ENDIF()
		ENDFOREACH()
		LIST(REMOVE_DUPLICATES lib_files)

		FOREACH(lib_file ${lib_files})
			IF(NOT APPLE)
				MESSAGE("Installing shared library ${lib_file}")
				EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${lib_file}" "${_abs_dest_dir}/" RESULT_VARIABLE _error_var)
				IF(_error_var)
					MESSAGE(FATAL_ERROR "Failed to copy shared library into build directory: ${lib_file}")
				ENDIF()
				IF(WIN32 OR NOT OVITO_BUILD_PYPI)
					INSTALL(FILES "${lib_file}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
				ELSE()
					# Detect if this .so file is a linker script starting with the string "INPUT". 
					# The TBB libraries use this special GNU ld feature instead of regular symbolic links to create aliases of a shared library in the same directory.
					FILE(READ "${lib_file}" _SO_FILE_HEADER LIMIT 5 HEX)
					IF("${_SO_FILE_HEADER}" STREQUAL "494e505554") # 494e505554 = "INPUT"
						INSTALL(FILES "${lib_file}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
					ELSE()
						# Use the objdump command to read out the SONAME of the shared library.
						GET_FILENAME_COMPONENT(lib_filename "${lib_file}" NAME)
						EXECUTE_PROCESS(COMMAND objdump -p "${lib_file}" COMMAND grep "SONAME" OUTPUT_VARIABLE _output_var RESULT_VARIABLE _error_var OUTPUT_STRIP_TRAILING_WHITESPACE)
						STRING(REPLACE "SONAME" "" lib_soname "${_output_var}")
						STRING(STRIP "${lib_soname}" lib_soname)
						IF(_error_var OR NOT lib_soname)
							MESSAGE(FATAL_ERROR "Failed to determine SONAME of shared library: ${lib_file}")
						ENDIF()
						# Use the SONAME as file name when installing the library in the OVITO directory.
						FILE(RENAME "${_abs_dest_dir}/${lib_filename}" "${_abs_dest_dir}/${lib_soname}")
						INSTALL(PROGRAMS "${_abs_dest_dir}/${lib_soname}" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/${destination_dir}/")
					ENDIF()
				ENDIF()
			ENDIF()
		ENDFOREACH()
		UNSET(lib_files)
	ENDIF()
ENDMACRO()

# Helper function that recursively gathers a list of libraries and other targets that the given 
# target depends on directly and indirectly.
FUNCTION(get_all_target_dependencies OUTPUT_LIST TARGET)

	# This special handling was adopted from __qt_internal_walk_libs() to avoid an error produced by older CMake versions:
	IF(${TARGET} STREQUAL "${OVITO_QT_MAJOR_VERSION}::EntryPoint")
        # We can't (and don't need to) process EntryPoint because it brings in $<TARGET_PROPERTY:prop>
        # genexes which get replaced with $<TARGET_PROPERTY:EntryPoint,prop> genexes in the code below
        # and that causes 'INTERFACE_LIBRARY targets may only have whitelisted properties.' errors
        # with CMake versions equal to or lower than 3.18. These errors are super unintuitive to
        # debug because there's no mention that it's happening during a file(GENERATE) call.
        RETURN()
    ENDIF()

	# Skip the following targets, because they cause problems when querying the LINK_LIBRARIES property below.
	IF(${TARGET} MATCHES "pybind11" OR ${TARGET} STREQUAL "documentation" OR ${TARGET} STREQUAL "scripting_documentation" OR ${TARGET} STREQUAL "scripting_documentation_prerun")
		RETURN()
	ENDIF()

	GET_TARGET_PROPERTY(IMPORTED ${TARGET} IMPORTED)
    IF(IMPORTED)
		GET_TARGET_PROPERTY(LIBS ${TARGET} INTERFACE_LINK_LIBRARIES)
    ELSE()
		GET_TARGET_PROPERTY(LIBS ${TARGET} LINK_LIBRARIES)
    ENDIF()
	GET_TARGET_PROPERTY(DEPENDENCIES ${TARGET} MANUALLY_ADDED_DEPENDENCIES)	
	LIST(APPEND LIBS ${DEPENDENCIES})
    FOREACH(LIB ${LIBS})
		IF(NOT LIB IN_LIST ${OUTPUT_LIST})
			LIST(APPEND ${OUTPUT_LIST} ${LIB})
			IF(TARGET ${LIB})
				get_all_target_dependencies(${OUTPUT_LIST} ${LIB})
			ENDIF()
		ENDIF()
    ENDFOREACH()
    SET(${OUTPUT_LIST} ${${OUTPUT_LIST}} PARENT_SCOPE)
ENDFUNCTION()

# This function deploys the required Qt libraries with the program package.
FUNCTION(deploy_qt_framework_files)

	# Gather all indirect dependencies of the main executable including all plugins.
	IF(OVITO_BUILD_APP)
		get_all_target_dependencies(ovito_dependency_libraries Ovito)
	ENDIF()
	# When building just the Python bindings, get the dependencies from this target.
	IF(TARGET ovito_bindings)
		get_all_target_dependencies(ovito_dependency_libraries ovito_bindings)
	ENDIF()

	# Filter dependency list to find all Qt framework modules (targets starting with Qt5:: or Qt6::).
	FOREACH(lib ${ovito_dependency_libraries})
		IF(lib MATCHES "^${OVITO_QT_MAJOR_VERSION}::(.+)")
			LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS ${CMAKE_MATCH_1})
		ENDIF()
	ENDFOREACH()

	# Amend Qt modules list with indirect dependencies.
	# DBus module is an indirect dependency of the Xcb platform plugin under Linux.
	IF(NOT "DBus" IN_LIST OVITO_REQUIRED_QT_COMPONENTS)
		LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS DBus)
	ENDIF()
	# Svg module is an indirect dependency of the SVG icon engine plugin.
	IF(NOT "Svg" IN_LIST OVITO_REQUIRED_QT_COMPONENTS)
		LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS Svg)
	ENDIF()

	# Pull in the Qt modules as CMake targets.
	FOREACH(qtmodule IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
		FIND_PACKAGE(${OVITO_QT_MAJOR_VERSION} ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS ${qtmodule} REQUIRED)
	ENDFOREACH()

	IF(UNIX AND NOT APPLE AND OVITO_REDISTRIBUTABLE_PACKAGE)

		# Install copies of the Qt libraries.
		FILE(MAKE_DIRECTORY "${OVITO_LIBRARY_DIRECTORY}/lib")
		FOREACH(component IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
			GET_TARGET_PROPERTY(lib ${OVITO_QT_MAJOR_VERSION}::${component} LOCATION)
			GET_TARGET_PROPERTY(lib_soname ${OVITO_QT_MAJOR_VERSION}::${component} IMPORTED_SONAME_RELEASE)
			CONFIGURE_FILE("${lib}" "${OVITO_LIBRARY_DIRECTORY}" COPYONLY)
			GET_FILENAME_COMPONENT(lib_realname "${lib}" NAME)
			EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${lib_realname}" "${OVITO_LIBRARY_DIRECTORY}/${lib_soname}")
			EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "../${lib_soname}" "${OVITO_LIBRARY_DIRECTORY}/lib/${lib_soname}")
			INSTALL(FILES "${lib}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")
			INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/${lib_soname}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")
			INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/lib/${lib_soname}" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/lib/")
			GET_FILENAME_COMPONENT(QtBinaryPath ${lib} PATH)
		ENDFOREACH()

		# Install Qt plugins.
		INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/platforms" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/" PATTERN "libqvkkhrdisplay.so" EXCLUDE PATTERN "libqvnc.so" EXCLUDE)
		INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/imageformats" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/" PATTERN "libqsvg.so" EXCLUDE)
		INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/iconengines" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
		INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/xcbglintegrations" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
		IF(OVITO_QT_MAJOR_VERSION STREQUAL "Qt6")
			INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/platformthemes" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
			INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/networkinformation" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
			INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/tls" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
		ELSE()
			INSTALL(DIRECTORY "${QtBinaryPath}/../plugins/bearer" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/plugins_qt/")
		ENDIF()
		# The XcbQpa library is required by the Qt Gui module.
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/lib${OVITO_QT_MAJOR_VERSION}XcbQpa.so" DESTINATION "./lib")

		# Distribute libxkbcommon.so with Ovito, which is a dependency of the Qt XCB plugin that might not be present on all systems.
		FIND_LIBRARY(OVITO_XKBCOMMON_DEP NAMES libxkbcommon.so.0 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64 NO_DEFAULT_PATH)
		IF(NOT OVITO_XKBCOMMON_DEP)
			MESSAGE(FATAL_ERROR "Could not find shared library libxkbcommon.so.0 in system path.")
		ENDIF()
		OVITO_INSTALL_SHARED_LIB("${OVITO_XKBCOMMON_DEP}" DESTINATION "./lib")
		UNSET(OVITO_XKBCOMMON_DEP CACHE)
		# Additionally, place a symlink into the parent lib/ovito/ directory.
		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -E create_symlink "lib/libxkbcommon.so.0" "${OVITO_LIBRARY_DIRECTORY}/libxkbcommon.so.0")
		INSTALL(FILES "${OVITO_LIBRARY_DIRECTORY}/libxkbcommon.so.0" DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}/")

		# Distribute libxkbcommon-x11.so with Ovito, which is a dependency of the Qt XCB plugin that might not be present on all systems.
		FIND_LIBRARY(OVITO_XKBCOMMONX11_DEP NAMES libxkbcommon-x11.so.0 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64 NO_DEFAULT_PATH)
		IF(NOT OVITO_XKBCOMMONX11_DEP)
			MESSAGE(FATAL_ERROR "Could not find shared library libxkbcommon-x11.so.0 in system path.")
		ENDIF()
		OVITO_INSTALL_SHARED_LIB("${OVITO_XKBCOMMONX11_DEP}" DESTINATION "./lib")
		UNSET(OVITO_XKBCOMMONX11_DEP CACHE)

		# Distribute libxcb-xinerama.so with Ovito, which is a dependency of the Qt XCB plugin that might not be present on all systems.
		FIND_LIBRARY(OVITO_XINERAMA_DEP NAMES libxcb-xinerama.so.0 PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64 NO_DEFAULT_PATH)
		IF(NOT OVITO_XINERAMA_DEP)
			MESSAGE(FATAL_ERROR "Could not find shared library libxcb-xinerama.so.0 in system path.")
		ENDIF()
		OVITO_INSTALL_SHARED_LIB("${OVITO_XINERAMA_DEP}" DESTINATION "./lib")
		UNSET(OVITO_XINERAMA_DEP CACHE)
		
	ELSEIF(WIN32 AND NOT OVITO_BUILD_PYPI AND NOT OVITO_BUILD_CONDA)

		# On Windows, the third-party library DLLs need to be installed in the OVITO directory.
		# Gather Qt dynamic link libraries.
		FOREACH(component IN LISTS OVITO_REQUIRED_QT_COMPONENTS)
			GET_TARGET_PROPERTY(dll ${OVITO_QT_MAJOR_VERSION}::${component} LOCATION_${CMAKE_BUILD_TYPE})
			IF(NOT TARGET ${OVITO_QT_MAJOR_VERSION}::${component} OR NOT dll)
				MESSAGE(FATAL_ERROR "Target does not exist or has no LOCATION property: ${OVITO_QT_MAJOR_VERSION}::${component}")
			ENDIF()
			OVITO_INSTALL_SHARED_LIB("${dll}")
			IF(${component} MATCHES "Core")
				GET_FILENAME_COMPONENT(QtBinaryPath ${dll} PATH)
				IF(dll MATCHES "Cored.dll$")
					SET(_qt_dll_suffix "d")
				ELSE()
					SET(_qt_dll_suffix "")
				ENDIF()
			ENDIF()
		ENDFOREACH()

		# Install Qt plugins required by OVITO.
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/platforms/qwindows${_qt_dll_suffix}.dll" DESTINATION "plugins/platforms/")
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qjpeg${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qgif${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/imageformats/qsvg${_qt_dll_suffix}.dll" DESTINATION "plugins/imageformats/")
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/iconengines/qsvgicon${_qt_dll_suffix}.dll" DESTINATION "plugins/iconengines/")
		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/styles/qwindowsvistastyle${_qt_dll_suffix}.dll" DESTINATION "plugins/styles/")
		IF(OVITO_QT_MAJOR_VERSION STREQUAL "Qt6")
			OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qcertonlybackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")
			OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qopensslbackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")
			OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../plugins/tls/qschannelbackend${_qt_dll_suffix}.dll" DESTINATION "plugins/tls/")
		ENDIF()

		# Install QML modules.
	#	IF(OVITO_QML_GUI)
	#		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../qml/QtQuick.2/qtquick2plugin${_qt_dll_suffix}.dll" DESTINATION "qml/QtQuick.2/")
	#		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../qml/QtQuick/Layouts/qquicklayoutsplugin${_qt_dll_suffix}.dll" DESTINATION "qml/QtQuick/Layouts/")
	#		OVITO_INSTALL_SHARED_LIB("${QtBinaryPath}/../qml/QtQuick/Controls.2/qtquickcontrols2plugin${_qt_dll_suffix}.dll" DESTINATION "qml/QtQuick/Controls.2/")

	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick.2/qmldir" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick.2/")
	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick.2/plugins.qmltypes" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick.2/")
	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick/Layouts/qmldir" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick/Layouts/")
	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick/Layouts/plugins.qmltypes" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick/Layouts/")
	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick/Layouts/qmldir" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick/Layouts/")
	#		EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "${QtBinaryPath}/../qml/QtQuick/Layouts/plugins.qmltypes" "${Ovito_BINARY_DIR}/${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick/Layouts/")

	#		INSTALL(FILES "${QtBinaryPath}/../qml/QtQuick.2/qmldir" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick.2/")
	#		INSTALL(FILES "${QtBinaryPath}/../qml/QtQuick.2/plugins.qmltypes" DESTINATION "${OVITO_RELATIVE_3RDPARTY_LIBRARY_DIRECTORY}/qml/QtQuick.2/")
	#	ENDIF()

	ENDIF()
ENDFUNCTION()
