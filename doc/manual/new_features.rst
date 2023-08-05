.. _new_features:

=========
Changelog
=========

---------------------------
Version 3.9.1 (06-Aug-2023)
---------------------------

* Fix: Voronoi Analysis modifier crashes if simulation cell is degenerate or atom count is zero, and option `Generate neighbor bonds` is turned on
* |ovito-pro| New Python class :py:class:`ovito.pipeline.PipelineSourceInterface`
* |ovito-pro| New Python method :py:meth:`ModifierInterface.compute_trajectory_length() <ovito.pipeline.ModifierInterface.compute_trajectory_length>`, which gives user-defined modifiers control over the timeline length
* |ovito-pro| New Python field :py:attr:`Modifier.title <ovito.pipeline.Modifier.title>`
* |ovito-pro| Fixed :command:`ovitos -m pip install` failure for packages that require a build step

---------------------------
Version 3.9.0 (02-Aug-2023)
---------------------------

.. rubric:: Dark mode support on Windows

To enable the dark UI theme on Windows, go to the :ref:`application settings <application_settings.general>` and switch on :guilabel:`Enable automatic dark mode`.
OVITO will follow the Windows system color theme.

.. image:: /images/new_features/windows_dark_mode_support.png
  :width: 60%

.. rubric:: OpenSSH client integration |ovito-pro|

OVITO Pro is now able to access data files on remote machines using OpenSSH's :program:`sftp` utility, which fully supports smartcard authentication and other advanced
ssh features. See :ref:`usage.import.remote.openssh_connection_method` for further information.

.. rubric:: User-defined file format readers |ovito-pro|

This program release introduces a :ref:`programming interface for user-defined file readers <writing_custom_file_readers>`, which enables
you to develop parser functions for new file formats in Python. User-defined file readers are fully integrated into the
GUI of OVITO Pro and work seamlessly with the :py:func:`~ovito.io.import_file` function from the OVITO Python module.

.. rubric:: Discovery mechanism for Python extensions |ovito-pro|

Python extensions for OVITO Pro or the OVITO Python module (i.e. user-defined modifiers and file readers) can now be :ref:`packaged as Python modules <registering_custom_python_classes>`,
making it easier to deploy and install them (using :command:`pip install`). Custom extensions you've developed can be put under version control in a Git repo
and shared online with other OVITO users if desired -- we have set up the new `OVITO Extensions Directory <https://ovito-org.github.io/extensions-contrib-page/>`__ for that purpose.
After :ref:`easy installation on a user's computer <particles.modifiers.python_script.installing_extensions>`, OVITO Pro automatically discovers all extensions
and makes them available in the GUI.

.. image:: /images/new_features/python_extension_workflow.jpg
  :width: 90%
  :align: center

.. image:: /images/new_features/empty.png
  :width: 1%
  :align: center

.. image:: /images/new_features/python_settings_dialog.png
  :width: 50%
  :align: right

.. rubric:: New *Python Settings* dialog |ovito-pro|

The new :ref:`Python Settings dialog <python_settings_dialog>` provides access to all things related to a Python extension in OVITO Pro:

  * Configure the current working directory used for file I/O operations
  * Hot reload function for imported Python modules, which streamlines development of Python code located in external source files
  * Import the source code of installed extensions into the current program session to selectively customize functions when needed
  * List of all installed Python packages that are available for import by user code

.. rubric:: Support for more file formats

OVITO can now import DCD trajectory files, which are written by the CHARMM, NAMD, and LAMMPS simulation codes.
OVITO Pro and the OVITO Python module can additionally read :ref:`ASE trajectory files <file_formats.input.ase_trajectory>`.

.. rubric:: Further changes in this program release:

* Support for additional property data types (`float32`, `int8`) to reduce memory footprint of particle properties with low precision requirements (e.g. `Color`, `Selection`)
* OpenGL renderer: Performance optimizations, direct upload of `float32` and `int8` array values to GPU memory
* GSD file reader: Do not skip ``log/`` chunks containing ``/`` in their names (`issue #226 <https://gitlab.com/stuko/ovito/-/issues/226>`__)
* Fix: Color Coding modifier's "Adjust Range" function does not follow option "Only selected"
* Search patterns for trajectory file series: Avoid asterisk in file extensions containing digits, e.g. :file:`snapshot0000.h5` → :file:`snapshot*.h5`
* Data table file exporter does not require a :py:attr:`~ovito.data.DataTable.y`-property anymore
* Automatic name mangling of atom attributes imported from LAMMPS dump, GSD, and XYZ files in case they do not conform to OVITO's property naming rules
* The :ref:`Vulkan viewport renderer <application_settings.viewports.graphics_implementation>` has been temporarily disabled in this release
* |ovito-pro| New Python methods :py:meth:`Property.add_type_id <ovito.data.Property.add_type_id>` and :py:meth:`Property.add_type_name <ovito.data.Property.add_type_name>`
* |ovito-pro| New Python method :py:meth:`VoxelGrid.view <ovito.data.VoxelGrid.view>`
* |ovito-pro| Performance optimizations for property data access from Python code

---------------------------
Version 3.8.5 (19-Jun-2023)
---------------------------

* :ref:`particles.modifiers.voronoi_analysis` modifier now outputs per-face ``Area`` and ``Voronoi Order`` mesh properties
* PDB file reader: Refined detection of cells with periodic boundary conditions
* GSD file reader: Support time-varying radii of spherical type shapes; display simulation step numbers in timeline
* LAMMPS dump file reader: Automatically :ref:`map file columns to standard particle properties <file_formats.input.lammps_dump.property_mapping>` if names match
* Bug fix: Particle selection in data inspector is lost when playing animation or moving viewport camera
* Workaround for macOS (Apple Silicon) OpenGL stencil buffer issue: Highlighted particles not rendered correctly
* Update third-party libraries: ffmpeg 6.0, OpenSSL 1.1.1u, libssh 0.10.5, Qt 6.5.1, PySide6 6.5.1.1, HDF5 1.14.1-2, NetCDF 4.9.2

---------------------------
Version 3.8.4 (03-May-2023)
---------------------------

* Fix: ffmpeg video encoding crashes on Windows if output path contains non-ascii characters
* Silence console message "Numeric mode unsupported in the posix collation implementation" on Linux by enabling ICU support in Qt build
* |ovito-pro| Fix: Segfault in PySide6 package initialization on Linux when adding a Python layer to a viewport
* |ovito-pro| Fix: Interchanged xz/yz simulation box shear components in :py:func:`~ovito.io.lammps.lammps_to_ovito` Python function

---------------------------
Version 3.8.3 (16-Apr-2023)
---------------------------

* Further improved performance of sequential loading of compressed trajectory files
* Fixed regression (since v3.8.0): :py:meth:`Viewport.render_anim() <ovito.vis.Viewport.render_anim>` renders only first animation frame
* |ovito-pro| Python exceptions raised in user-defined modifier functions are now propagated up the call chain to where the pipeline evaluation was triggered
* |ovito-pro| Included ``bz2`` and `sqlite3` standard modules, which were missing in embedded Python interpreter on Linux

---------------------------
Version 3.8.2 (04-Apr-2023)
---------------------------

* Implemented fast access to trajectory frames in compressed (gzipped) files
* Fix: Segfault when using zoom function in viewport with an attached camera object
* Fix: Segfault in :ref:`particles.modifiers.coordination_polyhedra` modifier on Linux
* Fix: Function 'load/save session state' does not follow global working directory

---------------------------
Version 3.8.1 (27-Mar-2023)
---------------------------

.. rubric:: Identification of volumetric regions using the Gaussian density method |ovito-pro|

The :ref:`particles.modifiers.construct_surface_mesh` modifier's implementation of the :ref:`Gaussian density method <particles.modifiers.construct_surface_mesh.gaussian_density_method>`
has been extended to support the :ref:`identification of volumetric regions <particles.modifiers.construct_surface_mesh.regions>`, e.g. pores, cavities, and filled spatial regions.
Their respective surface areas and volumes are calculated and output by the modifier in tabulated form.

To make this possible, we have developed an extension to the `Marching Cubes algorithm <https://en.wikipedia.org/wiki/Marching_cubes>`__ for isosurface construction, which provides
the capability to identify disconnected spatial regions separated by the surface mesh and compute their enclosed volumes -- of course with full support for periodic boundary conditions.

.. image:: /images/new_features/surface_mesh_regions_gaussian_density_example.png
  :width: 25%

.. image:: /images/modifiers/construct_surface_mesh_regions_example_table.jpg
  :width: 50%

.. rubric:: New efficient Python method for computing neighbor lists |ovito-pro|

OVITO's Python interface now offers the new :py:meth:`CutoffNeighborFinder.find_all() <ovito.data.CutoffNeighborFinder.find_all>` method
for vectorized computation of neighbor lists for many or all particles at once.

.. rubric:: Further changes:

* LAMMPS data file reader: Accept '#' in type names, which are referenced in data sections of the file

---------------------------
Version 3.8.0 (03-Mar-2023)
---------------------------

.. rubric:: Develop custom modifiers with extended capabilities |ovito-pro|

A newly devised programming interface enables you to write advanced modifier functions in Python that

  * access **more than one** frame of a simulation trajectory,
  * perform computations that involve data **from several input files**, or
  * need control over the **caching** of computational results.

Take simulation post-processing to the next level! Develop your own trajectory analysis algorithms
in Python, which are fully integrated into OVITO's pipeline system and the interactive interface of OVITO Pro.

.. code-block:: python

    class CalculateIncrementalDisplacementsModifier(ModifierInterface):
        def modify(self, data, frame, input_slots, **kwargs):
            next_frame = input_slots['upstream'].compute(frame + 1)
            displacements = next_frame.particles.positions - data.particles.positions

Have a look at our completely revised :ref:`introduction to user-defined modifiers <writing_custom_modifiers>`
and check out the new :ref:`advanced programming interface for user-defined modifiers <writing_custom_modifiers.advanced_interface>`.

.. rubric:: Improved color legends

OVITO can now render tick marks in :ref:`color mapping legends <viewport_layers.color_legend>` to label intermediate values.
Furthermore, the legend's title may be rotated by 90 degrees:

.. image:: /images/new_features/color_legend_ticks_horizontal.png
  :width: 34%

.. image:: /images/new_features/color_legend_ticks_vertical.png
  :width: 12%

.. rubric:: File reader for ASE database files |ovito-pro|

Load atomic structures from database files of the `Atomic Simulation Environment (ASE) <https://wiki.fysik.dtu.dk/ase/>`__ into OVITO.
The :ref:`new file reader <file_formats.input.ase_database>` lets you scroll through all structures in a database or pick specific structures
using a query string. Metadata associated with structures is made available in OVITO as :ref:`global attributes <usage.global_attributes>`.

.. image:: /images/new_features/ase_database_reader.gif
  :width: 60%

.. rubric:: New modifier: :ref:`modifiers.identify_fcc_planar_faults` |ovito-pro|

Easily identify different planar defect types, such as **stacking faults** and **coherent twin boundaries**, in face-centered cubic (fcc) crystals.
We have developed a powerful classification algorithm for hcp-like atoms that make up such planar defects:

.. image:: /images/new_features/planar_faults.jpg
  :width: 30%

.. rubric:: New modifier: :ref:`modifiers.render_lammps_regions` |ovito-pro|

Use this new tool to generate mesh-based representations of the parametric regions defined in your `LAMMPS <https://docs.lammps.org/>`__ simulation,
e.g., cylinders, spheres, or blocks, and visualize the boundaries of these spatial regions along with the particle model:

.. image:: /images/new_features/lammps_regions.png
  :width: 60%

.. rubric:: Spatial binning modifier: New unity input option |ovito-pro|

This options offers a shortcut for calculating particle density distributions, i.e. counting the particles per grid cell.
Previous versions required first defining an auxiliary particle property with a uniform value of 1 to calculate the number density:

.. image:: /images/new_features/spatial_binning.png
  :width: 60%

See :ref:`particles.modifiers.bin_and_reduce` modifier.

.. rubric:: Support for LAMMPS dump grid files

OVITO can now read and visualize the `new volumetric grid file format written by recent LAMMPS versions <https://docs.lammps.org/Howto_grid.html>`__
thanks to the newly added :ref:`file_formats.input.lammps_dump_grid`:

.. image:: /images/new_features/volumetric_grid_discrete.png
  :width: 25%

.. rubric:: Slice modifier on voxel grids

When you apply the :ref:`Slice <particles.modifiers.slice>` modifier to a voxel grid,
cell values now get copied to the mesh faces and interpolated field values to the mesh vertices of the generated cross-section.
This enables both discrete and interpolated visualizations of the field values along arbitrary planar cross-sections:

.. image:: /images/new_features/volumetric_grid_slice_discrete.png
  :width: 25%

.. image:: /images/new_features/volumetric_grid_slice_interpolated.png
  :width: 25%

See :ref:`particles.modifiers.slice` modifier and :ref:`scene_objects.voxel_grid`.

.. rubric:: Support for point-based volumetric grids

In addition to the classical *cell-based* voxel grids, OVITO now also supports *point-based* volumetric grids,
in which field values are associated with the grid points instead of the voxel cells. All functions in OVITO
that operate on grids, e.g. the :ref:`particles.modifiers.create_isosurface` modifier, also support periodic and
mixed boundary conditions.

.. image:: /images/io/voxel_grid_types.png
  :width: 30%

See :py:attr:`ovito.data.VoxelGrid.grid_type` and :ref:`file_formats.input.cube`.

.. rubric:: Load Trajectory modifier now supports removal of particles

Previously, the :ref:`particles.modifiers.load_trajectory` modifier required the trajectory file to contain coordinates for all particles
that were initially present in the topology dataset. The improved version of the modifier can now deal with particles disappearing in later frames of a trajectory, e.g.,
when particles get removed from the simulation over time.

