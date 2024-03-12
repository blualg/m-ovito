.. _particles.modifiers.create_isosurface:

Create isosurface
-----------------

.. image:: /images/modifiers/create_isosurface_panel.png
  :width: 30%
  :align: right

.. figure:: /images/modifiers/create_isosurface_example.png
  :figwidth: 30%
  :align: right

  Two isosurfaces of the charge density field

This modifier generates an `isosurface <https://en.wikipedia.org/wiki/Isosurface>`__ for a field quantity defined on a structured
:ref:`voxel grid <scene_objects.voxel_grid>`.
The computed isosurface is a :ref:`surface mesh <scene_objects.surface_mesh>` data object and
its visual appearance is controlled by the accompanying :ref:`surface mesh <visual_elements.surface_mesh>` visual element.

To create multiple isosurfaces at different iso-levels, insert several instances of the the modifier into the same pipeline.

See the :ref:`list of supported input file formats <file_formats.input>` to find out which ways of importing
voxel grid data into the program are available. OVITO Pro also offers the :ref:`particles.modifiers.bin_and_reduce` modifier,
which lets you dynamically generate a grid by binning a system of particles.

**Transfer field values**

The option :guilabel:`Transfer field values to surface` copies all field quantities defined on the input voxel grid over to the isosurface's mesh vertices.
This includes any secondary field quantities in addition to the selected primary field quantity for which the isosurface is being constructed (which is constant and equal to the iso-level across the
entire surface). The color mapping mode of the :ref:`visual_elements.surface_mesh` visual element lets you locally color the isosurface based on some secondary field quantity.
The local field values at each isosurface output vertex are computed from the data of the input voxel cells using trilinear interpolation.

**Surface smoothing**

The generated isosurface can optionally be smoothed using a mesh fairing algorithm to even out surface steps resulting from the discrete nature of the input voxel grid.
The :guilabel:`Smoothing level` parameter specifies the number of iterations of the smoothing algorithm to apply. A value of 0 disables smoothing, which is the default setting.
The post-processing procedure slightly displaces the surface mesh vertices to reduce steps and roughness of the isosurface.
Mesh smoothing is performed *after* interpolated field values have been transferred to the surface, which means that the surface property values reflect the
original vertex positions prior to smoothing.

**Identify volumetric regions** |ovito-pro|

This option lets the modifier identify individual volumetric regions (below or above the iso-level) and compute their volumes and surface areas.
See :ref:`particles.modifiers.construct_surface_mesh.regions` for more details.

.. seealso::

  :py:class:`ovito.modifiers.CreateIsosurfaceModifier` (Python API)
