.. _visual_elements.bonds:

Bonds
-----

.. image:: /images/visual_elements/bonds_panel.jpg
  :width: 30%
  :align: right

This :ref:`visual element <visual_elements>` controls how bonds between pairs of particles are rendered as cylindrical connections.

:ref:`Bonds <scene_objects.bonds>` are either loaded from the simulation file as part of the model, or they can be created within OVITO by the
:ref:`particles.modifiers.create_bonds` modifier. Alternatively, the :ref:`particles.modifiers.voronoi_analysis` modifier
can generate bonds between nearest neighbor particles.

Parameters
""""""""""

Appearance
''''''''''

These settings control the geometric representation of bonds:

Bond width
  Controls the uniform display diameter of bond cylinders (default: 0.4 distance units).
  This value is used for all bonds unless overridden by the per-bond ``Width`` property.
  The width is specified in simulation units of length.

Flat shading
  When enabled, bonds are rendered with flat shading to create the look of thick lines connecting particles.
  When disabled (default), bonds use smooth shading for a more realistic cylindrical appearance.

Coloring
''''''''

The color of bonds is determined through a priority hierarchy. The coloring mode selector controls
which method is used when explicit per-bond colors are not present:

By bond type
  Colors bonds according to their ``Bond Type`` property value. Each bond type can have a unique color
  assigned through the :ref:`particles.modifiers.edit_types` modifier.

By particle
  Bonds adopt colors from the particles they connect. Each bond is rendered as two half-cylinders,
  with each half colored according to its connected particle.

Uniform color
  All bonds are rendered using the same color specified by the color selector (default: gray).

All these options are disabled when the ``Color`` per-bond property is defined, which always takes precedence.

.. note::

   To set explicit colors for individual bonds, use the :ref:`particles.modifiers.compute_property`
   or :ref:`particles.modifiers.color_coding` modifiers to set the ``Color`` bond property.
   Once this property exists, it takes precedence over all coloring mode options.

The visualization can additionally be affected by certain properties of the bonds themselves, listed in the following table.

.. table::
  :widths: 20 15 65

  ================================== ======================= ==============================================================================
  Bond property                      Data type               Description
  ================================== ======================= ==============================================================================
  ``Color``                          Real (R,G,B)            Explicit RGB color for individual bonds. Red, green, and blue components must be in the range [0,1].
  ``Width``                          Real                    Controls the display diameter of individual bonds.
  ``Transparency``                   Real                    Controls the transparency of individual bonds. Must be in the range [0,1].
  ``Selection``                      Integer                 Marks currently selected bonds (1 = selected, 0 = unselected). Selected bonds are highlighted in red (only in interactive viewports but not in rendered images).
  ================================== ======================= ==============================================================================

To see the list of bond properties present in your dataset, open the :ref:`data inspector panel <data_inspector.bonds>`.

.. seealso::

  :py:class:`ovito.vis.BondsVis` (Python API)