.. _particles.modifiers.expand_selection:

Expand selection
----------------

.. image:: /images/modifiers/expand_selection_panel.png
  :width: 35%
  :align: right

This modifier expands an existing particle selection by selecting additional particles that are neighbors
of already selected particles. You can choose between four different modes:

  1. **Cutoff:** A distance threshold can be specified to select all particles that are within range of an already selected particle.
  2. **Nearest:** Selects those particles that are among the *N* nearest neighbors of an already selected particle. The number *N* is adjustable.
  3. **Bonded:** Extends the selection to particles connected by a bond to at least one already selected particle.
  4. **Molecule:** Expands the selection to all particles that belong to the same molecule(s) as already selected particles. In other words, one selected atom in a molecule is sufficient to select the entire molecule.

Parameters
""""""""""

Cutoff distance
  A particle will be selected if it is within this distance of an already selected particle.

N
  The number of nearest neighbors to select around each already selected particle.
  The modifier sorts the neighbor list of already selected particles by ascending distance and selects the leading *N* entries
  from the neighbor list.

Number of iterations
  This parameter allows you to expand the selection in multiple recursive steps.
  For example, setting this parameter to a value of 2 will expand the current selection to the second shell of neighbors,
  i.e., as if the modifier had been applied twice.

.. seealso::

  :py:class:`ovito.modifiers.ExpandSelectionModifier` (Python API)