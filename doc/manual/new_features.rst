.. _new_features:

=========
Changelog
=========

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.11.2>`__

----------------------------
Version 3.11.2 (xx-Nov-2024)
----------------------------

- :ref:`particles.modifiers.affine_transformation` modifier: (pure) rotations are now applied to the ``Orientation`` property of particles if present
- :ref:`particles.modifiers.coordination_analysis` modifier: Exclude types of unselected particles from partial RDF legend if :guilabel:`Use only selected particles` option is enabled
- Fixed crash when closing OVITO while :ref:`particles.modifiers.generate_trajectory_lines` modifier is still processing
- Fixed GUI issue on Linux: Pressing enter key while focus is in a text field may toggle a parent group checkbox
- |ovito-python| Added the :py:class:`ovito.traits.Matrix3` parameter trait type
- |ovito-pro| VisRTX: Performance improvements for rendering particles with user-defined shapes
- |ovito-pro| VisRTX: Fixed program crash when rendering multiple interactive viewports in parallel
- |ovito-pro| VisRTX: Fixed color compositing of semi-transparent objects in interactive viewports
- |ovito-pro| VisRTX: Reduced z-fighting artifacts along wireframe lines in interactive viewports
- |ovito-pro| VisRTX: Improved object picking speed in interactive viewports for large particle datasets

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.11.1>`__

----------------------------
Version 3.11.1 (11-Nov-2024)
----------------------------

