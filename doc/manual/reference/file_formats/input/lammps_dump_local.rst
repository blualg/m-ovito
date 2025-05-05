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

This reader specifically handles files written by the ``local`` `dump style <https://docs.lammps.org/dump.html>`__ of LAMMPS --
in contrast to regular *atom* and *custom* dump files. The latter store only atomistic data and are handled by OVITO's regular
:ref:`file_formats.input.lammps_dump`.

Since *dump local* files don't contain per-particle data but rather information related to interactions (bonds, angles, dihedrals, etc.),
the file reader is typically used in conjunction with the :ref:`particles.modifiers.load_trajectory` modifier to amend an already loaded
structure, with particle coordinates coming from another file. A common use case of the :ref:`file_formats.input.lammps_dump_local` is to load
dynamically changing bond topologies from a reactive MD simulation run.

The file reader detects the kind of data items (bonds, angles, dihedrals, impropers, or neighbors) in a dump local file by analyzing
the file's header. For example

.. code-block:: none

  ITEM: NUMBER OF BONDS

tells OVITO that the dump file contains a list of bonds and their properties. Instead of *"BONDS"* and *"ENTRIES"* (the default label), the reader also understands the labels:

  - *"ANGLES"* for angular data
  - *"DIHEDRALS"* for dihedral data
  - *"IMPROPERS"* for improper data
  - *"NEIGHBORS"* for neighbor lists

You must set the correct *label* using the `dump_modify label <https://docs.lammps.org/dump_modify.html>`__ command in your LAMMPS simulation script. For example,
to dump the current list of bonds in regular time intervals, you would use the following command sequence:

.. code-block:: none

  compute      bond all property/local batom1 batom2 btype
  dump         bond_dump all local 100 bonds.dump c_bond[1] c_bond[2] c_bond[3]
  dump_modify  bond_dump label BONDS

Furthermore, to make it easier for OVITO :ref:`to automatically recognize the meaning of the file's columns <file_formats.input.lammps_dump_local.property_mapping>`,
you should give them names using the LAMMPS `dump_modify colname <https://docs.lammps.org/dump_modify.html>`__ command:

.. code-block:: none

  dump_modify  bond_dump colname 1 batom1 colname 2 batom2 colname 3 btype

The reader can parse gzipped files (".gz" suffix) and zstd compressed files (".zst" suffix) directly. Binary files (".bin" suffix) are *not* supported.

.. _file_formats.input.lammps_dump_local.property_mapping:

Automatic column mapping
""""""""""""""""""""""""

OVITO uses the names of the columns in the dump local file to automatically map them to the right bond/angle/dihedral/improper properties in OVITO.
It therefore is important to give the columns in your dump file meaningful names using the LAMMPS `dump_modify colname <https://docs.lammps.org/dump_modify.html>`__ command
**already at simulation time**.

**Bonds**
  For *BONDS* dump files, the following column names are recognized by OVITO:

    - *batom1* and *batom2* for the two atom IDs of the bond
    - *btype* for the bond type (optional)
    - *dist* for the bond length computed by the `compute bond/local <https://docs.lammps.org/compute_bond_local.html>`__ command (optional)

  Use the following LAMMPS commands to generate such a dump file:

  .. code-block:: none

    compute     bond all property/local batom1 batom2 btype
    compute     dist all bond/local dist
    dump        bond_dump all local 100 bonds.dump c_bond[1] c_bond[2] c_bond[3] c_dist
    dump_modify bond_dump &
                  label BONDS &
                  colname 1 batom1 &
                  colname 2 batom2 &
                  colname 3 btype &
                  colname 4 dist

**Angles**
  For *ANGLES* dump files, the following column names are recognized by OVITO:

    - *aatom1*, *aatom2*, and *aatom3* for the three atom IDs defining the angular interaction
    - *atype* for the angle type (optional)
    - *theta* for the angle value computed by the `compute angle/local <https://docs.lammps.org/compute_angle_local.html>`__ command (optional)

  Use the following LAMMPS commands to generate such a dump file:

  .. code-block:: none

    compute     angle all property/local aatom1 aatom2 aatom3 atype
    compute     theta all angle/local theta
    dump        angle_dump all local 100 angles.dump c_angle[1] c_angle[2] c_angle[3] c_angle[4] c_theta
    dump_modify angle_dump &
                  label ANGLES &
                  colname 1 aatom1 &
                  colname 2 aatom2 &
                  colname 3 aatom3 &
                  colname 4 atype &
                  colname 5 theta

