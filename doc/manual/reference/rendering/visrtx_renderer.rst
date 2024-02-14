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

.. caution::

  VisRTX is currently under active development by the *HPC Visualization Developer Technology* team at NVIDIA
  in corporation with the OVITO developers, who integrate the technology.
  It is still considered experimental and not yet feature-complete.
  For more information, visit https://github.com/NVIDIA/VisRTX. Please report any issues you encounter
  to the `OVITO developers <https://gitlab.com/stuko/ovito/-/issues>`__.

.. note::

  On first use of the VisRTX renderer, it will compile RTX shader programs for your specific GPU hardware.
  This process can take up to several minutes, but happens only once. The compiled shader programs are cached
  on disk and reused in subsequent OVITO sessions.

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

Occlusion cutoff
  Maximum range of the ambient occlusion (AO) calculation (default value: 30.0). More distant objects beyond this cutoff range (given in simulation units) will not contribute to the computed
  local light occlusion effect. Decreasing this parameter will typically brighten up the inside of dark cavities that are otherwise fully occluded by the surrounding objects.
  Increasing it will make the AO effect stronger and lead to darker contrast.

  .. figure:: /images/rendering/visrtx_small_ao_cutoff.png
    :figwidth: 35%

    Small AO cutoff range

  .. figure:: /images/rendering/visrtx_large_ao_cutoff.png
    :figwidth: 35%

    Large AO cutoff range

Direct light
""""""""""""

Latitude & Longitude
  Latitude (north-south) and longitude (east-west) position of the direct light source relative to the camera (default values: 10.0° and -10.0°).
  Upon rotation of the viewport camera, this light source will move with the camera, maintaining a constant relative light direction. A value of 0.0° places the light source
  in line with the camera's viewing direction. Input is expected in degrees. The valid parameter range is [-90°, +90°] for latitude and [-180°, +180°] for longitude.

Brightness
  Irradiance of the direct light source (default value: 0.5).

.. seealso::

  :py:class:`~ovito.vis.AnariRenderer` (Python API)