- Added read/write support for zstandard (`*.zst`) compressed files (e.g. `custom/zstd` `LAMMPS dump style <https://docs.lammps.org/dump.html>`__)
- New :ref:`Viewport Graphics Configuration <viewports.configure_graphics_dialog>` dialog
- Added support for VTK files with header type `UInt32`
- Fixed regression: :ref:`particles.modifiers.identify_diamond_structure` modifier reports *all* atoms as "OTHER"
- |ovito-python| Added support for Python 3.13
- |ovito-python| Windows: Ensure that DLLs are always loaded from the package directory and never from other locations in the Windows search path

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.11.0>`__

----------------------------
Version 3.11.0 (08-Oct-2024)
----------------------------

.. image:: /images/new_features/3-11-0_render-vectors.*
  :width: 40%
  :align: right

.. rubric:: Visualization of vector information

We've added the :ref:`Vectors <scene_objects.vectors>` object type to OVITO and the corresponding :py:class:`ovito.data.Vectors` Python class,
which allow placing arrow glyphs at arbitrary locations in 3d space (independent from particles) to visualize vectorial information anywhere.

.. image:: /images/scene_objects/vectors_ellipsoids_example.*
  :width: 30%

.. rubric:: Symmetric range option for color mapping tools

.. image:: /images/new_features/3-11-0_symmetric-colormap.*
  :width: 40%
  :align: right

The :ref:`particles.modifiers.color_coding` modifier and other color mapping tools can now maintain
a symmetric value range, which is useful for visualizing scalar fields that can take both positive and negative values.
If this option is enabled, the color mapping range is automatically centered around zero.

.. rubric:: Template system for :ref:`viewport layers <viewport_layers>`

Similar to :ref:`modifier templates <modifier_templates>`, you can now create pre-configured templates for :ref:`viewport layers <viewport_layers>` and reuse them in future program sessions.
This feature is particularly useful for creating consistent overlays across multiple visualizations.

.. rubric:: Interactive viewport rendering with VisRTX |ovito-pro|

On Windows and Linux machines with a CUDA-capable NVIDIA GPU, OVITO Pro now supports interactive rendering with the :ref:`high-performance VisRTX ray-tracing backend <rendering.visrtx_renderer>`.
This allows you to explore your data interactively in the viewports with high-fidelity rendering quality.

.. |visrtx-interactive-demo-video1| raw:: html

  <video width="48%" controls autoplay muted loop playsinline>
    <source src="https://www.ovito.org/download/data/documentation/container_visrtx_cut.mp4" type="video/mp4">
  </video>

.. |visrtx-interactive-demo-video2| raw:: html

  <video width="45%" controls autoplay muted loop playsinline>
    <source src="https://www.ovito.org/download/data/documentation/visrtx_large_system.mp4" type="video/mp4">
  </video>

|visrtx-interactive-demo-video1|
|visrtx-interactive-demo-video2|

.. rubric:: Demo versions of the high-fidelity rendering backends in OVITO Basic

OVITO Basic now includes demo versions of the rendering backends :ref:`OSPRay <rendering.ospray_renderer>`,
:ref:`Tachyon <rendering.tachyon_renderer>`, and :ref:`VisRTX <rendering.visrtx_renderer>`.

.. image:: /images/modifiers/spatial_binning_example_dislocations.*
  :width: 40%
  :align: right

.. rubric:: Calculation of dislocation density and Nye tensor fields from DXA output |ovito-pro|

We've extended the :ref:`particles.modifiers.bin_and_reduce` modifier to compute the dislocation density field from the
output of the :ref:`DXA analysis <particles.modifiers.dislocation_analysis>`. The new option also supports calculating
the Nye tensor field from the discrete dislocation lines.

.. rubric:: Identification of dislocation core atoms |ovito-pro|

.. image:: /images/modifiers/dxa/dislocation_analysis_core_atoms_marked.*
  :width: 30%
  :align: right

We've developed a method for identifying atoms that are part of the defect core of individual dislocations.
This pro feature is now available in the :ref:`particles.modifiers.dislocation_analysis` modifier as a new option,
see :ref:`Identification of dislocation core atoms <particles.modifiers.dislocation_analysis.mark_core_atoms>`.

.. rubric:: Python code generator for complex pipeline architectures |ovito-pro|

The :ref:`Python code generator <python_code_generation>` now supports scenes containing multiple pipelines and can generate
valid script code for the most complex pipeline architectures, including branched pipelines, shared modifiers, and shared visual elements created with the interactive OVITO GUI.
It also got better at handling viewport layers in complex visualization setups.

.. image:: /images/new_features/3-11-0_code-generator-schematic.*
  :width: 100%

.. rubric:: Improved rendering quality for semi-transparent objects and antialiased edges |ovito-pro|

:ref:`rendering.ospray_renderer`: Improved rendering quality for semi-transparent objects and fixed dark artifacts along object edges on light backgrounds

.. figure:: /images/new_features/ospray_transparency_old.*
  :figwidth: 28%

  Old

.. figure:: /images/new_features/ospray_transparency_new.*
  :figwidth: 28%

  New

.. rubric:: Further improvements in this program release:

- Major code refactoring to improve performance and maintainability; elimination of unnecessary data copies in the pipeline system
- Modifiers now keep their computed results in memory when being temporarily turned off by the user to avoid recomputation when reenabled again
- Better decoupling of UI and pipeline system, in particular to speed up the processing of long trajectories, which caused an unnecessary number of display updates in the past
- :ref:`particles.modifiers.generate_trajectory_lines` modifier: Trajectory processing now happens automatically in the background - it is no longer necessary to start the process manually
- :ref:`particles.modifiers.affine_transformation` modifier: Rotational transformations now act on the Burgers vectors of :ref:`dislocation lines <scene_objects.dislocations>`
- :ref:`particles.modifiers.dislocation_analysis` modifier: Ensure cross-platform reproducible output
- :ref:`particles.modifiers.unwrap_trajectories` modifier: Performance optimizations
- Added a notification dialog that is shown on application startup when new program updates become available
- Simulation file import: Updated mass of *Zn* in internal table of elements from 65.409 (pre 2007 value) to 65.38 (see https://www.ciaaw.org/zinc.htm) - old value is still recognized for compatibility reasons
- The Aspherix file reader can now load both *particle-particle* and *particle-wall* contact networks
- Added parser support for modern Aspherix *.vtm* files containing a "Bodies" section. Bodies information from non-convex particle simulations is read in as a :ref:`data table <scene_objects.data_table>`.

  .. figure:: /images/new_features/3-11-0_bodies-before.*
    :figwidth: 40%

    Each Aspherix DEM body is originally composed of multiple convex sub-particles.

  .. figure:: /images/new_features/3-11-0_bodies-after.*
    :figwidth: 40%

    A Python extension allows combining the sub-particles to non-convex body particles.

- Import of existing CA files (i.e. loading of precomputed :ref:`DXA results <particles.modifiers.dislocation_analysis.fileformat>`) is now an exclusive OVITO Pro program feature
- :ref:`particles.modifiers.slice` and :ref:`particles.modifiers.affine_transformation` modifiers: Step-wise parameter increments are now proportional to the simulation box size instead of the current parameter value to improve usability during :ref:`interactive adjustments <usage.spinner_widgets>`
- Trailing "_" in imported particle property names are now stripped to avoid possible conflicts with the :py:ref:`underscore notation <underscore_notation>` in the Python API
- Updated third-party components to latest versions: Python, OpenSSL, Qt, PySide6, Qwt
- |ovito-pro| :ref:`particles.modifiers.time_averaging` modifier: Added time-averaging of the simulation cell shape and new option to overwrite original values with average
- |ovito-pro| :ref:`rendering.ospray_renderer`: Added support for pseudo-color mapping
- |ovito-pro| :ref:`rendering.visrtx_renderer`: Improved rendering performance for scenes with large numbers of cubic, ellipsoidal, or superquadric particles
- |ovito-pro| Environment variable ``OVITO_SAFE_MODE=1`` can be set to effectively block execution of Python scripts embedded in *.ovito* session state files from untrusted sources
- |ovito-pro| Updated the standard code template in the :ref:`particles.modifiers.python_script` modifier to use the :ref:`writing_custom_modifiers.advanced_interface`

.. rubric:: Python API additions and changes:

.. |point-in-mesh-demo-video| raw:: html

  <div class="align-right">
    <video width="300" height="300" controls autoplay muted loop playsinline>
      <source src="https://www.ovito.org/download/data/documentation/3-11-0_point-inside-mesh-demo.mp4" type="video/mp4">
    </video>
  </div>

- |ovito-python| Added support for NumPy 2.x
- |ovito-python| Full compatibility with Python 3.12
- |ovito-python| The OVITO module now initializes the global Qt application object only on demand to avoid conflicts with other Python packages that also use `PySide6 <https://doc.qt.io/qtforpython-6/>`__
- |ovito-python| :py:class:`~ovito.modifiers.ConstructSurfaceModifier`: Option :py:attr:`~ovito.modifiers.ConstructSurfaceModifier.map_particles_to_regions` now outputs per-region particle membership lists
- |point-in-mesh-demo-video| |ovito-python| The :py:meth:`ovito.data.SurfaceMesh.locate_point() <ovito.data.SurfaceMesh.locate_point>` method has been vectorized and can now process an array of input points using all CPU cores
- |ovito-python| New method :py:meth:`NearestNeighborFinder.find_all_at() <ovito.data.NearestNeighborFinder.find_all_at>` to efficiently determine the closest particles around several spatial locations at once
- |ovito-python| New method :py:meth:`SimulationCell.wrap_point() <ovito.data.SimulationCell.wrap_point>` to map one or more points into the primary simulation cell
- |ovito-python| New attribute :py:attr:`ovito.pipeline.Pipeline.num_frames`, which allows querying the number of (output) trajectory frames of a pipeline
- |ovito-python| New attribute :py:attr:`ovito.pipeline.Pipeline.frames`, which allows iterating over the output data collections computed by a pipeline for all trajectory frames
- |ovito-python| New attributes for file source objects:

  - :py:attr:`FileSource.playback_ratio <ovito.pipeline.FileSource.playback_ratio>`
  - :py:attr:`FileSource.playback_start_time <ovito.pipeline.FileSource.playback_start_time>`
  - :py:attr:`FileSource.static_frame <ovito.pipeline.FileSource.static_frame>`

