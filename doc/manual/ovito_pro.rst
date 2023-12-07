.. _credits.ovito_pro:

=========
OVITO Pro
=========

There exist two variants of our desktop application software: **OVITO Basic** and **OVITO Pro**.
The two versions differ in terms of available program features and licensing conditions.
This user manual covers both editions, and the program features exclusively available in the *Pro* edition have been
marked with the following tag found throughout the manual: |ovito-pro|

A (nearly complete) list of the program features available only in *OVITO Pro* and not in *OVITO Basic*:

.. image:: /images/team/ovito_logo_128.*
   :width: 15%
   :align: right

- :ref:`Multiple pipelines <usage.import.multiple_datasets>` in the same visualization scene (comparative analysis)
- Instant :ref:`Python code generation <python_code_generation>` to greatly simplify script development for the OVITO Python package
- :ref:`User-defined modifier functions <particles.modifiers.python_script>` including GUI controls for user-defined parameters
- Option to develop :ref:`file parsers for custom file formats <writing_custom_file_readers>` in Python
- LAMMPS integration via :ref:`data_source.lammps_script` pipeline source
- :ref:`data_source.python_script` pipeline source to generate new particle structures via scripts
- :ref:`OpenSSH client integration <usage.import.remote.openssh_connection_method>` for remote file access (support for smartcards and 2FA authentication methods)
- High-quality rendering engines:

  - :ref:`Tachyon <rendering.tachyon_renderer>`
  - :ref:`OSPRay <rendering.ospray_renderer>`
  - :ref:`VisRTX <rendering.visrtx_renderer>`

- :ref:`Remote rendering function <usage.remote_rendering>`
- :ref:`Multi-viewport layout rendering <viewport_layouts.rendering>`
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

- :ref:`file_formats.input.ase_database` and :ref:`file_formats.input.ase_trajectory`
- :ref:`file_formats.output.gltf`
- Use of Miller indices in the :ref:`particles.modifiers.slice` modifier

Please visit `www.ovito.org <https://www.ovito.org/about/ovito-pro/>`__ for further details on OVITO Pro, our support services, and pricing.
By licensing OVITO Pro you support the development and maintenance of both editions our desktop software and the :ref:`OVITO Python module <scripting_manual>`.

.. _license-management:

License management |ovito-pro|
==============================

OVITO Pro must be activated after installation in order to use it on a new computer. During this one-time activation step,
the validity of the license key and your entitlement to use the software will be verified by our cloud-based licensing server.

Activating OVITO Pro
--------------------

.. image:: /images/licensing/license_activation_dialog_1.*
   :align: right
   :width: 50%

The first time you start up OVITO Pro after installation,
you will see the :guilabel:`License Activation Dialog`.
Please note the following:

* You must complete the activation procedure to unlock and use the software.
  If you cancel the activation process, OVITO Pro will quit, and the dialog will reappear the next time
  you start the software.

* An active internet connection is required as OVITO Pro will contact the central license server
  to check the validity of your license key and register the software installation.

Please enter the OVITO Pro *license key* you have received from us or from
the person that purchased a group license for you. If you are the license owner,
you can retrieve the license key at https://www.ovito.org/account/purchases/.

Next, enter your OVITO account name or email address under which you are registered on our website `www.ovito.org <https://www.ovito.org>`__.
Note that if you are a team member using an OVITO Pro group license, you should enter your *personal* OVITO account
here, not the account name of the license owner.

Click :guilabel:`Continue` to perform the activation. OVITO Pro will contact the licensing server to
validate the entered license key and register your software installation. If either the entered license key or
account name is invalid, OVITO Pro will display an error message and let you correct your input.
If the activation was successful, you can start using the program.

Online license validation
-------------------------

From time to time, OVITO Pro needs to verify your license status by contacting the central license server.
The validation occurs sporadically (typically once a week) during program startup and
requires a working internet connection -- but no user interaction.

Without internet connectivity, you can continue using OVITO Pro offline -- but only for a limited period of time.
After a grace period of 7 days without successful validation attempts, OVITO Pro will block further use of the software
until the next time the license status can be successfully validated again by connecting to the central OVITO server.

.. _credits.ovito_pro.deactivation:

Deactivating an installation
----------------------------

.. image:: /images/licensing/deactivate_installation_screenshot.*
   :align: right
   :width: 60%

Your OVITO Pro license allows you to install the software only on a limited number of computers simultaneously.
This limit is enforced by the OVITO license server, which keeps track of all program installations.
Once the maximum allowed number of installations is exhausted, the license server will reject attempts to activate OVITO Pro on further machines.

Thus, in order to install OVITO Pro on a different machine, for instance, after a hardware replacement, employee turnover, or new installation
of the operating system, you must first deactivate one of the existing OVITO Pro installations which are no longer needed.
This is done online by visiting the URL https://www.ovito.org/account/myinstallations/ and logging in with
your personal OVITO account. The page lists all active OVITO Pro installations currently associated with your
account. Click :guilabel:`Deactivate installation` to remove an installation from our records,
which will also permanently disable the software on that machine after a synchronization period (up to 24 hours).

This deactivation step decrements the usage counter of the license, and you will subsequently be able to activate OVITO Pro
on a new workstation.

.. _credits.ovito_pro.group_license:

Managing a group license
------------------------

A group license key can be used by multiple team members. Each team member should create their own OVITO account
by going to https://www.ovito.org/register/.

The person who purchased the group license is the designated administrator and owner of the license key.
The owner can retrieve the key on the OVITO website by reviewing the `history of purchases <https://www.ovito.org/account/purchases/>`__
and signing in with their account name. The owner may share the license key with
all team members who use it to install OVITO Pro. However, it is the owner's legal responsibility to prevent unauthorized use and ensure that the secret license key
never leaves the group.

The team members should independently activate their OVITO Pro installation(s) by entering the license key
and their *own personal* OVITO account name. Only then will each team member be able to
independently manage their own OVITO Pro installation(s) (but not those of other members) on the OVITO website. In case a team member needs
to move their installation to a different computer, they can sign in with their personal account and deactivate the old installation
at https://www.ovito.org/account/myinstallations/. No action by the license administrator is needed.

The OVITO license server keeps track of all installations performed using the group license key and makes sure that
the aggregate number of installations of all team members does not exceed the limit permitted by the license.
The group license administrator can access the list of active installations (including the names of the corresponding team members)
by going to https://www.ovito.org/account/purchases/.
The license administrator has the power to deactivate any of the installations, for example, when a team member leaves the
organization and is no longer eligible for using OVITO Pro under the group license.

As a group license owner, if you notice that an unauthorized person continues to use the license key to activate
new OVITO Pro installations without your permission, a former team member for example,
please contact customer support. The old license key can be replaced with a new secret key.

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
