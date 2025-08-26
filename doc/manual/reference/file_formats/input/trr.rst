.. _file_formats.input.trr:

Gromacs TRR file reader
-----------------------

.. figure:: /images/io/trr_reader.*
  :figwidth: 30%
  :align: right

  User interface of the Gromacs TRR file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file format is used by the *GROMACS* molecular dynamics code. A format specification can be found `here <https://manual.gromacs.org/current/reference-manual/file-formats.html#trr>`__.

.. important::

  The file reader automatically converts atom coordinates and cell vectors from nanometers to Angstroms during import into OVITO, multiplying all values by a factor of 10.
  Velocities are converted from :math:`\mathrm{nm}/\mathrm{ps}` to :math:`:\text{Å}/\mathrm{ps}`, multiplying velocities by a factor of 10.
  Forces are converted from :math:`\mathrm{kJ} \mathrm{mol}^{-1} \mathrm{nm}^{-1}` to :math:`\mathrm{eV}/\text{Å}` using a factor of 0.00103643.

.. _file_formats.input.trr.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, centering = True)
  :noindex:

  :param centering: If set to ``True``, the simulation cell and all atomic coordinates are translated to center the box at the coordinate origin.
                    If set to ``False``, one corner of the simulation cell remains fixed at the coordinate origin.
  :type centering: bool
