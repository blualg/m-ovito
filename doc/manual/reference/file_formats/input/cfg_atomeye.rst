.. _file_formats.input.cfg_atomeye:

CFG (AtomEye) file reader
-------------------------

.. figure:: /images/io/cfg_atomeye_reader.*
  :figwidth: 30%
  :align: right

  User interface of the CFG file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file format is typically used by the `AtomEye <http://li.mit.edu/Archive/Graphics/A/>`__ visualization program.

.. _file_formats.input.cfg_atomeye.variants:

Supported format variants
"""""""""""""""""""""""""

The file reader supports the *Standard CFG* format as well as the *Extended CFG* format, see `here <http://li.mit.edu/Archive/Graphics/A/#formats>`__,
which can store auxiliary per-atom properties.

OVITO maintains the original order in which atoms are stored in the CFG file.

The geometry of the simulation cell is loaded from the CFG file. However, because the file format does not store the boundary conditions used in
the simulation, OVITO assumes periodic boundary conditions are enabled in all three cell directions by default when opening a CFG file. You can override the
boundary conditions by hand in the :ref:`scene_objects.simulation_cell` panel after import.

.. _file_formats.input.cfg_atomeye.property_mapping:

Auxiliary properties
""""""""""""""""""""

The auxiliary properties from an *Extended CFG* file get mapped to corresponding :ref:`particle properties <usage.particle_properties>` within OVITO during file import.
This happens automatically according to the following rules.

========================== ==========================
CFG property name          OVITO particle property
========================== ==========================
id                         ``Particle Identifier``
vx, vy, vz                 ``Velocity``
v                          ``Velocity Magnitude``
radius                     ``Radius``
q                          ``Charge``
ix, iy, iz                 ``Periodic Image``
fx, fy, fz                 ``Force``
mux, muy, muz              ``Dipole Orientation``
mu                         ``Dipole Magnitude``
omegax, omegay, omegaz     ``Angular Velocity``
angmomx, angmomy, angmomz  ``Angular Momentum``
tqx, tqy, tqz              ``Torque``
spin                       ``Spin``
========================== ==========================

Any other auxiliary property is mapped to a corresponding user-defined particle property with the same name in OVITO.

.. _file_formats.input.cfg_atomeye.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, sort_particles = False)
  :noindex:

  :param sort_particles: Makes the file reader reorder the loaded particles before passing them to the pipeline.
                         Sorting is based on the values of the ``Particle Identifier`` property loaded from the CFG file
                         (only if available as an auxiliary property in *Extended CFG* files).
  :type sort_particles: bool