.. rubric:: Further additions and changes in this program release:

* Added dark mode UI support for Linux platform.
* :ref:`particles.modifiers.correlation_function` modifier: Added support for 2d simulations.
* :ref:`particles.modifiers.wrap_at_periodic_boundaries` modifier: Added support for 2d simulations.
* Save and restore maximized state of main window across program sessions.
* :ref:`file_formats.input.lammps_data` & writer: Added support for extended *Velocities* file section for when using LAMMPS atom styles *electron*, *ellipsoid*, or *sphere*.
* LAMMPS data file writer: Added the option to renumber all particle/bond/angle/dihedral/improper types during export. Avoids conversion problems from 0-based type IDs loaded from GSD files.
* New option to clip surfaces at open box boundaries (see :py:attr:`SurfaceMeshVis.clip_at_domain_boundaries <ovito.vis.SurfaceMeshVis.clip_at_domain_boundaries>`).
* :ref:`particles.modifiers.cluster_analysis` modifier: Abort calculation of center of mass and radius of gyration if masses of all input particles are zero.
* |ovito-pro| Added user option that makes OVITO Pro import multiple files of the same kind as separate objects into the scene.
* |ovito-pro| Accept ``os.PathLike`` objects in Python functions :py:func:`~ovito.io.import_file` and :py:func:`~ovito.io.export_file`.
* |ovito-pro| :py:meth:`PropertyContainer.create_property <ovito.data.PropertyContainer.create_property>`: Accept ``data`` values that are broadcastable to shape of property array.

----------------------------
Version 3.7.12 (16-Dec-2022)
----------------------------

* GRO file reader: `Recognize additional chemical symbols SI, FE, BR <https://www.ovito.org/forum/topic/only-first-letter-of-particle-types-read-from-gro-file/>`_.
* STL file reader: Tolerate leading whitespace on first line.
* Updated third-party libraries on Windows: Qt 6.4.1, OpenSSL 1.1.1s, ffmpeg 4.2.8, zlib 1.2.23.
* Fix: Voronoi cavity radius calculation is `wrong by a factor of 2 <https://www.ovito.org/forum/topic/only-first-letter-of-particle-types-read-from-gro-file/>`_.
* Fix: Function "Make Independent" does not work correctly for surface mesh visual elements in cloned pipelines.
* |ovito-pro| Fix: Python method  :py:meth:`ovito.data.SurfaceMesh.locate_point() <ovito.data.SurfaceMesh.locate_point>` can yield wrong results for coarse, one-sided meshes.

----------------------------
Version 3.7.11 (29-Oct-2022)
----------------------------

* Added user option to application settings dialog for changing the working directory behavior.
* Fixed regression: Slice modifier does not work on voxel grids.
* |ovito-pro| Vectorized all query methods of ``SurfaceMeshTopology`` class.
* |ovito-pro| Provide PyPI package for Python 3.11.
* |ovito-pro| Added flat array option to method ``SurfaceMesh.get_face_vertices()``.

----------------------------
Version 3.7.10 (09-Oct-2022)
----------------------------

* Optimization of main window UI widgets to improve rapid animation playback at high frame rates.
* Enhancements to the pipeline editor: Brief information display for some modifiers.
* New right-click context menu in pipeline editor: :ref:`clone_pipeline` Added 'Copy to...' function for copying modifiers within and across pipelines.
* |ovito-pro| Standalone Python module: Run in headless mode by default. OVITO_GUI_MODE env variable requests :ref:`rendering.opengl_renderer` support.
* |ovito-pro| PyPI package on Linux: Switched back to PySide6 version 6.2.4 for better backward compatibility with older Ubuntu distros.
* |ovito-pro| Fixed loading of files opened via double click in case license validation dialog pops up.
* |ovito-pro| Generalized the :ref:`visual_elements.vectors` element to support visualization of vector quantities in more types of ``PropertyContainers``.
* |ovito-pro| New Python function :py:meth:`ovito.modifiers.PolyhedralTemplateMatchingModifier.calculate_misorientation() <ovito.modifiers.PolyhedralTemplateMatchingModifier.calculate_misorientation>`.
* |ovito-pro| Automatic conversion of NumPy array scalars to Python numbers when storing them as OVITO global :py:class:`ovito.data.DataCollection` attributes.

---------------------------
Version 3.7.9 (12-Sep-2022)
---------------------------

* :ref:`particles.modifiers.voronoi_analysis`: Added calculation of cavity radius.
* GSD file importer/exporter: Added support for particle attributes `"angmom" and "body" <https://gsd.readthedocs.io/en/latest/schema-hoomd.html#particle-data>`_.
* Fix: *Affine Transformation* modifier not transforming particles in target-cell mode in rare situations (when called from Python).
* Fix: File import via drag & drop not working when Vulkan viewport renderer is active.
* Upgraded Qt cross-platform framework to version 6.3.1.
* Cluster analysis modifier: Warn user if center of mass cannot be computed due to cluster's total mass being zero.
* |ovito-pro| Upgraded :ref:`ovitos_interpreter` embedded interpreter to Python 3.10.6.
* |ovito-pro| :ref:`ovitos_install_modules` Installation of PyPI packages with the ``--user`` option in the embedded interpreter is now supported.
* |ovito-pro| New Python API for creating :py:class:`ovito.data.SurfaceMesh` objects.
* |ovito-pro| Improved operation of Python module in Jupyter environments. Interrupting long-running operations is fully supported now.
* |ovito-pro| New experimental Jupyter notebook visualization widget (:py:meth:`ovito.vis.Viewport.create_jupyter_widget() <ovito.vis.Viewport.create_jupyter_widget>`).
* |ovito-pro| Added Python API :py:class:`ovito.vis.ColorLegendOverlay` color_mapping_source.
* |ovito-pro| Fix: Segfault during Python statement ``del ovito.scene.pipelines[:]``.

---------------------------
Version 3.7.8 (29-Jul-2022)
---------------------------

* Fix: Program crash when quickly skipping through a trajectory consisting of a series of files loaded via SSH (regression OVITO 3.7.0).
* Fix: Visual artifacts when rendering cone primitives (3d arrow heads) at small length scales due to numerical precision issue.
* |ovito-pro| Added conda packages for Python 3.10.
* |ovito-pro| Added conda packages for macOS arm64/M1 platform.
* |ovito-pro| Work around a memory leak in some OpenGL graphics driver implementations when the :py:meth:`ovito.vis.Viewport.render_image() <ovito.vis.Viewport.render_image>` Python function is called repeatedly.

---------------------------
Version 3.7.7 (06-Jul-2022)
---------------------------

* Ubuntu 22.04 compatibility - Linux package of OVITO now includes a private copy of OpenSSL 1.1 libraries.

---------------------------
Version 3.7.6 (23-Jun-2022)
---------------------------

* PDB file reader: Added support for CP2K trajectory format.
* LAMMPS dump file reader: Recognize ``quat{ijkw}`` and ``shape{xyz}`` columns and automatically them to correct particle properties.
* Fix: Camera FOV parameter not animatable when rendering a movie.
* Fix: Segfault when loading .ovito state files written by OVITO 3.3 or older containing a Python script.
* Fix: Grain segmentation algorithm never terminates for particular inputs.
* PyPI package for Linux: disabled built-in SSH client to improve compatibility with Ubuntu 22.04, which doesn't provide OpenSSL 1.1 libraries anymore.
* |ovito-pro| New Python class :py:class:`ovito.data.SurfaceMeshTopology`, which provides script access to the face connectivity information of surface meshes.
* |ovito-pro| Conda channel now provides additional variants of the ```ovito`` <https://conda.ovito.org>`_ package (built against ``tbb`` v2020 and v2021), which avoids dependency conflicts with certain third-party packages when installing them in the same environment.

---------------------------
Version 3.7.5 (28-May-2022)
---------------------------

* Smooth trajectory modifier now supports varying number of particles.
* SSH client: Try password first before keyboard-interactive authentication for successful handshaking with some SSH servers.
* Performance improvements to OpenGL high-quality sphere rendering code
* Bug fix: Data inspector shows a 3rd text label in bar charts with 2 bars.
* Bug fix: Sporadic program crashes when importing CA files.
* |ovito-pro| :py:class:`ovito.data.DataCollection` attributes dictionary can now store arbitrary Python objects.
* |ovito-pro| New Python method :py:meth:`ovito.data.Particles.remap_indices() <ovito.data.Particles.remap_indices>`.
* |ovito-pro| New Python method :py:meth:`ovito.data.SurfaceMesh.to_triangle_mesh() <ovito.data.SurfaceMesh.to_triangle_mesh>`.
* |ovito-pro| Bumped maximum neighbor limit of :py:class:`ovito.data.NearestNeighborFinder` to 64.
* |ovito-pro| Dropped support for Python 3.6, which has reached its end-of-life date.

---------------------------
Version 3.7.4 (18-Apr-2022)
---------------------------

* :ref:`particles.modifiers.centrosymmetry` modifier: New option '*Use only selected particles*'.
* :ref:`file_formats.input.lammps_data`: Added support for *Ellipsoids* section.
* Fix: Program crash during file format detection when importing file from path containing CJK or other non-ANSI characters.
* Fix: Error "The file source path is empty or has not been set" when picking a new simulation file of different format.
* |ovito-pro| :ref:`particles.modifiers.construct_surface_mesh` modifer: New option 'Map particles to regions'.
* |ovito-pro| New Python methods :py:meth:`ovito.data.DataCollection.create_cell() <ovito.data.DataCollection.create_cell>`, :py:meth:`ovito.data.DataCollection.create_particles() <ovito.data.DataCollection.create_particles>`, :py:meth:`ovito.data.Particles.create_bonds() <ovito.data.Particles.create_bonds>`.

-----------------------------
Version 3.7.3 (29-Mar-2022)
-----------------------------

* DXA modifier now picks up partitioning established by *Grain Segmentation* modifier in the upstream pipeline, see `discussion in the forum <https://www.ovito.org/forum/topic/how/>`_.
* Fix: XYZ file column mapping is reset when using "Pick new file" function.
* Fix: App closes when using the "Pick new file" function under Linux (`issue #216 <https://gitlab.com/stuko/ovito/-/issues/216>`_).
* Fix: Segfault when deleting a disabled modifier from a branched pipeline.
* Fix: *Construct surface mesh* modifier sometimes produces incorrect cap polygons if the alpha-shape complex contains degenerate elements (`issue #217 <https://gitlab.com/stuko/ovito/-/issues/217>`_).
* Regression: Progress bar not updated correctly during execution of *Construct Surface Mesh* and *DXA* modifiers.
* Regression: Program does not exit if ``--help`` command line option is used.
* |ovito-pro| Added user documentation for Python-based modifiers :ref:`modifiers.calculate_local_entropy` and :ref:`modifiers.shrink_wrap_box`.
* |ovito-pro| Added .pyi stub files to :ref:`scripting_manual` Python package to support `auto-completions and mouse-over documentation in Python IDEs <https://www.ovito.org/forum/topic/documentaton-typing-for-python-module-as-stubs-pyi-files-to-be-picked-up-by-ide-for-autocompletion-and-type-checking/>`_.
* |ovito-pro| :py:class:`ovito.data.CutoffNeighborFinder` now accepts non-periodic simulation cells that are degenerate.
* |ovito-pro| Fix: :py:meth:`ovito.data.DataTable.xy() <ovito.data.DataTable.xy>` method generates wrong x-coords array if data table interval doesn't start at 0.

---------------------------
Version 3.7.2 (03-Mar-2022)
---------------------------

* Improved render output window with image zoom function.
* Fix: Particle type colors not initialised correctly if imported LAMMPS dump file contains both 'type' and 'element' columns (`issue #193 note 403792737 <https://gitlab.com/stuko/ovito/-/issues/193#note_403792737>`_).
* |ovito-pro| macOS: Fixed PySide6 loading error due to wrong rpath information when importing PyPI ovito package.
* |ovito-pro| Linux: Fixed sqlite3 Python package included in the embedded Python interpreter of OVITO Pro.

---------------------------
Version 3.7.1 (26-Feb-2022)
---------------------------

* Fixed regression: Segfault when loading session state file containing a viewport camera object.
* |ovito-pro| New Python function :py:meth:`ovito.data.NearestNeighborFinder.find_all() <ovito.data.NearestNeighborFinder.find_all>`.
* |ovito-pro| :py:class:`ovito.data.PropertyContainer` classes support removing properties with the ``del`` statement.
* |ovito-pro| Inform user if insufficient file access permissions let license activation fail.

---------------------------
Version 3.7.0 (15-Feb-2022)
---------------------------

* Visual element and particle type settings can now be preserved when picking a new input simulation file in the :ref:`scene_objects.file_source` external file panel.
* Support for HTML formatted text in viewport layers :ref:`viewport_layers.text_label`, :ref:`viewport_layers.color_legend`, and :ref:`viewport_layers.coordinate_tripod`.
* Improved color quality of animated GIFs produced by OVITO
* Added dark mode UI support on macOS.
* Availability of native arm64/M1 builds of OVITO Basic, OVITO Pro and the OVITO Python package for Apple Silicon machines.
* Ported OVITO code base from C++14 to C++17 language standard.
* Switched from old Qt 5.x to version 6.2 of the Qt cross-platform C++ framework and, correspondingly, from PySide2 to `PySide6 <https://pypi.org/project/PySide6/>`_. (Exception: Packages for Anaconda, where dependencies Qt6/PySide6 are not yet available).
* Completely reworked and modernized the internal asynchronous task system and the scene rendering framework of OVITO.
* New standard :ref:`scene_objects.bonds`  property "Width", which allows controlling the diameter of bond cylinders on a per-bond basis.
* Added detailed documentation for some of the file readers of OVITO. See the :ref:`file_formats.input`.
* LAMMPS data file reader & writer: Added preliminary support for `type labels <https://github.com/lammps/lammps/pull/2531>`_, which will be supported by a future version of LAMMPS.
* :ref:`file_formats.input.lammps_dump`: Map columns ``c_diameter[...]`` to particle property ``Aspherical Shape`` and perform division by 2.
* GSD file reader & writer: Added support for angles/dihedrals/impropers.
* User can now rename individual structure types in the UI of structure identification modifiers.
* Implemented new OpenGL rendering technique `*Weighted Blended Order-Independent Transparency* <https://jcgt.org/published/0002/02/09/>`_, providing an alternative to the classical painter's algorithm. Can be activated in the app settings dialog and gives better results if there's a mix of several different object types (e.g. particles and surfaces) that are all semi-transparent.
* Detect if the triangle mesh is not closed when loading a custom particle shape. Automatically disable back-face culling for the particle type in this case.
* CA file reader: Compute dislocation line statistics for re-imported datasets the same way the DXA modifier does.
* Fix: Particles visual element does not use uniform scaling factor when rendering some non-spherical particle shapes.

