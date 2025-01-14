.. _viewport_layers:

Viewport layers
---------------

.. toctree::
   :hidden:

   Color legend <color_legend_layer>
   Coordinate tripod <coordinate_tripod_layer>
   Python script <python_script_layer>
   Text label <text_label_layer>

.. image:: /images/viewport_layers/viewport_overlay_command_panel.*
   :width: 40%
   :align: right

Viewport layers render two-dimensional text and graphics on top of the three-dimensional scene.
They allow you to include additional information in output pictures and animations.
OVITO provides several layer types (see table below), which you can add to a viewport.
Go to the :guilabel:`Viewport layers` tab of OVITO's command panel
(screenshot on the right) to manage the layers of the active viewport.

Note that viewport layers are only visible in the interactive viewport windows
while :ref:`render preview mode <usage.viewports.menu>` is turned on for a viewport.
OVITO activates the render preview mode automatically for the active viewport whenever you add a new layer.

.. image:: /images/viewport_layers/viewport_layers_schematic.*
   :width: 40%
   :align: right

**Available viewport layers:**

================================================================ ==================================
Layer type                                                       Description
================================================================ ==================================
:ref:`Color legend <viewport_layers.color_legend>`               Shows a color map for a :ref:`particles.modifiers.color_coding` modifier or a :ref:`typed property <scene_objects.particle_types>`.
:ref:`Coordinate tripod <viewport_layers.coordinate_tripod>`     Renders an axes tripod to indicate the view orientation
:ref:`Python script <viewport_layers.python_script>` |ovito-pro| Write your own overlay type in Python and draw arbitrary graphics and data plots on top of the 3d view
:ref:`Text label <viewport_layers.text_label>`                   Renders some text, which may be used to display dynamically computed quantities
================================================================ ==================================

.. image:: /images/viewport_layers/viewport_layer_data_plot.*
  :width: 35%
  :align: right

.. image:: /images/viewport_layers/python_script_scale_bar_example.*
  :width: 35%
  :align: right

More viewport layers are available as :ref:`installable extensions for OVITO Pro <topics.python_extensions.gallery>`,
for instance:

   * https://github.com/ovito-org/DataTablePlotOverlay

     Adds a data plot to a viewport on top of the rendered image. This extension allows to
     incorporate dynamically computed information in a simulation video, for example.

   * https://github.com/ovito-org/DistancesAndAnglesOverlay

     This extension lets you measure distances and angles between particles and visualize them as
     a graphical overlay in the viewports.

   * :ref:`Code example: Scale bar <example_scale_bar_overlay>`

     You can use this example Python script to paint a custom scale bar on top of a viewport or the rendered images.

.. seealso::

   * :py:class:`ovito.vis.ViewportOverlay` (Python API)
   * :py:attr:`Viewport.overlays <ovito.vis.Viewport.overlays>` (Python API)
   * :py:attr:`Viewport.underlays <ovito.vis.Viewport.underlays>` (Python API)
   * :ref:`writing_custom_viewport_overlays`
