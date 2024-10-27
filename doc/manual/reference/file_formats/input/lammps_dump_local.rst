.. _file_formats.input.lammps_dump_local:

LAMMPS dump local file reader
-----------------------------

.. figure:: /images/io/lammps_dump_local_reader.*
  :figwidth: 30%
  :align: right

  User interface of the LAMMPS dump local reader, when attached to a :ref:`particles.modifiers.load_trajectory` modifier.

For loading per-bond data from files written by the `dump command <https://docs.lammps.org/dump.html>`__ of the LAMMPS simulation code.

.. _file_formats.input.lammps_dump_local.variants:

Supported format variants
"""""""""""""""""""""""""

The reader specifically handles files written by the LAMMPS dump style ``local``.
Since such files only store per-bond information but no particle data, the file reader is typically used
in conjunction with a :ref:`particles.modifiers.load_trajectory` modifier to amend an already loaded
particle model with varying bonds, e.g. from a reactive MD simulation.

The reader can parse gzipped files (".gz" suffix) and zstd compressed files (".zst" suffix) directly. Binary files (".bin" suffix) are *not* supported.

.. _file_formats.input.lammps_dump_local.property_mapping:

Column-to-property mapping
""""""""""""""""""""""""""

The different data columns in a dump local file must be mapped to corresponding :ref:`bond properties <scene_objects.bonds>` within OVITO during file import.
Since OVITO cannot guess the right mapping automatically in most cases (because file columns have user-defined names),
you usually have to specify the correct mapping by hand in the following dialog displayed by the file reader:

.. image:: /images/io/lammps_dump_local_reader_mapping_dialog.*
  :width: 50%

File columns get automatically mapped to corresponding standard properties in OVITO if their names
match one of the predefined :ref:`standard bond properties <bond-types-list>` (case insensitive).
Spaces that are part of a standard property name must be left out, because LAMMPS dump files do not support column names containing spaces. For example,
a dump file column named ``BondType`` will be mapped to the standard property :guilabel:`Bond Type`.

For standard properties with multiple components, a component name must be appended with a dot. For example, a dump file column
named ``ParticleIdentifiers.A`` will automatically be mapped to the second component of the :guilabel:`Particle Identifiers` :ref:`standard bond property <bond-types-list>`
in OVITO. Note that you can use the LAMMPS `dump_modify colname` command to give the columns in your dump file specific names.

For further information on how to set up the bond property mapping correctly, see the :ref:`particles.modifiers.load_trajectory` modifier.

.. _file_formats.input.lammps_dump_local.python:

Python parameters
"""""""""""""""""

The file reader accepts the following keyword parameters in a call to the :py:meth:`~ovito.pipeline.FileSource.load` Python function:

.. py:function:: load(location, columns = None)
  :noindex:

  :param columns: A list of OVITO :ref:`bond property <bond-types-list>` names, one for each data column in the dump local file.
                  List entries may be set to ``None`` to skip individual file columns during parsing.
  :type columns: list[str|None]
