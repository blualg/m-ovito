.. _particles.modifiers.structure_factor:

Structure Factor
----------------

This modifier calculated the structure factor :math:`S(k)` using two different approaches. The direct method calculates structure factors from summation in the k-space while the Debye method relies on the integration of the radial distribution function :math:`g(r)`. In both cases the structure factor is given as function of the magnitude of the wave vector :math:`|\vec{k}|` also denoted as :math:`k`. This wave vector is related to the commonly defined scattering vector :math:`q = k / 2\pi`.

The direct method provides better results especially for small :math:`k` values, however, it is much slower and can only be used for small simulation cells densely packed with not too many atoms. The Debye method on the other hand is much faster and can be applied to large simulation cells containing millions of atoms. This method suffers from `ringing artifacts <https://en.wikipedia.org/wiki/Ringing_artifacts>`__ due to the finite cutoff radius in the calculation of the radial distribution function.

This modifier can either calculate the total structure factor :math:`S(k)` or the X-ray structure factor :math:`S^X(k)`, which takes the atomic X-ray form factors into account. This modifier can also calculate partial :math:`S_{\alpha\beta}(k)` and Faber-Ziman partial :math:`A_{\alpha\beta}(k)` structure factors. These partial structure factors are normalized such that

  :math:`{\displaystyle S(k) = \sum ( 2 - \delta_{\alpha\beta} ) S_{\alpha\beta}(k) }`,

for a 2 component system this would equate to,

  :math:`{\displaystyle S(k) = S_{\alpha\alpha}(k) + 2 S_{\alpha\beta}(k) + S_{\beta\beta}(k) }`.

The following figure shows a comparison between both methods for both partial and total structure factor results.

.. image:: /images/modifiers/structure_factor_debye-direct-comparison.png
  :width: 80%
  :align: center

The following figure shows partial :math:`S_{\alpha\beta}(k)` and total structure factors :math:`S(k)` calculated using the `freud  <https://freud.readthedocs.io/en/latest/>`__ Python package and the implementation of Linus C. Erhard used in LE2022. The analyzed structure is taken from their dataset.

.. image:: /images/modifiers/structure_factor_method-comparison.png
  :width: 80%
  :align: center

Parameters
""""""""""

.. image:: /images/modifiers/structure_factor_command_panel.png
  :width: 50%
  :align: right

Mode
  Selects the operating mode: Direct or Debye.

Minimum k value
  Minimum k value, length of :math:`\vec{k}`, for which the structure factor is calculated.

Maximum k value
  Maximum k value, length of :math:`\vec{k}`, for which the structure factor is calculated. Larger k values increase the processing time of this modifier.

Number of histogram bins
  Number of bins in which the histogram is accumulated into.

Compute partial structure factors
  Compute both partial and Faber-Ziman partial structure factors

Use atomic form factors
  Use the atomic form factors :math:`f(k)` tabulated `here <https://lampz.tugraz.at/~hadley/ss1/crystaldiffraction/atomicformfactors/formfactors.php>`__ to calculate the X-ray structure factors :math:`S^X(k)`.

Use only selected particles
  Constrain the calculation to particles included in the selection.

Background
""""""""""

Direct Method
#############

Computationally the structure factor :math:`S(k)` is calculated from the scattering function :math:`F(k)`.

  :math:`{\displaystyle F(k) = \frac{1}{\sqrt{N}} \sum_{i=0}^{N} \exp \left( j \vec{k} \vec{r}_i \right)}`,

where :math:`N` is the number of particles, :math:`\vec{r}` is the particle position, and :math:`j` is the imaginary unit.

Subsequently, the structure factor can be calculated,

  :math:`{\displaystyle S(k) = \Re\left( F^{*}(k) F(k) \right)}`,

the :math:`^*` denotes the complex conjugate and :math:`\Re` is the real part of the complex number.

This method can be be extended to the calculation of partial structures factors of particle types :math:`\alpha` and :math:`\beta` by calculating :math:`S(k)` for this subset of particles.

Debye Method
############

The Debye method allows the calculation of the

  :math:`{\displaystyle S(k) = \frac{4 \pi \rho}{k} \int_0^{L/2} r \sin(kr) g(r) w(r) }`,