- |ovito-python| New advanced methods:

  - :py:meth:`Pipeline.make_vis_element_independent() <ovito.pipeline.Pipeline.make_vis_element_independent>`
  - :py:meth:`Pipeline.get_replacement_vis_element() <ovito.pipeline.Pipeline.get_replacement_vis_element>`

- |ovito-python| New attributes in class :py:class:`ViewportOverlayInterface.Canvas <ovito.vis.ViewportOverlayInterface.Canvas>`:

  - :py:attr:`~ovito.vis.ViewportOverlayInterface.Canvas.camera_pos`
  - :py:attr:`~ovito.vis.ViewportOverlayInterface.Canvas.camera_dir`
  - :py:attr:`~ovito.vis.ViewportOverlayInterface.Canvas.camera_up`

- |ovito-python| New parameter trait types:

  - :py:class:`ovito.traits.DataObjectReference`
  - :py:class:`ovito.traits.PropertyReference`

- |ovito-python| New class :py:class:`ovito.data.DataObject.Ref` and methods :py:meth:`ovito.data.DataCollection.get` and :py:meth:`ovito.data.PropertyContainer.get`
- |ovito-python| :py:class:`~ovito.vis.ColorLegendOverlay`: Added :py:attr:`~ovito.vis.ColorLegendOverlay.pipeline` field
- |ovito-python| Added the :py:attr:`HistogramModifier.select_elements <ovito.modifiers.HistogramModifier.select_elements>` option
- |ovito-python| Added support for Python's `copy <https://docs.python.org/3/library/copy.html>`__ module, which allows creating exact copies of OVITO data objects
- |ovito-python| Deprecated method :py:meth:`!GenerateTrajectoryLinesModifier.generate` (trajectory line generation is now done automatically by the modifier)

.. rubric:: Bug fixes:

- :ref:`Data table plots <data_inspector.data_tables>`: Axis scales and labels are invisible (white on white) in exported plots if dark UI theme is active
- :ref:`Data table plots <data_inspector.data_tables>`: Regression due to updated Qt framework: missing colors in plot legends
- Mouse cursor wrapping in :ref:`spinner widgets <usage.spinner_widgets>` not working for vertical multi-screen setups
- Mouse cursor wrapping requires accessibility access on macOS
- |ovito-pro| :ref:`particles.modifiers.time_averaging` modifier uses wrong divisor in average calculation if trajectory length is not an integer multiple of the sampling frequency
- |ovito-pro| :ref:`modifiers.render_lammps_regions` modifier: unexpected error if the modifier is inserted more than once into the same pipeline

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.6>`__

----------------------------
Version 3.10.6 (03-May-2024)
----------------------------

- LAMMPS data and dump files: Added I/O support for general triclinic simulation cells, which will soon be introduced by LAMMPS
- LAMMPS dump and IMD file export: Use correct vector component names in output file columns for user-defined particle properties
- |ovito-python| :py:func:`ovito.io.ase.ase_to_ovito` function: filter out non-string metadata keys if present in the ASE `Atoms` object
- |ovito-python| :py:func:`ovito.io.import_file` function: Use Python's `getpass() <https://docs.python.org/3/library/getpass.html>`__ function to prompt for SSH password without echoing it to the console

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.5>`__

----------------------------
Version 3.10.5 (17-Apr-2024)
----------------------------

- Added new :ref:`standard particle property <particle-properties-list>` ``Vector Transparency``, which allows controlling the :ref:`transparency of vector glyphs <visual_elements.vectors>` on a per-particle basis
- Fixed endless loop when trying to cancel SSH authentification dialog after opening an existing session state file
- Fixed a bug that prevented user changes to particle type parameters in combination with the :ref:`particles.modifiers.unwrap_trajectories` modifier
- :ref:`particles.modifiers.coordination_analysis` modifier: Lifted upper limit on the number of RDF bins
- |ovito-python| Added option :py:attr:`OpenGLRenderer.order_independent_transparency <ovito.vis.OpenGLRenderer.order_independent_transparency>`
- |ovito-python| Documented the capability of :py:func:`ovito.io.import_file` to load files from remote SSH and HTTPS servers
- |ovito-pro| Corrected sun-sky light brightness scale of :ref:`rendering.ospray_renderer` to match old behavior (v3.10.0 regression)

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.4>`__

