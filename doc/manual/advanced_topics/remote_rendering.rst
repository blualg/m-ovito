.. _usage.remote_rendering:

Offloading animation rendering to remote machines |ovito-pro|
=============================================================

.. image:: /images/rendering/remote_render_dialog.*
  :width: 50%
  :align: right

.. versionadded:: 3.10.0

Rendering locally can be time-consuming, and configuring a scene on a headless
remote computer cluster can be a complex and error-prone task. The
:menuselection:`Render on Remote Computer` function in OVITO Pro allows you to
configure a scene locally and effortlessly render it on a remote machine.
Make sure to check out the :ref:`Remote rendering tutorial <tutorials.remote_rendering>`
to get an in-depth example on how to use this feature.

The :menuselection:`Render on Remote Computer` function instructs OVITO Pro to prepare a bundle
directory that can be sent to a different computer, such as a computer cluster,
enabling rendering on parallel scale. To achieve this, OVITO Pro either remaps
all file paths used in the current scene to remote paths, pointing to the same
files on the remote machine, or bundles local data into the directory.
Additionally, it generates an OVITO state file that contains the current scene
and various supporting files. This bundle can be transferred to the remote
machine and run directly to obtain both - rendered images and a video of the
scene.

Setting up the remote render in your local OVITO Pro instance
-------------------------------------------------------------

To prepare remote rendering follow these steps from your local OVITO Pro
instance:

#. Configure your viewports and :ref:`render settings <core.render_settings>` as
   you would for rendering an image or video on your local computer. Adjust
   settings such as resolution, animation range, and render engine as needed.
   Notably, you do not need to specify the :guilabel:`render output` file path,
   as this will be handled automatically.

#. Click on :menuselection:`File --> Render on Remote Computer...` to
   open the :menuselection:`Remote Render Settings` dialog window. In this dialog, you will
   find a table listing all the file paths used in the current OVITO scene
   (*Current path* in the left column). Paths starting with ``sftp://`` are loaded
   from a remote server, other parths point to local files. For each *Current path*,
   you can either enter a new *Remote target path* or check the *Bundle data*
   checkbox.

   - Remapping paths from a *current* to *remote target path* is useful when
     the data currently used in the OVITO scene is also available on
     the remote computer. For example, if the structures displayed in OVITO are
     loaded from ``/User/daniel/data`` and stored on the remote server under
     ``/scratch/daniel/simulation_data``, you can remap these paths accordingly.

   - Alternatively, you can toggle *Bundle data*. In this case, OVITO Pro will
     collect all data used in the scene form this source directory and copy it
     into the bundle directory. In this case, *Remote target path* will be
     managed automatically. When you decide to pack remote (``sftp://``) files,
     they will be downloaded once you click :guilabel:`Export`. This might take a
     substantial amount of time, depending on file sizes and network speed.

#. Additionally, you can configure the number of cores per render task (i.e. per
   worker) using the *Cores per task* option. A value of 0 will utilize all
   available cores on a node. For instance, on a compute node containing 96
   cores, setting *Cores per task* to 8 means that each node will spawn 12
   workers, rendering 12 images concurrently. A *Cores per task* of 0 on the
   other hand will spawn 1 worker running on 96 cores.

#. Finally, use the :guilabel:`Choose...` button to open the file explorer and select a
   directory for saving the bundle. Please note that this directory must be
   empty; otherwise, an error message will be displayed.

#. If you click :guilabel:`Close` all your settings in the :menuselection:`Remote Render Settings`
   dialog will be saved and the window is closed. This can be
   used to save your configuration into the OVITO state file. Clicking
   :guilabel:`Export` will start the bundling process.

Transferring the bundle directory
---------------------------------

Currently, OVITO Pro does not have the capability to transfer the bundle
directory to your remote computer cluster directly. To proceed, you must
manually transfer the bundle to your remote machine using a tool such as
`scp <https://linux.die.net/man/1/scp>`__,
`FileZilla <https://filezilla-project.org/>`__,
`Cyberduck <https://cyberduck.io/>`__ ,
`WinSCP <https://winscp.net/eng/index.php>`__,
or similar utilities.

Running the render on a remote computer cluster
-----------------------------------------------

To start the rendering task on a remote computer cluster, follow these steps:

