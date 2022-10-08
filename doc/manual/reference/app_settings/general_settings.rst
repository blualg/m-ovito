.. _application_settings.general:

General settings
================

.. image:: /images/app_settings/general_settings.*
  :width: 45%
  :align: right

On this tab of the :ref:`application settings dialog <application_settings>`, you can adjust global options affecting the program's user interface and graphics system. 

User interface
""""""""""""""

Load file: Use alternative file selection dialog
  Changes the type of file selector shown when you import a simulation file into OVITO. 
  By default, the operating system's file selection dialog is used. This option
  activates a built-in dialog instead, which may or may not provide certain advantages 
  over the standard file selector (depending on your needs).

Modifiers list: Sort by category
  If this option is set, the :ref:`list of available modifiers <particles.modifiers>` 
  is split into function groups. Otherwise, modifiers are shown in one alphabetically ordered list.

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

Program updates
"""""""""""""""

Periodically check ovito.org website for program updates
  If activated, the program checks for new software updates upon program start by contacting
  the server `www.ovito.org`. In case a new program release is available for download, a corresponding notice  
  is displayed in the command panel of the program. No personal information or usage data is transmitted to or logged by the
  software vendor.
