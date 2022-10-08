.. _howto.scale_bar:

Adding a scale bar to a viewport |ovito-pro|
============================================

.. image:: /images/viewport_layers/python_script_scale_bar_example.*
  :width: 35%
  :align: right  
       
OVITO currently has no built-in function to add a length scale to an image, but it is possible to
use an OVITO Python script to paint a custom scale bar on top of a viewport or the rendered images.

For this, you have to add a :ref:`Python script layer <viewport_layers.python_script>` |ovito-pro| to
the viewport where you want to display a scale bar. You can then copy/paste the code from the
:ref:`example_scale_bar_overlay` into the script editor of the viewport layer.
