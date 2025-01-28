.. _custom_initial_session_state:

Customizing the initial program state
=====================================

When OVITO starts, it searches for a file named :file:`defaults.ovito` on your system.
If found, this file is used to initialize the session, allowing you to override factory defaults for various settings.
This feature saves time by preserving your preferred configuration across sessions.

The :file:`defaults.ovito` file can define:

- The initial :ref:`viewport layout <viewport_layouts>`.
- The :ref:`projection type and camera orientation for each viewport <viewports.adjust_view_dialog>`.
- The :ref:`render settings <core.render_settings>`.

This mechanism is independent of other application-wide settings, which can be modified in the :ref:`application_settings` dialog
and are stored separately in a configuration file.

File locations
""""""""""""""

OVITO searches for the :file:`defaults.ovito` file in the following locations:

.. list-table::
    :widths: auto

    * - Windows:
      - :file:`C:\\Users\\<USER>\\AppData\\Roaming\\Ovito\\Ovito\\defaults.ovito`
    * - Linux:
      - :file:`~/.local/share/Ovito/Ovito/defaults.ovito`
    * - macOS:
      - :file:`~/Library/Application Support/Ovito/Ovito/defaults.ovito`

To create a :file:`defaults.ovito` file, use :menuselection:`File --> Save Session State As` in OVITO.
Ensure the saved session is empty (i.e., contains no datasets and pipelines), as this file serves only as a blank starting
template before you import simulation data.
