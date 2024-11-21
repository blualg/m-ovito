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

# This CMake script compiles the user manual for OVITO
# by transforming the reStructured text files into HTML using Sphinx.

# Controls the generation of the user manual.
OPTION(OVITO_BUILD_DOCUMENTATION "Build the user manual" "OFF")
OPTION(OVITO_BUILD_DOCUMENTATION_STRICT "Treat warnings as errors when building the documentation" "ON")

IF(OVITO_BUILD_APP)
    # Create destination directories.
    FILE(MAKE_DIRECTORY "${OVITO_SHARE_DIRECTORY}/doc/manual")
    FILE(MAKE_DIRECTORY "${OVITO_SHARE_DIRECTORY}/doc/manual/html")

    # Locate a Python interpreter.
    # Note: Sphinx 4.x requires at least Python 3.6.
    FIND_PACKAGE(Python3 REQUIRED COMPONENTS Interpreter)

    # Tell Intersphinx where to find the scripting reference manual files when building the user manual.
    # Use the existing Sphinx output tree of the scripting reference manual for resolving cross-references if it exists
    # in the python/ sub-directory (only the case when building OVITO Pro).
    # Otherwise, fall back to using the online version of the scripting reference at https://ovito.org/docs/current/python/.
    # For further information, see the 'intersphinx_mapping' configuration variable in the file 'conf.py'.
    IF(OVITO_BUILD_PROFESSIONAL)
        SET(OVITO_PYDOC_INTERSPHINX_LOCATION "${OVITO_SHARE_DIRECTORY}/doc/manual/html/python/objects.inv")
    ENDIF()

    IF(OVITO_BUILD_DOCUMENTATION_STRICT) # Turn warnings into errors
        SET(BUILD_DOCUMENTATION_STRICT "-W")
    ELSE()
        SET(BUILD_DOCUMENTATION_STRICT "")
    ENDIF()

    # Build documentation using Sphinx.
    ADD_CUSTOM_TARGET(documentation
        # Create the destination directory for the generated HTML files:
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OVITO_SHARE_DIRECTORY}/doc/manual/html"
        # Run Sphinx command to generate HTML files:
        COMMAND "${CMAKE_COMMAND}" -E env "OVITO_PYDOC_INTERSPHINX_LOCATION=${OVITO_PYDOC_INTERSPHINX_LOCATION}"
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sphinx-build.py"  # Run the sphinx-build.py script through the system's Python interpreter
            "."                                           # Sphinx source directory
            "${OVITO_SHARE_DIRECTORY}/doc/manual/html/"   # Destination directory
            -a -E                                         # Always write all output files. Don’t use a saved environment.
            -n                                            # Run in nit-picky mode
            "${BUILD_DOCUMENTATION_STRICT}"               # Turn warnings into errors
            # Additional config settings passed to Sphinx, which are added to the options found in conf.py:
            -D "version=${OVITO_VERSION_STRING}"
            -D "release=${OVITO_VERSION_STRING}"
            -D "copyright=${OVITO_CURRENT_YEAR} OVITO GmbH, Germany"

        # Extract the link targets from the Intersphinx inventory file and write them to a text file.
        # The file will be used by the OVITO application to open the correct HTML page corresponding to
        # a help topic ID. See UserInterface::openHelpTopic() method for further information.
        COMMAND "${CMAKE_COMMAND}" -E env DYLD_FALLBACK_LIBRARY_PATH=/opt/local/lib "${Python3_EXECUTABLE}"
            -m sphinx.ext.intersphinx "${OVITO_SHARE_DIRECTORY}/doc/manual/html/objects.inv"
            > "${OVITO_SHARE_DIRECTORY}/doc/manual/html/objects.txt"

        WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/doc/manual/"
        COMMENT "Generating user manual")

    # Build a single-page version of the doc (to be used for LLM machine learning applications).
    ADD_CUSTOM_TARGET(documentation_llmexport
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OVITO_SHARE_DIRECTORY}/doc/manual/llm_export"
        COMMAND "${CMAKE_COMMAND}" -E env "OVITO_PYDOC_INTERSPHINX_LOCATION=${OVITO_PYDOC_INTERSPHINX_LOCATION}"
            "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/sphinx-build.py"  # Run the sphinx-build.py script through the system's Python interpreter
            "."                                           # Sphinx source directory
            "${OVITO_SHARE_DIRECTORY}/doc/manual/llm_export/"   # Destination directory
            -c llm_export
            -b singlehtml                                 # Build a single-page version of the documentation
            -a -E                                         # Always write all output files. Don’t use a saved environment.
            -n                                            # Run in nit-picky mode
            "${BUILD_DOCUMENTATION_STRICT}"               # Turn warnings into errors
            # Additional config settings passed to Sphinx, which are added to the options found in conf.py:
            -D "version=${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}"
            -D "release=${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}"
            -D "copyright=${OVITO_CURRENT_YEAR} OVITO GmbH, Germany"
        WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/doc/manual/"
        COMMENT "Generating user manual (single page)")

    # Are we building OVITO Pro?
    IF(TARGET scripting_documentation_prerun)
        # Build user manual after the scripting reference manual was built for the first time (pre-run),
        # because Intersphinx need its inventory database.
        ADD_DEPENDENCIES(documentation scripting_documentation_prerun)
        # Build the final version of the scripting reference after the user manual, to make
        # Intersphinx cross-references from the scripting reference to the user manual work.
        ADD_DEPENDENCIES(scripting_documentation documentation)
        # Make both manuals dependencies of the spell-checking target.
        ADD_DEPENDENCIES(documentation_spellcheck documentation scripting_documentation)
    ENDIF()

    IF(OVITO_BUILD_DOCUMENTATION)
        # Run the documentation target automatically as part of the build process.
        ADD_DEPENDENCIES(Ovito documentation)
    ENDIF()

    # Install the generated documentation files alongside with the application.
    INSTALL(DIRECTORY "${OVITO_SHARE_DIRECTORY}/doc/manual/html/" DESTINATION "${OVITO_RELATIVE_SHARE_DIRECTORY}/doc/manual/html/" PATTERN "*.pptx" EXCLUDE)
ENDIF()

# Find the Doxygen program.
#FIND_PACKAGE(Doxygen QUIET)

# Generate API documentation files.
#IF(DOXYGEN_FOUND)
#   ADD_CUSTOM_TARGET(apidocs
#                   COMMAND "${CMAKE_COMMAND}" -E env
#                   "OVITO_VERSION_STRING=${OVITO_VERSION_MAJOR}.${OVITO_VERSION_MINOR}.${OVITO_VERSION_REVISION}"
#                   "OVITO_INCLUDE_PATH=${Ovito_SOURCE_DIR}/src/"
#                   ${DOXYGEN_EXECUTABLE} Doxyfile
#                   WORKING_DIRECTORY "${Ovito_SOURCE_DIR}/doc/develop/"
#                   COMMENT "Generating C++ API documentation")
#ENDIF()
