.. _particles.modifiers.bond_angle_analysis:

Ackland-Jones analysis
----------------------

.. image:: /images/modifiers/bond_angle_analysis_panel.png
  :width: 30%
  :align: right

This modifier implements a method for identifying common crystalline and other structures based on an analysis
of the distribution of angles formed by the pairs of neighbors of a central atom. The method is known as the Ackland-Jones bond-angle method
[`Ackland and Jones, Phys. Rev. B 73, 054104 <http://link.aps.org/doi/10.1103/PhysRevB.73.054104>`__]. The algorithm assigns a structural type
to each particle having a local environment that matches one of the known structures (FCC, BCC, HCP, icosahedral).

.. caution::

  The Ackland-Jones method tends to produce a lot of false positive identifications when applied to strongly distorted or amorphous structures.
  Please consider using one of the more robust and well-defined identification methods available in OVITO instead, e.g. :ref:`particles.modifiers.polyhedral_template_matching`
  or :ref:`particles.modifiers.common_neighbor_analysis`. The Ackland-Jones modifier has only been implemented in OVITO for the sake of completeness,
  not because it is a good structure identification algorithm.

Modifier outputs
""""""""""""""""

The modifier outputs the classification results as a new particle property named ``Structure Type``.
This information allows you to subsequently select particles of a certain structural type, e.g. using the
:ref:`particles.modifiers.select_particle_type` modifier.
The structural type determined by the algorithm is encoded as an integer value:

  * 0 = Other, unknown coordination structure
  * 1 = FCC, face-centered cubic
  * 2 = HCP, hexagonal close-packed
  * 3 = BCC, body-centered cubic
  * 4 = ICO, icosahedral coordination

In addition, the modifier assigns colors to the particles (by setting the ``Color``
particle property) to indicate their computed structural type. The color representing each structural type
can be customized by double-clicking the corresponding entry in the table or, permanently, in the
:ref:`application settings dialog <application_settings.particles>`.

Furthermore, the modifier emits global attributes to the data pipeline reporting the total number of particles matching
each of the supported structural types. These attributes are named ``AcklandJones.counts.XXX``, where `XXX`
stands for the name of a structure. These analysis statistics may be exported using OVITO's :ref:`data export function <usage.export>`
or displayed as live information in the viewports using a :ref:`text label <viewport_layers.text_label>`.

.. note::

  The modifier needs to see the complete set of input particles to perform the analysis. It should therefore be placed at the
  beginning of the data pipeline, preceding any modifiers that delete some of the particles.

The option :guilabel:`Use only selected particles` restricts the analysis to the
currently selected particles. In this case, unselected particles will be ignored
(as if they did not exist) and are all assigned the structure type "Other".
This option is useful if you want to identify defects in a crystal type
not directly supported by the bond-angle analysis algorithm but having a sub-lattice that is supported.

Alternatives
""""""""""""

OVITO provides implementations of other structure identification methods, for instance the
:ref:`particles.modifiers.common_neighbor_analysis`,
:ref:`particles.modifiers.identify_diamond_structure` or
:ref:`particles.modifiers.polyhedral_template_matching` modifiers.
Furthermore, the :ref:`particles.modifiers.centrosymmetry` modifier can be used to detect defects in crystal lattices.

.. seealso::

  :py:class:`ovito.modifiers.AcklandJonesModifier` (Python API)
