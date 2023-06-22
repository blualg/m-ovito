.. _modifiers.shrink_wrap_box:

Shrink-wrap simulation box |ovito-pro|
--------------------------------------

This modifier changes the simulation cell to tightly fit the particle model.
It computes the axis-aligned bounding box of all particle coordinates (ignoring particle size)
and replaces the original simulation cell with the new geometry.

The modifier can be useful in situations where the existing simulation is either too large
or too small to fit the particle model. The main purpose, however, is to demonstrate
the implementation of a simple :ref:`user-defined modifier function in Python <particles.modifiers.python_script>`.
Click the :guilabel:`Edit script` button in the modifier panel to see (and play with) the :ref:`source code <example_shrink_wrap_box>`
of this modifier.

.. figure:: /images/modifiers/shrink_wrap_box_before.*
  :figwidth: 25%

  Input simulation cell

.. figure:: /images/modifiers/shrink_wrap_box_after.*
  :figwidth: 25%

  Output simulation cell

.. seealso::

  :ref:`Source code of this modifier function <example_shrink_wrap_box>`