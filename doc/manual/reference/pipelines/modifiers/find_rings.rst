.. _particles.modifiers.find_rings:

Find rings |ovito-pro|
----------------------

.. image:: /images/modifiers/find_rings_panel.png
  :width: 35%
  :align: right

.. versionadded:: 3.14.0

This modifier identifies ring structures within a specified size range in particle systems that contain :ref:`bonds <scene_objects.bonds>`. These can be loaded from a file or generated using the :ref:`Create Bonds modifier <particles.modifiers.create_bonds>`.

In this context, a ring is a closed loop of particles, each connected to its neighbors by bonds. Rings cannot contain bridges—this means the shortest path between any two particles within a ring must pass only through other particles in the same ring. See the schematic below:

.. image:: /images/modifiers/find_rings_schematic.png
  :width: 60%
  :align: center

The modifier generates separate :ref:`data tables <scene_objects.data_table>` for each ring size, named ``N-rings``, where *N* represents the ring size. Each row lists particle indices forming a ring; note that the first and last particles listed are implicitly connected.

Additionally, a :ref:`data table <scene_objects.data_table>` showing the histogram of ring sizes is produced, summarizing the distribution. You can inspect this table by clicking :guilabel:`Show histogram in data inspector`.

The modifier also creates a surface mesh to visualize rings as polygons. The mesh polygons are color-coded by ring size, as illustrated below. You can adjust the color mapping via the :ref:`visual_elements.surface_mesh` visual element.

.. image:: /images/modifiers/find_rings_example.png
  :width: 50%
  :align: center

Minimum ring size
  Minimum size of the rings identified by the modifier (inclusive). It must be greater than 2.

Maximum ring size
  Maximum size of the rings identified by the modifier (inclusive). It must be greater than the minimum ring size and at most 36, the maximum ring size supported by this modifier.

.. seealso::

  :py:class:`ovito.modifiers.FindRingsModifier` (Python API)