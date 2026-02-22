.. _visual_elements.dislocations:

Dislocations
------------

.. image:: /images/visual_elements/dislocations_panel.png
  :width: 30%
  :align: right

This :ref:`visual element <visual_elements>` controls the rendering of dislocation lines
which have been extracted by the :ref:`particles.modifiers.dislocation_analysis` modifier
or loaded from a discrete dislocation dynamics simulation file. The visual element is responsible for
the graphical representation of the dislocation lines stored in a :ref:`dislocations data object <scene_objects.dislocations>`.

The parameters of the visualization element provide control over the appearance of the dislocation lines.
OVITO provides options to indicate the line sense and Burgers vector of each dislocation line segment.
Different coloring schemes allow to visualize other local properties of the dislocation defects such as the Burgers vector family or the screw/edge character (i.e. the angle between local line tangent and Burgers vector).

.. seealso::
  
  :py:class:`ovito.vis.DislocationVis` (Python API)