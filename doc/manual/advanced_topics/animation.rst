.. _usage.animation:

Animating the camera and other parameters
=========================================

OVITO offers powerful animation capabilities that allow you to animate the virtual camera, modifiers, and visual parameters.
This section explains the animation system in detail.

.. image:: /images/usage/animation/file_source_animation_panel.*
  :width: 45%
  :align: right

.. _usage.animation.frames:

Animation frames vs. trajectory frames
--------------------------------------

When you load a simulation trajectory into OVITO, the timeline below the viewport indicates the animation length.
Animation frames in OVITO start at 0, meaning an animation ending at frame 100 consists of 101 frames.

There is a key distinction between *trajectory frames* (snapshots from a simulation) and *animation frames* (units of time in OVITO's timeline).
By default, OVITO maps trajectory frames one-to-one to animation frames and the length of the animation timeline matches the number of imported trajectory frames.
However, you can override this in the :ref:`Animation settings <animation.animation_settings_dialog>` dialog and set the animation length manually.
Extending the animation interval beyond the available trajectory frames results in additional static frames at the end of the rendered video.

Further control over how trajectory frames map to animation frames is provided in the :ref:`Configure trajectory playback <scene_objects.file_source.configure_playback>` dialog,
which is shown in the screenshot on the right. This dialog is accessible from the :ref:`External file <scene_objects.file_source>` panel (pipeline data source).

- The :guilabel:`Playback rate` setting allows you to adjust the frame mapping. For instance:

  - A rate of **1/2** stretches the trajectory over twice as many animation frames, making each simulation snapshot persist for two consecutive frames.
  - A rate of **2/1** compresses the trajectory, skipping every other simulation frame in the animation.

- The :guilabel:`Starting at animation frame` setting determines when the simulation trajectory playback begins in OVITO's timeline.
  By default, it starts at frame 0, but you can change this to insert static frames at the beginning.

OVITO Pro also supports inserting :ref:`multiple simulation trajectories into a single scene <usage.import.multiple_datasets>` to display them side by side or layered.
By default, the timeline automatically adjusts to accommodate all loaded trajectories (respecting their individual playback rates and starting times).

.. seealso:: :ref:`scene_objects.file_source.configure_playback`

.. image:: /images/animation_settings_dialog/animation_settings_dialog.*
  :width: 30%
  :align: right

Playback speed
--------------

In the :ref:`Animation settings dialog <animation.animation_settings_dialog>`, you can set the playback speed (frames per second)
for the animation. This value determines the frame rate of exported video files (e.g., AVI, MPEG). It also affects viewport playback,
though real-time performance may be limited by how quickly OVITO can load simulation snapshots from disk and process them.

Using time-dependent formulas
-----------------------------

Basic animation effects can be achieved using the :ref:`particles.modifiers.expression_select` and :ref:`particles.modifiers.compute_property` modifiers.
These allow you to use mathematical formulas or Boolean expressions to manipulate particle properties (e.g., position, color, transparency)
and create dynamic selection sets.

You can incorporate the special variable ``Frame`` in expressions, which represents the current animation frame number.
Using this variable makes expressions time-dependent, meaning OVITO will recompute them for each animation frame.

.. _usage.animation.keyframes:

Keyframe-based animations
-------------------------

OVITO supports keyframe-based animations, where you define values for animatable parameters at specific frames
(e.g., at the start and end of an animation). The software interpolates between these keyframes,
typically using linear interpolation.

.. image:: /images/usage/animation/slice_modifier_animatable_parameter.*
  :width: 25%
  :align: right

The screenshot on the right shows the parameter panel for the :ref:`particles.modifiers.slice` modifier.
Animatable parameters are marked with an :guilabel:`A` button next to the input field.
Clicking this button opens the animation key dialog, allowing you to define time-value pairs (animation keys).

.. image:: /images/usage/animation/keyframe_dialog.*
  :width: 25%
  :align: right

For example, in the screenshot, two keyframes are set for the *Distance* parameter of the slicing plane:

  - Frame **0** --> value **20.0**
  - Frame **50** --> value **80.0**

This setup ensures a smooth transition from 20.0 to 80.0 over the animation interval.
Every animatable parameter has at least one keyframe. With a single keyframe, the parameter remains constant.
Adding a second keyframe enables animation through interpolation.

.. _usage.animation.auto_key_mode:

Auto-key mode
-------------

.. image:: /images/usage/animation/key_mode_button.*
  :width: 40%
  :align: right

The animation toolbar next to the time slider features a button that activates *Auto-key mode*.

When enabled, the time slider background turns red. Any change you make to an animatable parameter while this mode is active automatically
generates a keyframe at the current animation time.

Example workflow:

  1. Activate *Auto-key mode*.
  2. Move the time slider to frame **0** and set the *Distance* parameter of the :ref:`particles.modifiers.slice` modifier to **20**.
  3. Move the time slider to the final frame of the animation and set the parameter to **80**.
  4. OVITO automatically creates keyframes at these points of the timeline.

Remember to turn off *Auto-key mode* again once you've finished creating the animation keys to prevent unintended changes.
The *Auto-key mode* is a convenient alternative for creating new animation keys,
which can be quicker than using the animation key dialog introduced in the previous section.

.. _usage.animation.track_bar:

The animation track bar
-----------------------

The *track bar* beneath the time slider displays keyframes for the selected pipeline and all its animated parameters:

.. image:: /images/usage/animation/track_bar.*
  :width: 55%

Each keyframe appears as a small marker on the timeline. Hovering over a marker reveals keyframe details. You can:

- Drag keyframes to reposition them.
- Right-click to open a context menu and delete them.

The track bar shows markers only for animated parameters (i.e., those with at least two keyframes).
Parameters with a single keyframe remain constant and do not appear in the track bar.

.. _usage.animation.camera:

Animating the camera
--------------------

.. image:: /images/usage/animation/create_camera_function.*
  :width: 35%
  :align: right

To animate the camera, first create a *camera object* from a viewport's :ref:`context menu <usage.viewports.menu>` (see screenshot).
The camera object is placed in the 3D scene at the current viewpoint and is linked to the viewport.

.. image:: /images/usage/animation/viewports_with_camera.*
  :width: 35%
  :align: right

The camera object is visible in the other three viewports. Zoom out if necessary.
Select the camera by clicking on it in a viewport or using the object selector box in the top-right corner of the main window:

.. image:: /images/usage/animation/object_selection_box.*
  :width: 30%

Use the :guilabel:`Move` and :guilabel:`Rotate` tools in the main toolbar to adjust the camera position and orientation:

.. image:: /images/usage/animation/move_and_rotate_tool.*
  :width: 25%

While using these tools, you can:

  - Drag the camera object with the mouse.
  - Enter precise position and rotation values in the numeric input fields located in the status bar:

.. image:: /images/usage/animation/numeric_move_tool.*
  :width: 35%

Like other scene objects, the camera's position and orientation can be animated using keyframes. To do this:

  1. Enable *Auto-key mode*.
  2. Move the time slider to a different animation time and adjust the camera's position.
  3. OVITO automatically creates keyframes, animating the camera along an interpolated path.

The :ref:`track bar <usage.animation.track_bar>` displays camera animation keys when the camera is selected.

Instead of moving the camera, you can also animate the simulation model (e.g., by rotating it).
This is done similarly: Select the simulation model and use the :guilabel:`Rotate` tool while *Auto-key mode* is active.
See the tutorial :ref:`tutorials.turntable_animation` for more information.
