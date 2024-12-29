.. _file_formats.input.cube:

Gaussian Cube file reader
-------------------------

.. figure:: /images/io/gaussian_cube_reader.*
  :figwidth: 30%
  :align: right

  User interface of the Gaussian Cube file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file format is used by the *Gaussian* simulation package and other ab initio simulation codes. It stores the :ref:`simulation cell geometry <scene_objects.simulation_cell>`,
the :ref:`atomic structure <scene_objects.particles>`, and :ref:`volumetric grid data <scene_objects.voxel_grid>`.

Specifications of the format can be found `here <https://h5cube-spec.readthedocs.io/en/latest/cubeformat.html>`__
and `here <http://paulbourke.net/dataformats/cube/>`__, for example.

If the imported file uses Bohr units, OVITO converts them to Angstroms automatically.

.. _file_formats.input.cube.grid_type:

Volumetric grid type
""""""""""""""""""""

The user interface of the file reader provides an option that controls how the volumetric data should be interpreted by OVITO.
The field values may either be attributed to the grid line intersections (the default) or the cell centers:

.. image:: /images/io/voxel_grid_types.png
  :width: 40%
  :align: center

The selected grid type affects operations subsequently performed in OVITO, e.g. :ref:`constructing an iso-surface <particles.modifiers.create_isosurface>` from
the volumetric data. In all cases, the file reader assumes 3d periodic boundary conditions for the volumetric grid and the atomic simulation cell.

.. seealso:: :py:attr:`VoxelGrid.grid_type <ovito.data.VoxelGrid.grid_type>`

.. _file_formats.input.cube.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, grid_type = VoxelGrid.GridType.PointData, generate_bonds = False)
  :noindex:

  :param grid_type: Selects how OVITO should interpret the volumetric data loaded from the Cube file.
                    See :py:attr:`VoxelGrid.grid_type <ovito.data.VoxelGrid.grid_type>` for further information.

  :param generate_bonds: Activates the generation of ad-hoc bonds connecting the atoms loaded from the file.
                         Ad-hoc bond generation is based on the van der Waals radii of the chemical elements.
                         Alternatively, you can apply the :py:class:`~ovito.modifiers.CreateBondsModifier` to the
                         system after import, which provides more control over the generation of pair-wise bonds.
  :type generate_bonds: bool
