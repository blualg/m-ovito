.. _particles.modifiers.freeze_property:

Freeze Property
---------------

.. image:: /images/modifiers/freeze_property_panel.png
  :width: 35%
  :align: right

The *Freeze Property* modifier copies the values of a particle property from a specific frame to all other frames of the trajectory. This effectively "freezes" the property values,
making them constant.

This is useful when you want to preserve information from a particular simulation frame and use it throughout the entire trajectory, for comparison or visualization purposes.

By default, the modifier samples the input property at frame 0 of the animation. It then writes these values to the selected output property for all frames. The output property can be the same as the input, or a different one, allowing you to retain both the dynamically changing and the frozen property values.

.. tip::

  The *Freeze Property* modifier should be placed in the pipeline right after where the property you want to freeze gets created.
  When freezing a property loaded from the simulation trajectory file, place the *Freeze Property* modifier at the beginning of the pipeline.
  Make sure all other modifiers that dynamically remove particles are placed *after* the *Freeze Property* modifier. That's because
  the *Freeze Property* modifier needs to "see" all particles in the input data to copy their property values.

.. _particles.modifiers.freeze_property.varying_particle_numbers:

Simulations with changing particle numbers
""""""""""""""""""""""""""""""""""""""""""

The *Freeze Property* modifier is designed for the typical situation where the number of particles in the input data remains constant. Then it is possible to copy the property values from one frame to all other frames for each particle without any ambiguity issues.

If some particles, which were present in the frozen frame, are removed from the simulation, the modifier is still able to copy the property values to the remaining particles. However, if new particles are added to the simulation after the frozen frame, the modifier will not know how to assign property values to them, and an error will occur.

If your simulation has a varying number of particles (e.g., due to dynamic particle creation), you can enable the following options:

Tolerate newly appearing particles
  This option allows the modifier to handle particles that were not present in the initial simulation frame but appear in later frames. When enabled, these newly appearing particles will not cause errors and will be assigned a default value for the frozen property or simply keep their dynamically varying values.

Select newly appearing particles
  When this option is enabled, the modifier will select particles that appear in the simulation after the initial frame. This can be useful for identifying and analyzing particles that were not part of the original dataset. You can then
  assign a special property value to these particles using the :ref:`particles.modifiers.compute_property` or :ref:`particles.modifiers.assign_color` modifiers, for example.

Example applications
""""""""""""""""""""

Fixing particle colors
======================

In the first image below, particles have been colored based on their current x-coordinates using the :ref:`particles.modifiers.color_coding` modifier at frame 0.

As particles move during the simulation, their dynamically computed colors are automatically updated, which may not be desired.

.. figure:: /images/modifiers/freeze_property_example1_initial.png
  :figwidth: 30%

  Initial frame: color coding of current x-coordinate

.. figure:: /images/modifiers/freeze_property_example1_without.png
  :figwidth: 30%

  Colors get updated as particles move

.. figure:: /images/modifiers/freeze_property_example1_with.png
  :figwidth: 30%

  With *Freeze Property*: colors remain fixed

To maintain the coloring created in frame 0, apply the *Freeze Property* modifier to the ``Color`` particle property. This will copy the color values from frame 0 to all other frames, effectively freezing them, while all other properties of the particles still vary dynamically.

Computing property variations
=============================

This modifier is also useful for analyzing changes of a particle property with respect to its initial value.

Suppose the simulation code has computed a local particle property named ``Potential Energy`` that varies at each frame, and
you want to monitor how each particle's potential energy changes over time by taking the difference
with respect to the initial values at frame 0.

To compute the differences (deltas):

  1. Apply the *Freeze Property* modifier to take a snapshot of the property ``Potential Energy`` at simulation frame 0.
  2. Set :guilabel:`Output property` to ``Initial Energy`` so the current potential energy values are not overwritten by the frozen ones.
     This allows you to work with both the frozen *and* the dynamically varying values in the next step.
  3. Use the :ref:`particles.modifiers.compute_property` modifier to create a third property calculated as ``PotentialEnergy - InitialEnergy``.

Marker particles and freezing selections
========================================

This modifier allows you to preserve the particle selection state, created in one frame of the simulation trajectory,
by applying the modifier to the ``Selection`` particle property. See also the tutorial :ref:`tutorials.marker_particles`.

.. seealso::

  :py:class:`ovito.modifiers.FreezePropertyModifier` (Python API)