.. _scene_objects.lines:

Lines
-----

.. image:: /images/scene_objects/trajectory_lines_example.gif
  :width: 35%
  :align: right

A lines :ref:`data object <scene_objects>` stores one or more lines to be added to the data collection.
It can be created using the the :ref:`particles.modifiers.generate_trajectory_lines` modifier,
by sampling the current positions of the particles over time. Alternatively, users can create lines programatically
using the Python :py:attr:`DataCollection.lines.create() <ovito.data.DataCollection.lines>` method.

The visual appearance of the lines in rendered images is controlled by the associated
:ref:`lines <visual_elements.lines>` visual element.

.. seealso::

  :py:class:`ovito.data.Lines` (Python API)