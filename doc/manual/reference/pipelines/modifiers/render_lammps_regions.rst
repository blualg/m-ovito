.. _modifiers.render_lammps_regions:

Render LAMMPS Regions |ovito-pro|
------------------------------------
.. versionadded:: 3.8.0

This is a :ref:`Python-based modifier function <particles.modifiers.python_script>` for visualizing 
geometric regions of space in analogy to the
`region <https://docs.lammps.org/region.html>`__ command of the `LAMMPS <https://docs.lammps.org/>`__ MD code.  

.. |pic1| image:: /images/modifiers/lammps_regions_osp.*
  :width: 34%

.. |pic2| image:: /images/modifiers/lammps_regions_data_inspector.*
  :width: 64%
  :align: bottom

|pic1| |pic2|

You can enter one or more LAMMPS region commands into the text editor, separated by newlines. 
For each LAMMPS region command a new region is created as part of the same OVITO surface mesh object and will appear
as new region type listed under ``Regions`` in the :guilabel:`Surfaces` tab of OVITO's :ref:`data inspector <data_inspector>`.

Parameters
""""""""""

.. image:: /images/modifiers/lammps_regions_modifier_panel.*
  :width: 50%
  :align: right

Region command(s):
  Enter one or more LAMMPS region commands here. Following the LAMMPS command `parsing rules <https://docs.lammps.org/Commands_parse.html>`__, if a command line ends with the “&” character, 
  the command is expected to continue in the next line.

  This modifier currently supports the following region styles and keywords:

  :style:
    block, cone, cylinder, ellipsoid, plane, prism, sphere 
  :keyword:
    rotate, move, side, open
    
  In OVITO all regions have to be defined in simulation **box units**, not in lattice units.  

  Note that LAMMPS equal style variables *v_...* are currently **not** supported, meaning you cannot create
  dynamic regions whose location or orientation is changing with time.
  Please replace variables with their numerical values. 

Select particles inside region
  .. image:: /images/modifiers/lammps_regions_particles_inside.*
    :width: 30%
    :align: right

  Let's you select all particles inside a region. 
  Note, that this is only possible if the created surface mesh is **closed**, i.e., its faces form a contiguous manifold.
  
  This is obviously not the case for the region style *plane* and for the styles *block*, *prism*, *cone* and *cylinder*, when combined with 
  *open* keyword.
  A good indicator for that the surface mesh is not closed is when the *Cap polygons* options in the
  Surface Mesh Visual Element option is disabled.

Rendering options
  .. figure:: /images/modifiers/lammps_regions_clip_and_cap.*
    :width: 60%
    :align: right   
   
  The domain of the LAMMPS region surface mesh is 3d non-periodic, which is why the surface mesh does not get wrapped back into the cell during rendering.
  Areas of the surface mesh that are located outside the simulation cell can be hidden by enabling the *Clip at cell boundaries* option. 
  Futhermore, the *Cap polygons* options enables the display of caps where LAMMPS regions intersect with the simulation cell's boundaries.

.. seealso::
  - :ref:`visual_elements.surface_mesh`
  