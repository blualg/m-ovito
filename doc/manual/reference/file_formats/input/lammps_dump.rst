.. _file_formats.input.lammps_dump:

LAMMPS dump file reader
-----------------------

.. figure:: /images/io/lammps_dump_reader.*
  :figwidth: 30%
  :align: right

  User interface of the LAMMPS dump reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

For loading particle trajectory data from files written by the `dump command <https://docs.lammps.org/dump.html>`__ of the LAMMPS simulation code.

.. _file_formats.input.lammps_dump.variants:

Supported format variants
"""""""""""""""""""""""""

The reader handles files written by the following LAMMPS dump styles:

  - ``custom``, ``custom/gz``, ``custom/mpiio``
  - ``atom``, ``atom/gz``, ``atom/mpiio``
  - ``yaml``

.. note::

  Dump styles ``cfg``, ``xyz``, ``local``, ``xtc`` and ``netcdf`` are handled by :ref:`different file format readers <file_formats.input>` of OVITO.

In addition to text dump files, *binary* dump files (".bin" suffix) and *gzipped* dump files (".gz" suffix) can be read.

OVITO can process trajectories from a single large dump file or from a sequence of smaller dump files
(written by LAMMPS when the output filename contains a "*" wildcard character).
It can even concatenate a long trajectory from several dump files, each containing multiple frames.

The current program version does *not* support loading sets of parallel dump files, written by LAMMPS when the output filename contains a "%" wildcard character.

.. _file_formats.input.lammps_dump.dump_modify:

LAMMPS ``dump_modify`` options
""""""""""""""""""""""""""""""

LAMMPS lets you configure the dump output through its `dump_modify command <https://docs.lammps.org/dump_modify.html>`__.
OVITO provides support for the following dump_modify keywords:

  - The `time` keyword makes LAMMPS write the current elapsed simulation time to the dump file.
    In OVITO, the time value of each trajectory frame is made available as a :ref:`global attribute <usage.global_attributes>` named "Time".
    The timestep number, which is always present, is made available as global attribute "Timestep".

  - The `units` keyword makes LAMMPS write two extra lines "ITEM: UNITS" to the dump file header.
    OVITO currently ignores this information, since the program has no internal unit system (all quantities are treated as being without units).

  - The `sort` keyword requests LAMMPS to output the list of atoms to the dump file in sorted order (by unique atom ID).
    This option is normally not needed, because OVITO accepts an unsorted list of atoms and can handle trajectories in which the storage
    order of atoms varies throughout a simulation (which is common in LAMMPS). OVITO maintains the original order in which atoms appear in the dump file.
    Unique atom IDs (if present as a separate column in the dump file) are used to identify individual atoms in different trajectory frames.
    Nevertheless, if desired, the dump file reader provides a user option requesting OVITO to sort the atoms by ID during data import
    (``sort_particles=True`` keyword parameter in Python, see below). This option makes the ordering of
    atoms stable within OVITO even if the `sort` keyword was not used at the time LAMMPS wrote the dump file.

  - The `colname` keyword lets you override the column names in the dump file, which are otherwise automatically
    chosen by LAMMPS. This gives you full control over how the information in the dump file will be mapped to particle properties in
    OVITO, see the next section.

  - The `thermo` keyword tells LAMMPS to include the current thermo data in *yaml* style dump files. OVITO imports
    the thermo values as :ref:`global attributes <usage.global_attributes>`.

.. _file_formats.input.lammps_dump.property_mapping:

Column-to-property mapping
""""""""""""""""""""""""""

The data columns of a dump file get mapped to corresponding :ref:`particle properties <usage.particle_properties>` within OVITO during file import.
This happens automatically according to the following rules, but you can manually override the mapping if necessary by clicking the :guilabel:`Edit column mapping` button.
For certain dump file columns, the file parser may perform an automatic conversion as described in the third table column.