**Dihedrals**
  For *DIHEDRALS* dump files, the following column names are recognized by OVITO:

    - *datom1*, *datom2*, *datom3*, and *datom4* for the four atom IDs defining the dihedral interaction
    - *dtype* for the dihedral type (optional)
    - *phi* for the dihedral angle computed by the `compute dihedral/local <https://docs.lammps.org/compute_dihedral_local.html>`__ command (optional)

  Use the following LAMMPS commands to generate such a dump file:

  .. code-block:: none

    compute     dihedral all property/local datom1 datom2 datom3 datom4 dtype
    compute     phi all dihedral/local phi
    dump        dihedral_dump all local 100 dihedrals.dump c_dihedral[1] c_dihedral[2] c_dihedral[3] c_dihedral[4] c_dihedral[5] c_phi
    dump_modify dihedral_dump &
                  label DIHEDRALS &
                  colname 1 datom1 &
                  colname 2 datom2 &
                  colname 3 datom3 &
                  colname 4 datom4 &
                  colname 5 dtype &
                  colname 6 phi

**Impropers**
  For *IMPROPERS* dump files, the following column names are recognized by OVITO:

    - *iatom1*, *iatom2*, *iatom3*, and *iatom4* for the four atom IDs defining the improper interaction
    - *itype* for the improper type (optional)
    - *chi* for the improper angle computed by the `compute improper/local <https://docs.lammps.org/compute_improper_local.html>`__ command (optional)

  Use the following LAMMPS commands to generate such a dump file:

  .. code-block:: none

    compute     improper all property/local iatom1 iatom2 iatom3 iatom4 itype
    compute     chi all improper/local chi
    dump        improper_dump all local 100 impropers.dump c_improper[1] c_improper[2] c_improper[3] c_improper[4] c_improper[5] c_chi
    dump_modify improper_dump &
                  label IMPROPERS &
                  colname 1 iatom1 &
                  colname 2 iatom2 &
                  colname 3 iatom3 &
                  colname 4 iatom4 &
                  colname 5 itype &
                  colname 6 chi

Manual column mapping
"""""""""""""""""""""

In case OVITO is unable to guess the correct mapping automatically, e.g., when the file's columns have user-defined names,
you may need to specify the correct mapping by hand in the following dialog displayed by the file reader:

.. image:: /images/io/lammps_dump_local_reader_mapping_dialog.*
  :width: 50%

File columns get automatically mapped to corresponding standard properties in OVITO if their names
match one of the predefined :ref:`standard bond properties <bond-properties-list>` (case insensitive).
Spaces that are part of a standard property name must be left out, because LAMMPS dump files do not support column names containing spaces. For example,
a dump file column named ``BondType`` will be mapped to the standard property :guilabel:`Bond Type`.

For standard properties with multiple components, a component name must be appended with a dot. For example, a dump file column
named ``ParticleIdentifiers.A`` will automatically be mapped to the second component of the :guilabel:`Particle Identifiers` :ref:`standard bond property <bond-properties-list>`
in OVITO. Note that you can use the LAMMPS `dump_modify colname` command to give the columns in your dump file specific names.

For further information on how to set up the bond property mapping correctly, see the :ref:`particles.modifiers.load_trajectory` modifier.

.. _file_formats.input.lammps_dump_local.python:

Python parameters
"""""""""""""""""

The file reader accepts the following keyword parameters in a call to the :py:meth:`~ovito.pipeline.FileSource.load` Python function:

.. py:function:: load(location, columns = None)
  :noindex:

  :param columns: A list of OVITO :ref:`bond/angle/dihedral/improper property <bond-properties-list>` names, one for each data column in the dump local file.
                  List entries may be set to ``None`` to skip individual file columns during parsing.
  :type columns: list[str|None]
