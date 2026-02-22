.. _application_settings.particles:

Particle-related settings
=========================

.. image:: /images/app_settings/particle_settings.*
  :width: 45%
  :align: right

This page of the :ref:`application settings dialog <application_settings>` lets you
manage the program's presets for particle colors and radii.

Default particle colors and radii
"""""""""""""""""""""""""""""""""

When importing a simulation file, OVITO automatically assigns standard colors and radii to particle types based on their name.
This happens, in particular, for known chemical elements such as "He", "Fe", or "Si". The table shows the predefined association of named particle types
with corresponding default colors, display radii, and van der Waals radii. You can edit the values if needed. OVITO will remember these
default values across program sessions and apply them to newly imported structures.

Note that you can also set new default values for individual particle types in the :ref:`particles.modifiers.edit_types` modifier panel, where you
typically configure the appearance of element types after importing a simulation file.

Press the :guilabel:`Restore built-in defaults` button to reset all colors and radii back to the hard-coded factory default
values of OVITO and discard any customizations you have made.

.. _application_settings.particles.themes:

Exporting and importing themes
""""""""""""""""""""""""""""""

You can share your customized particle type colors and radii with other users or transfer them between machines using theme files.

The :guilabel:`Export theme...` button saves all current default colors and radii to a
JSON-based theme file (with extension :file:`.ovito-theme`). This file can then be shared or backed up.

The :guilabel:`Import theme...` button loads defaults from a previously exported theme file.
Only types present in the imported file will be updated; existing customizations for types not contained in the file are kept unchanged.

.. note::

   A theme file with element colors and radii matching those used by the `VESTA <https://jp-minerals.org/vesta/>`__ atomistic visualization software
   is available for download: `VESTA.ovito-theme <https://www.ovito.org/download/data/documentation/VESTA.ovito-theme>`__.
   Import this file to make OVITO's default particle appearance consistent with VESTA.
