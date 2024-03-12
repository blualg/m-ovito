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

This modifier computes an `isosurface <https://en.wikipedia.org/wiki/Isosurface>`__ for a field defined on a structured
:ref:`voxel grid <scene_objects.voxel_grid>`.
The resulting isosurface is a :ref:`surface mesh <scene_objects.surface_mesh>` object and
its visual appearance is controlled by the accompanying :ref:`surface mesh <visual_elements.surface_mesh>` visual element.

You can change the iso-level value by clicking into the histogram plot, which shows the distribution of field values.
To create multiple isosurfaces at different iso-levels, you can insert several instances of the modifier into the pipeline.

Voxel grids can be imported into OVITO from several :ref:`input file formats <file_formats.input>`.
OVITO Pro additionally offers the :ref:`particles.modifiers.bin_and_reduce` modifier, which lets you
map a particle model onto a voxel grid to obtain a coarse-grained field representation of the model.

**Transfer field values**

The modifier option :guilabel:`Transfer field values to surface` copies all field quantities defined on the input voxel grid over to the isosurface's mesh vertices.
You can then use the color mapping mode of the :ref:`visual_elements.surface_mesh` visual element to locally color the isosurface and visualize a secondary field quantity on the isosurface.
The field values at each isosurface vertex are computed from the input voxel data using trilinear interpolation.

.. _particles.modifiers.create_isosurface.smoothing:

**Surface smoothing**

The generated isosurface can optionally be smoothed using a mesh fairing algorithm to even out surface steps resulting from the discrete nature of the input voxel grid.
The :guilabel:`Smoothing level` parameter specifies the number of iterations of the smoothing algorithm to apply. A value of 0 disables smoothing, which is the default setting.
The post-processing procedure slightly displaces the surface mesh vertices to reduce steps and roughness of the isosurface.
Mesh smoothing is performed *after* interpolated field values have been transferred to the surface, which means that the surface property values reflect the
original vertex positions prior to smoothing.

.. _particles.modifiers.create_isosurface.regions:

**Identify volumetric regions** |ovito-pro|

This option lets the modifier identify the individual volumetric regions (below or above the iso-level), which are separated by the isosurface, and compute their respective volumes and surface areas.
See :ref:`particles.modifiers.construct_surface_mesh.regions` for more details on how this option works. This feature can be useful to analyze the topology of the isosurface and
to quantify the volume(s) enclosed by the surface.

.. seealso::

  :py:class:`ovito.modifiers.CreateIsosurfaceModifier` (Python API)