----------------------------
Version 3.10.4 (13-Mar-2024)
----------------------------

- :ref:`viewports.adjust_view_dialog`: Added option to numerically control the camera's roll angle
- Updated third-party libraries: OpenSSL 3.0.13, libssh 0.10.6, ffmpeg 6.1.1, Python 3.11.8, VisRTX 0.8.0
- |ovito-python| Added :py:attr:`CreateIsosurfaceModifier.smoothing_level <ovito.modifiers.CreateIsosurfaceModifier.smoothing_level>`
  and :py:attr:`CreateIsosurfaceModifier.identify_regions <ovito.modifiers.CreateIsosurfaceModifier.identify_regions>` options
- |ovito-pro| :ref:`particles.modifiers.create_isosurface` modifier: New option to identify spatial regions enclosed by the isosurface
- |ovito-pro| Improved :ref:`glTF file export <file_formats.output.gltf>`: particles and bonds may now be exported as single meshes for better rendering performance (but larger file size)
- |ovito-pro| :ref:`rendering.ospray_renderer`: Fixed picking of focal length in the interactive viewports
- |ovito-pro| :ref:`rendering.visrtx_renderer`: Added mesh backface culling support, gracefully handle initialization errors
- |ovito-pro| :ref:`particles.modifiers.construct_surface_mesh` modifier: Changed how the "external" spatial region is defined in open (non-periodic) systems, now having its volume reported as ``inf``

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.3>`__

----------------------------
Version 3.10.3 (19-Feb-2024)
----------------------------

- Fix: :ref:`visual_elements.lines` visual element renders wrong line caps when using option "Show up to current time only"
- Fix: Drop-down list of available modifiers inserts wrong modifier template into pipeline after an entry was added to the list
- |ovito-python| Fix: :py:meth:`~ovito.vis.Viewport.create_jupyter_widget` method failing (`issue #229 <https://gitlab.com/stuko/ovito/-/issues/229>`__)
- |ovito-python| Fix: Issue with :py:class:`ovito.traits.Color` not accepting a NumPy array
- |ovito-pro| Added parameter for ambient occlusion cutoff to :ref:`VisRTX renderer <rendering.visrtx_renderer>`
- |ovito-pro| Fix: Warnings in :ref:`modifiers.render_lammps_regions` modifier due to code line duplications

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.2>`__

----------------------------
Version 3.10.2 (02-Feb-2024)
----------------------------

- Rendering of animated GIFs with transparent background color
- LAMMPS data file exporter: Emit 0 atom/bond types if no types are defined and there are no particles/bonds
- :ref:`particles.modifiers.load_trajectory`: Drop 'Periodic Image' particle property from topology dataset if trajectory file does not contain dynamic image flags
- |ovito-python| Added :ref:`support for the Python multiprocessing module <multiprocess_module_usage>`
- |ovito-python| Added Python API for accessing the line connectivity information in a :py:class:`~ovito.data.DislocationNetwork`
- |ovito-pro| Fixed OSPRay image rendering on transparent background (v3.10.0 regression)
- |ovito-pro| macOS: Fixed crash in OSPRay renderer due to missing dylib (v3.10.0 regression)

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.1>`__

----------------------------
Version 3.10.1 (09-Jan-2024)
----------------------------

- Added support for the LAMMPS *dump yaml* file format to the :ref:`file_formats.input.lammps_dump`
- :ref:`particles.modifiers.show_periodic_images` modifier: Use particle property ``Periodic Image`` if present to yield correct replicated molecule identifiers
- Fix: Frame 0 of LAMMPS dump, xyz and pdb trajectory files gets loaded a second time unnecessarily
- |ovito-python| New Python properties :py:attr:`Pipeline.translation <ovito.pipeline.Pipeline.translation>` and :py:attr:`Pipeline.rotation <ovito.pipeline.Pipeline.rotation>`, which control the placement of a pipeline's visual output in the 3d scene
- |ovito-python| Fixed offscreen font rendering in standalone Python module on (headless) Linux platform

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.10.0>`__

----------------------------
Version 3.10.0 (28-Dec-2023)
----------------------------

.. rubric:: Perspective distortion for axis indicators

The :ref:`viewport_layers.coordinate_tripod` provides a new option to distort the displayed tripod
according to the perspective projection. Now the axes indicators exactly align with the edges of the simulation cell:

.. figure:: /images/new_features/tripod_perspective_3-9-4.*
  :figwidth: 25%

  OVITO 3.9

.. figure:: /images/new_features/tripod_perspective_3-10-0.*
  :figwidth: 25%

  OVITO 3.10

.. rubric:: 3D full-scene export to glTF format |ovito-pro|

The new :ref:`file_formats.output.gltf` can export OVITO models including all visual elements. This lets you export
entire scenes to PowerPoint, Blender, or the web as movable 3d objects:

.. image:: /images/io/gltf_export_powerpoint.*
  :width: 100%

.. |model-viewer-demo| raw:: html

  <script type="module" src="https://ajax.googleapis.com/ajax/libs/model-viewer/3.3.0/model-viewer.min.js"></script>
  <model-viewer src="_static/ovito_logo.glb" camera-controls poster="_static/ovito_logo_poster.webp" shadow-intensity="0.91" exposure="0.89" shadow-softness="0.03" environment-image="legacy" camera-orbit="155.9deg 67.65deg 28.94m" field-of-view="29.33deg"> </model-viewer>

Below is a `glTF model <_static/ovito_logo.glb>`__ exported from *OVITO Pro*, embedded into this HTML page. **Click and drag to rotate the model.**
The 3d viewer works only in the `online version of this document <https://docs.ovito.org/new_features.html#>`__ due to web browser security restrictions.
In the offline version of the OVITO docs, you will see only a static image of the model.
|model-viewer-demo|

