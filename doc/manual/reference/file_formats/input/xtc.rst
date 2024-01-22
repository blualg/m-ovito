.. _file_formats.input.xtc:

Gromacs XTC file reader
-----------------------

.. figure:: /images/io/xtc_reader.*
  :figwidth: 30%
  :align: right

  User interface of the Gromacs XTC file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file format is used by the *GROMACS* molecular dynamics code. A format specification can be found `here <https://manual.gromacs.org/current/reference-manual/file-formats.html#xtc>`__.

.. important::

  The file reader automatically converts atom coordinates and cell vectors from nanometers to Angstroms during import into OVITO, multiplying all values by a factor of 10.

.. _file_formats.input.xtc.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, centering = True)
  :noindex:

  :param centering: If set to ``True``, the simulation cell and all atomic coordinates are translated to center the box at the coordinate origin.
                    If set to ``False``, the corner of the simulation cell remains fixed at the coordinate origin.
  :type centering: bool