#. We recommend copying the bundle to a fast scratch file system that is also
   used for other computations. Please refer to your computing center's
   documentation for additional information.

#. Change into the bundle directory.

#. Install or Update the Conda Environment:

   - If you do not already have a conda environment named
     *remote_render_ovito*, you can set it up by running the command:

     .. code-block:: bash
      
         conda env create -f remote_render_ovito.yml

   - If you already have the environment, it is recommended to update it based
     on the ``remote_render_ovito.yml`` file to ensure that all required packages
     are installed. You can do this using the following two commands:
     
     .. code-block:: bash

         conda activate remote_render_ovito
         conda env update -f remote_render_ovito.yml --prune

#. Open the ``submit.sh.template`` file.Assuming you use the 
   `slurm <https://slurm.schedmd.com/quickstart.html>`__ queuing
   manager, this file already contains commands to activate the conda
   environment, set required environment variables, and run the rendering task
   (``srun ...``). However, you must add the header containing all the ``#SBATCH``
   settings required in your HPC environment. Additionally, you might need to
   include ``module load ...`` directives before the
   ``conda activate remote_render_ovito`` command to ensure Python is configured correctly.

#. Once you have filled in ``submit.sh.template``, rename the file to ``submit.sh``
   and submit it to your cluster queuing system using a command such as:
   ``sbatch submit.sh``

#. After the rendering job is complete, you can find all rendered images in the
   ``frames`` subdirectory. Moreover, a video file named ``video.mp4`` will be in
   the main bundle directory.

Additional notes and troubleshooting
------------------------------------

While the instructions provided above are designed to work in many remote
cluster environments, they may not be a perfect fit for every situation. Here
are some common questions and answers to help you address potential issues:

**Which remote operating systems are supported?**
    Remote rendering is currently only supported on Linux remote computers. Linux, Mac,
    and Windows are supported as local operating systems for preparing the bundle.

**What should I do on computer clusters without internet access?**
    If your computer cluster lacks internet access, you can consider using
    `conda-pack <https://conda.github.io/conda-pack/>`__, to relocate your local conda
    environment to your remote machine. This approach is untested and success is not
    guaranteed.

**Which queuing systems are supported?**
    The provided `submit.sh.template` file is tailored for use with 
    `slurm <https://slurm.schedmd.com/quickstart.html>`__. 
    If you are using a `flux <https://flux-framework.org/>`__-based
    queuing system, you should be able to directly submit the ``flux run ...`` command from the
    ``submit.sh.template`` in your flux environment.For all other queuing systems the
    ``submit.sh.template`` file needs to adapted manually.

**Are other conda-like package managers supported?**
    Yes, the rendering process has also been tested with micromamba and should
    work with microconda or anaconda as well. Please note that if you choose to
    use a conda alternative, you will need to replace all instances of the
    conda command.

**What if the render job times out?**
    Simply resubmit the render job from the bundle directory. OVITO Pro will
    automatically detect which frames have already been rendered and will
    continue from where it left off.

**How can I add missing packages to the conda environment?**
    During packaging, OVITO Pro attempts to identify all Python packages in the
    current environment. However, in rare cases where OS or package version
    mismatches occur, some packages may be missing. To resolve this, manually
    edit the ``remote_render_ovito.yml`` file and add the missing packages. If you
    encounter this issue frequently, please contact us.

**Which renderer should I use?**
    Many compute clusters do not provide the third-party libraries required for
    OpenGL rendering. Therefore, you might need to use one of the other
    rendering engines instead.

**How can I test / debug my bundle**
   If you are on a linux machine, make sure you have installed the conda environment.
   Afterwards you can run the following commands adapted from ``submit.sh.template``:

   .. code-block:: bash

      conda activate remote_render_ovito
      export CONDA_LD_LIBRARY_PATH=${CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/lib
      export LD_LIBRARY_PATH=${CONDA_LD_LIBRARY_PATH}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
      mpirun flux start remote_render_task.py

   Note, that ``srun`` had to be replaced by ``mpirun``. If you encounter any errors you can add 
   ``-v`` or ``-vv`` to the ``remote_render_task.py`` command to get additional output in the ``logs/``
   directory:

   .. code-block:: bash

      mpirun flux start remote_render_task.py -vv