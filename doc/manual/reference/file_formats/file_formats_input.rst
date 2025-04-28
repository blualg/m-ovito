.. _file_formats.input:

Input file formats
------------------

.. toctree::
  :hidden:

  input/ase_database
  input/ase_trajectory
  input/cfg_atomeye
  input/gaussian_cube
  input/gromacs
  input/gsd
  input/lammps_data
  input/lammps_dump
  input/lammps_dump_grid
  input/lammps_dump_local
  input/poscar
  input/reaxff
  input/trr
  input/xtc
  input/xyz

OVITO can read the following file formats:

.. list-table::
  :widths: 24 46 20 10
  :header-rows: 1

  * - Format name
    - Description
    - Imported data types
    -

  * - LAMMPS data
    - File format used by the `LAMMPS <https://docs.lammps.org/read_data.html>`__ molecular dynamics code.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers
    - :ref:`Details <file_formats.input.lammps_data>`

  * - LAMMPS dump atom |br|
      LAMMPS dump custom
    - File format used by the `LAMMPS <https://www.lammps.org/>`__  molecular dynamics code. OVITO supports both text-based and binary dump files.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.lammps_dump>`

  * - LAMMPS dump local
    - File format written by the `dump local <https://docs.lammps.org/dump.html>`__ command of LAMMPS.
      OVITO's :ref:`particles.modifiers.load_trajectory` modifier can read varying bond topology and
      per-bond quantities from such files generated in reactive molecular dynamics simulations.
    - :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers
    - :ref:`Details <file_formats.input.lammps_dump_local>`

  * - LAMMPS dump grid
    - File format containing volumetric data written by the *dump grid* command of LAMMPS.
    - :ref:`voxel grids <scene_objects.voxel_grid>`
    - :ref:`Details <file_formats.input.lammps_dump_grid>`

  * - ReaxFF bonds
    - File format written by the LAMMPS `fix reaxff/bonds <https://docs.lammps.org/fix_reaxff_bonds.html>`__ command and the original ReaxFF code of Adri van Duin.
      OVITO's :ref:`particles.modifiers.load_trajectory` modifier can read the bond topology, bond order and
      atomic charges dumped during ReaxFF molecular dynamics simulations.
    - :ref:`bonds <scene_objects.bonds>`
    - :ref:`Details <file_formats.input.reaxff>`

  * - XYZ
    - Simple column-based text format for particle data, which is documented `here <http://en.wikipedia.org/wiki/XYZ_file_format>`__.
      OVITO can read the :ref:`extended XYZ format <file_formats.input.xyz.extended_format>`,
      which supports arbitrary sets of particle data columns, and can store additional information such as the simulation cell geometry and boundary conditions.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.xyz>`

  * - POSCAR / XDATCAR / CHGCAR
    - File formats used by the *ab initio* simulation package `VASP <https://www.vasp.at/>`__.
      OVITO can import atomistic configurations and also charge density fields from CHGCAR files.
    - :ref:`particles <scene_objects.particles>`, :ref:`voxel grids <scene_objects.voxel_grid>`
    - :ref:`Details <file_formats.input.poscar>`

  * - Gromacs GRO
    - Coordinate file format used by the `GROMACS <http://www.gromacs.org/>`__ simulation code.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.gromacs>`

  * - Gromacs XTC
    - Trajectory file format used by the `GROMACS <http://www.gromacs.org/>`__ simulation code.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.xtc>`

  * - Gromacs TRR
    - Trajectory file format used by the `GROMACS <http://www.gromacs.org/>`__ simulation code.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.trr>`

  * - DCD
    - Trajectory file format written by the CHARMM, NAMD, and LAMMPS simulation codes.
    - :ref:`particles <scene_objects.particles>`
    -

  * - CFG (AtomEye)
    - File format used by the `AtomEye <http://li.mit.edu/Archive/Graphics/A/>`__ visualization program.
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.cfg_atomeye>`

  * - NetCDF
    - Binary format for molecular dynamics data following the `AMBER format convention <http://ambermd.org/netcdf/nctraj.pdf>`__. NetCDF files are produced by
      the LAMMPS `dump netcdf <https://docs.lammps.org/dump_netcdf.html>`__ command.
    - :ref:`particles <scene_objects.particles>`
    -

  * - CIF
    - `Crystallographic Information File <https://www.iucr.org/resources/cif>`__ format as specified by the
      International Union of Crystallography (IUCr). Parser supports only small-molecule crystal structures.
    - :ref:`particles <scene_objects.particles>`
    -

  * - PDB
    - Protein Data Bank (PDB) files.
    - :ref:`particles <scene_objects.particles>`
    -

  * - PDBx/mmCIF
    - The `PDBx/mmCIF <http://mmcif.wwpdb.org>`__ format stores
      macromolecular structures and is used by the Worldwide Protein Data Bank.
    - :ref:`particles <scene_objects.particles>`
    -

  * - Quantum Espresso
    - Input data format used by the `Quantum Espresso <https://www.quantum-espresso.org/>`__ electronic-structure calculation code.
    - :ref:`particles <scene_objects.particles>`
    -

  * - FHI-aims
    - Geometry and log-file formats used by the *ab initio* simulation package `FHI-aims <https://aimsclub.fhi-berlin.mpg.de/index.php>`__.
    - :ref:`particles <scene_objects.particles>`
    -

  * - GSD/HOOMD
    - Binary molecular dynamics format written by the `HOOMD-blue <https://glotzerlab.engin.umich.edu/hoomd-blue/>`__ code.
      See `GSD (General Simulation Data) format <https://gsd.readthedocs.io>`__.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, angles, dihedrals, impropers
    - :ref:`Details <file_formats.input.gsd>`

  * - CASTEP
    - File format used by the `CASTEP <http://www.castep.org>`__ *ab initio* code. OVITO can read the |castep formats|_.

        .. |castep formats| replace:: :file:`.cell`, :file:`.md` and :file:`.geom` formats
        .. _castep formats: http://www.tcm.phy.cam.ac.uk/castep/documentation/WebHelp/content/modules/castep/expcastepfileformats.htm
    - :ref:`particles <scene_objects.particles>`
    -

  * - XSF
    - File format used by the `XCrySDen <http://www.xcrysden.org/doc/XSF.html>`__ program.
    - :ref:`particles <scene_objects.particles>`, :ref:`voxel grids <scene_objects.voxel_grid>`
    -

  * - Cube
    - File format used by the *Gaussian* simulation package and other ab initio codes.
    - :ref:`particles <scene_objects.particles>`, :ref:`voxel grids <scene_objects.voxel_grid>`
    - :ref:`Details <file_formats.input.cube>`

  * - IMD
    - File format used by the molecular dynamics code `IMD <http://imd.itap.physik.uni-stuttgart.de/>`__.
    - :ref:`particles <scene_objects.particles>`
    -

  * - DL_POLY
    - File format used by the molecular simulation package `DL_POLY <https://www.scd.stfc.ac.uk/Pages/DL_POLY.aspx>`__.
    - :ref:`particles <scene_objects.particles>`
    -

  * - ASE database |ovito-pro|
    - `Structure database files written by the Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/ase/db/db.html>`__
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.ase_database>`

  * - ASE trajectory |ovito-pro|
    - `Simulation trajectory files written by the Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/ase/io/trajectory.html>`__
    - :ref:`particles <scene_objects.particles>`
    - :ref:`Details <file_formats.input.ase_trajectory>`

  * - GALAMOST
    - XML-based file format used by the *GALAMOST* molecular dynamics code (which has been discontinued). The file format is also
      used by the `PYGAMD <https://github.com/youliangzhu/pygamd-v1>`__ and the `DL_MESO <https://dl-sdg.github.io/RESOURCES/TUTORIALS/dlm_7.html#trajectxml>`__ simulation codes.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`
    -

  * - VTK (legacy format)
    - File format used by the *Visualization Toolkit* (VTK) and the software *ParaView*. The format is described `here <http://www.vtk.org/VTK/img/file-formats.pdf>`__.
      The file reader currently supports only ASCII-based files containing PolyData and UnstructuredGrid data with triangular cells.
    - :ref:`triangle meshes <scene_objects.triangle_mesh>`
    -

  * - VTI (VTK ImageData)
    - XML-based file format used by the *Visualization Toolkit* (VTK) and the software *ParaView*. The format is described `here <http://www.vtk.org/VTK/img/file-formats.pdf>`__.
      The file reader currently supports only a subset of the full format specification and is geared towards files written by the `Aspherix <https://www.aspherix-dem.com/>`__ simulation code.
    - :ref:`voxel grids <scene_objects.voxel_grid>`
    -

  * - VTP (VTK PolyData)
    - XML-based file format used by the *Visualization Toolkit* (VTK) and the software *ParaView*. The format is described `here <http://www.vtk.org/VTK/img/file-formats.pdf>`__.
      The file reader currently supports only a subset of the full format specification and is geared towards mesh geometry and particle data files written by the `Aspherix <https://www.aspherix-dem.com/>`__ simulation code.
      VTK PolyData blocks consisting of triangle strips or polygons are imported as surface meshes by OVITO. PolyData consisting of vertices only are imported as a set of particles.
    - :ref:`surface meshes <scene_objects.surface_mesh>`, :ref:`particles <scene_objects.particles>`
    -

  * - VTM (VTK MultiBlock)
    - XML-based file format used by the *Visualization Toolkit* (VTK) and the software *ParaView*. VTK multiblock data files are meta-files that point to a list of VTK XML files,
      which will all be loaded by OVITO as a single data collection.
    - :ref:`any <scene_objects>`
    -

  * - PVD (ParaView data file)
    - XML-based file format used by the software *ParaView*, which describes a trajectory formed by a sequence of individual data files.
      The file format is described `here <https://www.paraview.org/Wiki/ParaView/Data_formats#PVD_File_Format>`__.
    - :ref:`any <scene_objects>`
    -

  * - OBJ
    - Text-based file format for triangle meshes (see `here <https://en.wikipedia.org/wiki/Wavefront_.obj_file>`__).
    - :ref:`triangle meshes <scene_objects.triangle_mesh>`
    -

  * - STL
    - File format for triangle meshes, text and binary variants (see `here <https://en.wikipedia.org/wiki/STL_(file_format)>`__).
    - :ref:`triangle meshes <scene_objects.triangle_mesh>`
    -

  * - PARCAS
    - File format written by the MD simulation code *Parcas* developed in K. Nordlund's group at University of Helsinki.
    - :ref:`particles <scene_objects.particles>`
    -

  * - oxDNA
    - Configuration/topology file format used by the `oxDNA <https://dna.physics.ox.ac.uk/>`__ simulation code for coarse-grained DNA models.
    - :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`
    -

*OVITO Pro* additionally provides the option for you to write :ref:`custom file readers in Python <writing_custom_file_readers>` to import more formats.

.. seealso:: :py:func:`ovito.io.import_file` (Python API)