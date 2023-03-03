.. _new_features:

====================
New in version 3.8
====================

.. rubric:: New advanced programming interface: for user-defined Python modifiers |ovito-pro| 

Upon popular request: The newly revised and improved Python programming interface now enables development of 
custom modifier functions that 

* can access the data of **more than one** trajectory frame in the same computation,
* perform computations that involve data **from several input files**, or
* need control over the **caching** of computational results.

Take your custom data analysis to the next level! 

.. code-block:: python

    from ovito.data import DataCollection
    from ovito.pipeline import ModifierInterface, FileSource
    from ovito.traits import OvitoObjectTrait
    from ovito.vis import VectorVis
    from traits.api import Int, Bool

    class CalculateDisplacementsWithReference(ModifierInterface):

        # Give the modifier a second input slot for reading the reference config from a separate file:
        reference = OvitoObjectTrait(FileSource)

        # The trajectory frame from the reference file to use as (static) reference configuration (default 0).
        reference_frame = Int(default_value=0, label='Reference trajectory frame')

        # This flag controls whether the modifier tries to detect when particles have crossed a periodic boundary
        # of the simulation cell. The computed displacement vectors will be corrected accordingly.
        minimum_image_convention = Bool(default_value=True, label='Use minimum image convention')

        # A VectorVis visual element managed by this modifier, which will be assigned to the 'Displacement' output property to visualize the vectors.
        vector_vis = OvitoObjectTrait(VectorVis, alignment=VectorVis.Alignment.Head, flat_shading=False, title='Displacements')

        # Tell the pipeline system to keep two trajectory frames in memory: the current input frame and the reference configuration.
        def input_caching_hints(self, frame: int, **kwargs):
            return {
                'upstream': frame,
                'reference': self.reference_frame
            }

        # The actual function called by the pipeline system to let the modifier do its thing.
        def modify(self, data: DataCollection, *, input_slots: dict[str, ModifierInterface.InputSlot], **kwargs):
            ...


:ref:`See also... <writing_custom_modifiers.advanced_interface>`

.. rubric:: File reader for ASE database files |ovito-pro| 

Load all atomic structures from a database file of the Atomic Simulation Environment (ASE) into OVITO to 
scroll through the database or filter specific structures. 

.. image:: /images/new_features/ase_database_reader.gif
  :width: 60%

:ref:`See also... <file_formats.input.ase_database>`

.. rubric:: New modifier: Identify FCC planar faults |ovito-pro| 

Easily identify different planar defect types (like **stacking faults** and **coherent twin boundaries**) in face-centered cubic (fcc) crystals.

.. image:: /images/new_features/planar_faults.jpg
  :width: 30%

:ref:`See also ... <modifiers.identify_fcc_planar_faults>`

.. rubric:: New modifier: Render LAMMPS regions |ovito-pro| 

Generate explicit mesh-based representations of the parametric regions used in the `LAMMPS <https://docs.lammps.org/>`__ simulation code, 
e.g., cylinders, spheres, cones, planes, and blocks and visualize them along with your input data.

.. image:: /images/new_features/lammps_regions.png
  :width: 60%

:ref:`See also ... <modifiers.render_lammps_regions>`

.. rubric:: Spatial binning modifier: Added unity input option |ovito-pro| 

This options offers a shortcut to calculating the particle number density. 

.. image:: /images/new_features/spatial_binning.png
  :width: 60%

:ref:`See also ... <particles.modifiers.bin_and_reduce>`

.. rubric:: Slice modifier - Voxel grids

Voxel grid properties are now copied to faces of cross-section mesh (and interpolated property values to mesh vertices).

.. image:: /images/visual_elements/voxel_grid_example.png
  :width: 25%

.. image:: /images/visual_elements/voxel_grid_example_crosssection.png
  :width: 25%
  
:ref:`See also ... <particles.modifiers.slice>`

.. rubric:: Support point-based volumetric grid data 

In addition to the classical cell-based voxel grids, OVITO now also supports point-based volumetric grids.

.. image:: /images/io/voxel_grid_types.png
  :width: 30%

:py:attr:`See also ... <ovito.data.VoxelGrid.grid_type>`

.. rubric:: Load trajectory modifier: Support removal of particles during the course of a simulation trajectory.

Previously the Load Trajectory modifier required the trajectory file to contain all positions of all particles present
in the original topology dataset. Its improved version can now deal with particles missing in later animation frames.

:ref:`See also ... <particles.modifiers.load_trajectory>`


.. rubric:: Added file reader for the new LAMMPS dump grid file format. 

:ref:`See also ... <file_formats.input.lammps_dump_grid>`


.. rubric:: And more ...

* Dark mode UI support on Linux platform.
* Spatial correlation function modifier: Added support for 2d simulation cells.
* Wrap at periodic boundaries modifier: Added support for 2d simulation cells.
* Save and restore maximized state of main window across program sessions.
* Accept ``*.ovito`` state files in data import function.
* LAMMPS data reader & writer: Support extended Velocities section for atom styles electron, ellipsoid, and sphere.
* LAMMPS data writer: Added the option to renumber all ``particle/bond/angle/dihedral/improper`` types during export. Avoids problems with 0-based type IDs loaded from GSD files.
* New option to clip surfaces meshes at open simulation box boundaries (see :py:attr:`SurfaceMeshVis.clip_at_domain_boundaries <ovito.vis.SurfaceMeshVis.clip_at_domain_boundaries>`).
* Cluster analysis modifier: Stop calculation of center of mass and radius of gyration if all masses of particles are zero. 
* |ovito-pro| Added option to import multiple files of the same kind as separate scene objects in one step 
* |ovito-pro| Accept ``os.PathLike`` objects passed to :py:func:`~ovito.io.import_file` and :py:func:`~ovito.io.export_file` functions.
* |ovito-pro| :py:meth:`PropertyContainer.create_property <ovito.data.PropertyContainer.create_property>`: Accept any ``data`` value that is broadcastable to shape of property array. 