---------------------------
Version 3.6.0 (19-Nov-2021)
---------------------------

* :ref:`visual_elements.vectors`, :ref:`visual_elements.surface_mesh`, :ref:`visual_elements.voxel_grid`, :ref:`visual_elements.trajectory_lines` visual elements: Added direct color mapping option as a faster alternative to the :ref:`particles.modifiers.color_coding` modifier.
* :ref:`visual_elements.bonds` visual element: Added explicit control of the coloring mode.
* Made number and :ref:`viewport_layouts` configurable by the user.
* Visibility of pipelines can be controlled on a :ref:`usage.viewports.menu` per viewport basis.
* :ref:`particles.modifiers.coordination_polyhedra` modifier now makes particle properties available for the color coding as mesh region properties and mesh vertex properties.
* :ref:`particles.modifiers.generate_trajectory_lines` modifier: New capability to transfer time-dependent particle properties to the trajectory lines.
* :ref:`particles.modifiers.load_trajectory` modifier: Support non-contiguous atom IDs in LAMMPS bond dump files
* Added file reader for binary STL files.
* :ref:`custom_initial_session_state`: New mechanism for customizing the initial program session state.
* Raised limit on the number of FFT bins in *Spatial Correlation Modifier* to support finer grid resolutions.
* Fix: PTM modifier may crash if graphene/diamond are the only enabled structure types.
* Fix: Traced trajectory lines may be rendered in wrong colors.
* Took out code that transmits random installation ID to web server.
* OpenSSL shared libraries are no longer shipped with OVITO for Linux to avoid compatibility issues on some Linux distributions.
* |ovito-pro| :ref:`viewport_layouts.rendering`: Added capability to render multi-viewport layouts in one step.
* |ovito-pro| Python code generator has been extended to generate code for all visual elements and for reenacting manual changes made by the user to data objects (e.g. particle type names, color, radii).
* |ovito-pro| Added the ``input_format`` keyword parameter to the :py:func:`ovito.io.import_file` Python function for specifying the file format explicitly.
* |ovito-pro| Upgraded OSPRay to version 2.7.1.
* |ovito-pro| Renamed :py:meth:`ovito.vis.Viewport.create_qt_widget() <ovito.vis.Viewport.create_qt_widget>` method and made it work in all distributions of the ``ovito`` Python module.
* |ovito-pro| Added experimental :py:meth:`ovito.vis.Viewport.create_jupyter_widget() <ovito.vis.Viewport.create_jupyter_widget>` method for embedding OVITO viewports in Jupyter notebooks (see `demo binder <https://gitlab.com/stuko/ovito-binder>`_).
* |ovito-pro| Support for site-wide software licenses.
* |ovito-pro| Fix: Bounding box clipping artifact when rendering rotated superquadrics particles with OSPRay or Tachyon renderers.
* |ovito-pro| Fix: Warning "This plugin does not support createPlatformOpenGLContext!" when running in headless mode on Linux machines.

---------------------------
Version 3.5.4 (31-Jul-2021)
---------------------------

* LAMMPS data file reader and writer now support all LAMMPS atom styles, including the ``hybrid`` style.
* Fix: Construct surface mesh with region identification fails or never completes for some inputs.
* |ovito-pro| Fix: Tachyon renderer crashes when triangle mesh contains a degenerate vertex normal.

---------------------------
Version 3.5.3 (30-Jun-2021)
---------------------------

* Added two :ref:`tutorials` to the documentation.
* :ref:`visual_elements.voxel_grid` visual element now supports mouse-over data display in the status bar.
* Added invert function to :ref:`particles.modifiers.manual_selection` modifier.
* Warn user if OVITO Python module was installed via ``pip`` command in an Anaconda Python interpreter. Use ``conda install`` instead!
* Fix: *Configure Trajectory Playback* dialog shows no contents.
* Fix: Neighbor finder facilities do not ignore PBC flag along third dimension in 2D mode.

---------------------------
Version 3.5.2 (26-May-2021)
---------------------------

* :ref:`particles.modifiers.affine_transformation` modifier now allows entering the translation vector in reduced cell coordinates.
* :ref:`particles.modifiers.load_trajectory` modifier can now import ReaxFF bond information files written by the LAMMPS `fix reax/c/bonds <https://lammps.sandia.gov/doc/fix_reaxc_bonds.html>`_ command.
* GSD file reader: Fill particle property array with default values if a chunk is not present in current frame (`issue #206 <https://gitlab.com/stuko/ovito/-/issues/206>`_)
* |ovito-pro| Fix: Invisible simulation cell edges when rendering image with orthographic projection with OSPRay

---------------------------
Version 3.5.1 (18-May-2021)
---------------------------

* The :ref:`particles.modifiers.coordination_analysis` modifier has gained an option '*Only selected particles*', which restricts RDF calculation to a subset of particles.
* The '*Generate neighbor bonds*' option of the :ref:`particles.modifiers.voronoi_analysis` modifier is now able to deal with small periodic simulation cells.
* Fix: Wireframe line rendering issue in perspective viewports.
* |ovito-pro| The :ref:`particles.modifiers.slice` modifier now accepts (*hkl)* Miller indices as input for defining the plane orientation. The plane position can be specified in terms of the interplanar spacing.
* |ovito-pro| OVITO Pro for Linux now ships with a current Python 3.9.5 interpreter.
* |ovito-pro| Fix: :py:meth:`ovito.data.PropertyContainer.create_property() <ovito.data.PropertyContainer.create_property>` method cannot create user-defined property of data type ``int64``.

---------------------------
Version 3.5.0 (02-May-2021)
---------------------------

* Pipeline editor supports drag-and-drop operations, which allow easy rearranging of modifiers with the mouse.
* Modifiers can be grouped in the pipeline editor to collapse complex sequences of modifiers into a single list entry.
* :ref:`viewport_layers.color_legend` can render a legend for typed particle properties, showing the discrete colors representing the defined particle types.
* New implementation of the OpenGL viewport renderer. Provides better compatibility with GPU hardware, older OpenGL drivers, and virtual machine environments. OVITO now works on systems with only OpenGL 2.1 support (previous OVITO version required OpenGL 3.2).
* New viewport renderer based on the Vulkan graphic hardware interface as an alternative option to the OpenGL renderer. Can be activated in the application settings dialog (not available on macOS). Supports rendering in head-less mode on HPC nodes with GPU hardware.
* New :ref:`usage.modification_pipeline` pipeline selector widget in the toolbar of OVITO, which lets you manage the data pipelines in the current scene and add new pipelines.
* Extended the :ref:`particles.modifiers.create_bonds` modifier. A new parameter-free mode allows creating bonds based on van der Waals radii of the atoms.
* Performance improvement: *Create Bonds* modifier can now make use of multiple processor cores.
* *Affine Transformation* modifier can now transform triangle meshes (imported from STL, OBJ, VTK files).
* Several file format readers now provide the option to generate interatomic bonds during data import (relieves from having to apply the *Create Bonds* modifier).
* Some file format readers provide a new option to dynamically recenter the simulation cell on the coordinate origin. Useful for visualizing trajectories with varying cell shape.
* Gromacs, PDB, and mmCIF file readers now import atom names and residue names as particle properties.
* Internal chemical database of OVITO has been extended to include all elements and mass information, which will be assigned to particle types during file import.
* The *Particles* visual element provides a :ref:`visual_elements.particles` new parameter controlling the uniform scaling of atom radii. Useful for quickly producing a typical "balls-and-sticks" representation of a molecular structure.
* A bonds-only visualization of a molecular structure (with particles turned off) now adds spheres at the nodal points of the bond network to yield a typical "stick" representation.
* XYZ file reader now supports the exyz format variant of OpenBabel.
* Fix: CFG file reader loosing particle type settings during file reload.
* Fix: Segfault when loading certain NetCDF files with >1M particles.
* Fix: Error when deleting some regions of a surface mesh structure.
* Fix: Slow performance of *Particles* visual element when some particle types use mesh-based shapes.
* Rearranged the :ref:`core.render_settings` panel. The *viewport preview mode* can now be activated from here.
* Extended the Python API to support :py:class:`ovito.data.VoxelGrid` from scripts.
* `OVITO User Manual <https://ovito.org/docs/current/index.html>`_ uses a new layout theme and supports full-text search.
* Environment variable OVITO_LOG_FILE allows redirecting terminal output of OVITO to a text file (useful on Windows platform, where console output is otherwise inaccessible).
* |ovito-pro| New modifier :ref:`particles.modifiers.color_by_type` modifier for recoloring particles based on one of their typed properties, e.g. discrete ``Residue Type`` or ``Atom Name`` property.
* |ovito-pro| New pipeline data source type :ref:`data_source.python_script`. Run a user-defined Python function that builds or synthesizes an input ``DataCollection`` for a pipeline (instead of loading a structure from disk). Can also be used to import data formats into OVITO which are not directly supported by the software.
* |ovito-pro| :ref:`data_source.lammps_script`: The new data pipeline source type *LAMMPS script* allows editing and executing LAMMPS input scripts within OVITO to generate a dataset using LAMMPS commands. Useful for prototyping LAMMPS simulation setups with immediate visual feedback in OVITO.
* |ovito-pro| Updated OSPRay rendering library to version 2.5.0, offering a better denoising filter.
* |ovito-pro| *Spatial Binning* modifier can now process vector particle properties in addition to scalar properties.
* |ovito-pro| Python API: Added the method :py:meth:`ovito.data.CutoffNeighborFinder.find_at() <ovito.data.CutoffNeighborFinder.find_at>` for enumerating all particles around an arbitrary spatial position.
* |ovito-pro| Python code generator: Emit valid code for visualization setups including a ``PythonViewportLayer``.
* |ovito-pro| Python code generator: Emit call to ``generate()`` method of *Generate Trajectory Lines* modifier.
* |ovito-pro| Fix: Made auto-crop function work for pictures rendered with OSPRay and denoising filter enabled.
* |ovito-pro| Fix: Python viewport layer does not get called with current values of user-defined parameters.

---------------------------
Version 3.4.4 (12-Mar-2021)
---------------------------

