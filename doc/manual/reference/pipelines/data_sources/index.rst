.. _data_sources:

Data sources
============

.. image:: /images/scene_objects/data_source_and_data_objects.*
  :width: 35%
  :align: right

A *data source* is the starting point of every :ref:`data pipeline <usage.modification_pipeline>` in OVITO, providing the input data
that flows through the pipeline and is processed by the modifiers. The active data source is displayed under the **Data source** section in the
:ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`, as shown in the screenshot.

.. list-table::
  :widths: 35 65
  :header-rows: 1

  * - Data source type
    - Description

  * - :ref:`External file <scene_objects.file_source>`
    - Loads simulation data from external files (stored locally or remotely).

  * - :ref:`Python script <data_source.python_script>` |ovito-pro|
    - Executes a custom Python script to generate data dynamically.

  * - :ref:`LAMMPS script <data_source.lammps_script>` |ovito-pro|
    - Runs a LAMMPS simulation script within OVITO and forwards the output to the pipeline.

Additionally, the Python API provides the :py:class:`~ovito.pipeline.StaticSource` class,
which allows passing in-memory datasets to a pipeline.

.. toctree::
  :maxdepth: 1
  :hidden:

  external_file
  python_script
  lammps_script