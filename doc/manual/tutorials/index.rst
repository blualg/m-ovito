.. _tutorials:

=========
Tutorials
=========

.. toctree::
  :maxdepth: 1
  :hidden:

  marker_particles
  turntable_animation
  remote_rendering

.. rubric:: :ref:`Motion visualization with marker particles <tutorials.marker_particles>`

.. image:: /images/howtos/shear_marker.gif
   :width: 20%
   :align: right

Learn how to highlight a group of atoms, initially located in a narrow region, with a marker color to visualize the atomic motion
in the interior of the crystal during the course of the simulation. In particular, this tutorial will introduce you to the
:ref:`particles.modifiers.freeze_property` modifier in OVITO, which helps preserve the initial selection state of a group of particles.

.. rubric:: :ref:`Turntable animation of a model <tutorials.turntable_animation>`

.. image:: /images/tutorials/turntable_animation/turntable.gif
   :width: 20%
   :align: right

This tutorial teaches you how to create an animated movie of a simulation snapshot,
showing the model from all sides by slowly rotating it.
You will learn about OVITO's keyframe-based :ref:`parameter animation system <usage.animation>`
and different ways of repositioning a model in the three-dimensional scene.

.. rubric:: :ref:`Remote rendering tutorial <tutorials.remote_rendering>`

This tutorial walks you through the process of rendering a simulation video on a high-performance computing cluster using
the :ref:`Render on Remote Computer <usage.remote_rendering>` function of *OVITO Pro*.

.. - Identify local chemical ordering (PTM modifier)
.. - How to use the DXA modifier to analyze dislocations
.. - Analyze a bulk metalic glass simulation
.. - Visualize a LAMMPS simulations with separate topology/trajectory/bond files
.. - Creating good-looking renderings of a simulation model (OSPRay)
.. - Visualize particle resident time distribution (spatial binning, time averaging)
.. - Calculate diffusion constant (Python script)
.. - Python script modifier: Warren-Cowley-SRO