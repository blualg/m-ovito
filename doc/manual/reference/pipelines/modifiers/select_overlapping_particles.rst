.. _particles.modifiers.select_overlapping_particles:

Select Overlapping Particles
----------------------------

.. image:: /images/modifiers/select_overlapping_particles.png
  :width: 35%
  :align: right

Selects particles that are within a given distance of each other. This selection can be used to subsequently remove particles
that are too close to each other, for example after constructing an input structure for a simulation.

.. versionadded:: 3.15.0

Parameters
""""""""""

Overlap distance
  The maximum inter-particle distance at which particles are considered overlapping and selected by this modifier.

Use only selected particles
  Controls whether the modifier should operate only on currently selected particles. Requires an input selection.

Keep one particle unselected on overlap
  When two or more particles are within the :guilabel:`Overlap distance` of each other, this option controls whether the modifier
  selects all of the particles in the overlap group or excludes one of them from the selection.

  If this option is turned on (the default), one randomly picked particle from each overlap group will *not* be selected.
  Use this mode if the goal is to delete duplicate particles and keep exactly one.

  If this option is turned off, all particles in the overlap group will be selected.
  Use this mode when the goal is to identify particles that are close to each other.

.. seealso::
  
  :py:class:`ovito.modifiers.SelectOverlappingParticlesModifier` (Python API)