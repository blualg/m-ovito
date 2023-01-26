.. _usage.export:

Exporting data
==============


The :menuselection:`File --> Export File` function of OVITO exports the results of
the current data pipeline to a file. Depending on the selected output format (see table below), different fragments of the dataset are exported,
e.g. the particles and their properties, the bonds, other computed quantities, etc.
Furthermore, you can choose which animation frame(s) should be exported (just the current frame or a range) and whether the
datasets are saved to a single output file or to a sequence of files, one per frame.

OVITO will ask you for a destination filename. Note that, if you append the :file:`.gz` suffix, the output file(s) will automatically be
compressed for text-based file formats.

.. _usage.global_attributes:

Global attribute values
-----------------------

Some of OVITO's analysis functions compute scalar output values, e.g. the total number of atoms of a
particular type or the computed surface area of a solid. You can find a table of these *global attributes*
associated with the current dataset on the :ref:`Attributes page <data_inspector.attributes>` of the data inspector panel.
Attributes may have time-dependent values (i.e. their value is dynamically recomputed by the pipeline system for every animation frame).

You can export the time series formed by one or more such global attributes to a text file using OVITO's file export function described above.
Make sure to select the "*Table of Values*" export format in the file selection dialog.
This output format produces a tabular text file with the values of the selected attributes as functions of time.
You can plot the exported data with any external charting tool.

.. seealso:: :ref:`adding_global_attributes`

.. _usage.export.formats:

Supported output file formats
-----------------------------

.. list-table::
  :widths: 20 55 25
  :header-rows: 1

  * - File format
    - Description
    - Exported data
  * - LAMMPS dump
    - Text-based file format produced and read by the `LAMMPS <https://www.lammps.org/>`__ molecular dynamics code.
    - :ref:`particles <scene_objects.particles>`
  * - LAMMPS data
    - File format read by the `LAMMPS <https://www.lammps.org/>`__ molecular dynamics code.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers
  * - XYZ
    - A simple column-based text format, which is documented `here <http://en.wikipedia.org/wiki/XYZ_file_format>`__.

      ..
        and `here <http://libatoms.github.io/QUIP/io.html#module-ase.io.extxyz>`__. TODO

    - :ref:`particles <scene_objects.particles>`
  * - POSCAR
    - File format used by the *ab initio* simulation package `VASP <http://www.vasp.at/>`__.
    - :ref:`particles <scene_objects.particles>`
  * - IMD
    - File format used by the molecular dynamics code `IMD <http://imd.itap.physik.uni-stuttgart.de/>`__.
    - :ref:`particles <scene_objects.particles>`
  * - FHI-aims
    - File format used by the *ab initio* simulation package `FHI-aims <https://aimsclub.fhi-berlin.mpg.de/index.php>`__.
    - :ref:`particles <scene_objects.particles>`
  * - NetCDF
    - Binary format for molecular dynamics data following the `AMBER <http://ambermd.org/netcdf/nctraj.pdf>`__ format convention.
    - :ref:`particles <scene_objects.particles>`
  * - GSD/HOOMD
    - Binary format for molecular dynamics data used by the `HOOMD-blue <https://glotzerlab.engin.umich.edu/hoomd-blue/>`__ code. See `GSD (General Simulation Data) format <https://gsd.readthedocs.io>`__.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, :ref:`global attributes <usage.global_attributes>`
  * - VTK
    - Generic text-based data format used by the ParaView software.
    - :ref:`surface meshes <scene_objects.surface_mesh>`, :ref:`voxel grids <scene_objects.voxel_grid>`, :ref:`dislocations <scene_objects.dislocations>`
  * - POV-Ray scene
    - Exports the entire scene to a file that can be rendered with `POV-Ray <http://www.povray.org/>`__.
    - any
  * - Crystal Analysis (.ca)
    - Format that can store dislocation lines extracted from an atomistic crystal model by the :ref:`Dislocation Analysis <particles.modifiers.dislocation_analysis>` modifier.
      The format is documented :ref:`here <particles.modifiers.dislocation_analysis.fileformat>`.
    - :ref:`dislocations <scene_objects.dislocations>`, :ref:`surface meshes <scene_objects.surface_mesh>`
