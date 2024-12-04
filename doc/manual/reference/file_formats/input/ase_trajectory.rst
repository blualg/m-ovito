.. _file_formats.input.ase_trajectory:

ASE trajectory reader |ovito-pro|
---------------------------------

.. attention::

  This file format reader is written in Python and therefore is only available in :ref:`OVITO Pro <credits.ovito_pro>`
  and the :ref:`OVITO Python module <scripting_manual>`.

.. important::

  The file reader requires the `ASE module <https://wiki.fysik.dtu.dk/ase/install.html>`__ to work. Without this Python module, trajectory files will not be recognized
  by OVITO Pro or the :py:func:`~ovito.io.import_file` function. Thus, first make sure that ASE is installed in your Python interpreter or in the embedded interpreter
  of OVITO Pro. See :ref:`ovitos_install_modules`.

Loads particles coordinates from `trajectory files of the Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/ase/io/trajectory.html>`__.

Internally, the file reader is based on the :py:func:`ovito.io.ase.ase_to_ovito` function, which converts ASE atoms objects
to OVITO's :ref:`particle <scene_objects.particles>` datasets.

.. seealso:: :ref:`file_formats.input.ase_database`, :ref:`file_formats.output.ase_trajectory`
