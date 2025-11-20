.. _particles.modifiers.bond_order:


Assign bond order |ovito-pro|
------------------------------

This modifier assigns a bond order to each bond in the system. The method mostly follows the methodology described in 

    | `S. Artemova et al. <https://doi.org/10.1002/jcc.24309>`__
    | `Automatic Molecular Structure Perception for the Universal Force Field <https://doi.org/10.1002/jcc.24309>`__
    | `Journal of Computational Chemistry, 37, 1191-1205 (2016) <https://doi.org/10.1002/jcc.24309>`__
    | `doi:10.1002/jcc.24309`

For best results, initial bonds can be created using the "Covalent radii" mode of the :ref:`particles.modifiers.create_bonds` modifier
as both algorithms are based on the same paper. 

The modifier takes a bond topology as an input and tries to assign bond orders to each bond. Here, the topology remains unchanged, no 
bonds will be added or removed in the process. To find the optimum bond order assignment, different bond configurations are explored 
and scored based on a cost table. This cost is minimized to find the optimal bond order assignment. This atomic penalty score 
table can be found in the original publication, as table 1.

As the optimization problem is not trivial, the modifier splits larger molecules into smaller fragments or chunks which are subsequently
optimized separately. The maximum chunk size can be set using the :guilabel:`Maximum chunk size` parameter. Larger chunk sizes
lead to improved scores and therefore better results at the cost of increased computation time. This can be seen in Figure 3 of the 
the paper. To speed up the computation smaller chunk size might be used initial and the chunk size is increased automatically until
the comutation is converged or the maximum size is reached. The final bond order assignment score is stored in a 
:ref:`Global attributes <usage.global_attributes>` named ``BondOrder.penalty_score``. A value of 0 indicates that the assignment
was successful was successful. Higher scores might also be correct especially if the molecue has broken bonds or charged atoms.

.. attention::

    The modifier requires explicit H atoms to be present in the system. It cannot work with implicit hydrogens often found in pdb files. 
    Currently charges are ignored using during the bond order assignment.

Aromatic perception
"""""""""""""""""""

The modifier identifies aromatic 5- and 6-rings. Patterns making up the rings are referenced agains a database of known patterns for
which the number of electrons contributed into the ring are tabulated. If the total number of electrons in the ring fulfills the Hückel 
rule (:math:`N_{\text{e}^-} = 4n+2`) the ring and its atoms are tagged as aromatic. 

These patterns are shown in Figure 6 in the paper. We have made 2 adjustments to improve the aromatic perception.

    1. Atoms in the current ring, that are already part of an adjacent aromatic ring are assumed to contributed 1 electron into the current ring.
    2. Rings that contain 4 fold coordinate atoms which are presumed to be :math:`\text{sp}^3` hybridized cannot be aromatic.

Functional groups
"""""""""""""""""

In this process, the modifier needs to idenfity "Amide", "Nitro", and "Carboxylate" functional groups. These functional groups are
are output as results of the modifier and can be accessed in the "Functional Group" properties stored in both :ref:`scene_objects.bonds` and 
:ref:`scene_objects.particles` objects. Numeric id for teh different functional groups are assigned as follows:

============= =========================================================
Numeric id    Functional group
============= =========================================================
0             No known functional group
1             Amide
2             Aromatic
3             Carboxylate
4             Nitro
============= =========================================================

Fractional bond order
"""""""""""""""""""""

By default the assigned bond orders are integer values, this means that aromatic rings are for example assigned alternating bond orders
of 1 and 2. This can be changed by setting the :guilabel:`Fractional bond order` option to ``True``. In this case the bond order is
set to 1.5 for all bonds in the aromatic ring. Similar fractional bond orders can be applied to the other functional groups.