.. rubric:: New Python classes to paint arbitrary 3d lines |ovito-pro|

The new :py:class:`ovito.data.Lines` and :py:class:`ovito.vis.LinesVis` classes allow
visualizing line segments in 3d space, e.g., for augmenting particle models with additional
information.

.. image:: /images/new_features/lines_spirales_demo.gif
  :width: 30%

.. rubric:: |ovito-pro| New renderer: NVIDIA VisRTX

.. image:: /images/new_features/Anari_logo.svg
  :width: 30%
  :align: right

We've added the :ref:`rendering.visrtx_renderer` and a corresponding Python class :py:class:`ovito.vis.AnariRenderer`,
which make use of the cross-vendor `Khronos ANARI <https://www.khronos.org/anari/>`__ API.
The VisRTX rendering backend offers hardware-accelerated ray-tracing on NVIDIA GPUs and can generate high-fidelity scene renderings
in a fraction of a second -- even for complex datasets containing millions of objects.

.. |visrtx-video| raw:: html

  <video width="301" height="220" controls autoplay muted loop playsinline>
    <source src="https://www.ovito.org/download/data/documentation/visrtx_render_demo.mp4" type="video/mp4">
  </video>

|visrtx-video|

.. rubric:: Remote trajectory rendering function |ovito-pro|

Added a function for :ref:`usage.remote_rendering`. It lets you prepare trajectory visualizations on your local computer
and easily render them on a parallel HPC cluster. OVITO Pro takes care of packaging all required data files
and generating the necessary job scripts for you.

.. image:: /images/new_features/remote_rendering_function.png
  :width: 40%

.. rubric:: Further changes in this program release:

* :ref:`particles.modifiers.smooth_trajectory` modifier: Now supports trajectories with varying number of particles
* :ref:`particles.modifiers.smooth_trajectory` modifier: Fixed wrong interpolation/averaging of particle orientations
* :ref:`file_formats.input.lammps_data`: Tolerate more than one empty line after file section titles
* :ref:`file_formats.input.xyz`: Automatic detection of reduced coordinates turned off by default, because extended XYZ files with reduced coordinates are very rare
* |ovito-python| Some Python functions now return true NumPy arrays instead of Python tuples
* |ovito-python| New Python function :py:meth:`DislocatioNetwork.Line.point_along_line() <ovito.data.DislocationNetwork.Line.point_along_line>`
* |ovito-python| New parameter trait types :py:class:`ovito.traits.FilePath`, :py:class:`~ovito.traits.Vector2`, and :py:class:`~ovito.traits.Vector3`
* |ovito-python| Renamed existing parameter traits types :py:class:`ovito.traits.OvitoObject` and :py:class:`ovito.traits.Color`
* |ovito-python| Restricted :py:meth:`ovito.Scene.load` to session state files written by *OVITO Pro* or the Python module
* |ovito-python| Added function parameter ``pipeline_node`` to :py:meth:`ModifierInterface.modify() <ovito.pipeline.ModifierInterface.modify>`
* |ovito-python| :py:class:`~ovito.data.SurfaceMeshTopology` class now performs out-of-range checks on function parameters.
* |ovito-pro| OpenGL, OSPRay, and Tachyon renderers: Added buttons to reset numeric parameters to their default values
* |ovito-pro| :ref:`User-defined parameters <writing_custom_modifiers.advanced_interface.user_params>` can now be grouped in the UI by means of the new ``ovito_group`` metadata attribute

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.9.4>`__

---------------------------
Version 3.9.4 (04-Nov-2023)
---------------------------

* Fix: OpenGL rendering of ellipsoidal/superquadric/box particles with wrong orientations (regression since v3.9.0)

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.9.3>`__

---------------------------
Version 3.9.3 (01-Nov-2023)
---------------------------

* :ref:`particles.modifiers.expand_selection` modifier: Added new output attribute `ExpandSelection.num_added`
* :ref:`particles.modifiers.identify_diamond_structure` modifier: Added missing output attribute `IdentifyDiamond.counts.OTHER`
* Fix: Section "Modifier templates" of available modifiers list not updated correctly when adding/removing templates
* Fix: LAMMPS data file reader: vector vis settings of `Velocity` property lost after loading a state file
* Fix: Segfault when opening a .ovito state file with macOS Finder while OVITO is already running
* Fix: :ref:`particles.modifiers.construct_surface_mesh` modifier: Option `Map particles to regions` may yield invalid results when used with option `Use only selected input particles`
* Updated third-party components: OpenSSL 1.1.1w, Qt/PySide6 6.5.3, Python 3.11.6
* |ovito-python| PyPI packages for Python 3.12

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.9.2>`__

---------------------------
Version 3.9.2 (31-Aug-2023)
---------------------------

* Support Ctrl+C copy to clipboard in table of distances, angles, and dislocations in the data inspector
* Fix: Text label viewport layer accidentally disables 3d depth test in interactive viewports if used as an underlay
* |ovito-python| :py:meth:`~ovito.vis.Viewport.render_image` and :py:meth:`~ovito.vis.Viewport.render_anim` now raise exceptions in case an error occurs in any of the scene pipelines (can be changed via new parameter `stop_on_error`)
* |ovito-python| New flag :py:attr:`Pipeline.preliminary_updates <ovito.pipeline.Pipeline.preliminary_updates>`
* |ovito-python| Corrected data column headers in XYZ, LAMMPS dump, and IMD files written via :py:func:`~ovito.io.export_file` if a vector property was specified in the `columns` list
* |ovito-pro| New class-based programming interface for custom viewport overlays: :py:class:`ovito.vis.ViewportOverlayInterface`
* |ovito-pro| Build Conda package as monolithic binaries for improved performance of the Python interface
* |ovito-pro| Updated third-party components: OpenSSL 1.1.1v, PySide6 6.5.2, Python 3.11.5

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.9.1>`__