========================== ================================ =========================
LAMMPS column name         OVITO particle property          Comments
========================== ================================ =========================
x, y, z                    :guilabel:`Position`
xu, yu, zu                 :guilabel:`Position`
xs, ys, zs                 :guilabel:`Position`             Automatic conversion from reduced to Cartesian coordinates
xsu, ysu, zsu              :guilabel:`Position`             Automatic conversion from reduced to Cartesian coordinates
id                         :guilabel:`Particle Identifier`
vx, vy, vz                 :guilabel:`Velocity`             Automatic computation of ``Velocity Magnitude`` property
type                       :guilabel:`Particle Type`
element                    :guilabel:`Particle Type`        Named types (may be combined with numeric IDs from `type` column)
mass                       :guilabel:`Mass`
radius                     :guilabel:`Radius`
diameter                   :guilabel:`Radius`               Automatic division by 2
mol                        :guilabel:`Molecule Identifier`
q                          :guilabel:`Charge`
ix, iy, iz                 :guilabel:`Periodic Image`
fx, fy, fz                 :guilabel:`Force`
mux, muy, muz              :guilabel:`Dipole Orientation`
mu                         :guilabel:`Dipole Magnitude`
omegax, omegay, omegaz     :guilabel:`Angular Velocity`
angmomx, angmomy, angmomz  :guilabel:`Angular Momentum`
tqx, tqy, tqz              :guilabel:`Torque`
spin                       :guilabel:`Spin`
quati, quatj, quatk, quatw :guilabel:`Orientation`          Quaternion components X, Y, Z, W (see :ref:`here <howto.aspherical_particles.orientation>`)
c_epot                     :guilabel:`Potential Energy`
c_kpot                     :guilabel:`Kinetic Energy`
c_stress[1..6]             :guilabel:`Stress Tensor`        Symmetric tensor components XX, YY, ZZ, XY, XZ, YZ
c_orient[1..4]             :guilabel:`Orientation`          Quaternion components X, Y, Z, W (see :ref:`here <howto.aspherical_particles.orientation>`)
c_shape[1..3]              :guilabel:`Aspherical Shape`     Principal semi-axes (see :ref:`here <howto.aspherical_particles.ellipsoids>`)
c_diameter[1..3]           :guilabel:`Aspherical Shape`     Same as above but with automatic division by 2 (see :ref:`example <howto.aspherical_particles.orientation>`)
shapex, shapey, shapez     :guilabel:`Aspherical Shape`     Same as above but with automatic division by 2 (see :ref:`example <howto.aspherical_particles.orientation>`)
c_cna                      :guilabel:`Structure Type`
pattern                    :guilabel:`Structure Type`
========================== ================================ =========================

Generally, file columns are mapped to the corresponding standard property in OVITO if their name
matches one of the predefined :ref:`particle properties <particle-properties-list>` (case insensitive).
Spaces that are part of the standard property name must be left out, because LAMMPS dump files do not support column names containing spaces. For example,
a dump file column named ``ParticleType`` will be mapped to the standard property :guilabel:`Particle Type`.

For vectorial standard properties, a component name must be appended with a dot. For example, a dump file column
named ``AsphericalShape.Z`` will automatically be mapped to the z-component of the :guilabel:`Aspherical Shape` standard property
in OVITO. Note that you can use the LAMMPS `dump_modify colname` command to give the columns in your dump file specific names.

File columns having any other name not listed in the table above and not being a :ref:`standard property name <particle-properties-list>`
will get imported as user-defined particle properties in OVITO.

.. _file_formats.input.lammps_dump.further_notes:

Further notes
"""""""""""""

- LAMMPS can perform 2d and 3d simulations (see `dimension <https://docs.lammps.org/dimension.html>`__ command) and OVITO can also treat a system
  as either two- or three-dimensional (see :ref:`scene_objects.simulation_cell`). However, the dimensionality of a simulation is not encoded in the
  dump file. OVITO assumes that the simulation is two-dimensional if the dump file contains no z-coordinates. You can override this after import if necessary.

.. _file_formats.input.lammps_dump.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, columns = None, sort_particles = False)
  :noindex:

  :param columns: A list of OVITO particle property names, one for each data column in the dump file. Overrides the mapping
                  that otherwise gets set up automatically as described above. List entries may be set to ``None``
                  to skip individual file columns during parsing.
  :type columns: list[str | None] | None
  :param sort_particles: Makes the file reader reorder the loaded particles before passing them to the pipeline.
                         Sorting is based on the values of the ``Particle Identifier`` property loaded from the dump file.
  :type sort_particles: bool
