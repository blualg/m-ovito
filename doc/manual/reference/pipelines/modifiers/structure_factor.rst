.. _particles.modifiers.structure_factor:

Structure factor |ovito-pro|
----------------------------

.. image:: /images/modifiers/structure_factor_command_panel.png
  :width: 50%
  :align: right

.. versionadded:: 3.13.0

This modifier calculates the structure factor :math:`S(k)` of a system of particles, i.e., a virtual diffraction pattern.
It supports two different calculation methods: The **direct method** performs a summation in k-space
while the **Debye method** relies on the integration of the radial distribution function :math:`g(r)`.
In both cases the resulting structure factor is provided as a function of the magnitude of the wave vector, :math:`k`.
It is related to the commonly defined scattering vector by :math:`q = k / 2\pi`.

The direct method provides better results especially for small :math:`k` values, however, it is much slower and can only be used for
small simulation cells densely packed with not too many atoms. The Debye method, on the other hand, is much faster and can be applied to
large simulation cells containing millions of atoms. This method suffers from `ringing artifacts <https://en.wikipedia.org/wiki/Ringing_artifacts>`__ due to
the finite cutoff radius in the calculation of the radial distribution function.

The modifier can either calculate the **total structure factor**, :math:`S(k)`, or the **X-ray structure factor**, :math:`S^X(k)`, which takes the atomic
X-ray form factors into account. It can also calculate **partial structure factors**, :math:`S_{\alpha\beta}(k)`, and Faber-Ziman partial factors, :math:`A_{\alpha\beta}(k)`.
The partial structure factors are normalized such that

  :math:`{\displaystyle S(k) = \sum ( 2 - \delta_{\alpha\beta} ) S_{\alpha\beta}(k) }`.

For a 2-component system this equates to the following decomposition of the total structure factor into partials :math:`S_{\alpha\alpha}`, :math:`S_{\alpha\beta}`, and :math:`S_{\beta\beta}`:

  :math:`{\displaystyle S(k) = S_{\alpha\alpha}(k) + 2 S_{\alpha\beta}(k) + S_{\beta\beta}(k) }`.

The following figure shows a comparison between the two supported calculation methods, for total and partial structure factor results:

.. image:: /images/modifiers/structure_factor_debye-direct-comparison.png
  :width: 70%
  :align: center

The following figure shows total :math:`S(k)` and partial :math:`S_{\alpha\beta}(k)` structure factors calculated using this OVITO function (direct method),
the `freud  <https://freud.readthedocs.io/en/latest/>`__ Python package (direct method), and an implementation from :ref:`[LE2022] <particles.modifiers.structure_factor.references>` (Debye method).
The analyzed input structure was taken from the same paper:

.. image:: /images/modifiers/structure_factor_method-comparison.png
  :width: 70%
  :align: center

.. _particles.modifiers.structure_factor.parameters:

Parameters
""""""""""

Mode
  Selects the calculation method: Direct or Debye.

Minimum k value
  Minimum length of :math:`\vec{k}` for which the structure factor is calculated.

Maximum k value
  Maximum length of :math:`\vec{k}` for which the structure factor is calculated. Larger :math:`k` values increase the processing time of the modifier.

Number of histogram bins
  Controls the resolution of the resulting tabulated structure factor function.

Compute partial structure factors
  If enabled, computes both partial and Faber-Ziman partial structure factors.

Use atomic form factors
  Use the atomic form factors :math:`f(k)` tabulated `here <https://lampz.tugraz.at/~hadley/ss1/crystaldiffraction/atomicformfactors/formfactors.php>`__ to calculate the X-ray structure factors :math:`S^X(k)`.
  For this to work, the particle types in OVITO must have names matching the atomic species in the table.

Use only selected particles
  Constrain the calculation to atoms included in the current selection set. Unselected atoms will be treated as if they were not present in the system.

Background
""""""""""

.. _particles.modifiers.structure_factor.direct_method:

Direct method
#############

Computationally, the structure factor :math:`S(k)` is calculated from the scattering function :math:`F(k)`, defined as

  :math:`{\displaystyle F(k) = \frac{1}{\sqrt{N}} \sum_{i=0}^{N} \exp \left( j \vec{k} \vec{r}_i \right)}`,

where :math:`N` is the number of particles, :math:`\vec{r}` is the particle position, and :math:`j` is the imaginary unit.

Subsequently, the structure factor can be obtained from the scattering function:

  :math:`{\displaystyle S(k) = \Re\left( F^{*}(k) F(k) \right)}`.

The :math:`^*` denotes the complex conjugate and :math:`\Re` is the real part of the complex number.

This method can be be extended to the calculation of the *partial* structure factors of two particle types :math:`\alpha` and :math:`\beta` by
calculating :math:`S(k)` for this subset of particles only.

