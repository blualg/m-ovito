.. _data_inspector.particles:

Particles
=========

.. image:: /images/data_inspector/particles_page.*
  :width: 50%
  :align: right

This page of the :ref:`data inspector <data_inspector>` shows all particles and their property values
as a data table. The leftmost column shows the index number of particles, which starts at 0 in OVITO.
Each following column displays the values of a :ref:`particle property <usage.particle_properties>`.

You can dynamically filter the displayed list of particles by entering a Boolean expression in the input field at the top of the table.
Consider, for example, the table shown in the screenshot: To selectively list only those particles with a coordination
number of 11, you could enter the expression ``Coordination==11`` into the filter field.
Multiple criteria can be combined using logical AND and OR operators. The expression syntax is the same
used by the :ref:`Expression selection <particles.modifiers.expression_select>` modifier.
To reset the filter and show the complete list of particles again, use the :guilabel:`X` button.

The crosshair button activates a mouse input mode, which lets you pick individual particles in the viewports.
As you select particles in the viewports, the filter expression is automatically updated to show the properties of
the highlighted particles. Hold down the :kbd:`Ctrl` key (:kbd:`Command` key on macOS) to
select multiple particles. Click the crosshair button again or right-click in a viewport to deactivate the input mode.

The middle tool button shows a second table displaying the distances between particles.
Here, OVITO reports the pair-wise distances for the first four particles in the particle list.
Typically you want to filter the particle list (either using the interactive method or a filter expression)
to define a set of 2, 3, or 4 particles for which to compute the inter-particle distances.
Note that periodic boundary conditions are not taken into account when pair-wise distances are calculated
using this function. If you are interested in *wrapped* distances, you should
:ref:`create bonds <particles.modifiers.create_bonds>` between the particles and measure the length of these
bonds instead.

