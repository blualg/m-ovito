.. _file_formats.input.pdb:

PDB file reader
---------------
For loading model structures stored in the `RCSB Protein Data Bank <https://www.rcsb.org>`__ (PDB) format. 
OVITO's data file reader can directly parse gzipped PDB files (".gz" suffix).

Options
"""""""
Generate bounding box if needed
  If this option is enabled and no simulation cell is found in the input file, OVITO will generate an 
  axis-aligned bounding box containing all atoms. This bounding box is non-periodic
  and can be used as a simulation cell in subsequent calculations. Please note that this box is only 
  an approximation of the cell geometry used in the calculation.

Center simulation cell on coordinate origin
  In this mode, OVITO shifts the geometric center of the simulation cell to the origin of the coordinate system.

Generate bonds
  When selected, OVITO will generate bonds between atoms based on their van der Waals radii. This is 
  equivalent to inserting a :ref:`particles.modifiers.create_bonds` modifier into the pipeline.
  This option and "Read bonds from file" are mutually exclusive.

Read bonds from file
  When selected, OVITO will read bonds from the input file. These are identified by the ``CONECT`` 
  record in the PDB file. This option and "Generate bonds" are mutually exclusive.

.. _file_formats.input.pdb.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, sort_particles = False, bounding_box = False, centering = False, generate_bonds = True)
  :noindex:

  :param generate_bonds: Generate ad-hoc bonds between particles during file import based on van der Waals radii.
  :type generate_bonds: bool
  :param bounding_box: Generate an ad-hoc simulation cell as bounding box around the imported particles.
  :type bounding_box: bool
  :param centering: Translate particle coordinates and simulation box to center them at the coordinate origin. 
  :type centering: bool
  :param sort_particles: Makes the file reader reorder the loaded particles before passing them to the pipeline.
                         Sorting is based on the values of the ``Particle Identifier`` property loaded from the data file.
  :type sort_particles: bool
