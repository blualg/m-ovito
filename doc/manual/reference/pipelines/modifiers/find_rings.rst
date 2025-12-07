.. _particles.modifiers.find_rings:

Find rings |ovito-pro|
----------------------

.. image:: /images/modifiers/find_rings_panel.png
  :width: 35%
  :align: right

.. versionadded:: 3.14.0

This modifier identifies and visualizes ring structures formed by :ref:`bonds <scene_objects.bonds>` between particles.

You can load the bonds needed by this modifier either from a file or generate it within OVITO using the :ref:`particles.modifiers.create_bonds` modifier.

.. _particles.modifiers.find_rings.definition:

Definition of a ring
====================

A ring is a closed path made of bonds. Rings identified by the algorithm may not contain bridges. This means the shortest path between any two particles within the same ring must be a section of the same ring, i.e., the path cannot be a shortcut. This is shown in the following schematic. The 10-ring is not a valid ring, because a shortcut exists between two of its atoms. Instead, the modifier finds two 6-rings.

.. image:: /images/modifiers/find_rings_schematic.png
  :width: 60%
  :align: center

The modifier has two parameters specifying the rings to be identified:

Minimum ring size
  Minimum size of the rings identified by the modifier (inclusive). It must be at least 3.

Maximum ring size
  Maximum size of the rings identified by the modifier (inclusive). It must be greater than the minimum ring size and at most 36, the maximum ring size supported by this modifier.

Outputs
=======

List of rings
  For each ring size :math:`N` in the specified range, the modifier outputs a :ref:`data table <scene_objects.data_table>`, named ``N-rings``, listing each detected ring of that size. Each row in the table contains the 0-based indices of the particles forming a ring; the first and last particles listed are implicitly connected.

Ring size histogram
  Additionally, a :ref:`data table <scene_objects.data_table>` with the histogram of ring sizes is produced, summarizing the distribution. You can open this histogram by clicking :guilabel:`Show histogram in data inspector`.

Ring polygons
  The modifier also creates a :ref:`surface mesh <scene_objects.surface_mesh>` to visualize the rings as polygons. You can adjust the color mapping of the :ref:`visual_elements.surface_mesh` visual element to represent the size of each ring with a color, as in the example below.

  .. image:: /images/modifiers/find_rings_example.png
    :width: 50%
    :align: center

.. seealso::

  :py:class:`ovito.modifiers.FindRingsModifier` (Python API)