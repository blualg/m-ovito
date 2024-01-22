.. _file_formats.input.gromacs:

Gromacs GRO file reader
-----------------------

.. figure:: /images/io/gromacs_reader.*
  :figwidth: 30%
  :align: right

  User interface of the Gromacs GRO file reader, which appears as part of a pipeline's :ref:`file source <scene_objects.file_source>`.

This file format is used by the *GROMACS* molecular dynamics code. A format specification can be found `here <https://manual.gromacs.org/current/reference-manual/file-formats.html#gro>`__.

.. important::

  The file reader automatically converts atom coordinates and cell vectors from nanometers to Angstroms during import into OVITO, multiplying all values by a factor of 10.

.. _file_formats.input.gromacs.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, centering = True, generate_bonds = False)
  :noindex:

  :param centering: If set to ``True``, the simulation cell and all atomic coordinates are translated to center the box at the coordinate origin.
                    If set to ``False``, the corner of the simulation cell remains fixed at the coordinate origin.
  :type centering: bool

  :param generate_bonds: Activates the generation of ad-hoc bonds connecting the atoms loaded from the file.
                         Ad-hoc bond generation is based on the van der Waals radii of the chemical elements.
                         Alternatively, you can apply the :py:class:`~ovito.modifiers.CreateBondsModifier` to the
                         system after import, which provides more control over the generation of pair-wise bonds.
  :type generate_bonds: bool
