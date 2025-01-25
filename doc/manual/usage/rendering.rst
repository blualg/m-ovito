.. _usage.rendering:

Rendering
=========

.. image:: /images/rendering/render_tab.*
   :width: 35%
   :align: right

Once you have set up a data pipeline for analysis and visualization, you may want to generate high-quality images or
animations for publications or presentations. This can be done from the *Rendering* tab in the command panel, as shown on the right.

To render the :ref:`active viewport <usage.viewports>` (indicated by a yellow border), click the :guilabel:`Render active viewport` button.
OVITO will generate the image and display it in a separate window, where you can save it to disk or copy it to the clipboard.

The :ref:`Render settings <core.render_settings>` panel allows you to adjust rendering parameters, such as image resolution and background color.
You can specify a filename in advance to save the rendered image or manually save it later once you're satisfied with the result.

OVITO Pro offers multiple rendering engines, each optimized for different performance and quality needs:

:ref:`OpenGL renderer <rendering.opengl_renderer>`:
   The fastest option, producing images identical to the interactive viewports.

:ref:`Tachyon <rendering.tachyon_renderer>` |ovito-pro|:
   A software-based ray tracing engine capable of generating high-quality images with shadows, ambient occlusion, and depth of field.

:ref:`OSPRay <rendering.ospray_renderer>` |ovito-pro|:
   Another software-based ray tracer that enhances visual realism.

:ref:`VisRTX <rendering.visrtx_renderer>` |ovito-pro|:
   A hardware-accelerated ray tracing engine running on NVIDIA CUDA devices.
   Provides real-time rendering performance and can be used for :ref:`interactive visualization <viewports.configure_graphics_dialog>`
   instead of the default OpenGL renderer.

For more details, see :ref:`rendering` in the reference section.

.. |opengl-image| image:: /images/rendering/renderer_example_opengl.*
   :width: 100%
   :align: middle
.. |tachyon-image| image:: /images/rendering/renderer_example_tachyon.*
   :width: 100%
   :align: middle
.. |ospray-image| image:: /images/rendering/renderer_example_ospray.*
   :width: 100%
   :align: middle
.. |visrtx-image| image:: /images/rendering/renderer_example_visrtx.*
   :width: 100%
   :align: middle

============================= ============================= ============================= =============================
OpenGL renderer:              Tachyon renderer: |ovito-pro| OSPRay renderer: |ovito-pro|  VisRTX renderer: |ovito-pro|
============================= ============================= ============================= =============================
|opengl-image|                |tachyon-image|               |ospray-image|                |visrtx-image|
============================= ============================= ============================= =============================

.. _usage.rendering.animation:

Creating animations
-------------------

OVITO can render animations of simulation trajectories. To do this:

   1. In the :ref:`Render settings <core.render_settings>` panel, select :guilabel:`Complete animation`.
   2. Specify an output filename for the video.
   3. Choose a frame rate in the :ref:`Animation settings <animation.animation_settings_dialog>`.

OVITO's built-in video encoder supports formats such as AVI and MPEG. Alternatively, you can export individual frames
as image files and use an external encoding tool, such as :program:`ffmpeg`, to compile them into a video.

.. seealso:: :ref:`usage.animation`

.. _usage.rendering.show_render_frame:

..
  Viewport preview mode
  ---------------------

  .. |show-render-frame-example| image:: /images/rendering/show_render_frame_example.*
    :width: 100%
    :align: middle
  .. |show-render-frame-output| image:: /images/rendering/show_render_frame_output.*
    :width: 100%
    :align: middle

  ==================================== =============================
  Interactive viewport (preview mode): Rendered image:
  ==================================== =============================
  |show-render-frame-example|          |show-render-frame-output|
  ==================================== =============================

  To gauge the precise viewport region that will be visible in a rendered image,
  you can activate the :guilabel:`Preview Mode` for the active viewport.
  This option can be found in the :ref:`viewport menu <usage.viewports.menu>`, which can be opened by clicking
  the viewport's caption in the upper left corner.
