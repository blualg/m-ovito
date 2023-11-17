.. _rendering.visrtx_renderer:

VisRTX renderer (Experimental) |ovito-pro|
==========================================

.. image:: /images/rendering/visrtx_renderer_panel.*
  :width: 30%
  :align: right

Scientific visualization-focused renderer based on the NVIDIA OptiX™ Ray Tracing Engine.
It offers hardware accelerated ray-tracing which can generate high-fidelity scene renderings including
global illumination effects, shadows. Compared to software-based ray-tracing 
engines like :ref:`Tachyon <rendering.tachyon_renderer>` or :ref:`OSPRay <rendering.ospray_renderer>` 
this renderer should offer almost real-time performance. 

VisRTX is currently under active development, so expect more features and capabilities to be added in the future.
For more information and updates please visit https://github.com/NVIDIA/VisRTX. 

Please note that an NVIDIA GPU with OptiX support and a current NVIDIA graphics driver 
is required to use this renderer.

The "More Options" (vertical ellipsis) button next to each nummerical parameter opens a context menu with 
the option to reset each paramter to its default value.

Quality settings
""""""""""""""""

Samples per pixel
  The number of ray-tracing samples computed per pixel of the output image (default value: 16).
  Larger values can help reduce aliasing artifacts.  

Denoising filter
  Applies a denoising filter to the rendered image to reduce noise inherent to ray traced images (default value: on).

Ambient light 
"""""""""""""

Samples
  The number of ambient light samples computed per pixel (default value: 12). Larger values can help to reduce visual artifacts.

Brightness
  Radiance of the ambient light source (default value: 0.7).

Direct light 
""""""""""""

Latitude
  Latitude (north-south) position of the direct light source (default value: 10.0°). 
  The direct light source is placed at the given latitude and longitude on a virtual spherical sky. 
  Upon camera rotation this light source will move relative to the camera, maintaining a constant relative position. A value of ``0.0`` places the light source in line with the camera's view direction. 
  Input is expected in degrees, the valid parameter range is [-90°, +90°]. 

Longitude
  Longitude (east-west) position of the direct light source (default value: -10.0°). 
  The direct light source is placed at the given latitude and longitude on a virtual spherical sky. 
  Upon camera rotation this light source will move relative to the camera, maintaining a constant relative position. 
  A value of ``0.0`` places the light source in line with the camera's view direction. 
  Input is expected in degrees, the valid parameter range is [-180°, +180°]. 

Brightness
  Irradiance of the direct light source (default value: 0.5).

.. seealso::

  :py:class:`~ovito.vis.AnariRenderer` (Python API)
