.. _file_formats.output.gsd:

GSD/HOOMD file exporter
-----------------------

Python parameters
"""""""""""""""""

If you export a scene to the GSD/HOOMD file format with the :py:func:`~ovito.io.export_file` Python function, the following specific keyword parameter is available:

.. py:function:: export_file(pipeline, file, "gsd/hoomd", dtype=numpy.float32, ...)
  :noindex:

  :param numpy.typing.DTypeLike dtype: The numpy data type used to control the precision of the exported data. Can be ``numpy.float32`` or ``numpy.float64``.
                                       The default value is ``numpy.float32``. Not all versions of HOOMD support 64-bit floating point numbers.
