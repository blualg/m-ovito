.. _particles.modifiers.bond_order:

Bond order |ovito-pro|
----------------------

.. image:: /images/modifiers/bond_order_panel.png
  :width: 35%
  :align: right

.. versionadded:: 3.15.0

This modifier assigns a `bond order <https://en.wikipedia.org/wiki/Bond_order>`__ to each bond in a molecular structure.
The algorithm mostly follows the methodology described in

    | `S. Artemova et al. <https://doi.org/10.1002/jcc.24309>`__
    | `Automatic Molecular Structure Perception for the Universal Force Field <https://doi.org/10.1002/jcc.24309>`__
    | `Journal of Computational Chemistry, 37, 1191-1205 (2016) <https://doi.org/10.1002/jcc.24309>`__
    | `doi:10.1002/jcc.24309 <https://doi.org/10.1002/jcc.24309>`__

As input, the algorithm requires an existing bond topology.
For best results, initial bonds should be created using the :guilabel:`Covalent radii` mode of OVITO's :ref:`particles.modifiers.create_bonds` modifier,
as both algorithms (bond creation and bond order assignment) are based on the same paper.

.. image:: /images/modifiers/bond_order_example_double_bonds.png
  :width: 30%
  :align: right

The modifier takes a bond topology as an input and determines a bond order value for each bond (output :ref:`bond property <scene_objects.bonds.properties>` ``Bond Order``). The bond topology remains unchanged, i.e., no
bonds will be added or removed in the process. OVITO will render bonds with a bond order of 1 as single cylinders, bonds with a bond order of 2 as double cylinders, and so on.

To find the optimum bond order assignment throughout the molecular structure, different possible configurations are explored
and scored based on a cost table. This atomic penalty score table is defined in the original publication (Table 1).
As the global optimization problem is not trivial, the modifier splits larger molecules into smaller fragments or chunks that are subsequently
optimized separately. The maximum fragment size can be set using the :guilabel:`Molecular fragment size limit` parameter. Larger fragment sizes
lead to improved penalty scores and therefore better results at the cost of increased computation time. This effect can be seen in Figure 3 of the
paper. The computation always starts at a fragment size limit of 3, which is then incremented until
the computation fully converges or the user-specified maximum size is reached. The final bond order assignment score is stored as a
:ref:`global attribute <usage.global_attributes>` named ``BondOrder.PenaltyScore``. A value of 0 indicates that the assignment
was perfect everywhere. Non-zero penalty scores might still be correct, especially if the molecule has broken bonds or charged atoms.

.. attention::

    The modifier requires explicit hydrogen atoms to be present in the system. It cannot work with implicit hydrogens often found in PDB files.
    Charges are ignored by the bond order assignment algorithm.

.. caution::

    The bond order computation is a non-trivial optimization problem. Especially for highly interconnected systems, such as graphene sheets,
    carbon nanotubes, or fullerenes, the computation can take extremely long times. Reducing the fragment size limit can help address this, but there will
    always be systems where no solution can be found in a reasonable time.

Aromatic perception
"""""""""""""""""""

The modifier identifies aromatic 5- and 6-rings. Such rings are referenced against a database of known patterns for
which the number of electrons contributed to the ring is tabulated. If the total number of electrons in the ring fulfills the Hückel
rule (:math:`N_{\text{e}^-} = 4n+2`), the ring and its atoms are tagged as aromatic.

These patterns are shown in Figure 6 in the paper. We have made two adjustments in OVITO to improve the aromatic perception:

    1. Atoms that are already part of an adjacent aromatic ring are assumed to contribute one electron into the current ring.
    2. Rings that contain 4-fold coordinated atoms, which are presumed to be :math:`\text{sp}^3` hybridized, cannot be aromatic.

Functional groups
"""""""""""""""""

.. image:: /images/modifiers/bond_order_functional_groups.png
  :width: 35%
  :align: right

The modifier also identifies functional groups "Amide", "Nitro", and "Carboxylate". This information
is output as "Functional Group" properties of :ref:`scene_objects.bonds` and
:ref:`scene_objects.particles`. Numeric type IDs represent the different functional groups:

============= =========================================================
Numeric ID    Functional group
============= =========================================================
0             No known functional group
1             Amide
2             Aromatic
3             Carboxylate
4             Nitro
============= =========================================================

.. _particles.modifiers.bond_order.fractional:

Fractional bond orders
""""""""""""""""""""""

.. image:: /images/modifiers/bond_order_example_fractional_bonds.png
  :width: 30%
  :align: right

By default, bond orders are all integer values, which means that the bonds in aromatic rings are assigned alternating bond orders
of 1 and 2, for example. Activating the :guilabel:`Redistribute bond orders` option adjusts the bond orders of known functional groups to fractional values.
This redistributes the original integer bond orders such that, for example, all bonds in an aromatic ring will have a uniform bond order of 1.5.
OVITO will visualize such "fractional bonds" as dashed cylinders instead of solid ones. The representation can be customized in the settings
of the :ref:`visual_elements.bonds` visual element.

.. seealso::

  :py:class:`ovito.modifiers.BondOrderModifier` (Python API)