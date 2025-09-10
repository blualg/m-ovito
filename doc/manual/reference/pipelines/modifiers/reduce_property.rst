.. _particles.modifiers.reduce_property:

Reduce property |ovito-pro|
---------------------------

.. image:: /images/modifiers/reduce_property_panel.png
  :width: 30%
  :align: right

.. versionadded:: 3.14.0

This modifier reduces a selected data property to a single value by applying a specified *reduction operation* over all data elements.
For example, the modifier allows computing the mean value of a property across all particles.

The following reduction operations are available:

+--------------------------+-----------------------------------------------+
| Reduction operation      | Description                                   |
+==========================+===============================================+
| Minimum                  | Lowest value                                  |
+--------------------------+-----------------------------------------------+
| Maximum                  | Highest value                                 |
+--------------------------+-----------------------------------------------+
| Sum                      | Sum of all values                             |
+--------------------------+-----------------------------------------------+
| Non-zero count           | Number of elements with non-zero values       |
+--------------------------+-----------------------------------------------+
| Mean                     | Arithmetic mean                               |
+--------------------------+-----------------------------------------------+
| Median                   | Median value                                  |
+--------------------------+-----------------------------------------------+
| Variance                 | Sample variance                               |
+--------------------------+-----------------------------------------------+
| Standard deviation       | Sample standard deviation                     |
+--------------------------+-----------------------------------------------+
| Argmin                   | Zero-based index of the lowest value          |
+--------------------------+-----------------------------------------------+
| Argmax                   | Zero-based index of the highest value         |
+--------------------------+-----------------------------------------------+

The modifier can operate on the following kinds of data elements:

- :ref:`Particles <scene_objects.particles>`
- :ref:`Bonds <scene_objects.bonds>`
- :ref:`Surface mesh vertices/faces/regions <scene_objects.surface_mesh>`
- :ref:`Voxel grids <scene_objects.voxel_grid>`
- :ref:`Data tables <scene_objects.data_table>`
- :ref:`Lines <scene_objects.lines>`
- :ref:`Vectors <scene_objects.vectors>`

If the selected input property has multiple components, the reduction operation is applied to each vector component individually.

The option :guilabel:`Use only selected elements` restricts the computation to the currently selected subset of elements.
Note that the *Argmin* and *Argmax* operations still return an index into the full list of elements in this case.

The modifier stores the computed value as a :ref:`global attribute <usage.global_attributes>`, which can be accessed in the
:ref:`data inspector <data_inspector.attributes>` panel of OVITO.

The reduction is computed only for the current trajectory frame. To compute an average over multiple frames, you can additionally use the
:ref:`particles.modifiers.time_averaging` modifier.

.. seealso::

  :py:class:`ovito.modifiers.ReducePropertyModifier` (Python API)