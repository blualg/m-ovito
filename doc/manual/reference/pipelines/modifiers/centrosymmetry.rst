.. _particles.modifiers.centrosymmetry:

Centrosymmetry parameter
------------------------

.. image:: /images/modifiers/centrosymmetry_panel.*
  :width: 30%
  :align: right
  
This modifier calculates the *centrosymmetry parameter* (CSP) [`Kelchner, Plimpton, Hamilton, Phys. Rev. B, 58, 11085 (1998) <http://link.aps.org/doi/10.1103/PhysRevB.58.11085>`__] for each particle.
In solid-state systems, the centrosymmetry parameter is a useful measure of the local
lattice disorder around an atom and can be used to characterize whether the atom is
part of a perfect lattice, a local defect (e.g. a dislocation or stacking fault), or located at a surface.

Definition
""""""""""

The CSP value :math:`p_{\mathrm{CSP}}` of an atom having :math:`N` nearest neighbors (:math:`N = 12` for face-centered cubic, :math:`N = 8` for body-centered cubic lattices)
is given by

  :math:`p_{\mathrm{CSP}} = \sum_{i=1}^{N/2}{|\mathbf{r}_i + \mathbf{r}_{i+N/2}|^2}`,

where :math:`\mathbf{r}_i` and :math:`\mathbf{r}_{i+N/2}` are two neighbor vectors from the central atom to a pair of opposite neighbor atoms.
For lattice sites in an ideal centrosymmetric crystal, the contributions of all neighbor pairs in this formula will cancel, and
the resulting CSP value will hence be zero. Atomic sites within a defective crystal region, in contrast, typically have a disturbed, non-centrosymmetric
neighborhood. In this case, the CSP becomes positive. Using an appropriate threshold, to allow for small perturbations due to thermal displacements and elastic strains,
the CSP can be used as an order parameter to filter out atoms that are part of crystal defects.

The calculated atomic CSP values are stored in the ``Centrosymmetry`` output particle property by the modifier.
A histogram of the CSP values of the entire particle system is displayed in the modifier panel.

You can use the :ref:`particles.modifiers.color_coding` modifier to color atoms based on their CSP value
or use the :ref:`particles.modifiers.expression_select` modifier to select atoms having a CSP value below some threshold.
These undisturbed atoms can then be hidden to reveal crystal defect atoms by using the :ref:`particles.modifiers.delete_selected_particles` modifier.

Number of neighbors
"""""""""""""""""""

This parameter specifies the number of nearest neighbors that should be taken into account when computing the centrosymmetry value for an atom.
This parameter value should match the ideal number of nearest neighbors in the crystal lattice at hand (12 in fcc crystals; 8 in bcc). More generally, it must be a positive, even integer.

CSP algorithms
""""""""""""""

The modifier supports two modes of operation:

Conventional CSP
  This mode uses the same `algorithm as LAMMPS <https://docs.lammps.org/compute_centro_atom.html>`__ for calculating the centrosymmetry parameter.
  Weights are calculated between all :math:`N (N - 1) / 2` pairs of neighbor atoms, and the CSP is calculated as the summation of the :math:`N / 2` lowest weights.

Minimum-weight matching CSP
  The conventional CSP algorithm performs well on highly centrosymmetric structures. In acentrosymmetric structures, however, it often assigns similar CSP values to very different structures, which results
  from the "greedy" selection of neighbor pair weights. The *minimum-weight matching CSP* [`Larsen <https://arxiv.org/abs/2003.08879>`__]
  ensures that neighbor relationships are reciprocal, which results in a better separation of CSP values between e.g., HCP atoms and surface defect atoms.
  This algorithm is more computationally expensive.

The option :guilabel:`Use only selected particles` restricts the analysis to the
subset of currently selected particles only. Unselected particles will be ignored in the computation
of the centrosymmetry values of selected particles (as if they did not exist), and their own
centrosymmetry values will be set to zero. 
This option is useful if you want to analyze a sub-lattice made only of atoms of a certain type.

.. note::
  
  The modifier needs to see the complete set of particles to perform the computation. It should therefore be placed at the
  beginning of the data pipeline, preceding any modifiers that delete particles.


.. seealso::

  :py:class:`ovito.modifiers.CentroSymmetryModifier` (Python API)