---------------------------
Version 3.9.1 (06-Aug-2023)
---------------------------

* Fix: Voronoi Analysis modifier crashes if simulation cell is degenerate or atom count is zero, and option `Generate neighbor bonds` is turned on
* |ovito-python| New Python class :py:class:`ovito.pipeline.PipelineSourceInterface`
* |ovito-python| New Python method :py:meth:`ModifierInterface.compute_trajectory_length() <ovito.pipeline.ModifierInterface.compute_trajectory_length>`, which gives user-defined modifiers control over the timeline length
* |ovito-python| New Python field :py:attr:`Modifier.title <ovito.pipeline.Modifier.title>`
* |ovito-pro| Fixed :command:`ovitos -m pip install` failure for packages that require a build step

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.9.0>`__

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
and shared online with other OVITO users if desired -- we have set up the new `OVITO Extensions Directory <https://www.ovito.org/extensions/>`__ for that purpose.
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
* The :ref:`Vulkan viewport renderer <viewports.configure_graphics_dialog>` has been removed
* |ovito-python| New Python methods :py:meth:`Property.add_type_id <ovito.data.Property.add_type_id>` and :py:meth:`Property.add_type_name <ovito.data.Property.add_type_name>`
* |ovito-python| New Python method :py:meth:`VoxelGrid.view <ovito.data.VoxelGrid.view>`
* |ovito-python| Performance optimizations for property data access from Python code

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.5>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.4>`__

---------------------------
Version 3.8.4 (03-May-2023)
---------------------------

* Fix: ffmpeg video encoding crashes on Windows if output path contains non-ascii characters
* Silence console message "Numeric mode unsupported in the posix collation implementation" on Linux by enabling ICU support in Qt build
* |ovito-pro| Fix: Segfault in PySide6 package initialization on Linux when adding a Python layer to a viewport
* |ovito-python| Fix: Interchanged xz/yz simulation box shear components in :py:func:`~ovito.io.lammps.lammps_to_ovito` Python function

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.3>`__

---------------------------
Version 3.8.3 (16-Apr-2023)
---------------------------

* Further improved performance of sequential loading of compressed trajectory files
* Fixed regression (since v3.8.0): :py:meth:`Viewport.render_anim() <ovito.vis.Viewport.render_anim>` renders only first animation frame
* |ovito-python| Python exceptions raised in user-defined modifier functions are now propagated up the call chain to where the pipeline evaluation was triggered
* |ovito-pro| Included ``bz2`` and `sqlite3` standard modules, which were missing in embedded Python interpreter on Linux

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.2>`__

---------------------------
Version 3.8.2 (04-Apr-2023)
---------------------------

* Implemented fast access to trajectory frames in compressed (gzipped) files
* Fix: Segfault when using zoom function in viewport with an attached camera object
* Fix: Segfault in :ref:`particles.modifiers.coordination_polyhedra` modifier on Linux
* Fix: Function 'load/save session state' does not follow global working directory

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.1>`__

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

* :ref:`file_formats.input.lammps_data`: Accept '#' in type names, which are referenced in data sections of the file

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.8.0>`__

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
* |ovito-python| Accept ``os.PathLike`` objects in Python functions :py:func:`~ovito.io.import_file` and :py:func:`~ovito.io.export_file`.
* |ovito-python| :py:meth:`PropertyContainer.create_property <ovito.data.PropertyContainer.create_property>`: Accept ``data`` values that are broadcastable to shape of property array.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.12>`__

----------------------------
Version 3.7.12 (16-Dec-2022)
----------------------------

* GRO file reader: `Recognize additional chemical symbols SI, FE, BR <https://www.ovito.org/forum/topic/only-first-letter-of-particle-types-read-from-gro-file/>`_.
* STL file reader: Tolerate leading whitespace on first line.
* Updated third-party libraries on Windows: Qt 6.4.1, OpenSSL 1.1.1s, ffmpeg 4.2.8, zlib 1.2.23.
* Fix: Voronoi cavity radius calculation is `wrong by a factor of 2 <https://www.ovito.org/forum/topic/only-first-letter-of-particle-types-read-from-gro-file/>`_.
* Fix: Function "Make Independent" does not work correctly for surface mesh visual elements in cloned pipelines.
* |ovito-pro| Fix: Python method  :py:meth:`ovito.data.SurfaceMesh.locate_point() <ovito.data.SurfaceMesh.locate_point>` can yield wrong results for coarse, one-sided meshes.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.11>`__

----------------------------
Version 3.7.11 (29-Oct-2022)
----------------------------

* Added user option to application settings dialog for changing the working directory behavior.
* Fixed regression: Slice modifier does not work on voxel grids.
* |ovito-pro| Vectorized all query methods of ``SurfaceMeshTopology`` class.
* |ovito-pro| Provide PyPI package for Python 3.11.
* |ovito-pro| Added flat array option to method ``SurfaceMesh.get_face_vertices()``.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.10>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.9>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.8>`__

