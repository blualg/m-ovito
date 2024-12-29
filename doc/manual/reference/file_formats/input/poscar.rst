.. _file_formats.input.poscar:

POSCAR / XDATCAR / CHGCAR file reader
-------------------------------------

.. figure:: /images/io/poscar_reader.*
  :figwidth: 30%
  :align: right

  User interface of the POSCAR file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file reader parses various file format variants written by the `VASP <https://www.vasp.at/>`__ simulation package.

OVITO can import atomic structures from `POSCAR` files, simulation trajectories from `XDATCAR` files,
and charge density fields from `CHGCAR` files. The file reader supports both the standard and the
direct format of `POSCAR` files.

XDATCAR files
"""""""""""""

When you import a `XDATCAR` file, OVITO will display the trajectory as a sequence of frames in the animation timeline.
Important: This only works if the name of the file contains the string "XDATCAR". Otherwise, the file is treated as a single-frame `POSCAR` file.

CHGCAR files
""""""""""""

When you import a `CHGCAR` file, the charge density field is imported as a :ref:`voxel grid <scene_objects.voxel_grid>`
in addition to the :ref:`scene_objects.particles` structure, and OVITO will automatically insert the :ref:`particles.modifiers.create_isosurface` modifier
into the data pipeline to visualize the field as an isosurface. CHGCAR files are detected automatically
based on their contents.

For spin-polarized calculations, OVITO imports the total charge density (spin up + spin down) and the magnetization density (spin up - spin down)
as separate field properties of the same voxel grid.

For noncollinear calculations, OVITO reads the total charge density and the magnetization density. The latter gets stored
as a vectorial voxel field property with three components.

In all cases the field values loaded from the file get divided by the simulation cell volume during import.

.. _file_formats.input.poscar.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, centering = False, generate_bonds = False)
  :noindex:

  :param centering: If ``True``, the simulation cell and atomic coordinates are translated to center the box at the coordinate origin.
                    If ``False``, one corner of the simulation cell remains fixed at the coordinate origin.
  :type centering: bool

  :param generate_bonds: Activates the generation of ad-hoc bonds connecting the atoms loaded from the file.
                         Ad-hoc bond generation is based on the van der Waals radii of the chemical elements.
                         Alternatively, you can apply the :py:class:`~ovito.modifiers.CreateBondsModifier` to the
                         system after import, which provides more control over the generation of pair-wise bonds.
  :type generate_bonds: bool

For `CHGCAR` files, field values are imported into a :py:class:`~ovito.data.VoxelGrid` object with identifier ``charge-density``
having one or more field properties:

  - ``Charge Density`` (total charge density)
  - ``Magnetization Density``
     - For spin-polarized calculations: Scalar magnetization density (spin up - spin down)
     - For noncollinear calculations: Magnetization density vector with three components ``X``, ``Y``, and ``Z``
