.. _new_features:

==========
What's new
==========

-------------------------
Version 3.8.2 (04-Apr-23)
-------------------------

* Implemented fast access to trajectory frames in compressed (gzipped) files
* Fix: Segfault when using zoom function in viewport with an attached camera object
* Fix: Segfault in :ref:`particles.modifiers.coordination_polyhedra` modifier on Linux
* Fix: Function 'load/save session state' does not follow global working directory

-------------------------
Version 3.8.1 (27-Mar-23)
-------------------------

.. rubric:: Identification of volumetric regions using the Gaussian density method |ovito-pro|

The :ref:`particles.modifiers.construct_surface_mesh` modifier's implementation of the :ref:`Gaussian density method <particles.modifiers.construct_surface_mesh.gaussian_density_method>`
has been extended to support the :ref:`identification of volumetric regions <particles.modifiers.construct_surface_mesh.regions>`, e.g. pores, cavities, and filled spatial regions.
Their respective surface areas and volumes are calculated and output by the modifier in tabulated form.

To make this possible, we have developed an extension to the `Marching Cubes algorithm <https://en.wikipedia.org/wiki/Marching_cubes>`__ for isosurface construction, which provides
the capability to identify disconnected spatial regions separated by the surface mesh and compute their enclosed volumes -- of course with full support for periodic boundary conditions.

.. image:: /images/new_features/surface_mesh_regions_gaussian_density_example.png
  :width: 25%

.. image:: /images/modifiers/construct_surface_mesh_regions_example_table.jpg
  :width: 50%

.. rubric:: New efficient Python method for computing neighbor lists |ovito-pro|

OVITO's Python interface now offers the new :py:meth:`CutoffNeighborFinder.find_all() <ovito.data.CutoffNeighborFinder.find_all>` method
for vectorized computation of neighbor lists for many or all particles at once.

.. rubric:: Further changes:

* LAMMPS data file reader: Accept '#' in type names, which are referenced in data sections of the file

-------------------------
Version 3.8.0 (03-Mar-23)
-------------------------

.. rubric:: Develop custom modifiers with extended capabilities |ovito-pro|

A newly devised programming interface enables you to write advanced modifier functions in Python that

  * access **more than one** frame of a simulation trajectory,
  * perform computations that involve data **from several input files**, or
  * need control over the **caching** of computational results.

Take simulation post-processing to the next level! Develop your own trajectory analysis algorithms
in Python, which are fully integrated into OVITO's pipeline system and the interactive interface of OVITO Pro.

.. code-block:: python

    class CalculateIncrementalDisplacementsModifier(ModifierInterface):
        def modify(self, data, frame, input_slots, **kwargs):
            next_frame = input_slots['upstream'].compute(frame + 1)
            displacements = next_frame.particles.positions - data.particles.positions

Have a look at our completely revised :ref:`introduction to user-defined modifiers <writing_custom_modifiers>`
and check out the new :ref:`advanced programming interface for user-defined modifiers <writing_custom_modifiers.advanced_interface>`.

.. rubric:: Improved color legends

OVITO can now render tick marks in :ref:`color mapping legends <viewport_layers.color_legend>` to label intermediate values.
Furthermore, the legend's title may be rotated by 90 degrees:

.. image:: /images/new_features/color_legend_ticks_horizontal.png
  :width: 34%

.. image:: /images/new_features/color_legend_ticks_vertical.png
  :width: 12%

.. rubric:: File reader for ASE database files |ovito-pro|

Load atomic structures from database files of the `Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/>`__ into OVITO.
The :ref:`new file reader <file_formats.input.ase_database>` lets you scroll through all structures in a database or pick specific structures
using a query string. Metadata associated with structures is made available in OVITO as :ref:`global attributes <usage.global_attributes>`.

.. image:: /images/new_features/ase_database_reader.gif
  :width: 60%

.. rubric:: New modifier: :ref:`modifiers.identify_fcc_planar_faults` |ovito-pro|

Easily identify different planar defect types, such as **stacking faults** and **coherent twin boundaries**, in face-centered cubic (fcc) crystals.
We have developed a powerful classification algorithm for hcp-like atoms that make up such planar defects:

.. image:: /images/new_features/planar_faults.jpg
  :width: 30%

.. rubric:: New modifier: :ref:`modifiers.render_lammps_regions` |ovito-pro|

