.. _usage.rendering:

Rendering
=========
.. image:: /images/rendering/render_tab.*
   :width: 35%
   :align: right

After you have created a pipeline for data analysis and visualization, at some point you may want to
produce images or a movie for publications or presentations. For this, go to the *Rendering* tab
in the command panel as shown on the right.

The :guilabel:`Render active viewport` button launches the image rendering process for the active viewport (marked by a yellow border).
OVITO will open a separate window to show the generated image, which can be saved to disk or copied to the clipboard from there.

The :ref:`Render settings <core.render_settings>` panel controls various
settings such as the resolution of the generated image and its background color. You can set a filename in the render settings panel
in advance under which the rendered picture or movie will be saved. Or you can manually save the picture later on once rendering is complete
and you are happy with the result.

OVITO Pro comes with several rendering engines to choose from, which differ in terms of speed, visual quality, and memory requirements.
The default :ref:`OpenGL renderer <rendering.opengl_renderer>` is the fastest one and produces pictures that are more or less
identical to what you see in the interactive viewports. The :ref:`Tachyon <rendering.tachyon_renderer>` and
:ref:`OSPRay <rendering.ospray_renderer>` rendering engines, on the other hand,
are software-based ray tracing engines, which are able to generate high-quality visualizations that include shadows, ambient occlusion shading, and depth of field effects.
The :ref:`VisRTX renderer <rendering.visrtx_renderer>` offers similar capabilities using hardware-accelerated ray tracing.
See the :ref:`reference section <rendering>` to learn more about the rendering capabilities of OVITO.


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

OVITO can render a movie of the loaded simulation trajectory. To render an animation,
select the :guilabel:`Complete animation` option in the :ref:`Render settings <core.render_settings>` panel and
specify an output filename for the video. OVITO's built-in video encoder supports standard formats such as AVI and MPEG.
The frame rate for the output video is set in the :ref:`animation settings <animation.animation_settings_dialog>` dialog.
Alternatively, you can produce a series of image files, one per frame, and combine them later into a movie using an external video encoding tool.

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
