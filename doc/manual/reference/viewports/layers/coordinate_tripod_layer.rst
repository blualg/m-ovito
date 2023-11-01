.. _viewport_layers.coordinate_tripod:

Coordinate tripod layer
-----------------------

.. image:: /images/viewport_layers/coordinate_tripod_overlay_panel.*
  :width: 30%
  :align: right

This :ref:`viewport layer <viewport_layers>` inserts an axis tripod into the picture to
indicate the orientation of the simulation coordinate system.

.. image:: /images/viewport_layers/coordinate_tripod_example.*
  :width: 30%

Note that the coordinate axes depicted by the layer are those of the global Cartesian
simulation coordinate system. They are *not* tied to the simulation cell vectors.

Configuring the axes
""""""""""""""""""""

The viewport layer can render up to four different axes with configurable
text labels, colors, and spatial directions. By default, the three major Cartesian directions are shown.

You can include :ref:`HTML markup elements <viewport_layers.text_label.text_formatting>` in the axes labels to format
the text, e.g., to produce special notations such as superscripts, subscripts, or overlines.

.. seealso::

  :py:class:`ovito.vis.CoordinateTripodOverlay` (Python API)