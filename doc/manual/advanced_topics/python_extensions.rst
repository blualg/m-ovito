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
These ready-to-use extensions are hosted in external code repositories and can easily be installed in OVITO Pro
or your Python environment for using them in standalone Python programs.

