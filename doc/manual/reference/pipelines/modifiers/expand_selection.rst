.. _particles.modifiers.expand_selection:

Expand selection
----------------

.. image:: /images/modifiers/expand_selection_panel.png
  :width: 35%
  :align: right

This modifier expands an existing particle selection by selecting particles that are neighbors
of already selected particles. You can choose between three different modes:

  1. A cutoff radius can be specified to select particles that are within a specific range of already selected particles.
  2. The modifier can select particles that are among the *N* nearest neighbors of an already selected particle. The number *N* is adjustable.
  3. The modifier can expand the selection to particles that are connected by a bond to at least one particle that is already selected.

Parameters
""""""""""

Cutoff distance
  A particle will be selected if it is within this range of an already selected particle.

N
  The number of nearest neighbors to select around each already selected particle.
  The modifier will sort the list of neighbors of an already selected particles by ascending distance and selects the leading *N* entries
  from the list.

Number of iterations
  This parameter allows you to expand the selection in multiple recursive steps.
  For example, setting this parameter to a value of 2 will expand the current selection to the second shell of neighbors,
  as if the modifier had been applied twice.

.. seealso::

  :py:class:`ovito.modifiers.ExpandSelectionModifier` (Python API)