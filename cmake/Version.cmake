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

# This file defines release version information.

# This is the canonical program version number:
SET(OVITO_VERSION_MAJOR 		"3")
SET(OVITO_VERSION_MINOR 		"8")
SET(OVITO_VERSION_REVISION		"0")

# Increment the following version counter every time the .ovito file format
# changes in a backward-incompatible way.
#
# Format version 30006 - OVITO ver>3.2.1: TimeAveragingModifier changed.
# Format version 30007 - OVITO ver>3.3.5: New DataObject framework. Removed PropertyStorage class. Introduced PythonScriptDelegate class.
# Format version 30008 - OVITO ver>3.5.4: Added viewport layouts.
# Format version 30009 - OVITO ver>=3.8.0: New DataSet structure (per-viewport Scene, per-scene AnimationSettings, new AnimationTime data type)
#
SET(OVITO_FILE_FORMAT_VERSION	"30009")

# The application's default version string:
SET(OVITO_VERSION_STRING "${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}")

# Extract revision number from Git repository to tag development builds of OVITO.
FIND_PACKAGE(Git)
IF(GIT_FOUND AND OVITO_USE_GIT_REVISION_NUMBER)
	# Get the current commit hash:
	EXECUTE_PROCESS(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short" "HEAD"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		RESULT_VARIABLE GIT_RESULT_VAR1
		OUTPUT_VARIABLE GIT_COMMIT_REV_STRING
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
	# Get the current git branch:
	EXECUTE_PROCESS(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--abbrev-ref" "HEAD"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		RESULT_VARIABLE GIT_RESULT_VAR2
		OUTPUT_VARIABLE GIT_BRANCH_REV_STRING
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
	IF(GIT_RESULT_VAR1 STREQUAL "0" AND GIT_RESULT_VAR2 STREQUAL "0")
		SET(OVITO_VERSION_STRING "${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}-dev-${GIT_BRANCH_REV_STRING}-${GIT_COMMIT_REV_STRING}")
	ENDIF()
ENDIF()

# The application's name shown in the main window's title bar:
SET(OVITO_APPLICATION_NAME "Ovito")
IF(OVITO_APPLICATION_NAME_OVERRIDE)
	SET(OVITO_APPLICATION_NAME "${OVITO_APPLICATION_NAME_OVERRIDE}")
ENDIF()

# The copyright notice shown in the application's About dialog:
STRING(TIMESTAMP _CURRENT_YEAR "%Y")
SET(OVITO_COPYRIGHT_NOTICE
	"<p>A scientific data visualization and analysis software <br>for particle-based simulations.</p>\
	 <p>Copyright (C) ${_CURRENT_YEAR}, OVITO GmbH, Germany</p>\
	 <p>\
	 This is free, open-source software, and you are welcome to redistribute\
	 it under certain conditions. See the user manual for copying conditions.</p>\
	 <p><a href=\\\"https://www.ovito.org/\\\">https://www.ovito.org/</a></p>")
IF(OVITO_COPYRIGHT_NOTICE_OVERRIDE)
	SET(OVITO_COPYRIGHT_NOTICE "${OVITO_COPYRIGHT_NOTICE_OVERRIDE}")
ENDIF()

# Export variables to parent scope.
GET_DIRECTORY_PROPERTY(_hasParent PARENT_DIRECTORY)
IF(_hasParent)
	SET(OVITO_VERSION_MAJOR "${OVITO_VERSION_MAJOR}" PARENT_SCOPE)
	SET(OVITO_VERSION_MINOR "${OVITO_VERSION_MINOR}" PARENT_SCOPE)
	SET(OVITO_VERSION_REVISION "${OVITO_VERSION_REVISION}" PARENT_SCOPE)
	SET(OVITO_VERSION_STRING "${OVITO_VERSION_STRING}" PARENT_SCOPE)
	SET(OVITO_APPLICATION_NAME "${OVITO_APPLICATION_NAME}" PARENT_SCOPE)
	SET(OVITO_COPYRIGHT_NOTICE "${OVITO_COPYRIGHT_NOTICE}" PARENT_SCOPE)
ENDIF()