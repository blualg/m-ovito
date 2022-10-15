.. _application_settings.general:

General settings
================

.. image:: /images/app_settings/general_settings.*
  :width: 45%
  :align: right

On this tab of the :ref:`application settings dialog <application_settings>`, you can adjust global options affecting the program's user interface and graphics system. 

User interface options
""""""""""""""""""""""

Seperate folders for data import/export and session states
  If turned on, OVITO remembers across program sessions the most recently used working directories
  for different types of file I/O operations, e.g., data file import & export, 
  session state loading & saving, or image output. If turned off, 
  just one global working directory is used, which depends on where OVITO
  was launched from (on the command line).

Sort list of available modifiers by category
  If turned on, the :ref:`list of available modifiers <particles.modifiers>` displayed by OVITO
  is sub-divided into functional groups. If turned off, modifiers are offered in the form of a single, alphabetically 
  ordered list.

Program updates
"""""""""""""""

Periodically check ovito.org website for program updates
  If activated, the program checks for new software updates upon program start by contacting
  the server `www.ovito.org`. In case a new program release is available for download, a corresponding notice  
  is displayed in the command panel of the program. No personal information or usage data is transmitted to or logged by the
  software vendor.
