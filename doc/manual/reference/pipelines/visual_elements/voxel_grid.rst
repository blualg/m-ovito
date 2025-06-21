.. _visual_elements.voxel_grid:

Voxel grid
----------

.. image:: /images/visual_elements/voxel_grid_panel.jpg
  :width: 30%
  :align: right

This type of :ref:`visual element <visual_elements>` controls the visual appearance of
:ref:`voxel grid <scene_objects.voxel_grid>` data objects, which are structured grids made of
2- or 3-dimensional cells (voxels), each being associated with numeric property values.

.. figure:: /images/visual_elements/voxel_grid_example.png
  :figwidth: 32%

  Surface representation: Discrete cell values

.. figure:: /images/visual_elements/voxel_grid_example_interpolated.png
  :figwidth: 32%

  Surface representation: Color interpolation on

.. figure:: /images/visual_elements/voxel_grid_example_volume.png
  :figwidth: 32%

  Volume representation: Color and opacity transfer functions

The color mapping function of the visual element visualizes the selected scalar cell property
using a pseudo-color map. Alternatively, it's possible to control each voxel cell's RGB color explicitly
by setting the ``Color`` property of the :ref:`voxel grid <scene_objects.voxel_grid>` data object
using modifiers in the pipelines.

A three-dimensional voxel grid is rendered as a solid box, showing just the values of the grid cells located on the
outer boundaries of the domain. To visualize the interior values of the three-dimensional voxel grid, you can
use the :ref:`particles.modifiers.slice` modifier to extract a two-dimensional cross-section
or use the :ref:`particles.modifiers.create_isosurface` modifier to compute an isosurface
of the scalar field.

Parameters
""""""""""

Surface transparency
  The degree of semi-transparency of the grid surfaces.

Show grid lines
  Activates the rendering of wireframe lines along the edges of the grid cells.

Color interpolation
  Smoothly interpolate between the discrete colors of adjacent cells.

.. seealso::

  :py:class:`ovito.vis.VoxelGridVis` (Python API)