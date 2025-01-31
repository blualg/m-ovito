.. _credits.ovito_pro:

=========
OVITO Pro
=========

There exist two variants of our desktop application software: **OVITO Basic** and **OVITO Pro**.
The two versions differ in terms of available program features and licensing conditions.
This user manual covers both editions, and the program features exclusively available in the *Pro* edition have been
marked with the following tag found throughout the manual: |ovito-pro|

Exclusive features
==================

*OVITO Pro* provides additional features and capabilities beyond those available in the *Basic* edition:

.. image:: /images/team/ovito_logo_128.*
   :width: 15%
   :align: right

- :ref:`Multiple pipelines <usage.import.multiple_datasets>` in the same visualization scene (comparative analysis)
- Instant :ref:`Python code generation <python_code_generation>` to greatly simplify script development for the OVITO Python package
- :ref:`User-defined modifier functions <particles.modifiers.python_script>` including GUI controls for user-defined parameters
- :ref:`User-defined viewport layers <writing_custom_viewport_overlays>`
- :ref:`User-defined file readers <writing_custom_file_readers>`
- :ref:`User-defined file writers <writing_custom_file_writers>`
- :ref:`User-defined utility applets <writing_custom_utilities>`
- LAMMPS integration via :ref:`data_source.lammps_script` pipeline source
- :ref:`data_source.python_script` pipeline source to generate particle models via scripts
- :ref:`OpenSSH client integration <usage.import.remote.openssh_connection_method>` for remote file access (support for smartcards and 2FA authentication methods)
- High-quality rendering engines:

  - :ref:`Tachyon <rendering.tachyon_renderer>`
  - :ref:`OSPRay <rendering.ospray_renderer>`
  - :ref:`VisRTX <rendering.visrtx_renderer>`

- Use :ref:`NVIDIA VisRTX <rendering.visrtx_renderer>` as :ref:`real-time render in the interactive viewports <viewports.configure_graphics_dialog>`
- :ref:`Render trajectory videos on remote machines <usage.remote_rendering>`
- :ref:`Render multi-viewport layouts <viewport_layouts.rendering>`
- Additional modifier functions:

  - :ref:`particles.modifiers.bin_and_reduce`
  - :ref:`particles.modifiers.bond_analysis`
  - :ref:`particles.modifiers.construct_surface_mesh.regions`
  - :ref:`particles.modifiers.time_averaging`
  - :ref:`particles.modifiers.time_series`
  - :ref:`modifiers.identify_fcc_planar_faults`
  - :ref:`modifiers.render_lammps_regions`
  - :ref:`modifiers.calculate_local_entropy`
  - :ref:`particles.modifiers.color_by_type`
  - :ref:`Identification of dislocation core atoms <particles.modifiers.dislocation_analysis.mark_core_atoms>`
  - Miller index based :ref:`particles.modifiers.slice` modifier

- :ref:`file_formats.input.ase_database` and :ref:`file_formats.input.ase_trajectory`
- :ref:`file_formats.output.ase_trajectory`
- :ref:`file_formats.output.gltf`

Visit https://www.ovito.org/sales/ for pricing information.

.. _license-management:

License management |ovito-pro|
==============================

OVITO Pro requires activation after installation for the first time or when using it on a new computer.
This one-time activation step verifies the license key and user entitlement via OVITO's central
license server.

.. _credits.ovito_pro.activation:

Activating an OVITO Pro installation
------------------------------------

When you first launch OVITO Pro, the **License Activation Dialog** appears.

.. image:: /images/licensing/license_activation_dialog_1.*
   :align: right
   :width: 50%

* Activation is mandatory to use the software. If you cancel, OVITO Pro will close, and the dialog will reappear upon restart.
* An active internet connection is required during the activation process to validate your license key and register the installation
  -- unless you have purchased an *offline license* for air-gapped computers.

Enter your OVITO Pro **license key**, which you received during the purchase process or from your :ref:`group license administrator <credits.ovito_pro.group_license>`.
If you are the owner of the license, you can retrieve your key at https://www.ovito.org/account/purchases/.

Next, enter your OVITO **account name**, i.e., your registered email address.