.. _particles.modifiers.structure_factor.debye_method:

Debye method
############

The Debye method uses

  :math:`{\displaystyle S(k) = \frac{4 \pi \rho}{k} \int_0^{L/2} r \sin(kr) g(r) w(r) }`,

where

  :math:`{\displaystyle w(r) = \frac{ \sin(2 \pi r / L) }{ 2 \pi r / L } }`

is applied to reduce finite-cutoff ringing artifacts. Here, :math:`\rho` is the particle density,
:math:`L` the maximum simulation cell vector length, and :math:`g(r)` the radial distribution function (RDF) internally calculated
using the :ref:`particles.modifiers.coordination_analysis` function of OVITO.

Partial structure factors are calculated from partial radial distribution functions.

.. _particles.modifiers.structure_factor.faber_ziman:

Faber-Ziman partial structure factor
####################################

The partial structure factor :math:`S_{\alpha\beta}` discussed above can be converted to the Faber-Ziman partial structure factor :math:`A_{\alpha\beta}` often mentioned in the literature:

  :math:`{\displaystyle A_{\alpha\beta}(k) = \frac{ S_{\alpha\beta}(k) - x_\alpha \delta_{\alpha\beta} }{ x_\alpha x_\beta} + 1 }`,

where :math:`x` denotes the concentration of either species.

The figure below shows a comparison between the partial structure factors and the Faber-Ziman partial structure factors. One can see the different normalizations.
The partial structure factors are normalized so that the :math:`S_{\alpha\alpha}(k)` tend to the concentration of :math:`\alpha` while the :math:`S_{\alpha\beta}(k)`
tend towards 0 as :math:`k` goes to infinity.

The Faber-Ziman partial structure factors, on the other hand, all approach 1 for large :math:`k`.

.. image:: /images/modifiers/structure_factor_psf-fz-psf-comparison-small.png
  :width: 80%
  :align: center

.. _particles.modifiers.structure_factor.form_factors:

X-ray atomic form factors
#########################

For comparison with experiments, the X-ray atomic form factors :math:`f(k)` can be taken into account using this equation:

  :math:`{\displaystyle A^\mathrm{X}_{\alpha\beta}(k) = \frac{ f_\alpha(k) f_\beta(k) }{ \left[ \sum_i f_i(k) x_i \right] ^2 } A_{\alpha\beta}(k) }`.

The atomic form factors for each species are calculated following

   :math:`{ f(k) = c + \sum_{i=0}^4 a_i \exp \left( -b_i \left(\frac{k}{4 \pi}\right)^2 \right) }`,

with parameters taken from the `atomic form factors table from TU Graz <https://lampz.tugraz.at/~hadley/ss1/crystaldiffraction/atomicformfactors/formfactors.php>`__.

These Faber-Ziman structure factors :math:`A^\mathrm{X}_{\alpha\beta}(k)` can subsequently be converted into partial structure factor :math:`S^\mathrm{X}_{\alpha\beta}(k)` using
the equation outlined above. Lastly, these partial structure factors can be combined to obtain the total structure factor :math:`S^\mathrm{X}`.

.. _particles.modifiers.structure_factor.references:

References and further reading
""""""""""""""""""""""""""""""

Documentation links to the ``freud`` Python package, which also implements both calculation methods:

- https://freud.readthedocs.io/en/latest/modules/diffraction.html#freud.diffraction.StaticStructureFactorDirect

- https://freud.readthedocs.io/en/latest/modules/diffraction.html#freud.diffraction.StaticStructureFactorDebye

Discussion about the normalization of :math:`S(k)` with the `freud` developers:

- https://github.com/glotzerlab/freud/issues/1313

Detailed description of the calculation and normalization of :math:`S(k)` and :math:`A(k)`:

- Liu, H., & Paddison, S. J. (2016). Direct calculation of the X-ray structure factor of ionic liquids. In Physical Chemistry Chemical Physics (Vol. 18, Issue 16, pp. 11000-11007). Royal Society of Chemistry (RSC). https://doi.org/10.1039/c5cp06199g

We acknowledge the helpful contributions of `Linus C. Erhard <https://scholar.google.com/citations?user=1P2FElEAAAAJ&hl>`__ to the development and verification of this OVITO function. See also:

- [LE2022] Erhard, L. C., Rohrer, J., Albe, K., & Deringer, V. L. (2022). A machine-learned interatomic potential for silica and its relation to empirical models. In npj Computational Materials (Vol. 8, Issue 1). Springer Science and Business Media LLC. https://doi.org/10.1038/s41524-022-00768-w

.. seealso::

  :py:class:`ovito.modifiers.StructureFactorModifier` (Python API)
