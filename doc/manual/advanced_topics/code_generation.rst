.. _python_code_generation:

Python code generator |ovito-pro|
---------------------------------

.. image:: /images/scene_objects/code_generation.jpg
  :width: 60%
  :align: right

OVITO Pro features a Python code generation tool that converts any :ref:`data pipeline <usage.modification_pipeline>`
created via the graphical user interface into an equivalent Python script. This allows users to automate data post-processing
and visualization workflows outside of OVITO.

As you adjust modifier or rendering parameters interactively, the code generator dynamically produces Python statements
corresponding to your actions. The generated script can be saved, customized, and executed later as a standalone
program.

**Watch the Python code generator in action:** https://youtu.be/sAGpCIBlWyk

Using the code generator
""""""""""""""""""""""""

To generate a Python script, select :menuselection:`File --> Generate Python Script`. This opens the code generator window,
where the script updates in real time as you modify the visualization scene, the pipeline(s), the render settings or the viewports.

By default, the code generator only produces Python code for the simulation file import and the data pipeline setup, which is sufficient
for data processing and analysis. To include camera settings, rendering parameters, and visual element settings,
enable the option :guilabel:`Include visualization code`. This is particularly useful for automating image and animation rendering tasks.

Supported features
""""""""""""""""""

The code generator can produce Python scripts for:

  * Data file import (:py:func:`ovito.io.import_file` call with all necessary parameters)
  * Visual elements
  * Modifiers
  * Viewport layers
  * Viewport camera configurations
  * Rendering engine configuration
  * Image and animation render settings
  * Manual modifications made to the imported model in the GUI, e.g. colors/radii of particle types
  * Multiple or branched pipelines in the same scene

Limitations
"""""""""""

Currently, the code generator **does not** consider:

  * Exporting data to an output file
  * Key-frame based parameter animations