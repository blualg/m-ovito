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

# Define plugin module.
OVITO_STANDARD_PLUGIN(Gui
	SOURCES
		app/WasmApplication.cpp
		dataset/WasmDataSetContainer.cpp
		dataset/WasmFileManager.cpp
		mainwin/MainWindow.cpp
		mainwin/ViewportsPanel.cpp
		mainwin/MouseGrabWorkaround.cpp
		properties/ParameterUI.cpp
		properties/RefTargetListParameterUI.cpp
		properties/DataObjectReferenceParameterUI.cpp
		properties/ModifierDelegateParameterUI.cpp
		viewport/QuickViewportWindow.cpp
		resources/gui.qrc
	PLUGIN_DEPENDENCIES
		OpenGLRenderer
		GuiBase
	LIB_DEPENDENCIES
		Qt6::Qml 
		Qt6::Quick 
		Qt6::QuickControls2 
		Qt6::QuickTemplates2
		Qt6::LabsQmlModels
	PRECOMPILED_HEADERS 
		GUI.h
)

#LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS Qml QmlModels Quick QuickControls2 QuickTemplates2 Svg)
#LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS QuickControls2Impl QuickLayouts LabsQmlModels)
#SET(QT_MINIMUM_REQUIRED_VERSION 6.2)
# Additionally, when building for the desktop platform, we need the QtWidgets module.
#IF(NOT EMSCRIPTEN)
#	LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS Widgets)
#ENDIF()
#IF(NOT OVITO_DISABLE_THREADING)
#	LIST(APPEND OVITO_REQUIRED_QT_COMPONENTS QmlWorkerScript)
#ENDIF()

#QT6_ADD_QML_MODULE(
#	Gui
#	VERSION 1.0
#	URI "org.ovito"
#)
# Workaround for QTBUG-93443: Add directory containing QML class headers to include path.
#TARGET_INCLUDE_DIRECTORIES(Gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app")
#TARGET_INCLUDE_DIRECTORIES(Gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/properties")
#TARGET_INCLUDE_DIRECTORIES(Gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mainwin")
#TARGET_INCLUDE_DIRECTORIES(Gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/viewport")

IF(EMSCRIPTEN)
	FILE(GLOB _controls_basic_qml_files LIST_DIRECTORIES FALSE "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Basic/*.qml")
	FILE(GLOB _controls_universal_qml_files LIST_DIRECTORIES FALSE "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/*.qml")
	FILE(GLOB _controls_universal_impl_qml_files LIST_DIRECTORIES FALSE "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/impl/*.qml")
	IF(NOT OVITO_DISABLE_THREADING)
		SET(_packaged_qml_files_worker_script "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/WorkerScript/qmldir")
	ENDIF()
	QT_ADD_RESOURCES(Gui PackedQmlDirFiles
		PREFIX "/qt-project.org/imports"
		BASE "${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml"
		FILES 
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQml/Models/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/impl/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Basic/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Basic/impl/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Controls/Universal/impl/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Templates/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/QtQuick/Window/qmldir"
			"${QT_TOOLCHAIN_RELOCATABLE_INSTALL_PREFIX}/qml/Qt/labs/qmlmodels/qmldir"
			${_packaged_qml_files_worker_script}
			${_controls_basic_qml_files}
			${_controls_universal_qml_files}
			${_controls_universal_impl_qml_files}
		)

	FILE(GLOB _controls_basic_image_files LIST_DIRECTORIES FALSE "C:/Qt/6.2.0/Src/qtdeclarative/src/quickcontrols2/basic/images/*.png")
	QT_ADD_RESOURCES(Gui PackedQmlBasicImageFiles
		PREFIX "/qt-project.org/imports/QtQuick/Controls/Basic/"
		BASE "C:/Qt/6.2.0/Src/qtdeclarative/src/quickcontrols2/basic"
		FILES ${_controls_basic_image_files})
	
	FILE(GLOB _controls_universal_image_files LIST_DIRECTORIES FALSE "C:/Qt/6.2.0/Src/qtdeclarative/src/quickcontrols2/universal/images/*.png")
	QT_ADD_RESOURCES(Gui PackedQmlMaterialImageFiles
		PREFIX "/qt-project.org/imports/QtQuick/Controls/Universal/"
		BASE "C:/Qt/6.2.0/Src/qtdeclarative/src/quickcontrols2/universal"
		FILES ${_controls_universal_image_files})
ENDIF()

IF(NOT EMSCRIPTEN)
	TARGET_LINK_LIBRARIES(Gui PRIVATE Qt6::Widgets)
ENDIF()