.. tip::

  Team members using a group license should enter
  their *personal* OVITO account, not the license owner's account if possible. This will allow each team member
  to manage their own installation(s) and automatically grant them access to the OVITO Pro support forum.
  If you don't have an account yet, you can create one at https://www.ovito.org/register/.

Click :guilabel:`Continue` to complete the activation. OVITO Pro will verify the license key and register your installation.
If invalid information was entered, an error message will prompt you to correct it. Once activation succeeds, you can start using the software.

.. _credits.ovito_pro.verification:

Periodic online license verification
------------------------------------

OVITO Pro periodically verifies your license by connecting to the central license server,
typically once per day. This quick process requires an internet connection but no user action.

If you lose internet access, OVITO Pro can continue operating offline for up to seven days.
After this grace period, the software will be disabled until it successfully reconnects to the license server.

.. _credits.ovito_pro.deactivation:

Deactivating an OVITO Pro installation
--------------------------------------

.. image:: /images/licensing/deactivate_installation_screenshot.*
   :align: right
   :width: 60%

Your OVITO Pro license allows installation on a limited number of computers at the same time ("license seats").
The OVITO license server tracks all activations, and once the limit is reached, new activations will be blocked
until an existing installation is deactivated.

To install OVITO Pro on a different computer (e.g., after hardware replacement, employee turnover, or OS reinstallation),
you must first deactivate an existing installation that is no longer needed. Unlike the activation process, this is
done online on the ovito.org website:

1. Visit https://www.ovito.org/account/myinstallations/ and log in with the OVITO user account that was used for activation.
2. View the list of active installations currently associated with your account.
3. Click :guilabel:`Deactivate installation` to remove one. The software on that machine will be permanently disabled (within 24 hours).

This process immediately frees up a license seat, allowing activation on a new computer.

.. _credits.ovito_pro.group_license:

Group license management
------------------------

A group license allows multiple team members to activate and deactivate OVITO Pro independently.
Each group member must have a personal OVITO account, which can be created at https://www.ovito.org/register/.

**Activation:**

The license purchaser acts as the *administrator* and can find the group license key in their online account at https://www.ovito.org/account/purchases/.
The administrator is responsible for ensuring the license key is only shared with authorized team members.

.. important::

  The license key should remain within the authorized group. If you suspect misuse, contact customer support to request a key replacement.
  If administrator privileges need to be transferred to a different person, contact support as well.

Each user of the group license independently activates OVITO Pro on their computer by entering the license key and
their *personal* OVITO account name :ref:`as described above <credits.ovito_pro.activation>`.
This allows them to manage their own installation(s) without affecting others.

During the activation process, the license server will link the group member's account with the shared group license key.
This gives them access to the OVITO Pro support forum and allows them to deactivate the installation again later.

**Deactivation:**

To move an installation to a different computer, group members can log in at
https://www.ovito.org/account/myinstallations/ and deactivate their old installation -- no action from the administrator is required.
This self-service process is detailed :ref:`above <credits.ovito_pro.deactivation>`.

The administrator can view the current list of all active installations at https://www.ovito.org/account/purchases/.
If necessary, the administrator can deactivate any installation to free up a license seat. This will disable the software on
that computer within 24 hours.

.. Debugging license validation problems
.. -------------------------------------

.. If any problems occur during online license activation or validation, you can
.. have OVITO Pro print verbose logging messages to the console by setting the environment variable
.. ``OVITO_LICENSING_VERBOSE=1`` before invoking OVITO Pro from a terminal.
.. In situations where you need to contact our customer support, this information can also help us to diagnose the problem.

.. During the activation process, the *Machine ID* and the *User ID*, displayed
.. at the bottom of the dialog, will be transmitted to our licensing server. They are one-way hash values generated by OVITO Pro
.. to uniquely identify your local computer and your operating system account. To prevent unauthorized use
.. of the software, your activated installation will be tied to these identifiers.

.. If the activation was successful, you can close the dialog and start using OVITO Pro. A software entitlement record,
.. issued by our licensing server and digitally signed, is now stored in your computer's home directory
.. unlocking the software.
