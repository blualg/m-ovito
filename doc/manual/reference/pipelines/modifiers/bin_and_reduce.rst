.. _particles.modifiers.bin_and_reduce:

Spatial binning |ovito-pro|
---------------------------

.. image:: /images/modifiers/bin_and_reduce_panel.png
  :width: 30%
  :align: right

.. image:: /images/visual_elements/voxel_grid_example.png
  :width: 30%
  :align: right

This modifier produces a 1-, 2-, or 3-dimensional grid covering the entire simulation domain with uniformly shaped bins.
Thus, the simulation cell is subdivided into equally sized bins along one, two, or all three cell vectors.
The bins are always aligned with the edges of the (possibly sheared) simulation cell.

You can use this modifier to calculate a coarse-grained field representation of either the discrete :ref:`particles <scene_objects.particles>`
in a simulation or the discrete :ref:`dislocation lines <scene_objects.dislocations>` previously computed by
the :ref:`particles.modifiers.dislocation_analysis` modifier.

:ref:`Particles <scene_objects.particles>`
  The modifier assigns each particle into one of the uniformly sized bins. It then performs a
  reduction operation within each bin for a selected particle property (can be a scalar or vector property).
  This modifier thus allows you to project the particle-based information to a :ref:`structured grid <scene_objects.voxel_grid>`
  and produce a coarse-grained, continuous field representation of some particle property.
  You can choose between different reduction operations, e.g. sum, average (mean), minimum or maximum, to be applied per grid cell.

  .. image:: /images/modifiers/spatial_binning_example_particles.png
    :width: 50%

:ref:`Dislocations <scene_objects.dislocations>`
  The modifier can calculate the dislocation density in each grid cell. The local dislocation density is expressed in
  units of 1/length\ :sup:`2`. When projecting to a 1- or 2-dimensional binning grid,
  the dislocation density is still calculated from the 3-dimensional cell volume and dislocation lines.

  The local dislocation density in each grid cell is calculated using the method described in section 2 of this paper:

    | `N. Bertin, Connecting discrete and continuum dislocation mechanics: A non-singular spectral framework <https://doi.org/10.1016/j.ijplas.2018.12.006>`__
    | `Int. J. Plast. 122, (2019) [doi:10.1016/j.ijplas.2018.12.006] <https://doi.org/10.1016/j.ijplas.2018.12.006>`__

  A preprint is available `here <https://arxiv.org/abs/1804.00803>`__.

  .. image:: /images/modifiers/spatial_binning_example_dislocations.png
    :width: 50%

  .. versionadded:: 3.11.0

Data output options
"""""""""""""""""""

When mapping values to a one-dimensional bin grid using this modifier, you can subsequently
access the computed data table in the :ref:`data inspector <data_inspector.data_tables>`.
From here you can export the bin values to a text file.

When mapping the values to a three-dimensional :ref:`voxel grid <scene_objects.voxel_grid>` using this modifier, you can subsequently
employ the :ref:`particles.modifiers.create_isosurface` modifier to render isosurfaces of the computed field. Alternatively,
:ref:`particles.modifiers.slice` modifier can be used to visualize different slices through the voxel grid.

When creating two- or three-dimensional grids, you can also
export the computed grid data to a text file using OVITO's :ref:`file export <usage.export>` function.
Select `VTK Voxel Grid` as output format.

Parameters
""""""""""

Operate on
  Selects whether the modifier should use :ref:`particles <scene_objects.particles>` or :ref:`dislocations lines <scene_objects.dislocations>` as input.

Input property (only for :ref:`particles <scene_objects.particles>`)
  The source particle property the reduction operation should be applied to.
  Select *<None>* to take uniform 1 as input value for all particles, which can be useful for
  counting the number of particles in each bin (reduction operation: *sum*) or calculating
  the number of density of particles (reduction operation: *sum divided by bin volume*).

Use only selected elements (only for :ref:`particles <scene_objects.particles>`)
  Restricts the calculation to the subset of particles that are currently selected.

Binning direction(s)
  This option selects the axes of the simulation cell along which the bins are created. It determines the dimensionality of the generated grid.

Number of bins
  Number of bins in each of the active binning directions.

Reduction operation (only for :ref:`particles <scene_objects.particles>`)
  The type of reduction operation to be carried out. Available are *sum*, *mean*, *min*, and *max*.
  There is an additional option *sum divided by bin volume*, which sums over all particles of a bin and then divides the result
  by the volume of the bin. This operation is useful for computing pressure (or stress) within
  bins from the per-atom virial.

Compute first derivative
  Numerically calculates the first derivative of the binned data using a finite differences
  approximation. This works only for one-dimensional bin grids. (It is useful to e.g. compute the derivative
  of a flow velocity profile to obtain the local shear rate.)

Fix property axis range
  If selected, the plotted property range (or color scale for 2D grids) will be set to the values given
  in the :guilabel:`From` and :guilabel:`To` fields. Otherwise, the minimum and maximum data values will be used to automatically adjust the plotting range.

.. seealso::

  :py:class:`ovito.modifiers.SpatialBinningModifier` (Python API)