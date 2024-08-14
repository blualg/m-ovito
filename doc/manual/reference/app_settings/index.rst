.. _application_settings:

Application settings
====================

.. image:: /images/app_settings/general_settings.*
  :width: 35%
  :align: right

To open the application settings dialog, select :menuselection:`Edit --> Application settings` from the main menu on Windows/Linux.
On macOS, select :menuselection:`Ovito --> Preferences`. The dialog contains several tabs:

.. toctree::
  :maxdepth: 1

  general_settings
  viewport_settings
  modifier_templates
  particle_settings

.. _application_settings.storage_location:

Where does OVITO store its settings?
""""""""""""""""""""""""""""""""""""

OVITO stores the user's settings in a platform-dependent location to preserve them across program sessions. On Windows, the information
is saved in the system registry. On Linux and macOS, it is stored in a text-based configuration file under the user's home directory. 
The precise storage location on your computer is displayed at the bottom of the application settings dialog.

========================== ================================================================
Operating system           Storage location
========================== ================================================================
Windows                    :file:`\\HKEY_CURRENT_USER\\SOFTWARE\\Ovito\\Ovito\\`
Linux                      :file:`$HOME/.config/Ovito/Ovito.conf`
macOS                      :file:`$HOME/Library/Preferences/org.ovito.Ovito.plist`
========================== ================================================================

To reset OVITO to its factory default settings, simply delete the configuration file on Linux/macOS or remove the registry branch
on Windows using the `Windows Registry Editor <https://support.microsoft.com/en-us/windows/how-to-open-registry-editor-in-windows-10-deab38e6-91d6-e0aa-4b7c-8878d9e07b11>`__ program.

.. attention::

  On macOS, preferences are cached. After deleting the ``org.ovito.Ovito.plist`` file, you need to reboot your computer or run the ``killall -u $USER cfprefsd`` command in the terminal.