.. _modifiers.identify_fcc_planar_faults:

Identify fcc planar faults |ovito-pro|
--------------------------------------

.. image:: /images/modifiers/identify_fcc_planar_defects_panel.jpg
  :width: 25%
  :align: right

.. versionadded:: 3.8.0
    
This :ref:`Python-based modifier <particles.modifiers.python_script>` identifies different planar defect types in face-centered cubic (fcc) crystals, 
e.g. *stacking faults* and *coherent twin boundaries*. These planar defects have in common that they are made of hcp-like atoms arranged on parallel {111} planes of the fcc crystal.
The following pictures show the atomic stacking sequences that correspond to the different planar defect types recognized by the 
algorithm:

.. image:: /images/modifiers/identify_fcc_planar_faults_types.jpg
  :width: 70%

The identification algorithm relies on intermediate results provided by the :ref:`particles.modifiers.polyhedral_template_matching` modifier, which
must be inserted into the data pipeline first to identify all hcp-like defect atoms in the fcc crystal. Make sure the PTM output options 
:guilabel:`Interatomic distances` and :guilabel:`Lattice orientations` are checked as the planar defect algorithm needs this information as input.

.. image:: /images/modifiers/identify_fcc_planar_defects_ptm_settings.jpg
  :width: 25%
  :align: right

The modifier subsequently analyzes the neighborhood of all hcp-like atoms to identify which kind
of planar faults they are part of. Each atom in the input system is assigned to one of the following groups:

    * 0 = Non-hcp structure (e.g. perfect fcc)
    * 1 = Indeterminate hcp-like (isolated hcp-like atoms, not forming planar defects)
    * 2 = Intrinsic stacking fault (two adjacent hcp-like layers)
    * 3 = Coherent twin boundary (one hcp-like layer)
    * 4 = Multilayer stacking fault (three or more adjacent hcp-like layers)

The modifier writes the corresponding numeric values to the ``Planar Fault Type`` output particle property
and assigns correspondong colors to the hcp-like atoms to visualize their defect type. 

.. figure:: /images/modifiers/identify_fcc_planar_faults_example_ptm.jpg
  :figwidth: 40%

  PTM structure classification

.. figure:: /images/modifiers/identify_fcc_planar_faults_example_results.jpg
  :figwidth: 40%

  Planar defect classification

Additionally, the modifier outputs a :ref:`data table <scene_objects.data_table>` to report 
the total number of atoms in the system belonging to each planar fault type. Specifically for intrinsic stacking faults (ISFs)
and coherent twin boundaries (TBs), the modifier also calculates the aggregated areas of these defects.
They are calculated from the basal-plane projected area of each hcp-like atom that is 
part of these defects, divided by the number of atomic layers per planar defect (1 for TBs; 2 for ISFs). 

.. figure:: /images/modifiers/identify_fcc_planar_defects_table.jpg
  :figwidth: 100%

  Output data table of the *Identify fcc planar faults* modifier

The estimated areas in this table are given in units of length squared of the original simulation model.

.. seealso::

  :py:class:`ovito.modifiers.IdentifyFCCPlanarFaultsModifier` (Python API)