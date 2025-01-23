.. _topics.python_extensions:

Python extensions |ovito-pro|
=============================

OVITO Pro allows you to extend the functionality of the program by writing your own Python scripts and plugins.
This overview provides hints how to do that and where to find ready-to-use extensions written by other users.

The integrated Python interpreter of OVITO Pro is a powerful tool that allows you to automate tasks,
create new analysis functions, and customize the program to your needs. You can develop extensions for
the following key areas of OVITO:

* :ref:`User-defined modifiers <particles.modifiers.python_script>` that perform custom calculations or data transformations as part of data pipelines
* :ref:`User-defined pipeline sources <data_source.python_script>` that generate ad-hoc input data for a pipeline
* :ref:`User-defined file readers <writing_custom_file_readers>` that import simulation data from new file formats
* :ref:`User-defined file writers <writing_custom_file_writers>` that export computational results to new file formats
* :ref:`User-defined viewport layers <viewport_layers.python_script>` that enrich renderings with additional graphics or annotations
* :ref:`User-defined utility applets <writing_custom_utilities>` that make additional actions or automations available in the OVITO GUI

.. _topics.python_extensions.gallery:

Available extensions for OVITO Pro
----------------------------------

The `OVITO Pro Extension Directory <https://www.ovito.org/extensions/>`__ is a curated collection of open-source
Python extensions written and shared by members of the OVITO user community.
These ready-to-use extensions are hosted in external code repositories and can easily be installed in OVITO Pro.
Or your can use them in your standalone Python scripts by installing the packages in your Python interpreter.

From the OVITO Pro GUI, you can browse the extension directory by selecting :menuselection:`Edit --> Python Extensions` from the main menu:

.. image:: /images/python_settings_dialog/python_extension_gallery.*
  :width: 70%

The extension directory is organized by categories, such as modifiers, viewport layers, file import/export, and general utility applets.
It provides a short description for each extension, a link to the source code repository, and a button to download and install the extension in OVITO Pro.

.. note::

   The extension directory is a community-driven platform. The extensions are not officially supported by *OVITO GmbH* and may not be compatible with
   all versions of the program. They are written and maintained by third-party authors (some of which may be OVITO developers).

More extension packages for OVITO Pro are available from other online sources - or you may have developed your own extensions.
To install such extensions in OVITO Pro, go to the :ref:`application_settings.python` tab of the application settings dialog and use
the :guilabel:`Install additional package` function to install the package. Follow the instructions provided by the extension author.

.. seealso:: :ref:`registering_custom_python_classes`
