.. _scene_objects.file_source:

External file
-------------

.. image:: /images/scene_objects/file_source_panel.*
  :width: 35%
  :align: right

The **External File** panel allows you to configure the data source for the current pipeline,
specifying which external file(s) should be used as input. This panel appears automatically after you
:ref:`import a data file <usage.import>` using OVITO's :menuselection:`Load File` or :menuselection:`Load Remote File` functions.

To return to this panel at any time, select the topmost entry under the **Data Source** section in the
:ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`.

.. _file_formats:

Supported file formats
""""""""""""""""""""""

The *External File* data source supports various file formats containing different :ref:`data object types <scene_objects>`.
The program automatically detects the file format and loads the data accordingly.

.. toctree::
  :maxdepth: 1

  ../../file_formats/file_formats_input
  ../../file_formats/file_formats_output

Toolbar functions
"""""""""""""""""

.. image:: /images/scene_objects/file_source_toolbar.*
  :width: 35%

Pick new file
  Selects a different file as input for the pipeline while preserving existing modifiers.

  When you pick a new file using this function, OVITO will *not* automatically detect whether it belongs to a numbered file sequence.
  If necessary, manually re-enable the :guilabel:`auto-generate` option to load a trajectory made of a series of files.

Pick new remote file
  Same as above but allows selecting a :ref:`file from a remote server <usage.import.remote>` instead of the local filesystem.

Reload file
  Updates the data for the current animation frame by reloading the external file.
  This is useful if the file has been modified externally, such as after re-running a simulation.

Update trajectory frames
  Refreshes the trajectory sequence by scanning the directory for new files matching the filename pattern
  or by detecting newly appended frames in a trajectory file.

Load entire trajectory into memory
  Loads all trajectory frames into memory to enable smoother animation playback.
  Ensure your system has sufficient free memory before enabling this option.
  By default, OVITO loads only a single frame at a time to minimize memory usage.

File sequence -- Search pattern
"""""""""""""""""""""""""""""""

This section of the UI allows you to specify a filename pattern to load multiple files as a continuous trajectory.
The pattern must contain a single ``*`` wildcard, which matches any sequence of digits in filenames.
All matching files are combined into a single trajectory, displaying simulation frames sequentially.

When importing a file with a numeric name, OVITO automatically generates a default search pattern.
To disable this behavior, uncheck :guilabel:`auto-generate`.

See :ref:`this section <usage.import.sequence>` for details on importing simulation trajectories.

Trajectory -- Playback ratio
""""""""""""""""""""""""""""

When importing a trajectory with *N* frames, OVITO synchronizes the animation timeline with the number of
imported frames, creating a :ref:`1:1 mapping between trajectory and animation frames <usage.animation.frames>`.

To adjust this playback frame ratio, click :guilabel:`Change` to open the **Configure Trajectory Playback** dialog.

.. _scene_objects.file_source.configure_playback:
.. _scene_objects.file_source.configure_trajectory_playback:

Configure trajectory playback
"""""""""""""""""""""""""""""

.. image:: /images/scene_objects/configure_trajectory_playback.*
  :width: 40%
  :align: right

This dialog lets you modify how simulation trajectory snapshots map to OVITO's animation timeline. You can change the default 1:1 mapping to:

- **1:N mapping** -- Each trajectory frame is duplicated *N* times to stretch the animation.
- **N:1 mapping** -- Only every *N*-th trajectory frame is rendered, reducing the total number of frames.
- **Extract a static frame** -- Show only one particular snapshot from the trajectory.

A **1:N mapping** is useful for extending coarse trajectories and rendering longer animations.
The :ref:`particles.modifiers.smooth_trajectory` modifier can interpolate missing frames to create smooth particle motion.

An **N:1 mapping** is useful when working with extremely long trajectories, reducing the number of rendered frames for a shorter video.

The **Extract a static frame** option allows you to isolate a single frame from the trajectory, which is useful for
rendering camera animations with a static model (see tutorial :ref:`tutorials.turntable_animation`).

For more details, see :ref:`usage.animation`.

.. seealso::

  * :py:class:`ovito.pipeline.FileSource` (Python API)
  * :py:func:`ovito.io.import_file` (Python API)
