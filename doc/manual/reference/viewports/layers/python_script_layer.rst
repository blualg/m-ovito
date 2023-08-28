.. _viewport_layers.python_script:

Python script viewport layer |ovito-pro|
----------------------------------------

.. image:: /images/viewport_layers/python_script_overlay_panel.*
  :width: 30%
  :align: right

This type of :ref:`viewport layer <viewport_layers>` lets you write your own Python script function to paint arbitrary
text or graphics on top of 3d visualizations rendered by OVITO. This makes it possible to enrich figures or movies with
additional information, e.g., scale bars, data plots, or diagrams.

.. image:: /images/viewport_layers/python_script_overlay_code_editor.*
  :width: 40%
  :align: right

The :guilabel:`Edit code...` button opens a code editor, where you enter the source code for the user-defined viewport layer.
It will be invoked by OVITO each time the viewport is repainted or
whenever an image or movie frame is being rendered. Your Python script has full access to OVITO's data model and can access viewport properties,
camera and animation settings, and the data pipeline results to dynamically produce annotations or graphics.

The :guilabel:`Python settings...` button opens the :ref:`python_settings_dialog`.

For more information, please see :ref:`writing_custom_viewport_overlays` in the OVITO Python documentation.

Examples
""""""""

:ref:`This page <overlay_script_examples>` provides several code examples demonstrating how to write custom viewport layers in Python:

* :ref:`example_scale_bar_overlay`
* :ref:`example_data_plot_overlay`
* :ref:`example_highlight_particle_overlay`

.. seealso::

  * :ref:`writing_custom_viewport_overlays`
  * :py:class:`ovito.vis.ViewportOverlayInterface` (Python API)
  * :py:class:`ovito.vis.PythonViewportOverlay` (Python API)