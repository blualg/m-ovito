.. _particles.modifiers.invert_selection:

Invert selection
----------------

.. image:: /images/modifiers/invert_selection_panel.png
  :width: 35%
  :align: right

This modifier inverts the current selection of elements by flipping the values of the ``Selection`` property.
The :guilabel:`Operate on` field selects the kind of elements (particles, bonds, etc.) the modifier should act on.
See the introduction to :ref:`particle properties <usage.particle_properties>` to learn more
about the role of the special ``Selection`` property.

.. seealso::

  :py:class:`ovito.modifiers.InvertSelectionModifier` (Python API)