.. _howto.transparent_particles:

Semi-transparent particles
==========================

.. image:: /images/howtos/semitransparent_particles.*
   :width: 40%
   :align: right

Particles can be made semi-transparent by setting their ``Transparency`` :ref:`particle property <usage.particle_properties.special>`.
A value of 0.0 lets a particle appear fully opaque (the default), while values in the range 0.0 -- 1.0 render a particle translucent.

The easiest way to set the ``Transparency`` property to some value is by inserting the :ref:`particles.modifiers.compute_property` modifier into the data pipeline.
Simply enter the desired transparency value into the expression field and the :ref:`particles.modifiers.compute_property`
modifier will assign that value to all particles.

Setting the transparency selectively
""""""""""""""""""""""""""""""""""""

You may want to make only a subset of the particles semi-transparent. To do this, first select the particles to be made semi-transparent,
then apply the :ref:`particles.modifiers.compute_property` modifier. Activate its :guilabel:`Compute only for selected particles` option to restrict the assignment of the
new ``Transparency`` value only to the currently selected particles. Unselected particles will keep their transparency values (0.0 by default).

In the example picture on this page, the :ref:`particles.modifiers.compute_property`
modifier was used to set the ``Transparency`` property based on a simple math formula: ``ReducedPosition.X``.
Thus, instead of assigning a uniform transparency value to all particles, each particle's value depends on its local X coordinate.

Semi-transparent bonds
""""""""""""""""""""""

The :ref:`particles.modifiers.compute_property` modifier can also be used to set the transparency of bonds in a similar way as described above for particles.