.. _data_sources:

Data sources
============

.. image:: /images/scene_objects/data_source_and_data_objects.*
  :width: 35%
  :align: right

A *data source* is an essential part of every :ref:`pipeline <usage.modification_pipeline>` as it provides the input data that enters the 
pipeline and is passed to the modifiers. The current pipeline's data source appears under the `Data source` section of the pipeline editor as 
depicted in this screenshot. Most of the time, you will work with the data source type :ref:`External file <scene_objects.file_source>`, 
which imports the input data for the pipeline from one or more files stored on your local computer or a remote machine.

.. list-table::
  :widths: 35 65
  :header-rows: 1

  * - Data source
    - Description

  * - :ref:`External file <scene_objects.file_source>`
    - Reads the simulation data from an external file.

  * - :ref:`Python script <data_source.python_script>` |ovito-pro|
    - Runs a user-defined Python script function to generate a dataset.

  * - :ref:`LAMMPS script <data_source.lammps_script>` |ovito-pro|
    - Runs a user-defined LAMMPS script to generate a dataset. 


    
.. toctree::
  :maxdepth: 1
  :hidden:

  external_file
  python_script
  lammps_script  