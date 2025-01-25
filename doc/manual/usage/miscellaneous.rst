.. _usage.miscellaneous:

Miscellaneous
=============

This section introduces various useful features and functions of OVITO in no particular order.

.. _usage.saving_loading_scene:

Saving the session state
------------------------

You can save your current session, including the data pipeline, viewport settings, render settings, and other configurations,
to a :file:`.ovito` *state file* using :menuselection:`File --> Save Session State`.

This allows you to:

  - Preserve your visualization setup for future use or sharing it with others.
  - Resume your work at a later time without losing your progress.
  - Create a template to apply the same data analysis pipeline and camera setup to multiple simulations.

.. _usage.spinner_widgets:

Spinner controls
----------------

.. image:: /images/command_panel/spinner_widget_usage.*
  :width: 50%
  :align: left

A *spinner widget* is a user interface element for adjusting numeric values (see video on the left).

You can modify values using the following methods:

  - Click the up or down arrow to increment or decrement the value stepwise.
  - Click and hold the mouse button, then drag up or down to continuously adjust the value.
  - If the cursor reaches the screen edge while dragging, OVITO wraps it around to enable infinite scrolling.

.. attention::

  On macOS, OVITO requires *Accessibility* permissions to reposition the cursor programmatically during infinite scrolling. The system will prompt you
  for permission when needed. You can manage this setting at any time in :menuselection:`Apple menu --> System Settings --> Privacy & Security --> Accessibility`.

.. _usage.data_inspector:

Data inspector
--------------

.. image:: /images/usage/miscellaneous/data_inspector.*
  :width: 45%
  :align: right

The :ref:`Data Inspector <data_inspector>` is a panel located below the viewport area.
It can be accessed by clicking or dragging the tab bar, shown in the screenshot on the right.

This panel provides:

  - Multiple tabs displaying the different data elements in the current dataset (e.g., the list of particles and their properties).
  - A :ref:`measurement tool for determining distances and angles <data_inspector.particles>` between particles.
  - :ref:`2D data plots <data_inspector.data_tables>` computed by OVITO.

The data inspector displays the final *output* of the current data pipeline, i.e., the data as it appears after
all modifiers have been applied.

.. _usage.viewport_layers:

Viewport layers
---------------

Viewport layers allow you to overlay additional information and 2D graphical elements, such as:

  - Text labels
  - Color legends
  - Coordinate tripods
  - Data plots

on top of the three-dimensional scene. OVITO offers several different layer types, which can be added to a viewport from the
:ref:`Viewport Layers <viewport_layers>` tab of the command panel.

.. _usage.modifier_templates:

Modifier & viewport layer templates
-----------------------------------

Frequently used modifiers or combinations of modifiers can be saved as preconfigured *modifier templates*,
allowing quick insertion into the data pipeline with a single click.

Similarly, you can create *viewport layer templates* for commonly used visual overlays.
This feature helps streamline workflows and reduces repetitive setup steps. See :ref:`modifier_templates` for more details.

.. _usage.scripting:

Python scripting |ovito-pro|
----------------------------

:ref:`OVITO Pro <credits.ovito_pro>` includes a Python scripting interface for automating analysis and visualization tasks.
This is especially useful for batch-processing multiple input files or for developing custom modifiers, viewport layers,
or file importers and exporters.

For details, see the :ref:`OVITO Python manual <scripting_manual>`, accessible from the help menu.

The integrated Python interpreter in OVITO Pro allows you to automate workflows, develop custom analysis functions,
and tailor the software to your needs. You can use the Python language to write extensions for various key areas:

* :ref:`Custom modifiers <particles.modifiers.python_script>` - Perform user-defined calculations and data transformations within the modification pipeline.
* :ref:`Custom pipeline sources <data_source.python_script>` - Generate on-the-fly input data for a processing pipeline.
* :ref:`Custom file readers <writing_custom_file_readers>` - Import simulation data from unsupported file formats.
* :ref:`Custom file writers <writing_custom_file_writers>` - Export computed results to new file formats.
* :ref:`Custom viewport layers <viewport_layers.python_script>` - Overlay additional graphics or annotations in renderings.
* :ref:`Custom utility applets <writing_custom_utilities>` - Add new tools and automation features to the OVITO GUI.

Python scripting in OVITO Pro provides powerful customization options, making it easy to extend the software's functionality
to fit your specific analysis and visualization needs.

.. seealso::

  * :ref:`topics.python_extensions.gallery`
  * :ref:`python_code_generation`