---------------------------
Version 3.7.8 (29-Jul-2022)
---------------------------

* Fix: Program crash when quickly skipping through a trajectory consisting of a series of files loaded via SSH (regression OVITO 3.7.0).
* Fix: Visual artifacts when rendering cone primitives (3d arrow heads) at small length scales due to numerical precision issue.
* |ovito-pro| Added conda packages for Python 3.10.
* |ovito-pro| Added conda packages for macOS arm64/M1 platform.
* |ovito-pro| Work around a memory leak in some OpenGL graphics driver implementations when the :py:meth:`ovito.vis.Viewport.render_image() <ovito.vis.Viewport.render_image>` Python function is called repeatedly.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.7>`__

---------------------------
Version 3.7.7 (06-Jul-2022)
---------------------------

* Ubuntu 22.04 compatibility - Linux package of OVITO now includes a private copy of OpenSSL 1.1 libraries.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.6>`__

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
* |ovito-pro| Conda channel now provides additional variants of the `ovito <https://conda.ovito.org>`__ package (built against ``tbb`` v2020 and v2021), which avoids dependency conflicts with certain third-party packages when installing them in the same environment.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.5>`__

---------------------------
Version 3.7.5 (28-May-2022)
---------------------------

* Smooth trajectory modifier now supports varying number of particles.
* SSH client: Try password first before keyboard-interactive authentication for successful handshaking with some SSH servers.
* Performance improvements to OpenGL high-quality sphere rendering code
* Bug fix: Data inspector shows a 3rd text label in bar charts with 2 bars.
* Bug fix: Sporadic program crashes when importing CA files.
* |ovito-pro| :py:attr:`DataCollection.attributes <ovito.data.DataCollection.attributes>` dictionary can now store arbitrary Python objects.
* |ovito-pro| New Python method :py:meth:`ovito.data.Particles.remap_indices() <ovito.data.Particles.remap_indices>`.
* |ovito-pro| New Python method :py:meth:`ovito.data.SurfaceMesh.to_triangle_mesh() <ovito.data.SurfaceMesh.to_triangle_mesh>`.
* |ovito-pro| Bumped maximum neighbor limit of :py:class:`ovito.data.NearestNeighborFinder` to 64.
* |ovito-pro| Dropped support for Python 3.6, which has reached its end-of-life date.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.4>`__

---------------------------
Version 3.7.4 (18-Apr-2022)
---------------------------

* :ref:`particles.modifiers.centrosymmetry` modifier: New option '*Use only selected particles*'.
* :ref:`file_formats.input.lammps_data`: Added support for *Ellipsoids* section.
* Fix: Program crash during file format detection when importing file from path containing CJK or other non-ANSI characters.
* Fix: Error "The file source path is empty or has not been set" when picking a new simulation file of different format.
* |ovito-pro| :ref:`particles.modifiers.construct_surface_mesh` modifer: New option 'Map particles to regions'.
* |ovito-pro| New Python methods :py:meth:`ovito.data.DataCollection.create_cell() <ovito.data.DataCollection.create_cell>`, :py:meth:`ovito.data.DataCollection.create_particles() <ovito.data.DataCollection.create_particles>`, :py:meth:`ovito.data.Particles.create_bonds() <ovito.data.Particles.create_bonds>`.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.3>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.2>`__

---------------------------
Version 3.7.2 (03-Mar-2022)
---------------------------

* Improved render output window with image zoom function.
* Fix: Particle type colors not initialised correctly if imported LAMMPS dump file contains both 'type' and 'element' columns (`issue #193 note 403792737 <https://gitlab.com/stuko/ovito/-/issues/193#note_403792737>`_).
* |ovito-pro| macOS: Fixed PySide6 loading error due to wrong rpath information when importing PyPI ovito package.
* |ovito-pro| Linux: Fixed sqlite3 Python package included in the embedded Python interpreter of OVITO Pro.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.1>`__

---------------------------
Version 3.7.1 (26-Feb-2022)
---------------------------

* Fixed regression: Segfault when loading session state file containing a viewport camera object.
* |ovito-pro| New Python function :py:meth:`ovito.data.NearestNeighborFinder.find_all() <ovito.data.NearestNeighborFinder.find_all>`.
* |ovito-pro| :py:class:`ovito.data.PropertyContainer` classes support removing properties with the ``del`` statement.
* |ovito-pro| Inform user if insufficient file access permissions let license activation fail.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.7.0>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.6.0>`__

---------------------------
Version 3.6.0 (19-Nov-2021)
---------------------------

