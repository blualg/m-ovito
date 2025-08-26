.. _rendering.tachyon_renderer:

Tachyon renderer |ovito-pro|
============================

.. image:: /images/rendering/tachyon_renderer_panel.*
  :width: 30%
  :align: right

This is a software-based ray-tracing renderer. It can produce renderings of the scene with
ambient occlusion lighting, shadows, and depth of field. The visual quality of the Tachyon renderer
is slightly better than the one provided by the :ref:`OSPRay renderer <rendering.ospray_renderer>`,
but it requires more memory. Both rendering engines offer higher visual quality than the :ref:`OpenGL renderer <rendering.opengl_renderer>`
but are typically considerably slower.

The "More Options" (vertical ellipsis) button next to each numerical parameter opens a context menu with
the option to reset each parameter to its default value.

Parameters
""""""""""

Anti-aliasing samples
  To reduce `aliasing effects <https://en.wikipedia.org/wiki/Aliasing>`__,
  the Tachyon ray-tracer can perform *supersampling* by
  computing multiple rays per output pixel. This parameter controls the number of rays.

Direct light
  Enables the parallel light source, which is directed from an angle behind the camera.

Shadows
  Enables cast shadows for the directional light. Not that, with the current Tachyon version, you cannot turn off
  shadows when ambient occlusion shading is enabled. In this case, you can only completely turn off the directional light source..

Ambient occlusion
  Enabling this lighting technique mimics some of the effects that occur under conditions of omnidirectional diffuse illumination,
  e.g., outdoors on an overcast day.

Sample count
  Ambient occlusion is implemented using a Monte Carlo technique. This parameter controls the number of samples to compute.
  A higher sample number leads to a more even shading but requires more computation time.

Depth of field
  When `depth-of-field rendering <http://en.wikipedia.org/wiki/Depth_of_field>`__ is active, only objects located exactly at the distance from the camera specified by
  the *focal length* will appear sharp. Objects closer to or farther from the camera will appear blurred.

  To focus on a specific object, use the :guilabel:`Pick in viewport` button
  and click on the desired object in the viewport to be rendered. The *focal length* parameter will be automatically adjusted so that the picked location is in focus.
  The *aperture* radius controls how blurred out-of-focus objects will appear.

  Note that the focal blur effect requires a perspective projection; it does not work in :ref:`viewports <usage.viewports>` using a parallel projection.

.. seealso::

  :py:class:`~ovito.vis.TachyonRenderer` (Python API)
