.. _visual_elements.voxel_grid:

Voxel grid
----------

.. image:: /images/visual_elements/voxel_grid_panel.jpg
  :width: 30%
  :align: right

This :ref:`visual element <visual_elements>` controls the visual appearance of
:ref:`voxel grid <scene_objects.voxel_grid>` objects, which are two- or three-dimensional structured grids made of
voxel cells that represent spatially varying scalar fields.

The visual element can either show a **boundary representation** of the grid, with or without interpolation
of the discrete grid values on the outer surface of the domain:

.. figure:: /images/visual_elements/voxel_grid_example_interpolated.png
  :figwidth: 32%

.. figure:: /images/visual_elements/voxel_grid_example.png
  :figwidth: 32%

Or it can visualize a **volume**, with varying opacity and color depending on the local field values:

.. figure:: /images/visual_elements/voxel_grid_example_volume_with_transfer.png
  :figwidth: 80%

.. attention::

  The volumetric representation is only supported by the :ref:`VisRTX <rendering.visrtx_renderer>` and
  :ref:`OSPRay <rendering.ospray_renderer>` rendering engines. If you are using the default OpenGL renderer,
  the volume will remain completely empty.

In both representation modes a **transfer function** is used to map the grid values to a local color
using a pseudo-color gradient. Alternatively, it's possible to specify each voxel cell's RGB color directly
by setting the ``Color`` property of the :ref:`voxel grid <scene_objects.voxel_grid>` data object
using a modifier in the pipeline (only in *boundary* representation mode).

.. tip::

  Other ways of visualizing the interior field values of the three-dimensional voxel grid are the
  :ref:`particles.modifiers.slice` modifier, which allows extracting a two-dimensional cross-section, and the
  :ref:`particles.modifiers.create_isosurface` modifier, which computes an isosurface of the scalar field.

Parameters
""""""""""

.. _visual_elements.voxel_grid.boundary_parameters:

Boundary representation
=======================

Grid lines
  Activates the display of wireframe lines along voxel cell edges.

Interpolation
  Smoothly interpolate between the colors of adjacent cells.

Transparency
  The degree of semi-transparency of the boundary surface.

Color mapping
  Here you select a **property** of the voxel grid to be used as input for the color mapping.
  In addition, you can select a **color gradient**, which maps the local property values to corresponding colors.
  The **start and end values** of the gradient can be adjusted to control the range of property values.

  To include the color map in rendered images, you can add a :ref:`viewport_layers.color_legend`
  to the viewport.

.. image:: /images/visual_elements/voxel_grid_panel_volume.jpg
  :width: 30%
  :align: right

.. _visual_elements.voxel_grid.volume_parameters:

Volume representation
=====================

Opacity function
  This function defines how local grid values are translated into local opacity values
  during volume rendering. You can click and drag the mouse in the graph to freely adjust
  the function.

  The :guilabel:`Reset` button restores the default opacity function, which is a linear ramp function.

.. seealso::

  :py:class:`ovito.vis.VoxelGridVis` (Python API)