* :ref:`visual_elements.vectors`, :ref:`visual_elements.surface_mesh`, :ref:`visual_elements.voxel_grid`, :ref:`visual_elements.lines` visual elements: Added direct color mapping option as a faster alternative to the :ref:`particles.modifiers.color_coding` modifier.
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
* |ovito-pro| Added experimental :py:meth:`ovito.vis.Viewport.create_jupyter_widget() <ovito.vis.Viewport.create_jupyter_widget>` method for embedding OVITO viewports in Jupyter notebooks (see `demo binder <https://gitlab.com/ovito-org/ovito-binder>`_).
* |ovito-pro| Support for site-wide software licenses.
* |ovito-pro| Fix: Bounding box clipping artifact when rendering rotated superquadrics particles with OSPRay or Tachyon renderers.
* |ovito-pro| Fix: Warning "This plugin does not support createPlatformOpenGLContext!" when running in headless mode on Linux machines.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.5.4>`__

---------------------------
Version 3.5.4 (31-Jul-2021)
---------------------------

* LAMMPS data file reader and writer now support all LAMMPS atom styles, including the ``hybrid`` style.
* Fix: Construct surface mesh with region identification fails or never completes for some inputs.
* |ovito-pro| Fix: Tachyon renderer crashes when triangle mesh contains a degenerate vertex normal.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.5.3>`__

---------------------------
Version 3.5.3 (30-Jun-2021)
---------------------------

* Added two :ref:`tutorials` to the documentation.
* :ref:`visual_elements.voxel_grid` visual element now supports mouse-over data display in the status bar.
* Added invert function to :ref:`particles.modifiers.manual_selection` modifier.
* Warn user if OVITO Python module was installed via ``pip`` command in an Anaconda Python interpreter. Use ``conda install`` instead!
* Fix: *Configure Trajectory Playback* dialog shows no contents.
* Fix: Neighbor finder facilities do not ignore PBC flag along third dimension in 2D mode.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.5.2>`__

---------------------------
Version 3.5.2 (26-May-2021)
---------------------------

* :ref:`particles.modifiers.affine_transformation` modifier now allows entering the translation vector in reduced cell coordinates.
* :ref:`particles.modifiers.load_trajectory` modifier can now import ReaxFF bond information files written by the LAMMPS `fix reax/c/bonds <https://lammps.sandia.gov/doc/fix_reaxc_bonds.html>`_ command.
* GSD file reader: Fill particle property array with default values if a chunk is not present in current frame (`issue #206 <https://gitlab.com/stuko/ovito/-/issues/206>`_)
* |ovito-pro| Fix: Invisible simulation cell edges when rendering image with orthographic projection with OSPRay

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.5.1>`__

---------------------------
Version 3.5.1 (18-May-2021)
---------------------------

* The :ref:`particles.modifiers.coordination_analysis` modifier has gained an option '*Only selected particles*', which restricts RDF calculation to a subset of particles.
* The '*Generate neighbor bonds*' option of the :ref:`particles.modifiers.voronoi_analysis` modifier is now able to deal with small periodic simulation cells.
* Fix: Wireframe line rendering issue in perspective viewports.
* |ovito-pro| The :ref:`particles.modifiers.slice` modifier now accepts (*hkl)* Miller indices as input for defining the plane orientation. The plane position can be specified in terms of the interplanar spacing.
* |ovito-pro| OVITO Pro for Linux now ships with a current Python 3.9.5 interpreter.
* |ovito-pro| Fix: :py:meth:`ovito.data.PropertyContainer.create_property() <ovito.data.PropertyContainer.create_property>` method cannot create user-defined property of data type ``int64``.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.5.0>`__

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

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.4.4>`__

---------------------------
Version 3.4.4 (12-Mar-2021)
---------------------------

* Fix: Number of data columns not correctly detected for XYZ files with 5 atoms or less.
* Fix: Program crash when playing back animation with less than 1 frame per second in interactive viewports.
* Fix: Simulation cell not visible in interactive viewports on some computer systems (`issue #203) <https://gitlab.com/stuko/ovito/-/issues/203>`_.
* Fix: CIF file reader not automatically recognizing files written by Open Babel (`issue #204) <https://gitlab.com/stuko/ovito/-/issues/204>`_.
* |ovito-pro| Fix: OSPRay not rendering arrow glyphs correctly.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.4.3>`__

---------------------------
Version 3.4.3 (25-Feb-2021)
---------------------------

* Added text outline option to *Coordinate Tripod* viewport layer.
* Fixed UI issue: Status bar resizing due to invalid unicode character in text string.
* Corrected camera orientation of "Bottom" viewport view type when rotation constraint is turned on.
* Improved automatic detection of PDB file format.
* |ovito-pro| It's now okay to assign a simple string to the :py:class:`ovito.modifiers.ExpressionSelectionModifier` expression field.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.4.2>`__

---------------------------
Version 3.4.2 (15-Feb-2021)
---------------------------

* Long text strings displayed in the status bar of OVITO now get broken into two lines in order to show more property values in the available space.
* Bug fix: Status bar doesn't display latest set of particle properties while positioning the mouse cursor over a particle. This fix corrects a regression introduced with OVITO 3.4.0.
* Fixed a limitation of the PTM modifier not identifying diamond and graphene structures in small periodic simulation cells.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.4.1>`__

---------------------------
Version 3.4.1 (03-Feb-2021)
---------------------------

* Fixed runtime linker error when importing ``ovito`` Python module installed via pip on Linux.
* |ovito-pro| Spatial binning modifier can now operate on vectorial particle properties.

.. sidebar::

  * `Download this version <https://www.ovito.org/download_history/#3.4.0>`__

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
