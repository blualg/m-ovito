.. _file_formats.input.reaxff:

ReaxFF file reader
------------------

For loading interatomic bonds and their properties from text files written by the LAMMPS `fix reaxff/bonds <https://docs.lammps.org/fix_reaxff_bonds.html>`__ command
or the original standalone ReaxFF code of Adri van Duin.

This file reader is typically used in conjunction with a :ref:`particles.modifiers.load_trajectory` modifier to add a
varying list of bonds from a reactive MD simulation to a particle model.

In addition to the time-dependent :ref:`bond connectivity <scene_objects.bonds>`, the ReaxFF reader loads the particle properties
``Charge``, ``Atom Bond Order``, and ``Lone Pairs`` from the selected file and attaches them to the existing atomic model.
Furthermore, the property ``Bond Order`` is assigned to the :ref:`bonds <scene_objects.bonds>` as a new bond attribute.

This reader can load gzipped files (".gz" suffix) and zstd compressed files (".zst" suffix).