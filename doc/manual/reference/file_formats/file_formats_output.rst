.. _file_formats.output:

Output file formats
-------------------

OVITO can export data to the following file formats:

.. list-table::
  :widths: 20 55 25
  :header-rows: 1

  * - Format name
    - Description
    - Exported data types

  * - LAMMPS data
    - File format read by the `LAMMPS <https://www.lammps.org/>`__ molecular dynamics code.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers

  * - LAMMPS dump
    - Trajectory format used by the `LAMMPS <https://www.lammps.org/>`__ molecular dynamics code.
    - :ref:`particles <scene_objects.particles>`

  * - XYZ
    - A simple column-based text format, which is documented `here <http://en.wikipedia.org/wiki/XYZ_file_format>`__ and
      `here <http://libatoms.github.io/QUIP/io.html#module-ase.io.extxyz>`__.
    - :ref:`particles <scene_objects.particles>`

  * - POSCAR
    - File format used by the *ab initio* simulation package `VASP <http://www.vasp.at/">`__.
    - :ref:`particles <scene_objects.particles>`

  * - IMD
    - File format used by the molecular dynamics code `IMD <http://imd.itap.physik.uni-stuttgart.de/>`__.
    - :ref:`particles <scene_objects.particles>`

  * - FHI-aims
    - File format used by the *ab initio* simulation package `FHI-aims <https://aimsclub.fhi-berlin.mpg.de/index.php>`__.
    - :ref:`particles <scene_objects.particles>`

  * - NetCDF
    - Binary format for molecular dynamics data following the `AMBER format convention <http://ambermd.org/netcdf/nctraj.pdf>`__.
    - :ref:`particles <scene_objects.particles>`

  * - GSD/HOOMD
    - Binary molecular dynamics format used by the `HOOMD-blue <https://glotzerlab.engin.umich.edu/hoomd-blue/>`__ code.
      See `GSD (General Simulation Data) format <https://gsd.readthedocs.io>`__.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers, :ref:`global attributes <usage.global_attributes>`

  * - Table of values
    - A simple tabular text file with scalar quantities computed by OVITO's data pipeline.
    - :ref:`global attributes <usage.global_attributes>`

  * - VTK
    - Generic text-based data format used by the ParaView software.
    - :ref:`surface meshes <scene_objects.surface_mesh>`, :ref:`voxel grids <scene_objects.voxel_grid>`, :ref:`dislocations <scene_objects.dislocations>`

  * - glTF
    - Exports the entire scene to a 3d model file in the `glTF <https://www.khronos.org/gltf/>`__ format, which can be imported by other
      applications such as Blender or PowerPoint.
    - :ref:`any <scene_objects>`

  * - POV-Ray scene
    - Exports the entire scene to a file that can be rendered with `POV-Ray <http://www.povray.org/>`__.
    - :ref:`any <scene_objects>`

  * - Crystal Analysis (.ca)
    - Format that can store dislocation lines extracted from an atomistic crystal model by the :ref:`particles.modifiers.dislocation_analysis` modifier.
      The format is documented :ref:`here <particles.modifiers.dislocation_analysis.fileformat>`.
    - :ref:`dislocations <scene_objects.dislocations>`, :ref:`surface meshes <scene_objects.surface_mesh>`