.. _particles.modifiers.bin_and_reduce:

Spatial binning |ovito-pro|
---------------------------

.. image:: /images/modifiers/bin_and_reduce_panel.png
  :width: 30%
  :align: right

.. image:: /images/visual_elements/voxel_grid_example.png
  :width: 30%
  :align: right

This modifier generates a 1-, 2-, or 3-dimensional grid that covers the simulation domain with uniform bins.
This means that the simulation domain can be subdivided into equally sized bins along one, two or all three 
of its axes. The spatial bins are always aligned parallel to the simulation cell edges.

The binning grid is used to calculate coarse-grained information of either the :ref:`Particles <scene_objects.particles>` 
or :ref:`Dislocations <scene_objects.dislocations>` contained in the DataCollection.

:ref:`Particles <scene_objects.particles>` 
  In this mode the modifier assigns each particle into one of the uniformly sized bins. It then performs a 
  reduction operation for a selected particle property,
  mapping the values of all particles contained in a cell to a single output value.
  This modifier can thus be used to project the per-particle data to a :ref:`structured grid <scene_objects.voxel_grid>`, 
  for example to coarse-grain
  the atomistic data and generate a continuous field representation of a particle property.
  You can choose between different reduction operations, e.g. sum, average (mean), minimum or maximum.

:ref:`Dislocations <scene_objects.dislocations>`

  Here, the modifier 
  calculates the dislocation density for each bin. The dislocation density is expressed in units of 1/length\ :sup:`2`. When calculating a 1- or 
  2-dimensional binning grid, the number of bins in the other binning directions is set to `1`. Consequently, the dislocation density is 
  always relative to the total :ref:`simulation cell <scene_objects.simulation_cell>` volume.

  The dislocation density is calculated using the method described in section 2 of this publication:

    | `N. Bertin <https://doi.org/10.1016/j.ijplas.2018.12.006>`__
    | `Connecting discrete and continuum dislocation mechanics: A non-singular spectral framework <https://doi.org/10.1016/j.ijplas.2018.12.006>`__
    | `Int. J. Plast. 122, (2019) <https://doi.org/10.1016/j.ijplas.2018.12.006>`__
    | `doi:10.1016/j.ijplas.2018.12.006`
  
  Please cite this paper when you use this method in your work. A preprint is available `here <https://arxiv.org/abs/1804.00803>`__.

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
  :ref:`Data Object <scene_objects>` which is used as input for this modifier. Can be either :ref:`Particles <scene_objects.particles>` or :ref:`Dislocations <scene_objects.dislocations>`. 

Input property (Operate on = :ref:`Particles <scene_objects.particles>`)
  The source particle property the reduction operation should be applied to.
  Select *<None>* to take uniform 1 as input value for all particles, which can be useful for 
  counting the number of particles in each bin (reduction operation: *sum*) or calculating
  the number of density of particles (reduction operation: *sum divided by bin volume*).

Use only selected elements (Operate on = :ref:`Particles <scene_objects.particles>`)
  Restricts the calculation to the subset of particles that are currently selected.

Binning direction(s)
  This option selects the axes of the simulation cell along which the bins are created. It determines the dimensionality of the generated grid.

Number of bins
  Number of bins in each of the active binning directions.

Reduction operation (Operate on = :ref:`Particles <scene_objects.particles>`)
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