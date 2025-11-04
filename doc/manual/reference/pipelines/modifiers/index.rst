.. _particles.modifiers:

Modifiers
=========

Modifiers are the basic building blocks for creating a :ref:`data pipeline <usage.modification_pipeline>` in OVITO.
Like tools in a toolbox, each modifier implements a very specific, well-defined type of operation or computation, and typically you will need to
combine several modifiers to accomplish more complex tasks.

**List of modifiers available in OVITO:**

.. table::
  :width: 100%
  :widths: 28 72

  ================================================================= ==========================================================
  Analysis
  ================================================================= ==========================================================
  :ref:`particles.modifiers.atomic_strain`                          Calculates local strain tensors based on the relative motion of neighboring particles.
  :ref:`particles.modifiers.bond_analysis` |ovito-pro|              Computes bond angle and bond length distributions.
  :ref:`particles.modifiers.cluster_analysis`                       Decomposes a particle system into clusters of particles.
  :ref:`particles.modifiers.coordination_analysis`                  Determines the number of neighbors of each particle and computes the radial distribution function for the system.
  :ref:`particles.modifiers.difference_between_frames` |ovito-pro|  Calculates differences (delta values) between two simulation trajectory frames.
  :ref:`particles.modifiers.dislocation_analysis`                   Identifies dislocation defects in a crystal.
  :ref:`particles.modifiers.displacement_vectors`                   Calculates the displacements of particles based on an initial and a deformed configuration.
  :ref:`particles.modifiers.elastic_strain`                         Calculates the atomic-level elastic strain tensors in crystalline systems.
  :ref:`particles.modifiers.find_rings`                             Finds all rings in the bond topology of a particle system.
  :ref:`particles.modifiers.grain_segmentation`                     Determines the grain structure in a polycrystalline microstructure.
  :ref:`particles.modifiers.histogram`                              Computes the histogram of a property.
  :ref:`particles.modifiers.reduce_property` |ovito-pro|            Applies common reduction operations to properties.
  :ref:`particles.modifiers.scatter_plot`                           Generates a scatter plot of two properties.
  :ref:`particles.modifiers.bin_and_reduce` |ovito-pro|             Aggregates a particle property over a one-, two- or three-dimensional bin grid.
  :ref:`particles.modifiers.correlation_function`                   Calculates the spatial cross-correlation function between two particle properties.
  :ref:`particles.modifiers.structure_factor` |ovito-pro|           Calculates the structure factor :math:`S(k)` for a system of particles.
  :ref:`particles.modifiers.time_averaging` |ovito-pro|             Computes the average of some time-dependent input quantity over the entire trajectory.
  :ref:`particles.modifiers.time_series` |ovito-pro|                Plots the value of a global attribute as function of simulation time.
  :ref:`particles.modifiers.voronoi_analysis`                       Computes the coordination number, atomic volume, and Voronoi index of particles from their Voronoi polyhedra.
  :ref:`particles.modifiers.wigner_seitz_analysis`                  Identifies point defects (vacancies and interstitials) in a crystal lattice.
  ================================================================= ==========================================================

.. _particles.modifiers.coloring:

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Coloring
  =====================================================================================================================
  :ref:`particles.modifiers.ambient_occlusion`               Performs an ambient occlusion calculation to shade particles.
  :ref:`particles.modifiers.assign_color`                    Assigns a color to all selected elements.
  :ref:`particles.modifiers.color_by_type` |ovito-pro|       Colors particles or bonds according to a typed (discrete) property.
  :ref:`particles.modifiers.color_coding`                    Colors particles or bonds based on the value of a scalar (continuous) property.
  ========================================================== ==========================================================

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Modification
  =====================================================================================================================
  :ref:`particles.modifiers.affine_transformation`           Applies an affine transformation to the system.
  :ref:`particles.modifiers.combine_particle_sets`           Merges the particles and bonds from two separate input files into one dataset.
  :ref:`particles.modifiers.compute_property`                Assigns property values to particles or bonds according to a user-defined formula.
  :ref:`particles.modifiers.delete_selected_particles`       Removes the selected elements from the visualization.
  :ref:`particles.modifiers.edit_types`                      Lets you edit the types associated with a typed property, e.g. particle types.
  :ref:`particles.modifiers.freeze_property`                 Freezes the values of a dynamic particle property at a given animation time to make them available at other times.
  :ref:`particles.modifiers.load_trajectory`                 Loads time-dependent atomic positions from a separate trajectory file.
  :ref:`particles.modifiers.python_script` |ovito-pro|       Lets you write your own modifier function in Python.
  :ref:`particles.modifiers.show_periodic_images`            Duplicates particles and other data elements to visualize periodic images of the system.
  :ref:`particles.modifiers.slice`                           Cuts the structure along an infinite plane.
  :ref:`particles.modifiers.smooth_trajectory`               Computes time-averaged particle positions using a sliding window or generates intermediate sub-frames using linear interpolation.
  :ref:`particles.modifiers.unwrap_trajectories`             Computes unwrapped particle coordinates in order to generate continuous trajectories at periodic cell boundaries.
  :ref:`particles.modifiers.wrap_at_periodic_boundaries`     Folds particles located outside of the periodic simulation box back into the box.
  ========================================================== ==========================================================