Use this new tool to generate mesh-based representations of the parametric regions defined in your `LAMMPS <https://docs.lammps.org/>`__ simulation,
e.g., cylinders, spheres, or blocks, and visualize the boundaries of these spatial regions along with the particle model:

.. image:: /images/new_features/lammps_regions.png
  :width: 60%

.. rubric:: Spatial binning modifier: New unity input option |ovito-pro|

This options offers a shortcut for calculating particle density distributions, i.e. counting the particles per grid cell.
Previous versions required first defining an auxiliary particle property with a uniform value of 1 to calculate the number density:

.. image:: /images/new_features/spatial_binning.png
  :width: 60%

See :ref:`particles.modifiers.bin_and_reduce` modifier.

.. rubric:: Support for LAMMPS dump grid files

OVITO can now read and visualize the `new volumetric grid file format written by recent LAMMPS versions <https://docs.lammps.org/Howto_grid.html>`__
thanks to the newly added :ref:`file_formats.input.lammps_dump_grid`:

.. image:: /images/new_features/volumetric_grid_discrete.png
  :width: 25%

.. rubric:: Slice modifier on voxel grids

When you apply the :ref:`Slice <particles.modifiers.slice>` modifier to a voxel grid,
cell values now get copied to the mesh faces and interpolated field values to the mesh vertices of the generated cross-section.
This enables both discrete and interpolated visualizations of the field values along arbitrary planar cross-sections:

.. image:: /images/new_features/volumetric_grid_slice_discrete.png
  :width: 25%

.. image:: /images/new_features/volumetric_grid_slice_interpolated.png
  :width: 25%

See :ref:`particles.modifiers.slice` modifier and :ref:`scene_objects.voxel_grid`.

.. rubric:: Support for point-based volumetric grids

In addition to the classical *cell-based* voxel grids, OVITO now also supports *point-based* volumetric grids,
in which field values are associated with the grid points instead of the voxel cells. All functions in OVITO
that operate on grids, e.g. the :ref:`particles.modifiers.create_isosurface` modifier, also support periodic and
mixed boundary conditions.

.. image:: /images/io/voxel_grid_types.png
  :width: 30%

See :py:attr:`ovito.data.VoxelGrid.grid_type` and :ref:`file_formats.input.cube`.

.. rubric:: Load Trajectory modifier now supports removal of particles

Previously, the :ref:`particles.modifiers.load_trajectory` modifier required the trajectory file to contain coordinates for all particles
that were initially present in the topology dataset. The improved version of the modifier can now deal with particles disappearing in later frames of a trajectory, e.g.,
when particles get removed from the simulation over time.

.. rubric:: Further additions and changes in this program release:

* Added dark mode UI support for Linux platform.
* :ref:`particles.modifiers.correlation_function` modifier: Added support for 2d simulations.
* :ref:`particles.modifiers.wrap_at_periodic_boundaries` modifier: Added support for 2d simulations.
* Save and restore maximized state of main window across program sessions.
* :ref:`file_formats.input.lammps_data` & writer: Added support for extended *Velocities* file section for when using LAMMPS atom styles *electron*, *ellipsoid*, or *sphere*.
* LAMMPS data file writer: Added the option to renumber all particle/bond/angle/dihedral/improper types during export. Avoids conversion problems from 0-based type IDs loaded from GSD files.
* New option to clip surfaces at open box boundaries (see :py:attr:`SurfaceMeshVis.clip_at_domain_boundaries <ovito.vis.SurfaceMeshVis.clip_at_domain_boundaries>`).
* :ref:`particles.modifiers.cluster_analysis` modifier: Abort calculation of center of mass and radius of gyration if masses of all input particles are zero.
* |ovito-pro| Added user option that makes OVITO Pro import multiple files of the same kind as separate objects into the scene.
* |ovito-pro| Accept ``os.PathLike`` objects in Python functions :py:func:`~ovito.io.import_file` and :py:func:`~ovito.io.export_file`.
* |ovito-pro| :py:meth:`PropertyContainer.create_property <ovito.data.PropertyContainer.create_property>`: Accept ``data`` values that are broadcastable to shape of property array.

-----------------
Previous versions
-----------------

For a list of changes in previous version of OVITO, go to https://www.ovito.org/about/version-history/.