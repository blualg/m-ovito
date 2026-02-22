.. _particles.modifiers.vorotop_analysis:

VoroTop analysis
""""""""""""""""

.. image:: /images/modifiers/vorotop_analysis_panel.png
  :width: 35%
  :align: right

This modifier uses the Voronoi cell topology of a particle to characterize its local environment 
[`Lazar, Han, Srolovitz, PNAS 112:43 (2015) <http://dx.doi.org/10.1073/pnas.1505788112>`__].

The Voronoi cell of a particle is the region of space closer to it than to any other particle.
The topology of the Voronoi cell is the manner in which its faces are connected, and describes
the manner in which a particle's neighbors are arranged.  The topology of a Voronoi cell can be
completely described in a vector of integers called a *Weinberg vector* 
[`Weinberg, IEEE Trans. Circuit Theory 13:2 (1966) <http://dx.doi.org/10.1109/TCT.1966.1082573>`__].

This modifier requires loading a *filter*, which specifies structure types and associated
Weinberg vectors. Filters for several common structures can be obtained from the 
`VoroTop website <https://www.vorotop.org/download.html>`__.  The modifier calculates the Voronoi cell topology of each particle, uses the provided
filter to determine the structure type, and stores the results in the ``Structure Type`` particle property.  This allows the user to subsequently select particles
of a certain structural type, e.g. by using the :ref:`particles.modifiers.select_particle_type` modifier. The VoroTop
modifier requires access to the complete set of input particles to perform the analysis, and
should therefore be placed at the beginning of the processing pipeline, preceding any modifiers
that delete particles.

The VoroTop analysis method is well-suited for analyzing finite-temperature systems, including those heated to
their bulk melting temperatures.  This robust behavior relieves the need to quench a sample
(such as by energy minimization) prior to analysis.

Further information about the Voronoi topology approach for local structure analysis, as well
as additional filters, can be found on the website https://www.vorotop.org/

.. seealso::
  
  :py:class:`ovito.modifiers.VoroTopModifier` (Python API)