* Fix: Number of data columns not correctly detected for XYZ files with 5 atoms or less.
* Fix: Program crash when playing back animation with less than 1 frame per second in interactive viewports.
* Fix: Simulation cell not visible in interactive viewports on some computer systems (`issue #203) <https://gitlab.com/stuko/ovito/-/issues/203>`_.
* Fix: CIF file reader not automatically recognizing files written by Open Babel (`issue #204) <https://gitlab.com/stuko/ovito/-/issues/204>`_.
* |ovito-pro| Fix: OSPRay not rendering arrow glyphs correctly.

---------------------------
Version 3.4.3 (25-Feb-2021)
---------------------------

* Added text outline option to *Coordinate Tripod* viewport layer.
* Fixed UI issue: Status bar resizing due to invalid unicode character in text string.
* Corrected camera orientation of "Bottom" viewport view type when rotation constraint is turned on.
* Improved automatic detection of PDB file format.
* |ovito-pro| It's now okay to assign a simple string to the :py:class:`ovito.modifiers.ExpressionSelectionModifier` expression field.

---------------------------
Version 3.4.2 (15-Feb-2021)
---------------------------

* Long text strings displayed in the status bar of OVITO now get broken into two lines in order to show more property values in the available space.
* Bug fix: Status bar doesn't display latest set of particle properties while positioning the mouse cursor over a particle. This fix corrects a regression introduced with OVITO 3.4.0.
* Fixed a limitation of the PTM modifier not identifying diamond and graphene structures in small periodic simulation cells.

---------------------------
Version 3.4.1 (03-Feb-2021)
---------------------------

* Fixed runtime linker error when importing ``ovito`` Python module installed via pip on Linux.
* |ovito-pro| Spatial binning modifier can now operate on vectorial particle properties.

---------------------------
Version 3.4.0 (28-Jan-2021)
---------------------------

* Backward incompatible .ovito state file format change: Program sessions saved with OVITO 3.4 or later cannot be opened in previous versions!
* Extensive redesign of OVITO's internal C++ data object model to make it thread-safe. User experience and Python API remain largely unaffected.
* State files (.ovito) now store relative paths to imported data files, enabling the relocation of an entire directory tree containing the state file and the data file without breaking the reference.
* Rewrite of the OpenGL rendering code, making use of geometry shaders on a wider range of hardware. OVITO now requires OpenGL 3.0 or higher (previous releases required OpenGL 2.1).
* Color Coding modifier: Added an auto-adjust option, which dynamically adjusts the min/max interval to the current range of input values.
* File importers reading the ``Velocity`` vector particle property automatically generate the ``Velocity Magnitude`` particle property too.
* OVITO can now visualize particles with `superellipsoid shapes <https://en.wikipedia.org/wiki/Superellipsoid>`_, which are controlled by the ``Superquadric Roundness`` particle property.
* Preliminary file reader support for ParaView VTP, VTI, VTM and PVD formats, as written by the Aspherix DEM simulation code.
* |ovito-pro| OVITO Pro gives the user the option to edit Python scripts in an external editor application or IDE (e.g. Visual Studio Code). Changes the user makes to the script code in the external editor are automatically loaded back into OVITO Pro.
* |ovito-pro| The Python script modifier displays the current working directory and lets the user control it if necessary.
* |ovito-pro| Python-based viewport layers now support user-defined parameters passed to the ``render()`` function.
* |ovito-pro| OSPRay and Tachyon renderers can now render polyhedral meshes with highlighted edges (wireframe overlay).
* |ovito-pro| New Python method :py:meth:`ovito.data.NearestNeighborFinder.find_at() <ovito.data.NearestNeighborFinder.find_at>`.

---------------------------
Version 3.3.5 (12-Dec-2020)
---------------------------

* Extended the :ref:`particles.modifiers.smooth_trajectory` modifier to interpolate/average all scalar and continuous particle properties.
* Fixed handling of stacking faults of arbitrary thickness in :ref:`particles.modifiers.grain_segmentation` algorithm.
* |ovito-pro| Fixed shading issue for ellipsoidal particles in OSPRay renderer.
* |ovito-pro| Fixed z-clipping issue in OSPRay and Tachyon renderers for viewports with parallel projections.

---------------------------
Version 3.3.4 (27-Nov-2020)
---------------------------

* Another tweak to the PTM algorithm to fix a regression, which let the PTM modifier fail to correctly identify some BCC atoms.

---------------------------
Version 3.3.3 (23-Nov-2020)
---------------------------

* :ref:`particles.modifiers.construct_surface_mesh` modifier: New capability to compute distance of each particle from closest point on the surface.
* Added a user option to :ref:`viewport_layers.text_label`, which allows controlling the output precision and formatting of decimal values.
* Added support for the improved binary dump file format introduced with `LAMMPS stable release 29-Oct-2020 <https://github.com/lammps/lammps/releases/tag/stable_29Oct2020>`_.
* Fixed an `issue in the PTM algorithm <https://www.ovito.org/forum/topic/problems-when-using-polyhedral-template-matching-ptm-modifier/>`_ letting the identification of BCC atoms sporadically fail if exactly arranged on a perfect lattice.
* |ovito-pro| Fixed visual issue in OSPRay renderer when rendering semi-transparent ellipsoidal particles.
* |ovito-pro| New code example showing how to :py:class:`ovito.modifiers.CoordinationAnalysisModifier` access partial RDFs computed by ``CoordinationAnalysisModifier``.

---------------------------
Version 3.3.2 (12-Nov-2020)
---------------------------

* Included shared library ``libxcb-xinerama.so`` in binary package for Linux, which may not be present on some systems by default.

---------------------------
Version 3.3.1 (13-Oct-2020)
---------------------------

* Grain segmentation modifier: Fixed a bug in the handling of stacking faults.
* |ovito-pro| Support license option that allows running the GUI on arbitrary nodes of a computing cluster.

---------------------------
Version 3.3.0 (07-Oct-2020)
---------------------------

* New option in :ref:`application_settings.general`: Sort list of available modifiers by name instead of category.
* Data plot window now support mouse interaction (zooming/panning), allowing you to take a closer look at the displayed graph.
* :ref:`particles.modifiers.create_isosurface` modifier: Iso-value can now be set with the mouse by clicking into the histogram plot.
* :ref:`particles.modifiers.create_isosurface` modifier: New option 'Transfer field values to surface' for mapping the all voxel grid properties to the isosurface, which can subsequently be used to locally color the generated surface according to some secondary field quantity.
* :ref:`particles.modifiers.slice` modifier: Now supports slicing of :ref:`scene_objects.voxel_grid` to extract planar cross-sections.
* Added a quick search field for quickly accessing modifiers and other program commands in the GUI.
* The file selection dialog now allows importing multiple files at once. In particular, combinations of topology and trajectory files can now be opened in one step, with OVITO inserting the :ref:`particles.modifiers.load_trajectory` modifier automatically.
* Added a file reader for the Gromacs GRO and XTC file formats.
* Added option 'Ignore particle identifiers' to LAMMPS data file exporter, which will reassign a contiguous range of identifiers to the exported atoms.
* New implementation of the PDB file reader based on the third-party `*Gemmi* <https://gemmi.readthedocs.io/en/latest/>`_ library, providing better compatibility with a wide range of files.
* :ref:`particles.modifiers.expression_select` modifier: Expressions for selecting bonds can now reference properties of the two connected particles.
* Viewport camera can now be controlled using arrow keys. Use shift key for panning.
* OVITO is now based on Qt 5.15.1, fixing some UI issues on high-DPI screens under Windows.
* |ovito-pro| New modifier: :ref:`particles.modifiers.time_series` - a conventient tool for plotting the time evolution of global simulation attributes in OVITO.
* |ovito-pro| :ref:`particles.modifiers.time_averaging` modifier: Support multiple input quantities to be averaged simultaneously.
* |ovito-pro| :ref:`particles.modifiers.bin_and_reduce` modifier: The output ``VoxelGrid`` object now has the identifier ``'binning'`` instead of ``'binning[]'``. Note that this represents a breaking change to existing Python scripts employing the :py:class:`ovito.modifiers.SpatialBinningModifier` class.
* |ovito-pro| Bug fix: Python code generator includes ``'LAMMPSAtomStyle.'`` prefix in ``import_file()`` calls.

---------------------------
Version 3.2.1 (28-Aug-2020)
---------------------------

* Load Trajectory modifier: Support trajectory datasets containing more particles than the topology dataset.
* LAMMPS dump file reader: Support files containing both ``type`` and ``element`` data columns (`issue #193 <https://gitlab.com/stuko/ovito/-/issues/193>`_).
* OVITO for Windows: Fixed UI layout on high-DPI screens.
* Upgraded third-party software components: Qt 5.15, Python 3.8.5, NetCDF 4.7.4, Libssh 0.9.4, OSPRay 2.2.0.
* |ovito-pro| OSPRay: Applied patch to fix a memory leak (`issue #196 <https://gitlab.com/stuko/ovito/-/issues/196>`_).

---------------------------
Version 3.2.0 (10-Aug-2020)
---------------------------

* Added a file reader for the `PDBx/mmCIF <http://mmcif.wwpdb.org>`_ file format.
* :ref:`visual_elements.vectors`: Added offset and transparency parameters.
* Extended the :ref:`particles.modifiers.load_trajectory` modifier to support loading dynamic bond topologies from ```dump local`` <https://lammps.sandia.gov/doc/dump.html>`_ files written in LAMMPS reactive MD simulations.
* New modifier: :ref:`particles.modifiers.interactive_molecular_dynamics` for live visualization of running MD simulations.
* :ref:`particles.modifiers.cluster_analysis` modifier: Added calculation of weighted center of mass and radius of gyration.
* LAMMPS dump file reader: Support files written with ```dump_modify units yes`` <https://lammps.sandia.gov/doc/dump_modify.html>`_.
* |ovito-pro| New :ref:`particles.modifiers.bond_analysis` modifier for calculating bond-angle and bond-length distributions.
* |ovito-pro| New Python-based modifier for calculating the `local entropy fingerprint <https://doi.org/10.1063/1.4998408>`_ proposed by P. M. Piaggi and M. Parrinello.
* |ovito-pro| Python modifiers can now define their own function parameters. The values can be edited in the user interface and get stored in *.ovito* files.
* |ovito-pro| Added the :py:meth:`ovito.data.PropertyContainer.delete_elements() <ovito.data.PropertyContainer.delete_elements>` and :py:meth:`ovito.data.PropertyContainer.delete_indices() <ovito.data.PropertyContainer.delete_indices>` methods for deleting particles, bonds, etc.
* |ovito-pro| Added the :py:meth:`ovito.data.CutoffNeighborFinder.neighbor_distances() <ovito.data.CutoffNeighborFinder.neighbor_distances>` and :py:meth:`ovito.data.CutoffNeighborFinder.neighbor_vectors() <ovito.data.CutoffNeighborFinder.neighbor_vectors>` Python methods.
* |ovito-pro| Added the :py:meth:`ovito.vis.Viewport.create_jupyter_widget() <ovito.vis.Viewport.create_jupyter_widget>` method for building simple user interfaces that make use of OVITO's interactive visualization capabilities.

---------------------------
Version 3.1.3 (30-Jul-2020)
---------------------------

* PTM modifier: Fixed identification of chemically ordered binary structures, which got broken in a recent update
* PDB file format reader: Support for datasets with more than 9,999 atoms (see `merge request 25 <https://gitlab.com/stuko/ovito/-/merge_requests/25>`_)

---------------------------
Version 3.1.2 (13-Jul-2020)
---------------------------

* New option for turning off :ref:`scene_objects.file_source` automatic generation of file search patterns
* New :ref:`scene_objects.file_source` Configure Trajectory Playback dialog, allows controlling the mapping of trajectory frames to animation frames
* GSD file reader: Now accepts ellipsoid shape definitions with principal axes b=0 and/or c=0
* Bug fix: Animation rendering process cannot be canceled sometimes
* |ovito-pro| Added :py:meth:`ovito.data.DislocationNetwork.set_segment() <ovito.data.DislocationNetwork.set_segment>` method for manipulating dislocation line data from Python

---------------------------
Version 3.1.1 (21-Jun-2020)
---------------------------

* Bug fix: LAMMPS data file reader fails to correctly read '*Masses*' file section with irregularly ordered atom types
* |ovito-pro| Time averaging modifier: Check that x-values of data points are constant when averaging a data table

---------------------------
Version 3.1.0 (14-Jun-2020)
---------------------------

* |ovito-pro| New feature: :ref:`python_code_generation`
* New feature: :ref:`particles.modifiers.grain_segmentation`

---------------------------
Version 3.0.1 (05-Jun-2020)
---------------------------

* Bug fix: Adjusted internal parameter of :ref:`particles.modifiers.construct_surface_mesh` modifier to avoid sporadic error "Adjacent cell face not found" for periodic systems.
* Enhancement: LAMMPS dump file reader can now parse "diameter" file column as ``Radius`` particle property, automatically performing division by 2.

---------------------------
Version 3.0.0 (30-May-2020)
---------------------------

* :ref:`particles.modifiers.smooth_trajectory` modifier interpolates particle orientations in addition to particle positions.
* Added capability to :ref:`particles.modifiers.construct_surface_mesh` modifier to identify pores/voids and individually compute their volumes and surface areas.
* Extended the :ref:`rendering.ospray_renderer` with a sky & sun light source.
* Integrated copy protection into OVITO Pro builds.
* Introduced `OVITO_THREAD_COUNT </docs/current/python/introduction/advanced_topics.php#multithreading-settings>`_ environment variable for controlling the number of CPU cores used by OVITO.
* Added old VASP 4.x file format to POSCAR reader (in order to support files written by ASE).
* Added user option to LAMMPS data writer for omitting the "Masses" file section.
* OVITO can now read/write extended topology information from/to LAMMPS data files: angles/dihedrals/impropers.
* Bug fix: Output of Displacements/Atomic Strain/Wigner-Seitz modifiers can be all zeros in relative offset mode (dev679 regression).
* Eliminated bottlenecks in GUI, which slowed down animation playback of long trajectories (>100k frames).
* Correct handling of LAMMPS dump files with dual data columns (`issue #193 <https://gitlab.com/stuko/ovito/-/issues/193>`_).
* Updated OSPRay renderer to version 2.1.0 and integrated OSPRay plugin into Anaconda build of OVITO.
* Bug fix: Color Coding modifier does not list available bond properties.
* Added HTTP protocol support. OVITO can now :ref:`import data files stored on a web server <usage.import.remote>`.
* OVITO is now also available as an Anaconda package on Windows.
* Improvements to the :ref:`Adjust View <viewports.adjust_view_dialog>` dialog, which now allows interacting with the viewports while being open.
* Bug fix: Installing a third-party extension module in *ovitos* fails on macOS due to security restrictions (`issue #191 <https://gitlab.com/stuko/ovito/-/issues/191>`_).
* Bug fix: Checkbox " File contains multiple timesteps" in GUI does not reflect correct state of file reader.
* Integrated PM Larsen's scheme of calculating the :ref:`particles.modifiers.centrosymmetry` parameter using minimum-weight matching.
* New ``crop`` option in :py:meth:`ovito.vis.Viewport.render_image() <ovito.vis.Viewport.render_image>` Python function.
* Windows installer is now digitally signed to be compatible with Microsoft Authenticode.
* The embedded Python interpreter ``ovitos`` now ignores ``PYTHONPATH`` environment variable and user package directories (`issue #189 <https://gitlab.com/stuko/ovito/-/issues/189>`_).
* Fixed LAMMPS data file export for simulation cells aligned along negative coordinate axes (`issue #188 <https://gitlab.com/stuko/ovito/-/issues/188>`_).
* New algorithm type in :ref:`particles.modifiers.common_neighbor_analysis`: Interval CNA
* Bug fix: Crash during animation frame change when using DXA modifier.
* New "Denoising filter" option in :ref:`OSPRay renderer <rendering.ospray_renderer>`, which greatly improves the visual quality of rendered images.
* New "Every Nth frame" option in :ref:`animation settings dialog <animation.animation_settings_dialog>`.
* Combined ``bin_count_{xyz}`` parameter fields of :ref:`SpatialBinningModifier <particles.modifiers.bin_and_reduce>` class into single ``bin_count`` field.
* GSD file reader: Added support for the ``SphereUnion`` particle shape specification (see `here <https://gitlab.com/stuko/ovito/-/issues/177>`_).
* Fix of UI bug: Data inspector panel not opening correctly in some situations.
* OVITO now available as a `Conda package <https://www.ovito.org/python-downloads/>`_ for Linux and macOS.
* Extended the :py:meth:`ovito.vis.Viewport.zoom_all() <ovito.vis.Viewport.zoom_all>` Python function.
* Fixed broken (since dev679) OVITO program package for Windows, which crashed when inserting modifiers into the pipeline.
* Bug fix: Pipeline stuck in infinite update loop when setting fractional playback rate for trajectory (`issue #181 <https://gitlab.com/stuko/ovito/issues/181>`_)
* Added Python function :py:func:`ovito.enable_logging()`, which lets OVITO print activity information to the terminal during long-running operations.
* New implementation of OVITO's asynchronous task framework and pipeline execution/caching system.
* New modifier: :ref:`particles.modifiers.time_averaging`
* New modifier: :ref:`particles.modifiers.smooth_trajectory`
* New option to :ref:`data_sources` load all frames of the trajectory into memory.
* Modifiers, viewport layers, and pipelines can now `be given user-defined names <https://www.ovito.org/forum/topic/adding-comments-custom-names-to-modifiers-and-overlay-elements/>`_ in the pipeline editor.
* :py:class:`ovito.data.DataTable` can now be created from Python, e.g. to have custom analysis modifiers that generate data plots.
* Simplified usage of the :ref:`particles.modifiers.unwrap_trajectories` modifier, which now scans the input trajectory automatically in the background.
* `Changed default color scheme for unnamed particle types. <https://gitlab.com/stuko/ovito/issues/179>`_
* `Updating the trajectory from the external file will automatically jump to the end of the trajectory. <https://gitlab.com/stuko/ovito/issues/178>`_
* Fixed ImportError in PyPI package on macOS (see `this discussion <https://www.ovito.org/forum/topic/ovito-package-on-python/>`_).
* Support for version 2.0 of the GSD file format (issue `#176 <https://gitlab.com/stuko/ovito/issues/176>`_).
* Fixed regression `#175 <https://gitlab.com/stuko/ovito/issues/175>`_ in Expression Selection modifier, which was sporadically crashing since build dev476.
* Now offering two separate program editions: `OVITO Basic and OVITO Pro <https://www.ovito.org/about/ovito-pro/>`_.
* Added file reader for the `oxDNA <https://dna.physics.ox.ac.uk/index.php/Main_Page>`_ file format and a specialised visual element for nucleotides.
* Removed the POV-Ray rendering engine (POV-Ray scene file export is still available).
* Renamed DataSeries to :py:class:`ovito.data.DataTable` throughout the scripting API, user interface and documentation.
* Enhancements to the :ref:`particles.modifiers.cluster_analysis` modifier: calculation of cluster centers of mass and unwrapping of particle coordinates.
* Python interface: Introduced the :py:class:`ovito.vis.Viewport`::underlays stack. Redesigned the :ref:`viewport_layers` command panel tab.
* Voronoi Analysis modifier provides new option to visualise the computed Voronoi cells .
* Workaround for video encoding issue resulting in invalid MP4/MOV files for frame rates 2/4/8/16 fps.
* Updated Qt libraries to version 5.12.6.
* The :ref:`particles.modifiers.color_coding` and the :ref:`particles.modifiers.assign_color` modifier can now operate on surface meshes.
* Bug fix: Segfault in Combine Datasets modifier second first dataset contains bonds but first doesn't (`issue #173 <https://gitlab.com/stuko/ovito/issues/173>`_).
* Bug fix: Segfault when accessing a mutable sub-object field (e.g. *Particles.bonds_*) whose value is *None* (`issue #172 <https://gitlab.com/stuko/ovito/issues/172>`_).
* :ref:`particles.modifiers.construct_surface_mesh` modifier: New option to transfer particle properties to generated surface mesh.
* Replaced video encoding component Libav with FFmpeg, bringing high-quality animated GIF rendering.
* Fixed issue that prevented relative paths to external data files in a .ovito state file to get updated when files are moved.
* Standalone Python package with the ``ovito`` module now available at `https://pypi.org/project/ovito/ <https://pypi.org/project/ovito/>`_.
* Replaced PyQt5 with PySide2 Python module.
* Fixed loading of multi-frame PDB files.
* XYZ file exporter now includes cell origin in header line if non-zero.
* New 'Use mesh color' option for particle types, which renders particles having a user-defined shape with the original mesh colors.
* Added keyboard shortcut for the *New Program Window* function.
* Added the :py:meth:`SimulationCell.delta_vector() <ovito.data.SimulationCell.delta_vector>` and :py:meth:`Particles.delta_vector() <ovito.data.Particles.delta_vector>` methods for calculating the vector connecting two particles.
* Added the :py:meth:`DataCollection.create_particles() <ovito.data.DataCollection.create_particles>` and :py:meth:`Particles.create_bonds() <ovito.data.Particles.create_bonds>` Python methods for adding new particles and bonds to a system.
* Let the LAMMPS data file parser accept additional spaces between header keywords.
* Included missing Tcl/Tk support files of Python interpreter in the Windows installation package.
* Bug Fix: Affine Transformation modifier does not transform dislocation lines.
* New option in the Slice modifier to visualize the plane in rendered images.
* Added a file writer for the GSD/HOOMD file format.
* Added a visualization element for voxel grids computed by the *Spatial Binning* modifier or imported charge density fields.
* Implemented the `QuickSurf algorithm <http://dx.doi.org/10.2312/PE/EuroVisShort/EuroVisShort2012/067-071>`_ in the *Construct Surface Mesh* modifier as an alternative surface generation algorithm.
* Added a file reader for the CIF format (Crystallographic Information File).
* Extended sections on :ref:`data manipulation <pydoc:data_manipulation_intro>` and :ref:`custom modifiers <pydoc:writing_custom_modifiers>` in the Python documentation.
* GSD file reader reads `particle shape definitions <https://gsd.readthedocs.io/en/stable/shapes.html>`_ (ellipsoids, polygons, convex polyhedra, general 3d meshes).
* Create Bonds modifier remembers if the user explicitly re-enables the display of an unusually large number of bonds.
* GSD file reader supports user-defined per-particle, per-bond and global data chunks.
* Upgraded build environment to Qt release 5.12.5.
* Settings for animation FPS and rendering resolution are retained across program session.
* Added graphics export function for data series plots.
* Display of bonds now takes into account particle radii when calculating the length of half-bond cylinders.
* Documented the :ref:`clone_pipeline` function in the user manual.
* Use Ctrl/Command key modifiers to zoom all viewports to scene extents at once.
* Renamed the '*Correlation function*' modifier to '*Spatial correlation function*' modifier.
* Replaced calls to FFTW library with more lightweight KISS FFT library.
* CASTEP .md file reader: Automatic conversion from Bohr to Angstrom units.
* Reimplemented how animation timeline is adjusted to accommodate loaded trajectories; redesign of the animation settings dialog.
* Bug fix: Memory footprint continuously increases during animation rendering.
* Fixed bug in CASTEP MD file reader not recognizing '<-- hv' lines.
* New axis style option in coordinate tripod viewport overlay.
* Fix for ovitos.exe error "unable to find Qt5Core.dll on PATH" on Windows.
* New file exporter for VTK voxel grid files. Allows to export results of Spatial Binning modifier.
* Set up automatic mapping to particle properties for 4-column XYZ files.
* New 'Constrain Rotation' option in the viewport context menu to turn on/off camera alignment with z-axis.
* Maximized state of active viewport is kept across program sessions.
* Bug fix: 'Save as defaults' function in particle type editor not working for numeric particle types (issue #157)
* Bug fix: export_file() Python function always performs pipeline evaluation at frame 0.
* Added the Viewport.create_widget() Python method, which allows embedding an OVITO viewport into a PyQt5 GUI.
* Bug fix: LAMMPS data exporter not writing 'dipole' atom style files correctly.
* Extended LAMMPS data file reader to support a wider range of atom styles, including 'hybrid'.
* Extended the GSD file reader to parse the periodic image information, which can be used by the Unwrap Trajectories modifier to unfold particle positions.
* Added a file reader for ParaDiS data files containing discrete dislocation lines.
* The Unwrap Trajectories modifier can now undo the cell flipping performed by LAMMPS to keep the tilt factors within certain limits.
* New modifier: :ref:`particles.modifiers.chill_plus` for identifying water structures.
* Added support for user-defined particle shapes.
* Added file reader for the OBJ and STL formats, which store triangle meshes.
* Extended the Construct Surface Mesh modifier to identify disconnected regions of the filled volume.
* Added the :py:class:`ovito.vis.Viewport`.camera_up parameter, which gives control over the orientation of the vertical axis in rendered images.
* New implementation of the surface mesh data structure, now supporting assignment of arbitrary properties to the vertices, faces and spatial regions enclosed by surface mesh.
* OSPRay renderering engine is now available in Windows builds of Ovito.
* Added the :py:meth:`ovito.data.DataCollection.apply()` Python method, which allows to directly apply a built-in modifier to a dataset.
* Updated the Gaussian Cube file reader to auto-detect Bohr/Angstrom units and support files containing multiple field values.
* Extended PDB file reader to support multi-frame trajectory files.
* Added option to POSCAR file writer for outputting atomic positions and velocities in reduced cell coordinates.
* Added new option to :ref:`particles.modifiers.construct_surface_mesh` modifier that allows selecting all atoms on the constructed surface.
* Extended the :py:class:`ovito.modifiers.ColorCodingModifier` interface to simply definition of custom color gradients.
* The :ref:`particles.modifiers.wigner_seitz_analysis` modifier now supports loading varying reference configurations, which depend on the current timestep.
* Improved appearance of axis tripod viewport overlay when looking head-on to an axis (issue `#88 <https://gitlab.com/stuko/ovito/issues/88>`_)
* Added the new :ref:`particles.modifiers.unwrap_trajectories` modifier.
* Bug fix: XYZ files with varying numbers of named atom types (issue `#137 <https://gitlab.com/stuko/ovito/issues/137>`_)
* Bug fix: Globbing algorithm did not recognize file sequences such as 'file-0.001', 'file-0.0001', etc. correctly.
* :ref:`particles.modifiers.create_bonds` modifier now defines a new bond type and assigns it to all newly created bonds
* Reimplemented the :ref:`particles.modifiers.generate_trajectory_lines` modifier, now supporting systems with varying number of particles. Unwrapped trajectory lines can now optionally be displayed in wrapped form.
* Updated version of the PTM modifier, which now supports OVITO standard reference orientations for computing crystal orientations. See `merge request 15 <https://gitlab.com/stuko/ovito/merge_requests/15>`_.
* Built-in SSH client now supports servers that use two-factor authentication and keyboard-interactive authentication.
* Basic support for Quantum Espresso data file format.
* Added file reader for `GALAMOST <http://galamost.ciac.jl.cn>`_ file format.
* Added an 'enabled' property to :py:class:`ovito.vis.ViewportOverlay` Python class, which allows users to temporarily turn off individual overlays.
* Changed the signature of :ref:`particles.modifiers.python_script` user-defined modifier functions. Instead of separate *input* and *output* data collections, a user-defined modifier function now takes just a single data collection, which can be modified in-place by the function. A fallback to the old function signature is implemented to maintain backward compatibility.
* Per-type masses are now read from LAMMPS data files.
* Bug fix: Elastic Strain Calculation modifier yields off-diagonal elements of atomic Green-Lagrangian strain tensor with wrong sign.
* Fixed UI issue `#119 <https://gitlab.com/stuko/ovito/issues/119>`_.
* Voronoi analysis modifier now dynamically adjusts the number of output columns of the Voronoi Index property to the maximum face order.
* Renamed the "Bond-Angle Analysis" modifier to "Ackland-Jones Analysis" modifier.
* Renamed the "Bin & Reduce" modifier to "Spatial Binning" modifier.
* Extended the Spatial Binning modifier to support 3d grids.
* Fixed bug in the marching cubes algorithm used by the Create Isosurface modifier, which sometimes let the surface construction fail with an error message.
* Added file reader for the DL_POLY format.
* Added calculation of partial (element-wise) RDFs to the :ref:`particles.modifiers.coordination_analysis` modifier.
* Generalized the :ref:`particles.modifiers.compute_property` modifier, which can now compute bond properties too. Furthermore, neighbor terms can now reference properties of the central particle as well.
* Added computation of height-difference correlation function to :ref:`particles.modifiers.correlation_function` modifier.
* Removed the *Compute Bond Lengths* modifier from the program. The extended :ref:`particles.modifiers.compute_property` modifier provides a similar functionality.
* Lifted 1 billion atom limit of the LAMMPS, CFG and IMD file parsers.
* Fixed program crash at program startup due to unhandled exception in case the OpenGL initialization fails.
* Generalized the :ref:`particles.modifiers.manual_selection` modifier, which now supports selecting bonds too.
* Bug fix: 'Move plane to simulation box center' function of Slice modifier not calculating the distance value correctly for non-unit normal vectors
* Implemented correct 2D shear and volumetric strain calculation in :ref:`particles.modifiers.atomic_strain` modifier for two-dimensional system.
* Added the '*Transparency*' bond property and partial support for rendering semi-transparent bonds.
* Added a '*Color particles by type*' option to structure identification modifiers such as CNA and PTM, providing the option to preserve the current colors of particles.
* Added a '*Sort particles by ID*' option to some file readers, automatically sorting the imported particles w.r.t. unique identifiers to obtain a stable ordering.
* Bug fix: Correlation function modifier crashes when simulation cell is smaller than selected FFT grid spacing (`issue #106 <https://gitlab.com/stuko/ovito/issues/106>`_).
* Bug fix: Atomic Strain modifier ignores turning off the '*Select invalid particles*' user option.
* File sequence globbing now works for hidden files (`issue #98 <https://gitlab.com/stuko/ovito/issues/98>`_)
* Modifiers no longer overwrite global attributes with the same name produced by upstream modifiers in the pipeline. Instead, new unique attribute names are generated whenever needed.
* Renamed the *ovito.DataSet* Python class to :py:class:`ovito.Scene`.
* The VASP file reader can now handle more variants of XDATCAR files, e.g. those with varying cell size
* Animation playback direction can be reversed using the *Shift* keyboard modifier
* New SSH client for loading files from remote hosts, which supports public key authentication and a wider range of key exchange methods.
* Bug fix: File descriptors do not get closed when importing new files due to bug in GSD file I/O layer.
* The Expression Selection modifier now supports selection of other kinds of elements, for example bonds, in addition to particles.
* The :ref:`particles.modifiers.polyhedral_template_matching` (PTM) function has been extended and can now identify diamond structures.
* The *Generate Trajectory Lines* utility has been replaced with the new :ref:`particles.modifiers.generate_trajectory_lines` modifier.
* Viewport overlays now provide a background option, putting them behind the three-dimensional scene.
* OVITO now reads Gaussian Cube files (atoms and voxel grid data).
* The new :ref:`data_inspector` has been introduced. It replaces the old *Inspect Particles* and *Inspect Dislocations* utilities.
* "Display objects" are now called :ref:`visual_elements` within OVITO.
* Replaced the Viewport.render() Python method with the new :py:meth:`Viewport.render_image() <ovito.vis.Viewport.render_image>` and :py:meth:`Viewport.render_anim() <ovito.vis.Viewport.render_anim>` methods. The RenderSettings and the AnimationSettings classes have been removed from the Python interface.
* The :ref:`particles.modifiers.combine_particle_sets` modifier has become smarter: Particle types from the two input datasets are now merged and type IDs are automatically remapped.
* The LAMMPS dump and XYZ file exporters now provide control over the output precision for floating-point numbers.
* Added a .keep_current_config option to the :py:class:`ovito.modifiers.WignerSeitzAnalysisModifier` Wigner-Seitz defect analysis modifier, which allows to retain the current atomic configuration instead of the reference configuration.
* Implemented automatic detection of multi-timestep files (for LAMMPS dump and XYZ formats).
* Added Python bindings for the :ref:`particles.modifiers.correlation_function`.
* Added a file reader for the XSF data format of `XCrySDen <http://www.xcrysden.org>`_ (atomic structures as well as 3d voxel fields are supported by OVITO).
* Added Python bindings for the :py:class:`ovito.vis.CoordinateTripodOverlay`.
* :ref:`modifier_templates` can now be exported/imported to transfer them between computers.
* Added the new :ref:`rendering.ospray_renderer`.
* Added handling of out-of-range atom serial numbers in PDB files with more than 99,999 atoms.
* Bug fix: Stack overflow in Tachyon renderer when rendering a large number of semi-transparent objects
* Bug fix: Made calls to the FFTW3 library from the Correlation Function modifier thread-safe
* Reimplemented the displacement vector calculation in the Atomic Strain modifier to fix an error that occurred when the cutoff radius was larger than half the simulation cell size.
* Added a file writer that can export dislocation lines to the VTK file format used by ParaView.
* Extended the dislocation inspection utility to display a dislocation's start and end vertex position.
* Added support for 64-bit integer particle properties and large datasets with >2 billion particles (only usable from analysis scripts, rendering won't work for more than 2 billion particles).
* Changed internal floating-point type from *float* to *double*, i.e., all c alculations will now be performed with double precision. Memory footprint of the program increases because of this.
* Performance improvement: The Wigner-Seitz defect analysis now makes use of all processor cores (`issue #50 <https://gitlab.com/stuko/ovito/issues/50>`_).
* Bug fix: Progress bar is now updated during file export.
* Added a file writer for the binary NetCDF format according to the AMBER convention (`issue #41 <https://gitlab.com/stuko/ovito/issues/41>`_).
* Thanks to a complete redesign of the data pipeline system, modifiers can now access the entire input trajectory, making it possible to implement temporal analyses that operate not just on instantaneous simulation snapshots. A first, simple example is the new *Interpolate trajectory* modifier, which can interpolate particle positions between simulation frames to create smooth looking animations from coarse sequences of simulation snapshots.
* Furthermore, modifiers such as *Atomic strain*, *Displacement vectors* and *Wigner-Seitz defect analysis* can now use frame 0 of the currently loaded simulation sequence as reference configuration. It is no longer necessary to load the reference configuration from a separate input file.
* Several modifier have been generalized and can now operate on other forms of data in addition to particles, e.g. bonds and their properties. Examples are the *Assign color*, *Invert selection*, *Select Type*, *Histogram*, *Scatter plot, Replicate* and *Delete selected* modifiers.
* The Python programming interface has been redesigned and extended. See `this page <https://www.ovito.org/docs/current/python/introduction/introduction.html>`_ for more information.
* :py:class:`ovito.data.SurfaceMesh`, dislocation networks and voxel data grids now carry their own periodic domain information with them. Changing the master simulation cell size no longer screws up the display of these data types.
* The LAMMPS binary dump file reader automatically detects the endianess used in the file. It can now read files produced on IBM BlueGene/Q machines, for example (`issue #23 <https://gitlab.com/stuko/ovito/issues/23>`_).
* The *Correlation function* modifier now exports radial distribution function and structure factor as extra columns to a text file.
* A VTK file writer has been added, which can export surface triangle meshes produced by the *Construct surface mesh* and *Create Isosurface* modifiers.
* The ovitos script interpreter now accepts (and silently ignores) the -u command line option of CPython for better compatibility with pip install scripts that use this option (`issue #43 <https://gitlab.com/stuko/ovito/issues/43>`_).
* The Tachyon renderer now obeys the maximum number of parallel threads set by the --nthreads command line option.
* The *Show Periodic Images* modifier has been renamed to *Replicate* modifier. In addition to particles and bonds it now can replicate surfaces, dislocation lines and voxel data grids too (`issue #39 <https://gitlab.com/stuko/ovito/issues/39>`_).

---------------------------
Version 2.9.0 (27-Jul-2017)
---------------------------

* Added the --nthreads command line option to **ovitos** as an alternative to -nt (`issue #35 <https://gitlab.com/stuko/ovito/issues/35>`_).
* Brought back missing *stderr* output from calls to sys.exit() in **ovitos** interpreter.
* Extended the Particle Inspection utility to allow expression-based selection of particles in addition to picking them using the mouse (`issue #19 <https://gitlab.com/stuko/ovito/issues/19>`_).
* Added a bond-based mode to the Cluster Analysis modifier. It allows forming clusters based on the bond network topology.
* The OVITO main window now accepts data files and .ovito files via drag & drop (`issue #28 <https://gitlab.com/stuko/ovito/issues/28>`_).
* Extended the Load Trajectory modifier to also copy other varying particle properties in addition to the particle positions (`issue #29 <https://gitlab.com/stuko/ovito/issues/29>`_).
* The particle indices displayed by the Particle Inspection utility are now zero-based, consistent with the ParticleIndex variable used by the Expression Selection modifier (`issue #21 <https://gitlab.com/stuko/ovito/issues/21>`_).
* Added the new 'Relative face area' threshold parameter to the Voronoi analysis modifier, which allows filtering out small faces with an area below a specified fraction of the total Voronoi cell surface area (`issue #7 <https://gitlab.com/stuko/ovito/issues/7>`_).
* Added file parsers for the `CASTEP <http://www.castep.org>`_ *.cell*, *.geom* and *.md* simulation file formats.
* Replaced the 'eliminate homogeneous deformation' option of the Displacement Vectors modifier with the more general affine mapping setting.
* On macOS, data files and .ovito files can now be associated with OVTIO and directly opened from the Finder (`issue #22 <https://gitlab.com/stuko/ovito/issues/22>`_).
* Added the FileSource.loaded_file attribute, which allows accessing the filename of the currently loaded simulation file from a Python script.
* New modifier: The Voronoi Topology Analysis modifier can classify the Voronoi polyhedra of particles, e.g. to perform structural filtering.
* New modifier: The Correlation Function modifier has been contributed by `Lars Pastewka <http://www.yin.kit.edu/english/1103_1692.php>`_. It allows computing the spatial correlation between two particle properties.
* Bug fix: LAMMPS data file parser ignored *Bonds* section at end of file when number of bonds is zero
* New modifier: The Create Isosurface modifier allows to visualize field quantities like the electron density that are defined on a structured data grid. So far, only the POSCAR file parser has been extended to read charge density data from `CHGCAR files <https://cms.mpi.univie.ac.at/vasp/vasp/CHGCAR_file.html>`_, which can serve as input for the isosurface modifier.
* New modifier: The Coordination Polyhedra modifier constructs convex hulls from the bonded neighbours of atoms.
* Bug fix: Unexpected error message during file export when the old mapping for the output file columns has become invalid.
* Added the '`Adjust range (all frames) <https://gitlab.com/stuko/ovito/issues/15>`_' function to the Color Coding modifier, which takes into account all frames of the animation sequence when determining the min/max values of the input property.
* Added a new user option to the Affine Transformation modifier that enables the transformation of vectorial particle properties like Force and velocity together with the particle positions (`issue #11 <https://gitlab.com/stuko/ovito/issues/11>`_).
* `issue #16 <https://gitlab.com/stuko/ovito/issues/16>`_).
* Bug fix: Assertion error in Ambient Occlusion modifier when modifier input is empty.
* The *ovito* Python module is now usable from external Python interpreters as well, not only **ovitos**.
* Complete overhaul of the internals of the asynchronous data pipeline framework; many improvements to the code.
* Dropped backward compatibility with Qt library versions less than 5.4.
* Bug fix: Parser error when reading a LAMMPS file containing very small numbers on the order of 1e-200 (`issue #12 <https://gitlab.com/stuko/ovito/issues/12>`_).
* Bug fix: Windows version always appends .pov to expected file names (`issue #13 <https://gitlab.com/stuko/ovito/issues/13>`_).
* Bug fix: Create trajectories function does not use correct number of frames from input sequence (`issue #6 <https://gitlab.com/stuko/ovito/issues/6>`_).
* Added support for omnidirectional stereoscopic rendering to the POV-Ray renderer plugin. This allows producing 360 degrees VR movies (requires POV-Ray 3.7.1).
* The required system library *libstdc++.so.6* is no longer bundled with the Linux version of OVITO, because it causes conflicts with OpenGL drivers on some systems.
* Reading multi-frame GSD files that contain static data now works correctly.
* Bug fix: Program crash during parallel access to NetCDF files. Calls to NetCDF library functions are now serialized, because they are not thread-safe.

---------------------------
Version 2.8.2 (24-Jan-2017)
---------------------------

* The Histogram modifier can now compute the distribution of bond properties too (e.g. bond lengths).
* Modifiers now report an out-of-memory condition. Ovito no longer crashes when a memory allocation fails during modifier evaluation.
* Fixed viewport rendering and other issues for simulation datasets with a very small length scale (~10\ :sup:`-11`\ ).
* Added Python bindings for particle trajectory line generation and visualization.

---------------------------
Version 2.8.1 (17-Dec-2016)
---------------------------

* Bug fix: Segmentation fault after applying a Color Coding modifier with Viridis color map to particle data containing NaN values.
* Bug fix: Segmentation fault when closing Scatter Plot editor panel.
* Bug fix: Compute Property modifier failed with an error when preceded by a Dislocation Analysis (DXA) modifier, due to expression variable names containing invalid characters.
* Added read/write support for LAMMPS data files with atom_style *sphere*.
* Added the NearestNeighborFinder.find_at() Python method for querying the nearest particle(s) around an arbitrary spatial position.
* Added a detailed usage example to the scripting documentation of the WignerSeitzAnalysisModifier to demonstrate the identification of specific point defect types, e.g. antisites.
* Bug fix: Viewport.render() Python method failed in GUI mode with an error.
* Bug fix: FileSource.load() Python method failed when called with keyword arguments.
* Bug fix: Serial computations performed by the data pipeline occupy two processor cores instead of one.
* Bug fix: A custom modifier script function, raising an exception and running as part of a data pipeline within **ovitos**, produced assertion error.
* Bug fix: Cyclic reference between DataSet and ScriptEngine classes led to a potential memory leak when using Python script modifiers or viewport overlays within a script executed by **ovitos**.
* MacOS version of OVITO is now distributed as a signed application bundle in the form of a DMG disk image (avoids "unidentified developer" warning message on first start).
* Disabled geometry shaders by default for AMD/ATI hardware on Windows due to compatibility problems reported by some users.
* Python modifier and Python viewport overlay scripts can now be edited in a separate code editor window.

---------------------------
Version 2.8.0 (23-Nov-2016)
---------------------------

* Added the POV-Ray rendering backend and the POV-Ray scene file exporter.
* Fully transparent, invisible particles are no longer sent to the Tachyon renderer to avoid artifacts.
* Replaced QCustomPlot component with QwtPlot component for graph plotting within the GUI.
* Replaced Boost.Python with pybind11 library to implement OVITO's Python bindings.
* The DXA modifier now outputs attributes for the computed line lengths which are broken down by dislocation type.
* Bug fix: Segmentation fault in Color Coding modifier when input particle property contains infinite values.
* Fixed an issue with the Freeze Property modifier, which didn't attach a display object to vector properties.
* The "Fusion" UI theme is now explicitly activated on Linux. Older builds of OVITO used the "Windows" theme for some reason.
* Bug fix: Atomic strain calculation failed for 2d simulation cells with zero length along Z.
* The Linux binaries are now built with the gcc 5.1 compiler.
* Bug fix: Identify diamond structure modifier did not output the structure counts as global attributes.
* Removed restriction of LAMMPS binary dump file reader to less than 200k atoms.
* Added a command line option to the **ovitos** program, which gives users control over the number of parallel threads used by OVITO.
* Added depth-of-field rendering to the Tachyon renderer.
* OVITO can now color dislocation lines based on their Burgers vectors (only BCC crystals so far). Before, coloring was possible only on the basis of dislocation type or dislocation character.
* Added the 'Generate perfect dislocations' option to the DXA modifier, which suppresses the identification of partial dislocations.
* Improved I/O performance of the LAMMPS data file reader when reading bonds information.
* Made PDB file reader compatible with files generated by the Gromacs *trjconv* tool.
* Extended the Color Coding modifier to support the coloring of vector arrows (in addition to particles and bonds).
* Bug fix: If the Compute Property modifier is used to create a vector property, the display settings for vector arrows are no longer lost every time the modifier is re-evaluated.
* The Text Label and Color Legend viewport overlays can now draw an outline around text to make it easier to read on all backgrounds.
* Disabled some strict conformance checks in the SSH module to improve compatibility with some SSH servers.
* Bug fix: Viewport.render() Python function raised a Boost.Python.ArgumentError when being called in GUI mode.
* Bug fix: NetCDF file parser did not recognize unwrapped particle coordinates in files written by LAMMPS.
* Raised the threshold values for the automatic selection of particle rendering quality. High quality mode is now being used for <4,000 particles, medium mode for <400,000 particles, and low quality mode for everything above.
* OVITO now ships with the `matplotlib <http://matplotlib.org>`_ Python module. This makes it possible to integrate custom data plots into images or movies rendered by OVITO.
* Bug fix: Unicode characters are now correctly rendered by the Color Legend viewport overlay.
* Added Python interface for the FreezePropertyModifier.
* Added two additional colormaps to the Color Coding modifier: Magma and Viridis.
* Added a 'Combine particle sets' modifier, which allows merging two datasets into one.
* Added GSD (General Simulation Data) file reader for HOOMD-blue simulation files.
* Bug fix: Inserting a modifier while scanning an XYZ/LAMMPS input file to discover simulation frames may crash program.
* The 'ovitos' interactive interpreter now creates/loads a separate IPython profile named 'ovito' instead of the 'default' profile. This reduces inteference with an existing IPython installation on the same system.
* Workaround: Exporting denormalized floating-point numbers to an output file crashes program due to bug in Boost.Karma library.
* Regression: Compatibility with high-resolution displays (Retina/Mac OS)
* Regression: OpenGL rendering does not work in console mode on Windows
* NetCDF reader now accepts files where particle data is stored in a subgroup named 'AMBER'.

---------------------------
Version 2.7.1 (28-Aug-2016)
---------------------------

* Integrated IPython in Linux and Mac OS builds of OVITO.
* Small bug fix in Animation Settings dialog: Changing the frame rate made the time slider jump.
* Updated the NetCDF file reader to make it compatible with files written by the SimPARTIX code.
* Updated the VTK file reader to accept empty lines in the header.
* Updated the IMD file reader, which did not correctly map file columns to standard particle properties with vector components.
* Updated the CFG file reader to accept empty lines preceding the first header line.
* Updated the PDB file reader to parse molecule identifiers and types.
* Added the ParticleTypeProperty.get_type_by_id() and get_type_by_name() methods to the Python interface.
* Added the DislocationSegment.spatial_burgers_vector property to the Python interface.
* Added a confirmation message before resetting the URL history list in the SSH connection dialog.

---------------------------
Version 2.7.0 (25-Jul-2016)
---------------------------

* Bug fix: Program crashed when entering a non-valid text into a numeric input field.
* Added OpenGL driver-bug workaround to fix high-quality rendering of particles on Linux/Intel graphics systems.
* Improved visual appearance of large number of semi-transparent particles in images generated by the Tachyon renderer.
* Fixed a bug in the line coarsening routine of the DXA modifier.
* Periodic image shift vectors of bonds are now accessible from Python scripts.
* The Atomic Strain calculation modifier can now perform a polar decomposition F=RU of the deformation gradient into a rotation and stretch tensor.
* Added a 'Use only selected particles' option to the Histogram and Bin and Reduce modifiers, which lets you to restrict the calculation to a subset of particles.
* Extended the Color Coding modifier so that it can also operate on bonds instead of particles.
* Added the Compute Bond Lengths modifier, which can be used in conjunction with the Color Coding modifier to color bonds according to their length.
* Added the Text Label viewport overlay, which provides an easy way of inserting a text label into rendered images and movies. The text can contain placeholders which are replaced with quantities computed by OVITO.
* Added a text file export function, which allows exporting scalar quantities computed by OVITO to a tabular text file as functions of simulation time.
* OVITO can now load XDATCAR trajectory files written by Vasp.
* Added the Polyhedral Template Matching modifier, which can robustly identify lattice structures at high temperature.
* The LAMMPS dump file exporter now writes out the simulation timestep number read from an input dump file to the file header instead of the animation frame.
* Added the Load Trajectory modifier, which allows loading datasets that consist of separate topology and trajectory files (e.g. a LAMMPS data file with bond definitions and a LAMMPS dump file with atom trajectories).
* The pair-wise cutoff mode of the Create Bonds modifier is now usable from Python.
* Improved visual quality of particle display for very distant and small (sub-pixel) particles.
* Added a 'lower cutoff' parameter to the Create Bonds modifier.
* Scene files that refer to external data file in the same directory can now be transferred to another computer without breaking the link.
* Documented the CA dislocation file format in the user manual. This makes it possible to export the extracted dislocation lines and process/analyze them outside of OVITO.
* Fixed a bug in the Create Bonds modifier, which produced wrong results when all pair-wise cutoffs were set to values smaller than 1.0.
* OVITO no longer depends on the CGAL library, making it easier to build it from source.
* The CA dislocation file importer now supports multi-tilmestep files.
* Fixed a bug in the Show Periodic Images modifier, which did not replicate bond properties.
* The behavior of the ovito.io.import_file() Python function has been changed. It no longer adds the created node to the scene, and it is now okay to call this function repeatedly. It also accepts wildcard filenames now to import file sequences. See the updated :ref:`Python documentation <pydoc:file_io_overview>` for details.
* The ovito.io.export_file() Python function now gives full control over which animation frames are being exported.
* Fixed bug in the Coordination Analysis modifier, which prevented the RDF calculation for 2D systems.

---------------------------
Version 2.6.2 (19-Mar-2016)
---------------------------

* Updated the built-in SSH client to support more encryption methods and improve compatibility with some SSH servers.
* Small bug fix that solves a (rare) problem with the display of dislocation lines in periodic systems.
* For very small, periodic simulation cells the Elastic Strain Calculation modifier silently failed for diamond crystals. Now the user is informed that the Show Periodic Images modifier should be applied first to extend the box size.
* Fixed output of animated GIF files on Linux.
* Reduced memory footprint of the DXA analysis modifier.
* The Slice modifier now also cuts surface meshes generated by the Construct Surface Mesh modifier and dislocation lines generated by the DXA analysis modifier.
* Introduced basic support for 2D systems. The 2D flag can be set in the simulation cell properties panel. The Atomic Strain and Coordination Analysis modifiers then perform the computation in 2D (i.e. the XY plane).
* Key-value pairs read from *extended XYZ* file headers and the LAMMPS timestep number are now accessible from Python via the new DataCollection.attributes property.
* Added the 'Number of bins' parameter to the Coordination Analysis modifier, allowing the user to control the resolution of the generated RDF histogram.
* Errors generated by Python scripts that are run via the 'Run script file' function are now displayed in the GUI.
* Re-added the 'Every nth frame' field to the Render Settings panel, which allows rendering only a subset of animation frames.
* Fixed import of 'Aspherical Shape' particle property values from a file.
* The export_file() script function can now produce multi-timestep files.
* Improved automatic detection of PDB files and added parsing of bonds defined by CONECT records.
* Fixed bug in Elastic Strain Calculation modifier, which got stuck in infinite loop when applied to some complex polycrystalline structures.
* Moving the viewport camera no longer stops animation playback.
* Added the 'Vector Color' particle property, which allows changing the display color of vector arrows on a per-particle basis.
* Bug fix: The Create Bonds modifier reported the number of half-bonds, not the number of full bonds generated.
* Regression: Restricting the Compute Property modifier to selected particles did not work correctly; existing values for unselected particles were always reset to zero.
* Added the CA file exporter, which allows saving DXA analysis results to disk (and load them again at a later time).

---------------------------
Version 2.6.1 (15-Nov-2015)
---------------------------

* Arrows can now be centered on particles, e.g. to visualize magnetic moments and other vector properties. Atomic force vectors and dipole vectors read from simulation files can now be directly visualized.
* The Tachyon renderer now supports transparent backgrounds.
* The Color Legend and Coordinate Tripod viewport overlays can now be repositioned with the mouse.
* Display objects that are generated by modifiers no longer temporarily disappear from the pipeline editor list while playing an animation.
* The PDB file parser now accepts lines with up to 83 characters to support files written by Accelrys Discovery Studio.
* Fixed regression in CA file importer, which is broken in previous release.

---------------------------
Version 2.6.0 (02-Nov-2015)
---------------------------

* Added the <a+c> dislocation type for HCP crystals to the Dislocation Analysis modifier.
* Added the Elastic Strain Calculation modifier, which computes the atomic-level elastic strain and deformation gradient tensors in crystalline systems. It can be used to analyze local elastic distortions in a crystal lattice and to determine the local crystal orientation.
* The modification pipeline editor now allows changing the application order of modifiers via drag and drop.
* Added the Dislocation Inspection utility, which lets you obtain further information about dislocation segments extracted by the Dislocation Analysis modifier.
* Added an introduction to OVITO's animation system to the manual, which explains how to animate parameters and the camera.
* The Compute Property modifier can now perform computations that take into account the local neighborhood of particles. For example, you can use this to average a quantity over spherical regions around each particle.
* The Wigner-Seitz analysis modifier can now break down the computed site occupancy number into per-type numbers. This makes it easier to identify antisite defects, for example.
* Viewports are no longer automatically zoomed to show everything when replacing the currently loaded dataset with a new one. This makes it easier to preserve the current view configuration when switching to a different simulation file.
* Fixed the assignment of dislocation lines that were loaded from a CrystalAnalysis file to Burgers vectors families. The CA file import was broken since version 2.4.4.
* Added a 'bond-based' mode to the Common Neighbor Analysis modifier, which computes the CNA indices based on existing bonds between particles. The modifier also outputs the computed CNA bond indices as a new bond property. This enables, for example, analyses of disordered systems using the classical CNA.
* It is now possible to write you own modifiers in Python.
* Added a 'Use only selected particles' option to the Common Neighbor Analysis and the Identify Diamond Structure modifiers, which allows identifying sub-lattices.
* Added the NearestNeighborFinder Python class, which can be used from Python scripts to find the *N* nearest neighbors of a particle.
* Python scripts can now access the dislocation lines extracted by the DislocationAnalysisModifier.
* Bug fix: Bond properties like the bond type are now updated when dangling bonds are removed due to deleted particles.
* Fixed bug in Atomic Strain modifier, which produced weird results when PBC flags of reference configuration do not match PBC flags of deformed configuration. Now PBC flags of deformed configuration always override the boundary conditions of the reference simulation cell.
* Fixed error in ``ovito.dataset.scene_nodes.__iter__()`` Python method.
* Fixed bug in file parser that led to wrong particle type names when loading a multi-frame XYZ file with varying set of named atom types.
* Bug fixes in Python-ASE interface - Cell matrix is transposed and duplicate properties are handled.

---------------------------
Version 2.5.1 (07-Aug-2015)
---------------------------

* The LAMMPS data file exporter can now produce files with LAMMPS atom styles other than 'atomic'. It also exports bonds if present.
* Arbitrary triclinic simulation cells can now be exported to the LAMMPS data file format. They will be automatically transformed to the canonical LAMMPS representation.
* The LAMMPS data file parser now reads bond types.
* Added the *fmod(A,B)* math function to the Compute Property and Expression Select modifiers.
* Added visualization support for cylindrical and spherocylindrical particles.
* Added a file parser for FHI-aims log files, which can contain multiple simulation frames.
* Added the 'Indicate line direction' option to the dislocation display object.

---------------------------
Version 2.5.0 (25-Jul-2015)
---------------------------

* Added Python interface for Bin and Reduce modifier.
* Fixed viewport font issue on Macs with high-dpi display.
* Added the Expand Selection modifier.
* Added a 'No bonds between different molecules' option to the Create Bonds modifier.
* Changed the behavior of the IMD file exporter: All particle properties to be exported, including the standard ones defined by the IMD format, must now be explicitly selected by the user.
* Integrated DXA (Dislocation Extraction Algorithm) into OVITO.
* Voronoi analysis modifier can now output bonds between neighboring atoms which share a Voronoi face.
* Python scripting interface now allows conversion to/from ASE Atoms objects.
* Python scripting interface now supports write access to particle properties and procedural generation of input particle datasets for OVITO's modification pipeline.
* Bug fix: Large number of simultaneous SFTP download requests led to error message 'SFTP error: Server could not start session'.
* Bug fix: Using the Python viewport overlay led to program crash on Windows 8 x64.
* Scripting function Viewport.render() now returns the rendered image, which can be manipulated before saving it to disk.
* Certain modifier and display parameters can now be animated using animation keys.
* Fixed OpenGL rendering of bonds/arrows on certain Windows/NVidia systems.
* Added import/export support for FHI-aims file format.
* Colors and radii used for particle types (as well as structure types) can now be predefined by the user.
* Migrated to version 5.4 of the Qt library. This may affect OpenGL rendering and the viewport display. Please report any issues that you experience.
* Bin & Reduce modifier now takes into account the simulation box origin, uses double precision numbers to perform calculations, and 1D-plot now spans the entire interval.
* Bug fix: NetCDF file importer doesn't close file handle, leading to error after loading several thousand frames.
* Bug fix: LAMMPS data file parser stumbles over 'AngleTorsion Coeffs' file section.
* Visualization of particle trajectory lines.
* Frequently used modifiers or combinations of modifiers can be saved (including modifier settings) for quick access.
* Rendering of ellipsoidal and box particles with orientation.
* Positioning the mouse over a bond shows its properties in the status bar.
* Fixed initialization of the *Select Particle Type* modifier.
* Fixed error when loading a compressed simulation file >2GB.

---------------------------
Version 2.4.4 (29-Mar-2015)
---------------------------

* Fixed error when rendering a high-resolution video.
* Surface mesh computed by ConstructSurfaceModifier can now be exported to a VTK file from Python.
* Added Python class ovito.data.CutoffNeighborFinder, which enables access to particle neighbor lists from Python.
* Particles and bonds are now rendered in chunks in the OpenGL viewports to work around a memory limit on some graphics hardware.
* Bond cylinders are now rendered using a geometry shader if supported by the graphics card.
* The IMD file exporter now lets the user select the particle properties to export (instead of exporting all).
* The VTK triangle mesh importer now reads per-face color information.

---------------------------
Version 2.4.3 (02-Mar-2015)
---------------------------

* **Upgraded integrated script interpreter from Python 2.7 to Python 3.4. Please update your scripts to make them compatible with Python 3**.
* Added rendering support for particles with non-cubic, axis-aligned box shape (via 'Aspherical shape' particle property)
* Added a dialog box to the Affine Transformation modifier, which lets the user enter a rotation axis, angle, and center.
* Removed cutoff option from Voronoi Analysis modifier in favour of a faster algorithm for orthogonal simulation cells, which is based on Voro++ container classes.
* The Voronoi Analysis modifier now determines the maximum number of edges per face in the Voronoi tessellation and warns if it exceeds the truncation length of computed Voronoi index vectors.
* OVITO can now load bonds from LAMMPS data files.
* The Freeze Property modifier now works when particles are lost during the simulation.
* Similarly, the Atomic Strain analysis can now deal with simulations where the number of particles is not constant.
* The Wrap at Periodic Boundaries modifier now wraps bonds crossing a periodic boundary.
* Added a scriptable viewport overlay, which allows to paint custom text and graphics over the rendered image. See how you can use it to add a scale bar to a viewport.
* The Show Periodic Images modifier now replicates bonds too.
* The XYZ file import now displays the file's comment line in the status field.
* Switched from MinGW to Visual C++ 2013 compiler to build Windows version. Python scripting is now supported by the 64-bit program version for Windows too.
* Removed old Javascript plugin.
* Bug fix: --version command line option causes program to crash.
* Bug fix: XYZ file column mapping dialog showed the column names from the last loaded extended XYZ file.

---------------------------
Version 2.4.2 (14-Nov-2014)
---------------------------

* The Color Coding modifier now supports user-defined color maps.
* Significantly improved performance of cutoff-based neighbor finding and *k*-nearest neighbor search routines. This code optimization speeds up many analysis algorithms in OVITO, in particular for large datasets.
* Added the Identify Diamond Structure analysis function, which finds atoms that form a cubic or hexagonal diamond lattice.
* Dialog box asking to save changes is only shown when scene has already been saved before.
* The Color Legend overlay now provides an option to overwrite the numeric labels with a custom text.
* Bug fix: Periodic boundary flags were not correctly updated when loading a new file using the 'Pick new local input file' button.
* Bug fix: Viewport.render() Python function raised error when called without a RenderSettings object.

---------------------------
Version 2.4.1 (01-Nov-2014)
---------------------------

* New integrated Python engine, which provides a powerful scripting interface (see scripting documentation). This is going to replace the Javascript engine, which has been deprecated and will be removed in a future program version. Command line options to run old scripts have been renamed to *--jsscript* and *--jsexec*.
* New Voronoi analysis modifier, which can compute atomic volumes, coordination numbers and Voronoi indices.
* It's now possible to include the coordinate system tripod and a color legend in the rendered image.
* Particle properties are displayed in the status bar when hovering over particles in the viewports.
* Periodic boundary conditions can be overridden by the user without the changes being lost when a new simulation frame is loaded.
* Added import/export support for extended XYZ format (see http://jrkermode.co.uk/quippy/io.html#extendedxyz), which includes metadata describing the data columns and the simulation cell.
* Improved input and output performance for text-based file formats.
* The OpenGL renderer can now display semi-transparent particles and surfaces.
* Added calculation of non-affine displacements to Atomic strain modifier. (This is Falk & Langer's D\ :sup:`2`\ :sub:`min` measure, see the 1998 PRB.)
* New Bin and reduce analysis modifier.
* The Create bonds modifier can now handle particles that are located outside a (periodic) simulation box.
* The Color coding modifier can display a color legend in the rendered image.
* Added a file parser for PDB files (still experimental).
* Added basic keyframe animation support. Some modifier parameters and other settings can now be animated. Future versions will also offer camera animation capabilities.
* The Show periodic images modifier can now assign unique IDs to particle copies.
* LAMMPS data file parser now supports additional LAMMPS atom styles such as 'charge' and 'bond'.
* Fixed high-quality particle rendering on Windows computers with Intel HD 4000 graphics.
* Bug fix: Export of compressed LAMMPS data files could result in truncated files.
* Bug fix: Solid volume computed by 'Construct surface mesh' modifier could be inaccurate due to low numerical precision
* Bug fix: 'Construct surface mesh' modifier crashed with certain input data.
* Bug fix: VTK mesh file parser couldn't handle multiple points per line (as written by ParaView).
* Bug fix: LAMMPS data file parser did not parse atom IDs.
* Bug fix: Particle inspection utility did not recalculate displayed distances and angles upon simulation frame change.
* Bug fix: StrainTensor.XZ and StrainTensor.YZ components output by Atomic Strain modifier were swapped.
* Bug fix: Fixed issue in Histogram modifier that occured when the x-range was fixed to an interval smaller than the value range.
* Bug fix: Atom type ordering is now maintained when importing a sequence of LAMMPS dump files with named atom types.

---------------------------
Version 2.3.3 (22-May-2014)
---------------------------

* Added user options to application settings dialog that provide control over certain OpenGL-related settings. This allows working around compatibility problems on some systems.
* User can now choose between a dark and a light viewport color scheme.
* Added scripting interface for Tachyon renderer.
* Added support for NetCDF files with variable particle numbers and with named particle types.
* Added user options that control the automatic fetching of the news page from the web server and the transmission of the installation ID.
* Fixed bug in camera orbit mode, which was not correctly restricting the camera rotation for some coordinate system orientations.

---------------------------
Version 2.3.2 (07-Apr-2014)
---------------------------

* Fixed bug in Wigner-Seitz analysis modifier, which could cause a program crash when numbers of atoms in reference and current configuration differ.

---------------------------
Version 2.3.1 (01-Apr-2014)
---------------------------

* Added saving and loading of presets for file-column-to-property mappings.
* Added the *--exec* command line option, which allows to directly execute a script command or to pass parameters to a script file.
* When opening an XYZ file, the column mapping dialog now displays an excerpt from the file's header to help the user figure out the mapping.
* The Construct Surface Modifier no longer creates cap polygons if the periodic simulation cell contains no particles.

---------------------------
Version 2.3.0 (29-Mar-2014)
---------------------------

* Added the new scripting interface, which allows to automate tasks.
* Added the 'Freeze property' modifier, which can prevent a particle property from changing over time.
* Added the 'Scatter plot' modifier, which plots one particle property against another. This modifier has been contributed by Lars Pastewka.
* Added the 'Wigner-Seitz analysis' modifier, which can identify vacancies and interstitials in a lattice.
* Added a file importer for NetCDF files. Code was contributed by Lars Pastewka.
* Added more input variables to the 'Compute property' and 'Expression select' modifiers (e.g. reduced particle coordinates and simulation cell size).
* It's now possible to load a sequence of files with each file containing multiple frames.
* Fixed bug in CFG file importer, which did not read triclinic simulation cells correctly.
* Fixed shader compilation error on OpenGL 2.0 systems and some other OpenGL related issues.

---------------------------
Version 2.2.4 (29-Jan-2014)
---------------------------

* Fixed particle picking issue on computers with Intel graphics.
* Fixed OpenGL display issues on systems with Intel graphics.
* Fixed blurred viewport captions.
* Fixed program crash when changing particle radius/color without having selected a particle type first.

---------------------------
Version 2.2.3 (16-Jan-2014)
---------------------------

* Fixed the CFG file importer, which can now read CFG files written by newer versions of LAMMPS correctly. Auxiliary file columns are automatically mapped to OVITO's standard particle properties if possible.
* Modified particle file importers to ensure stable ordering of particle types (using lexicographical ordering when atom types have names, and ID-based ordering otherwise). The ordering of named particle types is now independent of their first occurrence in the input file.
* Improved compatibility with some OpenGL implementations (Intel HD graphics on Windows and ATI Mobility Radeon HD 5470).
* A 64-bit version of the program is now available for Windows.
* A construction grid can be displayed in the viewports.

---------------------------
Version 2.2.2 (05-Jan-2014)
---------------------------

* Fixed the following regression: Rendering a video with OVITO 2.2.1 resulted in an empty movie file.
* Fixed display of the polygon path when using Fence selection (Manual Selection modifier).

---------------------------
Version 2.2.1 (26-Dec-2013)
---------------------------

* Added a file parser for *binary* LAMMPS dump files.
* Added a dialog window that displays information about the system's OpenGL graphics driver. This dialog can be accessed via the Help menu.
* Fixed bug in the Expression Select and Compute Property modifiers, which couldn't handle particle property names that start with a number.
* The OpenGL compatibility profile is now used instead of the core profile on Windows and Linux platforms.
* Fixed an issue in the Construct Surface Mesh modifier, which sometimes led to a program crash on Windows.

---------------------------
Version 2.2.0 (15-Dec-2013)
---------------------------

* The Construct Surface Mesh modifier has been added, which builds a polygonal mesh around a particle set.
* The Cluster Analysis modifier has been added, which decomposes a particle system into clusters.
* A new experimental visualization module has been added, which allows working with data generated by the Crystal Analysis Tool and the Dislocation Extraction Algorithm (DXA).
* The Coordination Analysis modifier can now export the computed radial distribution function to a text file.
* Added a new user option to the application settings dialog that allows turning off the restriction of the vertical camera rotation.
* Added the File->New Window menu item, which opens another OVITO window. This makes life easier on the Mac OS platform, where starting multiple instances of the application is difficult.
* The XYZ file exporter now writes particle type names instead of numeric type identifiers.
* Added help buttons to parameter panels, which open the corresponding page in the user manual.
* The manual is now included in every installation package. An internet connection is no longer necessary to access the manual.
* Fixed the rendering of particle markers.

---------------------------
Version 2.1.0 (15-Nov-2013)
---------------------------

* The **Manual Selection** modifier has been added, which allows selecting individual particles with the mouse in the viewports. With the "Fence selection" mode, a group of particles can be easily selected by drawing a closed path around it.
* OVITO is now able to display **particles with cubic and square shape**. This can be useful in visualizing large 2d lattice systems or Ising models.
* OVITO now gives the user the option to import more than one dataset into the same scene and display them side by side.
* A newly added VTK file importer allows reading **triangle meshes** to visualize geometric objects such as an indentor tip.
* Camera objects can be created through the viewport context menu. A viewport can be linked to a camera object to show the corresponding view.
* The OpenGL rendering code has been updated to better support older graphics cards and to improve compatibility with more graphics drivers.
* The Tachyon renderer now supports **semi-transparent particles**. The transparency is controlled through the "Transparency" particle property. Use, for instance, the *Computer Property* modifier to set this property for certain particles. Transparency values can range from 0 (=fully opaque) to 1 (=not visible). Note that the interactive OpenGL renderer does not support transparency yet. Thus, all particles will still appear fully opaque here.
* When importing a sequence of simulation snapshots, one can now configure the mapping of input frames to OVITO's internal animation frames. This allows to generate output movies with fewer (or more) frames than the imported snapshot sequence. This feature is in preparation for a future camera animation system.
* The Mac OS version is now built against version 5.2 (beta) of the Qt library. This should fix a nasty UI bug on this platform (due to the old version of that library), which made text fields lose input focus.
* Fixed saving/loading of the gradient type selected in the Color Coding modifier.
* Fixed a program deadlock when dragging the time slider with the mouse after loading a file sequence from a remote location.

---------------------------
Version 2.0.3 (22-Oct-2013)
---------------------------

* Ported Tachyon raytracing renderer from old OVITO 1.1.0 release. This software-based rendering engine allows to produce images with high-quality shading and ambient occlusion lighting.
* The *Create Bonds* modifier will automatically turn off the display of bonds when (accidentally?) creating a large number of bonds (>1 million), which would make the program freeze for at least several seconds.
* The *Displacement Vectors* modifier now supports relative reference frames, i.e., displacements can be calculated from two snapshots separated by a fixed time interval. Before this addition, the modifier could only compute displacements with respect to a fixed reference simulation snapshot.
* The *Inspect Particle* applet now lets one select multiple particles and can report distances and angles between particles.
* Added 'Clear history' button to remote file import dialog.
* The POSCAR file exporter now writes the new file format, which includes atom type names.
* Added support for computers with high-resolution (Retina) displays.
* Fixed bug in the *Affine Transformation* modifier leading to recursive updates.

---------------------------
Version 2.0.2 (30-Sep-2013)
---------------------------

* Fixed loading of multi-timestep files with names containing a digit.
* Fixed import of CFG file with atom type information.

---------------------------
Version 2.0.1 (27-Sep-2013)
---------------------------

* Fixed loading of file sequences based on wildcard pattern on Windows platform.
* Replaced const arrays in GLSL shaders with uniform variables to support older Intel graphics chips.

---------------------------
Version 2.0.0 (25-Sep-2013)
---------------------------

* Many changes, almost complete rewrite of OVITO's code base.