where

  :math:`{\displaystyle w(r) = \frac{ \sin(2 \pi r / L) }{ 2 \pi r / L } }`,

is applied to limit finite cutoff ringing artifacts. Here :math:`\rho` is the particle density, :math:`L` is the maximum simulation cell vector length, and :math:`g(r)` is the radial distribution function calculated using the :ref:`particles.modifiers.coordination_analysis` modifier.

Partial structure factors can be calculated from partial radial distribution functions.

Faber-Ziman partial structure factor
####################################

The partial structure factors :math:`S_{\alpha\beta}` above can be converted to the Faber-Ziman partial structure factor :math:`A_{\alpha\beta}` often mentioned in literature using

  :math:`{\displaystyle A_{\alpha\beta}(k) = \frac{ S_{\alpha\beta}(k) - x_\alpha \delta_{\alpha\beta} }{ x_\alpha x_\beta} + 1 }`,

where :math:`x` denotes the concentration of any species.

This figure shows a comparison between the partial structure factors and the Faber-Ziman partial structure factor. One can see the difference in normalization. The partial structure factors are normalized so that :math:`S_{\alpha\alpha}(k)` tend to the concentration of :math:`\alpha` while :math:`S_{\alpha\beta}(k)` tend towards 0 as :math:`k` goes to infinity.

The  Faber-Ziman partial structure factors on the other hand all approach 1 for large :math:`k`.

.. image:: /images/modifiers/structure_factor_psf-fz-psf-comparison-small.png
  :width: 80%
  :align: center


X-ray atomic form factors
#########################

For comparison with experiments, the X-ray atomic form factors :math:`f(k)` can be taken into account using this equation:

  :math:`{\displaystyle A^\mathrm{X}_{\alpha\beta}(k) = \frac{ f_\alpha(k) f_\beta(k) }{ \left[ \sum_i f_i(k) x_i \right] ^2 } A_{\alpha\beta}(k) }`.

The atomic form factors for each species are calculated following

   :math:`{ f(k) = c + \sum_{i=0}^4 a_i \exp \left( -b_i \left(\frac{k}{4 \pi}\right)^2 \right) }`,

with parameters taken from the `atomic form factors table from TU Graz <https://lampz.tugraz.at/~hadley/ss1/crystaldiffraction/atomicformfactors/formfactors.php>`__.

These Faber-Ziman structure factors :math:`A^\mathrm{X}_{\alpha\beta}(k)` can subsequently be converted into partial structure factor :math:`S^\mathrm{X}_{\alpha\beta}(k)` using the equation outlined above. Lastly, these partial structure factors can be combined back to the total structure factor :math:`S^\mathrm{X}`.

References and further reading
""""""""""""""""""""""""""""""

Documentation of the ``freud`` package that implements both the direct and Debye calculation of the structure factor:

- https://freud.readthedocs.io/en/latest/modules/diffraction.html#freud.diffraction.StaticStructureFactorDirect

- https://freud.readthedocs.io/en/latest/modules/diffraction.html#freud.diffraction.StaticStructureFactorDebye

Very helpful discussion about the normalization of :math:`S(k)` in ``freud`` with the developers

- https://github.com/glotzerlab/freud/issues/1313

Detailed description of the calculation and normalization of :math:`S(k)` and :math:`A(k)`

- Liu, H., & Paddison, S. J. (2016). Direct calculation of the X-ray structure factor of ionic liquids. In Physical Chemistry Chemical Physics (Vol. 18, Issue 16, pp. 11000–11007). Royal Society of Chemistry (RSC). https://doi.org/10.1039/c5cp06199g

We would like to acknowledge the help of `Linus C. Erhard <https://scholar.google.com/citations?user=1P2FElEAAAAJ&hl>`__ in the development and verification of this modifier.

- [Le2022] Erhard, L. C., Rohrer, J., Albe, K., & Deringer, V. L. (2022). A machine-learned interatomic potential for silica and its relation to empirical models. In npj Computational Materials (Vol. 8, Issue 1). Springer Science and Business Media LLC. https://doi.org/10.1038/s41524-022-00768-w

.. seealso::

  :py:class:`ovito.modifiers.StructureFactorModifier` (Python API)
