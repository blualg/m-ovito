.. _file_formats.output.ase_trajectory:

ASE trajectory writer |ovito-pro|
---------------------------------

.. attention::

  This file format writer requires Python and therefore is only available in :ref:`OVITO Pro <credits.ovito_pro>`
  and the :ref:`OVITO Python module <scripting_manual>`.

.. important::

  The file writer requires the `ASE module <https://wiki.fysik.dtu.dk/ase/install.html>`__ to work.
  Please first make sure that ASE is installed in your Python interpreter or in the embedded interpreter
  of OVITO Pro. See :ref:`ovitos_install_modules`.

Saves the particles and their trajectories to a `trajectory file of the Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/ase/io/trajectory.html>`__.

Internally, the file writer is based on the :py:func:`ovito.io.ase.ovito_to_ase` function, which converts OVITO's :ref:`particles <scene_objects.particles>`
to ASE atoms objects.

.. seealso:: :ref:`file_formats.input.ase_trajectory`
