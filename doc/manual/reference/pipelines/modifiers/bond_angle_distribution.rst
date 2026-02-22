.. _particles.modifiers.bond_angle_distribution:

Bond angle distribution |ovito-pro|
-----------------------------------

.. image:: /images/modifiers/bond_angle_distribution_panel.png
  :width: 35%
  :align: right

This modifier computes the distribution of the **bond angles** formed by pairs of :ref:`bonds <scene_objects.bonds>`
meeting at the same particle.

The modifier only includes :ref:`bonded <scene_objects.bonds>` particles in the distribution,
whereas non-bonded pairs will be ignored. Typically, the bond topology is read from the
input simulation file or it needs to be generated within OVITO using the :ref:`particles.modifiers.create_bonds` modifier.

.. versionchanged:: 3.15.0
  This modifier partially replaces the functionality of the "Bond Analysis" modifier found in earlier program versions.

Bond angle distribution
"""""""""""""""""""""""

The computed histogram counts in equisized bins the angles formed by pairs of bonds meeting at the same particle.
Bond angles :math:`\theta` may range from 0 to 180 degrees. The modifier option :guilabel:`Use cosines of angles` switches the
x-axis of the histogram to the range [-1, +1], now computing the distribution of :math:`\cos{\theta}` values in the system.

The modifier outputs the bond angle histogram as a :ref:`data table <scene_objects.data_table>`, which can be opened in the
pipeline data inspector using the button :guilabel:`Show in data inspector`.

Partitioned distributions
"""""""""""""""""""""""""

The modifier can break the computed distribution down into several partial histograms,
one for each combination of bond or particle types. The following partitioning modes are available:

Bond type
  Computes a separate bond angle histogram for each pair-wise combination of two bond types.

Bond selection
  Treats currently selected bonds and unselected bonds as two different kinds of bonds.
  Computes three separate bond angle histograms: one for angles formed by two selected bonds,
  one for angles formed by two unselected bonds, and one for angles formed by one selected and one unselected bond.
  This option allows you to calculate the bond angle distribution only for a specific subset of bonds.

Particle type
  Computes separate bond angle histograms for each combination of three particle types.
  If the number of particle types in the system is :math:`N`,
  then the bond angle distribution, involving triplets of particles (with the central particle discriminable),
  will be partitioned into :math:`N^2 (N+1)/2` partial bond angle distributions.

Particle selection
  Treats currently selected and unselected particles as two different species.
  The bond angle histogram will be partitioned into six partial histograms, one for each triplet combination
  of selected and unselected particles.

Time-averaged distributions
"""""""""""""""""""""""""""

The *Bond angle distribution* modifier calculates the instantaneous bond angle distribution for
just one simulation frame at a time. You can subsequently apply the :ref:`particles.modifiers.time_averaging` modifier to reduce
the instantaneous distributions to a single mean distribution by averaging the
output :ref:`data table <scene_objects.data_table>` over all frames of the loaded MD trajectory.

.. seealso::

  :py:class:`ovito.modifiers.BondAngleDistributionModifier` (Python API)