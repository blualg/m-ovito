.. _particles.modifiers.reduce_property:

Reduce property modifier |ovito-pro|
------------------------------------

This modifier performs a reduction operation on a selected data property. Properties can be taken from the following containers:

+------------------------------------------------------+--------------------------------------------------+
| Object                                               | ``<PropertyContainer>``                          |
+======================================================+==================================================+
| :ref:`Particles <scene_objects.particles>`           | "Particles"                                      |
+------------------------------------------------------+--------------------------------------------------+
| :ref:`Bonds <scene_objects.bonds>`                   | "Bonds"                                          |
+------------------------------------------------------+--------------------------------------------------+
| :ref:`Surface mesh <scene_objects.surface_mesh>`     | "SurfaceMeshVertices"                            |
|                                                      | "SurfaceMeshFaces"                               |
|                                                      | "SurfaceMeshRegions"                             |
+------------------------------------------------------+--------------------------------------------------+
| :ref:`Voxel grid <scene_objects.voxel_grid>`         | "VoxelGrid"                                      |
+------------------------------------------------------+--------------------------------------------------+
| :ref:`Lines <scene_objects.lines>`                   | "Lines"                                          |
+------------------------------------------------------+--------------------------------------------------+
| :ref:`Vectors <scene_objects.vectors>`               | "Vectors"                                        |
+------------------------------------------------------+--------------------------------------------------+

and the available reduction operations are:

+--------------------------+-----------------------------------------------+
| ``<ReductionOperation>`` | Description                                   |
+==========================+===============================================+
| Minimum                  | Minimum value                                 |
+--------------------------+-----------------------------------------------+
| Maximum                  | Maximum value                                 |
+--------------------------+-----------------------------------------------+
| Sum                      | Sum of all values                             |
+--------------------------+-----------------------------------------------+
| Non-zero                 | Count of non-zero values                      |
+--------------------------+-----------------------------------------------+
| Mean                     | Arithmetic mean                               |
+--------------------------+-----------------------------------------------+
| Median                   | Median value                                  |
+--------------------------+-----------------------------------------------+
| Variance                 | Sample variance                               |
+--------------------------+-----------------------------------------------+
| Standard deviation       | Sample standard deviation                     |
+--------------------------+-----------------------------------------------+

If the property has multiple components (e.g., *Position*), the operation is applied to each component individually.
By default, the modifier operates on a particle property. You can change this behavior by setting the *Operate on* parameter.
This can be seen in the following code snippet:

  .. versionadded:: 3.14.0

Data output options
"""""""""""""""""""

The result is stored in the :ref:`Attributes <data_inspector.attributes>` tab in the command panel using the key format: ``ReducePropertyModifier.<ReductionOperation>.<PropertyContainer>.<PropertyName>.<Component>``. The ``.<Component>`` label is included only if the property has more than one component.

Parameters
""""""""""

Operate on
  Selects whether to bin data from :ref:`particles <scene_objects.particles>` or any other data objects listed in the table above. Default: :ref:`particles <scene_objects.particles>`

Input property
  The particle property to apply the reduction operation to.

Use only selected elements
  Restricts the computation to the currently selected subset of particles. Requires a selection in the current data objects. Default: *False*

Reduction operation
  Determines how data is aggregated per bin. Options are listed as ``<ReductionOperation>`` in the table above. Default: *Mean*

.. seealso::

  :py:class:`ovito.modifiers.ReducePropertyModifier` (Python API)