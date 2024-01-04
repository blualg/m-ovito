.. _particles.modifiers.unwrap_trajectories:

Unwrap trajectories
-------------------

This modifier "unwraps" particle coordinates to make the particle trajectories continuous. It reverses the effect of
periodic boundary conditions, i.e. the folding of particle positions back into the primary simulation cell, which is typically
performed by molecular dynamics codes or OVITO's :ref:`particles.modifiers.wrap_at_periodic_boundaries` modifier.

.. figure:: /images/modifiers/unwrap_trajectories_example_before.svg
  :figwidth: 23%

  Input

.. figure:: /images/modifiers/unwrap_trajectories_example_after.svg
  :figwidth: 23%

  Output

The unwrapping of particle trajectories may be done in two different ways: If available, the modifier
uses the information stored in the ``Periodic Image`` particle property to map particles from the primary
periodic cell image to their actual positions. The ``Periodic Image`` particle property consists
of a triplet of integers, :math:`(i_x, i_y, i_z)`,
specifying for each particle which periodic image of the cell it is currently in. Some MD simulation codes
write this information to their output trajectory file, and OVITO will make use of it if available.

If the ``Periodic Image`` particle property is not present, the modifier automatically uses a heuristic to
unwrap the particle trajectories. It steps through all simulation frames to detect jumps (discontinuities) in the input trajectories.
Such jumps usually occur with every transition of a particle from one periodic cell image to another.
After the modifier has scanned the input trajectories and recorded all particle jumps, it subsequently modifies the coordinates of particles which have crossed a periodic boundary to "unfold" their
trajectories.

The modifier uses the `minimum image principle <https://en.wikipedia.org/wiki/Periodic_boundary_conditions#Practical_implementation:_continuity_and_the_minimum_image_convention>`__
to detect the transition of a particle through a periodic cell boundary from one frame to the next.

Note that the unwrapping of trajectories is only performed along those cell directions for which periodic
boundary conditions (PBC) are enabled. The PBC flags are read from the
input simulation file if available, or can manually be set in the :ref:`Simulation cell <scene_objects.simulation_cell>` panel.

.. seealso::

  :py:class:`ovito.modifiers.UnwrapTrajectoriesModifier` (Python API)
