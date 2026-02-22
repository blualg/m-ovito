.. _visual_elements.simulation_cell:

Simulation cell
---------------

.. image:: /images/visual_elements/simulation_cell_panel.png
  :width: 35%
  :align: right

This :ref:`visual element <visual_elements>` renders the :ref:`simulation cell <scene_objects.simulation_cell>` as a wireframe box.

Parameters
""""""""""

Visible in rendered images
  Controls the visibility of the simulation cell in rendered images and animations. If you turn
  this option off, the cell still remains visible in the interactive viewports of OVITO. To
  hide it in the interactive viewports as well, uncheck the
  box next to the "Simulation cell" visual element in the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`.

Line width
  OVITO renders the simulation cell edges as thin cylinders of finite diameter.
  This parameter controls the diameter of these cylinders in units of length of the simulation.
  When importing a new simulation cell, the line width is automatically configured by OVITO depending on
  the overall size of the simulation cell.

Line color
  The color used for rendering the cell edges.

Note that the :guilabel:`Line width` and :guilabel:`Line color` settings
control how the cell appears in :ref:`rendered images <usage.rendering>` of the scene. In the
:ref:`interactive viewports <usage.viewports>` of OVITO, the cell is always rendered as a white wireframe model.

.. seealso::
  
  :py:class:`ovito.vis.SimulationCellVis` (Python API)