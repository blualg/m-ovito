.. _particles.modifiers.wrap_at_periodic_boundaries:

Wrap at periodic boundaries
---------------------------

This modifier maps the positions of all particles to the primary image of the simulation cell.
In other words, it wraps the particle coordinates at the periodic boundaries of the simulation cell.

.. figure:: /images/modifiers/wrap_at_periodic_boundaries_example_before.*
  :figwidth: 20%

  Input

.. figure:: /images/modifiers/wrap_at_periodic_boundaries_example_after.*
  :figwidth: 20%

  Output

The wrapping is only performed along those directions for which periodic
boundary conditions (PBC) are enabled for the simulation cell. The PBC flags are read from the
input simulation file if available and can be manually set in the :ref:`Simulation cell <scene_objects.simulation_cell>` panel.

.. versionadded:: 3.10.1

  As a side effect of the coordinate wrapping, the ``Periodic Image`` property gets created by the modifier — or updated if already present.
  This particle property stores for each particle which periodic image of the simulation cell it was located in originally.
  This information may be used later on to unwrap the particle coordinates again using the :ref:`particles.modifiers.unwrap_trajectories` modifier.

  If the dataset contains bonds, the modifier also updates the per-bond ``Periodic Image`` property, adjusting the PBC shift vectors
  of bonds to remain consistent with the wrapped particle positions.

.. seealso::

  :py:class:`ovito.modifiers.WrapPeriodicImagesModifier` (Python API)