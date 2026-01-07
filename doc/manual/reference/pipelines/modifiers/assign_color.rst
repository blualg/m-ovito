.. _particles.modifiers.assign_color:

Assign color
------------

.. image:: /images/modifiers/assign_color_panel.png
  :width: 30%
  :align: right

This modifier assigns a uniform color to all selected elements (particles, bonds, etc.) by setting their
``Color`` property. Which elements are currently selected is determined by the value of their
``Selection`` property. If no selection is defined, i.e. if the ``Selection`` property
does not exist, the color is assigned to all elements by the modifier.

The field :guilabel:`Operate on` selects the type of elements that get assigned the color:

.. table::
  :widths: auto

  ================ =================================================================================
  Operate on       Description
  ================ =================================================================================
  Particles        Selected particles will be colored by setting their ``Color`` property.
  Bonds            Selected bonds will be colored by setting their ``Color`` property.
  Particle vectors The :ref:`vector glyphs <visual_elements.vectors>` of selected particles will be colored by setting their ``Vector Color`` property.
  Lines            Selected line segments will be colored by setting their ``Color`` property.
  Mesh faces       Selected facets of a :ref:`surface mesh <scene_objects.surface_mesh>` will be colored by setting their ``Color`` property.
  Mesh vertices    Selected vertices of a :ref:`surface mesh <scene_objects.surface_mesh>` will be colored by setting their ``Color`` property.
  ================ =================================================================================

Note that OVITO uses a red color to highlight selected particles in the interactive viewports.
Since this accentuation color would mask the actual particle color assigned by this modifier, the modifier clears the current selection
by default (by completely removing the ``Selection`` property). If you would like to preserve the particle selection
so that it remains available to subsequent modifiers in the data pipeline, you can request the modifier to not
delete the ``Selection`` property by activating the option :guilabel:`Keep selection`.

.. seealso::

  :py:class:`ovito.modifiers.AssignColorModifier` (Python API)