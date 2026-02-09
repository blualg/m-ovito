.. _file_formats.input.pdb:

PDB file reader
---------------

For loading model structures stored in the `RCSB Protein Data Bank <https://www.rcsb.org>`__ (PDB) format.
OVITO can directly load gzipped PDB files (".gz" suffix).

Options
"""""""

Generate bounding box if needed
  If this option is enabled and no simulation cell is found in the PDB file (no ``CRYST1`` record), OVITO will generate an
  axis-aligned bounding box containing all atoms. This bounding box has open boundary conditions
  and can be used as an approximate simulation cell in subsequent calculations.

Center simulation cell on coordinate origin
  If enabled, OVITO shifts the geometric center of the simulation cell to the origin of the coordinate system, including all atoms.
  Otherwise, the simulation cell is positioned such that its corner is located at the coordinate origin.

Generate distance-based bonds
  Lets OVITO create ad-hoc :ref:`bonds <scene_objects.bonds>` between atoms based on their van der Waals radii. This is
  equivalent to inserting a :ref:`particles.modifiers.create_bonds` modifier into the pipeline.

Load bonds from file
  Tells OVITO to read pair-wise connections from the input file. These are identified by the ``CONECT``
  record in the PDB file.

.. _file_formats.input.pdb.python:

Python parameters
"""""""""""""""""

The file reader accepts the following optional keyword parameters in a call to the :py:func:`~ovito.io.import_file` or :py:meth:`~ovito.pipeline.FileSource.load` Python functions.

.. py:function:: import_file(location, sort_particles = False, bounding_box = False, centering = False, generate_bonds = True)
  :noindex:

  :param sort_particles: Makes the file reader reorder the loaded atoms before passing them to the pipeline.
                         Sorting is based on the atom serial numbers loaded from the PDB file.
  :type sort_particles: bool
  :param bounding_box: Generate an ad-hoc simulation cell as bounding box around the imported atoms.
  :type bounding_box: bool
  :param centering: Translate atom coordinates and simulation box to center them at the coordinate origin.
  :type centering: bool
  :param generate_bonds: If true, ad-hoc bonds are created between atoms during file import based on van der Waals radii. If false, bonds are loaded from ``CONECT`` records in the input file if present.
  :type generate_bonds: bool
