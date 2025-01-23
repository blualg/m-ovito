.. _usage.export:

Exporting data
==============

The :menuselection:`File --> Export File` function of OVITO exports the results of
the current data pipeline to a file. Depending on the selected output format (see table below), different fragments of the dataset are exported,
e.g. the particles and their properties, the bonds, other computed quantities, etc.
Furthermore, you can choose which animation frame(s) should be exported (just the current frame or a range) and whether the
datasets are saved to a single output file or to a sequence of files, one per frame.

OVITO will ask you for a destination filename. Note that, if you append a :file:`.gz` or :file:`.zst` suffix, the output file(s) will automatically be
compressed for text-based file formats.

.. seealso:: :ref:`Output file formats support by OVITO <file_formats.output>`

.. _usage.global_attributes:

Global attributes
-----------------

*Global attributes* are numeric values or other pieces of information associated with the dataset as a whole,
not individual particles or bonds.

OVITO's analysis functions may produce new global attributes as an output, e.g., the number of atoms of a
particular type or the computed total surface area of a structure. You can open the :ref:`Attributes page <data_inspector.attributes>`
of the data inspector to see the list of global attributes associated with the current dataset at the current simulation timestep.

Global attributes often have time-dependent values, i.e., they are dynamically recomputed by the pipeline system for every animation frame.
Plotting the value(s) of one or more global attributes as functions of time can be done using the :ref:`particles.modifiers.time_series`
modifier of OVITO Pro.

You can export global attribute values to a text file using OVITO's file export function described above.
Make sure to select the "*Table of Values*" :ref:`export format <file_formats.output>`. This output format yields a tabular data file
with one row per animation frame and one column per global attribute.

.. seealso::

  * :ref:`adding_global_attributes`
  * `Reduce Property modifier <https://github.com/ovito-org/ReduceProperty>`__
