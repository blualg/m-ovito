.. _scene_objects.vectors:

Vectors
-----

.. image:: /images/scene_objects/vectors_example.png
  :width: 50%
  :align: right

A vectors :ref:`data object <scene_objects>` stores one or more vectors to be added 
to the data collection for visualization. Users can create lines programmatically
using the Python :py:attr:`DataCollection.DataCollection.vectors.create() <ovito.data.DataCollection.vectors>` 
method.

Each vector is defined by a base point (`Position`) and a `Direction`. Other per-vector
:py:ref:`standard properties <vectors-property-list>` can be defined. The 
`Color` property controls the color for each arrow. `Transparency` sets the individual
arrow's transparency.

The visual appearance of the lines in rendered images is controlled by the associated
:ref:`vectors <visual_elements.vectors>` visual element.

.. versionadded:: 3.11.0

.. seealso::

  :py:class:`ovito.data.Vectors` (Python API)