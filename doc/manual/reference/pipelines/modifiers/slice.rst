.. _particles.modifiers.slice:

Slice
-----

.. image:: /images/modifiers/slice_panel.png
  :width: 30%
  :align: right

This modifier deletes (or selects) all elements on one side of an infinite cutting plane.
Alternatively, the modifier can cut out a slab of a given thickness from the structure:

.. figure:: /images/modifiers/slice_example_input.png
  :figwidth: 20%

  Input

.. figure:: /images/modifiers/slice_example_output1.png
  :figwidth: 20%

  Output (slab width = 0)

.. figure:: /images/modifiers/slice_example_output2.png
  :figwidth: 20%

  Output (slab width > 0)

When applied to a :ref:`voxel grid <scene_objects.voxel_grid>`, the slice modifier allows you to extract a
planar cross-section from the volumetric field data:

.. image:: /images/visual_elements/voxel_grid_example_interpolated.png
  :width: 30%

.. image:: /images/visual_elements/voxel_grid_example_crosssection.png
  :width: 30%

Parameters
""""""""""

Cartesian coordinates / Miller indices |ovito-pro|
  Selects whether the `Distance` and `Normal` parameters are specified in terms of
  the global Cartesian coordinate system or in `reciprocal cell space <https://en.wikipedia.org/wiki/Miller_index>`__.

  Note: Miller indices are specified in terms of the periodic lattice established by the three simulation cell vectors only,
  not the physical lattice possibly formed by atoms/particles within the simulation cell, which may be different.

Distance
  The (signed) distance of the cutting plane from the origin measured parallel to the plane normal.
  In `Cartesian` mode, the origin is the point (0,0,0) of the global simulation coordinate system
  and the distance is specified in simulation units of length. In `Miller index` mode, the
  plane's distance is measured from the origin of the simulation cell and specified in terms of the interplanar spacing :math:`d_{\mathrm{hkl}}`,
  which depends on the entered Miller indices.

Normal
  The three components of the plane's normal vector, which defines the orientation of the plane.
  This vector does not have to be a unit vector. Note that you can click on the blue labels
  next to each input field to reset the vector to point along the corresponding axis.
  In `Cartesian` mode, the normal vector is specified in Cartesian coordinates of the global simulation coordinate system.
  In `Miller index` mode, the normal vector must be specified `in terms of the reciprocal lattice vectors <https://en.wikipedia.org/wiki/Miller_index>`__ (inverse
  simulation cell matrix).

Slab width
  Specifies the width of the slab to cut out from the input structure.
  If this value is zero (the default), everything on one side of the
  cutting plane is deleted. If `slab width` is set to a positive value (measured in simulation units of length),
  a slice of the given thickness is cut out.

Reverse orientation
  Effectively flips the cutting plane's orientation. If the `slab width`
  parameter is zero, activating this option will remove all elements on the opposite side
  of the plane. Otherwise this option will let the modifier cut away a slab of
  the given thickness from the input structure.

Create selection (do not delete)
  This option lets the modifier select elements instead of deleting them.

Apply to selection only
  Restricts the effect of the modifier to the subset of elements that are currently selected.

Visualize plane
  Lets the modifier generate polygonal geometry to visualize the plane in rendered images.
  Otherwise the plane is only indicated in the interactive viewports of OVITO.

Operate on
  The field :guilabel:`Operate on` selects the type of data elements that will be sliced by the modifier:

  .. table::
    :widths: auto

    =================== ============================================================================================================
    Operate on          Description
    =================== ============================================================================================================
    Particles           Slices :ref:`particles <scene_objects.particles>` and deletes dangling :ref:`bonds <scene_objects.bonds>`
    Voxel grids         Slices :ref:`voxel grids <scene_objects.voxel_grid>`
    Surfaces            Slices :ref:`surface meshs <scene_objects.surface_mesh>`
    Dislocation lines   Slices :ref:`dislocation lines <scene_objects.dislocations>`
    Lines               Slices :ref:`lines <scene_objects.lines>`
    =================== ============================================================================================================

Alignment functions
"""""""""""""""""""

These functions reposition or align the cutting plane:

  * :guilabel:`Center in simulation cell` moves the plane to the center of the simulation cell by shifting it parallel to the plane's normal vector.

  * :guilabel:`Align view to plane` rotates the viewport camera to make it look perpendicular onto the cutting plane.

  * :guilabel:`Align plane to view` rotates the plane such that its normal vector becomes parallel with the camera viewing direction of the active viewport.

  * :guilabel:`Pick three points` lets you pick three spatial points with the mouse in the viewports. The cutting plane will be repositioned such that it goes through all three points.

Animating the plane
"""""""""""""""""""

The position of the cutting plane can be animated. Use the :guilabel:`A` button
next to each numerical parameter field to open the corresponding key-frame animation dialog.
See the :ref:`animation section <usage.animation>` of this manual for more information on this topic.

.. seealso::

  :py:class:`ovito.modifiers.SliceModifier` (Python API)
