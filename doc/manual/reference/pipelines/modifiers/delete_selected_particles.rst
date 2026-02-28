.. _particles.modifiers.delete_selected:

Delete selected
---------------

.. image:: /images/modifiers/delete_selected_panel.png
  :width: 35%
  :align: right

This modifier deletes all currently selected data elements (particles, bonds), i.e., those
data elements whose ``Selection`` property is non-zero.

The modifier displays a list of object classes it can operate on. Classes currently not present in the modifier's input are grayed out.
If there are multiple objects of a class present in the modifier's input, you can optionally restrict the modifier to a particular one of them.
Otherwise, the modifier will process all objects of the selected class and delete their currently selected data elements.

.. seealso::

  :py:class:`ovito.modifiers.DeleteSelectedModifier` (Python API)
