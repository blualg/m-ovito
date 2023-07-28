.. _file_formats.input.ase_database:

ASE database reader |ovito-pro|
-------------------------------

.. attention::

  This file format reader is written in Python and therefore is only available in :ref:`OVITO Pro <credits.ovito_pro>`
  and the :ref:`OVITO Python module <scripting_manual>`.

.. important::

  The file reader requires the `ASE module <https://wiki.fysik.dtu.dk/ase/install.html>`__ to work. Without this Python module, database files will not be recognized
  by OVITO Pro or the :py:func:`~ovito.io.import_file` function. Thus, first make sure that ASE is installed in your Python interpreter or in the embedded interpreter
  of OVITO Pro. See :ref:`ovitos_install_modules`.

.. figure:: /images/io/ase_database_reader.*
  :figwidth: 36%
  :align: right

  User interface of the ASE database file reader.

Loads atomic structures from `database files of the Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/ase/db/db.html>`__.

All structures found in the imported database file get loaded in the form of a trajectory sequence in OVITO, which means you can
scroll through the structures using the time slider (shown only if more than one structure is found in the database).

The file reader lets you specify an optional `filter query string <https://wiki.fysik.dtu.dk/ase/ase/db/db.html#querying>`__ in the parameters panel
(see screenshot) to selectively load just a subset of the structures from the database.

Internally, the file reader is based on the :py:func:`ovito.io.ase.ase_to_ovito` function, which converts ASE atoms objects
to OVITO's :ref:`particle <scene_objects.particles>` datasets.

.. seealso:: :ref:`file_formats.input.ase_trajectory`

.. _file_formats.input.ase_database.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, query_string = '')
  :noindex:

  :param query_string: An optional `ASE query expression <https://wiki.fysik.dtu.dk/ase/ase/db/db.html#querying>`__ for filtering
                       the set of structures loaded from the database file.
  :type query_string: str
