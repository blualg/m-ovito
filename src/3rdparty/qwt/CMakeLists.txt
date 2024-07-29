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

# Find the required Qt modules.
FIND_PACKAGE(Qt6 ${OVITO_MINIMUM_REQUIRED_QT_VERSION} COMPONENTS Core Gui Widgets Concurrent REQUIRED)

# Build the library.
SET(QWT_SOURCE_FILES
    qwt_abstract_scale_draw.cpp
    qwt_abstract_scale.cpp
    qwt_bezier.cpp
    qwt_clipper.cpp
    qwt_color_map.cpp
    qwt_column_symbol.cpp
    qwt_date.cpp
    qwt_date_scale_draw.cpp
    qwt_date_scale_engine.cpp
    qwt_dyngrid_layout.cpp
    qwt_event_pattern.cpp
    qwt_graphic.cpp
    qwt_interval.cpp
    qwt_interval_symbol.cpp
    qwt_math.cpp
    qwt_magnifier.cpp
    qwt_null_paintdevice.cpp
    qwt_painter.cpp
    qwt_painter_command.cpp
    qwt_panner.cpp
    qwt_picker.cpp
    qwt_picker_machine.cpp
    qwt_pixel_matrix.cpp
    qwt_point_3d.cpp
    qwt_point_polar.cpp
    qwt_round_scale_draw.cpp
    qwt_scale_div.cpp
    qwt_scale_draw.cpp
    qwt_scale_map.cpp
    qwt_spline.cpp
    qwt_scale_engine.cpp
    qwt_symbol.cpp
    qwt_system_clock.cpp
    qwt_text_engine.cpp
    qwt_text_label.cpp
    qwt_text.cpp
    qwt_transform.cpp
    qwt_widget_overlay.cpp
    qwt_vectorfield_symbol.cpp
    qwt_weeding_curve_fitter.cpp

    # QwtPlot files:
    qwt.cpp
    qwt_curve_fitter.cpp
    qwt_abstract_legend.cpp
    qwt_legend.cpp
    qwt_legend_data.cpp
    qwt_legend_label.cpp
    qwt_plot.cpp
    qwt_plot_renderer.cpp
    qwt_plot_abstract_canvas.cpp
    qwt_plot_axis.cpp
    qwt_plot_curve.cpp
    qwt_plot_dict.cpp
    qwt_plot_directpainter.cpp
    qwt_plot_grid.cpp
    qwt_plot_histogram.cpp
    qwt_plot_item.cpp
    qwt_plot_abstract_barchart.cpp
    qwt_plot_barchart.cpp
    qwt_plot_multi_barchart.cpp
    qwt_plot_intervalcurve.cpp
    qwt_plot_zoneitem.cpp
    qwt_plot_tradingcurve.cpp
    qwt_plot_spectrogram.cpp
    qwt_plot_spectrocurve.cpp
    qwt_plot_scaleitem.cpp
    qwt_plot_legenditem.cpp
    qwt_plot_seriesitem.cpp
    qwt_plot_shapeitem.cpp
    qwt_plot_marker.cpp
    qwt_plot_textlabel.cpp
    qwt_plot_layout.cpp
    qwt_plot_canvas.cpp
    qwt_plot_panner.cpp
    qwt_plot_rasteritem.cpp
    qwt_plot_picker.cpp
    qwt_plot_zoomer.cpp
    qwt_plot_magnifier.cpp
    qwt_plot_rescaler.cpp
    qwt_plot_graphicitem.cpp
    qwt_plot_vectorfield.cpp
    qwt_point_mapper.cpp
    qwt_raster_data.cpp
    qwt_matrix_raster_data.cpp
    qwt_sampling_thread.cpp
    qwt_series_data.cpp
    qwt_point_data.cpp
    qwt_scale_widget.cpp
    qwt_spline_basis.cpp
    qwt_spline_cubic.cpp
    qwt_spline_curve_fitter.cpp
    qwt_spline_local.cpp
    qwt_spline_parametrization.cpp
    qwt_spline_pleasing.cpp
)

# Build the library.
ADD_LIBRARY(Qwt ${QWT_SOURCE_FILES})

# Give our library file a new name to not confuse it with any system versions of the library.
SET_TARGET_PROPERTIES(Qwt PROPERTIES OUTPUT_NAME "ovito_qwt")

# Link to Qt.
TARGET_LINK_LIBRARIES(Qwt PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent)

# Tell CMake to run Qt moc on source files added to the target.
SET_TARGET_PROPERTIES(Qwt PROPERTIES AUTOMOC ON)
# Tell CMake to run the Qt resource compiler on all .qrc files added to a target.
SET_TARGET_PROPERTIES(Qwt PROPERTIES AUTORCC ON)

# Some compile time flags for Qwt.
TARGET_COMPILE_DEFINITIONS(Qwt PRIVATE QWT_NO_SVG QWT_NO_OPENGL QT_NO_PRINTER)
IF(BUILD_SHARED_LIBS)
    TARGET_COMPILE_DEFINITIONS(Qwt PRIVATE QWT_MAKEDLL)
    TARGET_COMPILE_DEFINITIONS(Qwt PUBLIC QWT_DLL)
ENDIF()

# Add standard compile and link options to the CMake target.
OVITO_ADD_STANDARD_COMPILE_OPTIONS(Qwt)

# Make header files of this library available to dependent targets.
TARGET_INCLUDE_DIRECTORIES(Qwt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Export this target.
INSTALL(TARGETS Qwt EXPORT OVITO
    RUNTIME DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    LIBRARY DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}"
    ARCHIVE DESTINATION "${OVITO_RELATIVE_LIBRARY_DIRECTORY}" COMPONENT "development")
