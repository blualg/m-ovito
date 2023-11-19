.. _rendering.visrtx_renderer:

VisRTX renderer (experimental) |ovito-pro|
==========================================

.. image:: /images/rendering/visrtx_renderer_panel.*
  :width: 30%
  :align: right

.. versionadded:: 3.10.0

VisRTX is a scientific visualization renderer based on the NVIDIA OptiX™ Ray Tracing Engine.
It offers hardware-accelerated ray-tracing and can generate high-fidelity scene renderings including
global illumination effects and shadows. Compared to CPU-based ray-tracing
engines like :ref:`Tachyon <rendering.tachyon_renderer>` or :ref:`OSPRay <rendering.ospray_renderer>`,
this renderer can achieve almost real-time performance on modern GPU hardware.

**VisRTX requires NVIDIA hardware with CUDA support and a current NVIDIA graphics driver.**
The renderer is not available on the macOS platform.

.. note::

  VisRTX is currently under active development by the *HPC Visualization Developer Technology* team at NVIDIA,
  so expect more features and capabilities to be added in the future.
  For more information, visit https://github.com/NVIDIA/VisRTX.

Parameters
----------

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

Direct light
""""""""""""

Latitude & Longitude
  Latitude (north-south) and longitude (east-west) position of the direct light source relative to the camera (default values: 10.0° and -10.0°).
  Upon rotation of the viewport camera, this light source will move with the camera, maintaining a constant relative light direction. A value of ``0.0`` places the light source
  in line with the camera's viewing direction. Input is expected in degrees. The valid parameter range is [-90°, +90°] for latitude and [-180°, +180°] for longitude.

Brightness
  Irradiance of the direct light source (default value: 0.5).

.. seealso::

  :py:class:`~ovito.vis.AnariRenderer` (Python API)
