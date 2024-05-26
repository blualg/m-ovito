.. _scene_objects.vectors:

Vectors
-------

.. image:: /images/scene_objects/vectors_example.png
  :width: 50%
  :align: right

A vectors :ref:`data object <scene_objects>` stores one or more vectors to be added
to the data collection for visualization. Users can create arrow glyphs programmatically
using the Python :py:attr:`DataCollection.DataCollection.vectors.create() <ovito.data.DataCollection.vectors>`
method.

Each vector is defined by a base point (property `Position`) and a direction (property `Direction`). More
:py:ref:`per-vector properties <vectors-property-list>` can be defined for each data element. The
`Color` property controls the color for each rendered arrow. The property `Transparency` controls the
transparency of individual arrow glyphs.

The visual appearance of the vector glyphs in rendered images is controlled by the associated
:ref:`vectors visual element <visual_elements.vectors>`.

.. versionadded:: 3.11.0

.. seealso::

  :py:class:`ovito.data.Vectors` (Python API)