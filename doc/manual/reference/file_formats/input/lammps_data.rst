.. _file_formats.input.lammps_data:

LAMMPS data file reader
-----------------------

.. figure:: /images/io/lammps_data_reader.*
  :figwidth: 30%
  :align: right

  User interface of the LAMMPS data reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

For loading model structures used with the `LAMMPS <https://docs.lammps.org/>`__ simulation code.

.. _file_formats.input.lammps_data.variants:

Supported format variants
"""""""""""""""""""""""""

The file reader can load files conforming to the format specification of the LAMMPS `read_data command <https://docs.lammps.org/read_data.html#format-of-a-data-file>`__.
Such files may be produced by LAMMPS' own `write_data command <https://docs.lammps.org/write_data.html>`__ or external structure building tools.

OVITO's data file reader can directly parse gzipped files (".gz" suffix) and zstd compressed files (".zst" suffix).

The reader loads a complete :ref:`particles <scene_objects.particles>` model including topological information (:ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers)
if present.

OVITO maintains atoms in the order in which they are stored in the data file.
The unique ID associated with each atom is used to identify individual atoms in different trajectory frames even if the storage order changes
during the simulation.
If an ordered atoms list is desired, the data file reader provides a user option making it sort the atoms by ID during import
(``sort_particles=True`` keyword parameter in Python, see below).

Data files written with the `triclinic/general` flag are supported.

.. _file_formats.input.lammps_data.atom_style:

LAMMPS atom style
"""""""""""""""""

In LAMMPS, the selected `atom style <https://docs.lammps.org/atom_style.html>`__ determines what attributes are associated with each atom.
Depending on this choice, a data file contains different data columns in the `Atoms` section of the file.
For OVITO to correctly parse a data file, it must first guess the `atom style <https://docs.lammps.org/atom_style.html>`__ that
is used in the simulation.

A typical practice is to make it easier for an application like OVITO to parse a data file by including the atom style
as a comment in the `Atoms` section line::

  Atoms   # full

  1 2 4 -0.159 49.37632 62.94025 48.46742
  2 2 5 0.053 49.75864 63.35641 49.41517
  ...

This tells the file reader to assume the ``full`` atom style. Subsequently, it will parse the 7 columns (``atom-ID molecule-ID atom-type q x y z``)
that are mandated by the ``full`` style according to the specification.

If the file does *not* contain an atom style hint as shown above, then OVITO will ask you to specify the correct
atom style during file import by displaying the following dialog box:

.. image:: /images/io/lammps_data_reader_hybrid_style_selection.*
  :width: 50%

Here, you can also specify the list of sub-styles in case the special ``hybrid`` atom style is used by your simulation model.

.. _file_formats.input.lammps_data.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, atom_style = None, atom_substyles = None, sort_particles = False)
  :noindex:

  :param atom_style: Specifies the LAMMPS `atom style <https://docs.lammps.org/atom_style.html>`__ used in the data file. Required if the data file contains no style hint.
  :type atom_style: str
  :param atom_substyles: List of sub-styles. Required if the data file contains no style hint and the simulation model uses the ``hybrid`` atom style.
  :type atom_substyles: list[str]
  :param sort_particles: Makes the file reader reorder the loaded particles before passing them to the pipeline.
                         Sorting is based on the values of the ``Particle Identifier`` property loaded from the data file.
  :type sort_particles: bool
