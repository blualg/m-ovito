.. _viewport_layers.python_script:

Python script viewport layer |ovito-pro|
----------------------------------------

.. image:: /images/viewport_layers/python_script_overlay_panel.*
  :width: 30%
  :align: right

This type of :ref:`viewport layer <viewport_layers>` lets you write your own Python script function to paint arbitrary
text and graphics on top of images rendered in OVITO. This makes it possible to enrich figures or movies with
additional information (e.g. a scale bar or a data plot).

The :guilabel:`Edit script` button opens a code editor, where you enter the code for the user-defined ``render()`` function.
This function will be invoked by OVITO each time the viewport is repainted or
whenever an image or movie frame is being rendered. The :py:class:`args <ovito.vis.PythonViewportOverlay.Arguments>` function parameter 
gives access to a `QPainter <https://doc.qt.io/qtforpython/PySide6/QtGui/QPainter.html>`__ object,
which allows issuing arbitrary drawing commands to paint over the three-dimensional objects rendered by OVITO.

Any Python exceptions raised during script execution are displayed in the output area below.
It also shows any output from calls to the ``print()`` Python function.

.. image:: /images/viewport_layers/python_script_overlay_code_editor.*
  :width: 40%
  :align: right

The user-defined script has full access to OVITO's data model and can access viewport properties,
camera and animation settings, modifiers, and data pipeline outputs.
For more information on OVITO's Python interface and the object model, see the :ref:`scripting_running`.

Examples
""""""""

:ref:`This page <overlay_script_examples>` provides several code examples demonstrating how to write a ``render()`` function for a Python viewport layer:

* :ref:`example_scale_bar_overlay` 
* :ref:`example_data_plot_overlay` 
* :ref:`example_highlight_particle_overlay` 

.. seealso::

  :py:class:`ovito.vis.PythonViewportOverlay` (Python API)