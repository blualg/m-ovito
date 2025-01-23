.. _rendering.visrtx_renderer:

VisRTX renderer |ovito-pro|
===========================

.. versionadded:: 3.10.0

VisRTX is a scientific visualization renderer based on the NVIDIA OptiX™ Ray Tracing Engine.
It offers hardware-accelerated ray-tracing and can generate high-fidelity scene renderings including
global illumination effects and shadows. Compared to CPU-based ray-tracing
engines like :ref:`Tachyon <rendering.tachyon_renderer>` or :ref:`OSPRay <rendering.ospray_renderer>`,
this renderer can achieve near real-time performance on modern GPU hardware.

**VisRTX requires NVIDIA hardware with CUDA support and a current NVIDIA graphics driver (CUDA 12.0+).**
The renderer is not available on the macOS platform and doesn't work on *Windows Subsystem for Linux* (WSL),
because this environment lacks the NVIDIA OptiX™ driver components required by VisRTX.

.. caution::

  VisRTX is still under active development by the *HPC Visualization Developer Technology* team at NVIDIA
  in close corporation with the OVITO developers, who integrate the technology.
  For more information, visit https://github.com/NVIDIA/VisRTX. Please report any issues you encounter
  to the `OVITO developers <https://gitlab.com/stuko/ovito/-/issues>`__. Missing features and capabilities:

    - Meshes with per-vertex transparency values
    - Meshes with :ref:`highlighted edges (wireframes) <visual_elements.surface_mesh>`
    - Materials with adjustable specular reflection parameters
    - Flat-shaded primitives

.. note::

  On first use of the VisRTX renderer, it will compile RTX shader programs for your GPU architecture.
  This process can take up to several minutes, but happens only once. The compiled shader programs get cached
  on disk and are reused in subsequent OVITO sessions.

.. tip::

  The VisRTX renderer can also be used in the interactive viewports of OVITO to visualize the scene
  in real-time with high-quality ray-tracing effects. To enable the VisRTX renderer in the viewports,
  open the :ref:`viewport graphics configuration <viewports.configure_graphics_dialog>` dialog and select the
  "NVIDIA VisRTX" renderer.

Parameters
----------

.. image:: /images/rendering/visrtx_renderer_panel.*
  :width: 30%
  :align: right

Quality settings
""""""""""""""""

Samples per pixel
  The number of ray-tracing samples computed per pixel of the output image (default value: 16).
  Larger values can help reduce aliasing artifacts.

Ambient occlusion samples
  The number of samples used to compute ambient occlusion effects (default value: 8). Larger values can help to reduce visual artifacts.

Denoising filter
  Applies a denoising filter to the rendered image to reduce noise inherent to ray-traced images (default value: on).

Ambient light
"""""""""""""

Brightness
  Radiance of the ambient light source (default value: 0.7).

Occlusion cutoff
  Maximum range of the ambient occlusion (AO) calculation (default value: 30.0). More distant objects beyond this cutoff range (given in simulation units) will not contribute to the computed
  local light occlusion effect. Decreasing this parameter will typically brighten up the inside of dark cavities that are otherwise fully occluded by the surrounding objects.
  Increasing it will make the AO effect stronger and lead to darker contrast.

  .. figure:: /images/rendering/visrtx_small_ao_cutoff.png
    :figwidth: 30%

    Small AO cutoff range

  .. figure:: /images/rendering/visrtx_large_ao_cutoff.png
    :figwidth: 30%

    Large AO cutoff range

Direct light
""""""""""""

Latitude & Longitude
  Latitude (north-south) and longitude (east-west) position of the direct light source relative to the camera (default values: 10.0° and -10.0°).
  Upon rotation of the viewport camera, this light source will move with the camera, maintaining a constant relative light direction. A value of 0.0° places the light source
  in line with the camera's viewing direction. Input is expected in degrees. The valid parameter range is [-90°, +90°] for latitude and [-180°, +180°] for longitude.

Brightness
  Irradiance of the direct light source (default value: 0.5).

Post-processing effects
"""""""""""""""""""""""

Outlines
  Enables depth-aware outlines. They can be used either in the interactive viewport or
  VisRTX final frame rendering.

  .. figure:: /images/rendering/visrtx_viewport_outlines.*
    :figwidth: 55%

    Outlines in the interactive viewport.

  .. figure:: /images/rendering/visrtx_render_outlines.*
    :figwidth: 44%

    Outlines in the rendered image.

  Depth Difference & Outline Width
    Uniform Width Mode
      In this mode, a single value is used for both the depth difference and the outline width.
      An outline with a constant width is drawn around all objects that have a depth difference
      greater than the specified value relative to the background.

    Variable Width Mode
      In this mode, the outline width increases linearly from the minimum to the maximum width
      as the depth difference between overlapping objects varies from the minimum to the maximum depth difference.
      When switching from Uniform Width Mode to Variable Width Mode, any missing values for
      depth or line width will be automatically set to their default values.

  Custom Color
    When disabled, the outline color is automatically determined based on the background color:
    white outlines for dark backgrounds and black outlines for light backgrounds.
    When enabled, the manually selected color is used instead.


.. seealso::

  :py:class:`~ovito.vis.AnariRenderer` (Python API)
