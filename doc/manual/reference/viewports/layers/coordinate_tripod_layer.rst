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

Note that the coordinate axes rendered by the layer are those of the global Cartesian
simulation coordinate system. They are *not* tied to the simulation cell vectors. 

Configuring the axes
""""""""""""""""""""

The viewport layer can render up to four different axes with configurable 
text labels, colors, and spatial directions. By default, three axes along the 
major Cartesian direction are shown.

HTML text formatting
""""""""""""""""""""

You can include HTML markup elements in the label text of the axes to format
the text, e.g., to produce special notations such as superscripts or subscripts.
See :ref:`here <viewport_layers.text_label.text_formatting>` for further information.

.. seealso::

  :py:class:`ovito.vis.CoordinateTripodOverlay` (Python API)