.. _application_settings.viewports:

Viewport settings
=================

.. image:: /images/app_settings/viewport_settings.*
  :width: 45%
  :align: right

This page of the :ref:`application settings dialog <application_settings>`
contains options related to the interactive viewports of the OVITO.

Camera
""""""

Coordinate system orientation
  OVITO can restrict the viewport camera rotation such that the selected Cartesian coordinate axis
  always points upward. Default: z-axis.

Restrict camera rotation to keep the major axis pointing upward
  This option constrains the camera's orientation to prevent the camera from turning upside down.

Color scheme
""""""""""""

This option selects between the default dark viewport background and a white background.

3D graphics
"""""""""""

Graphics hardware interface
  Selects the application programming interface used by OVITO for rendering the contents of the interactive 
  viewports. Currently, OVITO supports the `OpenGL <https://www.opengl.org/>`__ and the `Vulkan <https://www.vulkan.org/>`__ graphics interface. The OpenGL interface is more mature
  and should work well on most systems. Vulkan is the more modern programming interface, and some graphics drivers
  may still exhibit compatibility problems. Please inform the OVITO developers about any such problems.
  
  The Vulkan interface provides the advantage of letting you explicitly select the graphics
  device to be used by OVITO when the system contains several GPUs and/or integrated graphics processors. In contrast,
  you have to make the device selection on the `operating system or graphics driver level <https://answers.microsoft.com/en-us/windows/forum/windows_10-hardware/select-gpu-to-use-by-specific-applications/eb671f52-5c24-428d-a7a0-02a36e91ee2f>`__
  for the OpenGL interface.

  .. note::

    The Vulkan renderer option is *not* available on the macOS platform or in OVITO for Anaconda builds.

  Select :menuselection:`System Information` from the :menuselection:`Help` menu of OVITO to access further information 
  about the graphics hardware available in your system. Submit this information to the OVITO developers when
  reporting graphics compatibility problems.

Transparency rendering method
  This option affects the rendering of semi-transparent objects when they occlude other objects
  or overlap with each other. Both available rendering methods represent different approximations of how a true rendition of 
  semi-transparent objects would look like - which is not achievable in real-time visualization using OpenGL/Vulkan.
  
  Back-to-front ordered rendering (default) gives correct results if there is only one kind of semi-transparent object in the scene,
  e.g. just particles, but likely fails to render a mixture of different semi-transparent objects correctly, e.g. semi-transparent particles combined with 
  semi-transparent surface meshes.

  `Weighted Blended Order-Independent Transparency <https://jcgt.org/published/0002/02/09/>`__ is an alternative method more suitable 
  for overlapping semi-transparent objects of different kinds. But it delivers only a rough approximation of translucency. 