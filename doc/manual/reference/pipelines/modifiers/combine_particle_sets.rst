.. _particles.modifiers.combine_particle_sets:

Combine datasets
----------------

.. image:: /images/modifiers/combine_datasets_panel.png
  :width: 35%
  :align: right

This modifier loads a set of particles from a second file and merges it into the current dataset
to build a system with both sets of particles combined.

If both the primary and the secondary data sources contain multiple simulation frames, then the modifier combines
corresponding frames from each source. For the secondary data source, you may have to
explicitly specify a wildcard filename pattern to
make OVITO load more than just one frame from the simulation sequence to be merged.

New unique IDs are assigned to the added particles and molecules from the merged file to ensure that they
do not collide with any of the existing IDs in the current dataset.

.. note::

  The simulation cell loaded from the secondary input file is ignored. The modifier does not replace or extend the
  existing simulation box of the primary dataset. If needed, you can use e.g. the :ref:`particles.modifiers.affine_transformation` modifier to expand the
  simulation cell and accommodate all particles of the combined system.

.. seealso::

  :py:class:`ovito.modifiers.CombineDatasetsModifier` (Python API)