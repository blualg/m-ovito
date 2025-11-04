.. _particles.modifiers.correlation_function:

Spatial correlation function
----------------------------

.. image:: /images/modifiers/correlation_function_panel.png
  :width: 30%
  :align: right

This modifier calculates the spatial correlation function between two particle properties,
:math:`C(r) = \langle P_1(0) P_2(r) \rangle` where :math:`P_1` and :math:`P_2` are the two properties.

OVITO uses a fast Fourier transform (FFT) to compute the convolution. It then computes a radial average in reciprocal
and real space. This gives the correlation function up to half of the cell size. The modifier can additionally compute
the short-ranged part of the correlation function from a direct summation over neighbors.

For example, when both particle properties (:math:`P_1` and :math:`P_2`) are constant and unity for all particles in the system,
the modifier returns the pair distribution function. The reciprocal space representation is then the structure factor (see also :ref:`particles.modifiers.structure_factor` modifier).

Parameters
""""""""""

First property
  First particle property for which to compute the correlation (:math:`P_1`).

Second property
  Second particle property for which to compute the correlation (:math:`P_2`).
  If both particle properties are identical, the modifier computes the autocorrelation.

FFT grid spacing
  This property sets the approximate size of the FFT grid cell.
  The actual size is determined by the distance of the cell faces which must contain an integer number of grid cells.

Apply window function to non-periodic directions
  This property controls whether non-periodic directions have a Hann window applied to them.
  Applying a window function is necessary to remove spurious oscillations and power-law scaling of
  the (implicit) rectangular window of the non-periodic domain.

Direct summation
  If enabled, the real-space correlation plot will show the result of a direct calculation of the correlation function,
  obtaining by summing over neighbors. This short-ranged part of the correlation function is displayed as a red line.

Neighbor cutoff radius
  This property determines the cutoff of the direct calculation of the real-space correlation function.

Number of neighbor bins
  This property sets the number of bins for the direct calculation of the real-space correlation function.

Type of plot
  * :guilabel:`Value correlation` computes :math:`C(r) = \langle P_1(0) P_2(r) \rangle`.
  * :guilabel:`Difference correlation` computes :math:`\langle (P_1(0) - P_2(r))^2 \rangle/2 = (\langle P_1^2 \rangle + \langle P_2^2 \rangle ) / 2 - C(r)`.

Normalize by RDF
  Divide the value correlation function :math:`C(r)` by the radial distribution function (RDF).
  If difference correlation is selected, then :math:`C(r)` is divided by the RDF before the difference correlation is computed.

Normalize by covariance
  Divide the final value or difference correlation function by the covariance.

**Acknowledgment**

The code for this modifier was contributed to OVITO by `Lars Pastewka <https://scholar.google.de/citations?user=9oWKrs4AAAAJ>`__.

.. seealso::

  :py:class:`ovito.modifiers.SpatialCorrelationFunctionModifier` (Python API)