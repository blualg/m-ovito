.. _animation.animation_settings_dialog:

Animation settings
------------------

.. image:: /images/animation_settings_dialog/animation_settings_dialog.*
  :width: 35%
  :align: right

The *Animation Settings* dialog can be opened from the animation toolbar:

.. image:: /images/animation_settings_dialog/animation_settings_button.*
  :width: 25%

This dialog controls animation parameters such as playback frame rate and animation length.
For a complete overview of OVITO's animation system, see :ref:`usage.animation`.

Settings
""""""""

Frames per second
  Sets the animation frame rate (FPS). This affects both movie file export and viewport playback.
  Note, however, that realtime rendering in the viewport may not be able to keep up with high frame rates.

Playback speed
  Multiplies the set FPS to control viewport playback speed.
  Actual speed may be slower if frame loading, computing, or rendering take significant time.

Every Nth frame
  Skips frames during viewport playback to improve performance, useful for long trajectories.

Loop playback
  Enables or disables continuous looping in the viewport.
  When off, playback stops at the animation's end.

Timeline display
  Chooses how timeline labels are shown: frame numbers (0, 1, 2, …) or simulation timesteps / simulated time from loaded trajectories.

Custom animation interval
  Overrides the natural animation length in the timeline.
  Normally, OVITO matches this length to the loaded simulation trajectory sequence.
  Use this option for static datasets to create camera animations or other time-based effects,
  see :ref:`usage.animation` and :ref:`tutorials.turntable_animation`.
