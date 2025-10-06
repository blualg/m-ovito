.. _data_inspector.particles:

Particles
=========

.. image:: /images/data_inspector/particles_page.*
  :width: 50%
  :align: right

This page of the :ref:`data inspector <data_inspector>` shows all particles and their property values
as a data table. The leftmost column shows the indices of the particles, which start at 0 in OVITO.
The other columns display the :ref:`properties <usage.particle_properties>` associated with the particles.

You can dynamically filter the displayed list of particles by entering a Boolean expression in the input field above the table.
Consider, for example, the table shown in the screenshot: To selectively list only particles having a coordination
number of 11, you can enter the expression ``Coordination==11`` into the filter field.
Multiple criteria can be combined using logical *AND* and *OR* operators. The expression syntax is the same
as the one used by the :ref:`Expression selection <particles.modifiers.expression_select>` modifier.

The crosshair button activates a mouse input mode, which lets you pick individual particles in the viewports.
As you select particles in the viewports, the filter expression is automatically updated to show the properties of
the highlighted particles. Hold down the :kbd:`Ctrl` key (:kbd:`Command` key on macOS) to
select multiple particles. Click the crosshair button again or right-click in a viewport to deactivate the input mode.

The second button shows two more tables displaying the distances between pairs of particles and the angles formed by triplets of particles.
OVITO only computes the pair-wise distances for the first four particles in the particles list.
Therefore, you typically want to filter the particles list (either using the interactive selection method or a filter expression)
to pick a set of just 2, 3, or 4 particles for which to compute the inter-particle distances.
Note that periodic boundary conditions are not taken into account when pair-wise distances are calculated
using this function. If you are interested in *wrapped* distances, you should
:ref:`create bonds <particles.modifiers.create_bonds>` between the particles and measure the length of these
bonds instead.
