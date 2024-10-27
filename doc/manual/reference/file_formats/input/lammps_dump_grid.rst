.. _file_formats.input.lammps_dump_grid:

LAMMPS dump grid file reader
----------------------------

Loads :ref:`voxel grid <scene_objects.voxel_grid>` data written by the *dump grid* command of the LAMMPS simulation code.

The file reader can decompress gzipped files (".gz" suffix) and zstd compressed files (".zst" suffix) on the fly.

.. seealso:: `LAMMPS documentation <https://docs.lammps.org/Howto_grid.html>`__