.. _particles.modifiers.selection:

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Selection
  =====================================================================================================================
  :ref:`particles.modifiers.clear_selection`                 Resets the selection state of all elements.
  :ref:`particles.modifiers.expand_selection`                Selects particles that are neighbors of already selected particles.
  :ref:`particles.modifiers.expression_select`               Selects particles and other elements based on a user-defined criterion.
  :ref:`particles.modifiers.manual_selection`                Lets you select individual particles or bonds with the mouse.
  :ref:`particles.modifiers.invert_selection`                Inverts the selection state of each element.
  :ref:`particles.modifiers.select_particle_type`            Selects all elements of a particular type, e.g. all atoms of a chemical species.
  ========================================================== ==========================================================

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Structure identification
  =====================================================================================================================
  :ref:`particles.modifiers.bond_angle_analysis`             Identifies common crystal structures by an analysis of the bond-angle distribution.
  :ref:`particles.modifiers.centrosymmetry`                  Calculates the centrosymmetry parameter for every particle.
  :ref:`particles.modifiers.chill_plus`                      Identifies hexagonal ice, cubic ice, hydrate and other arrangements of water molecules.
  :ref:`particles.modifiers.common_neighbor_analysis`        Performs the common neighbor analysis (CNA) to determine local crystal structures.
  :ref:`particles.modifiers.identify_diamond_structure`      Identifies atoms that are arranged in a cubic or hexagonal diamond lattice.
  :ref:`particles.modifiers.polyhedral_template_matching`    Identifies common crystal structures using the PTM method and computes local crystal orientations.
  :ref:`particles.modifiers.vorotop_analysis`                Identifies local structure of particles using the topology of their Voronoi polyhedra.
  ========================================================== ==========================================================

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Visualization
  =====================================================================================================================
  :ref:`particles.modifiers.construct_surface_mesh`          Constructs a triangle mesh representing the surface of a solid.
  :ref:`particles.modifiers.create_bonds`                    Creates bonds between particles.
  :ref:`particles.modifiers.create_isosurface`               Generates an isosurface from a scalar field.
  :ref:`particles.modifiers.coordination_polyhedra`          Shows coordination polyhedra.
  :ref:`particles.modifiers.generate_trajectory_lines`       Generates trajectory lines from the time-dependent particle positions.
  ========================================================== ==========================================================

.. _particles.modifiers.pythonbased:

.. table::
  :width: 100%
  :widths: 28 72

  ========================================================== ==========================================================
  Python-based modifiers
  =====================================================================================================================
  :ref:`modifiers.calculate_local_entropy` |ovito-pro|       Computes local pair entropy fingerprints of particles.
  :ref:`modifiers.identify_fcc_planar_faults` |ovito-pro|    Discerns between stacking fault and twin boundary crystal defects.
  :ref:`modifiers.render_lammps_regions` |ovito-pro|         Visualize LAMMPS regions as surface meshes.
  :ref:`modifiers.shrink_wrap_box` |ovito-pro|               Resets the simulation cell to tightly fit all particles.
  ========================================================== ==========================================================

.. toctree::
  :maxdepth: 1
  :hidden:

  bond_angle_analysis
  affine_transformation
  ambient_occlusion
  assign_color
  atomic_strain
  bond_analysis
  calculate_local_entropy
  centrosymmetry
  chill_plus
  clear_selection
  cluster_analysis
  color_by_type
  color_coding
  combine_particle_sets
  common_neighbor_analysis
  compute_property
  construct_surface_mesh
  coordination_analysis
  coordination_polyhedra
  create_bonds
  create_isosurface
  delete_selected_particles
  difference_between_frames
  dislocation_analysis
  displacement_vectors
  edit_types
  elastic_strain
  find_rings
  expand_selection
  expression_select
  freeze_property
  generate_trajectory_lines
  grain_segmentation
  histogram
  identify_diamond
  identify_fcc_planar_faults
  invert_selection
  load_trajectory
  manual_selection
  polyhedral_template_matching
  python_script
  render_lammps_regions
  show_periodic_images
  scatter_plot
  select_particle_type
  shrink_wrap_box
  slice
  smooth_trajectory
  bin_and_reduce
  reduce_property
  correlation_function
  structure_factor
  time_averaging
  time_series
  unwrap_trajectories
  voronoi_analysis
  vorotop_analysis
  wigner_seitz_analysis
  wrap_at_periodic_boundaries
