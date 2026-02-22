.. _scene_objects.data_table:

Data tables
-----------

Data tables in OVITO consist of numeric data organized into one or more data columns. 
Each row of the data table represents one data point, and OVITO can plot the data points in various ways, for 
example in the form of scatter plots, curve plots or bar charts.

You can view all data tables produced by the current data pipeline by opening the :ref:`data inspector <data_inspector.data_tables>` panel in OVITO's main window.
Modifiers such as :ref:`particles.modifiers.radial_distribution_function`, :ref:`particles.modifiers.common_neighbor_analysis` or
:ref:`particles.modifiers.histogram` output their computation results in the form of a data table.

You can export the contents of a data table to a text file using OVITO's :ref:`file export function <usage.export>`.
Exporting the graphical representation of the data table as plot or chart to a graphics file works in the same way.

.. seealso::
  
  :py:class:`ovito.data.DataTable` (Python API)