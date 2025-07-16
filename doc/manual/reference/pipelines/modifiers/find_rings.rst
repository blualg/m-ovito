.. _particles.modifiers.find_rings:

Find rings |ovito-pro|
----------------------

.. image:: /images/modifiers/find_rings_panel.png
  :width: 35%
  :align: right

This modifier finds all rings within a given size range in a system of particles with :ref:`bonds <scene_objects.bonds>`.

The modifier outputs a :ref:`data table <scene_objects.data_table>` for each ring size. Each table contains the topology information for all rings of the corresponding size. The tables are named ``N-rings``, where *N* is the ring size and can be viewed in the pipeline data inspector using the button :guilabel:`Show in data inspector`. Each row lists all particle indices belonging to a ring, the first and last entry in the list are connected to close off the ring.

Furthermore, the modifier outputs a :ref:`data table <scene_objects.data_table>` containing the ring size histogram to give an overview over the distribution of ring sizes.

To visualize the rings found by the modifier a surface mesh is generated. This mesh is color coded by the ring size as seen in the image. The color coding can be adjusted in :ref:`visual_elements.surface_mesh` visual element. Note that the vertex indices in the surface mesh do not correspond to the particle indices tabulated in the ``N-rings`` tables.

.. image:: /images/modifiers/find_rings_example.png
  :width: 50%

Minimum ring size
  Minimum size of the rings identified by the modifier (inclusive). It must be greater than 2.

Maximum ring size
  Maximum size of the rings identified by the modifier (inclusive). It must be greater than the minimum ring size and less than or equal to 36.

.. seealso::

  :py:class:`ovito.modifiers.FindRingsModifier` (Python API)