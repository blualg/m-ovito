.. _application_settings.python:

Python settings |ovito-pro|
===========================

.. image:: /images/app_settings/python_settings.*
  :width: 45%
  :align: right

This page of the :ref:`application settings dialog <application_settings>`
lets you configure the embedded Python interpreter of OVITO Pro.

Source code editor
""""""""""""""""""

OVITO Pro can run Python code that you write, e.g., as part of a :ref:`user-defined modifier <particles.modifiers.python_script>`.
For editing such Python source code, you can choose between a simple integrated code editor and an external editor application or Python IDE of your choice (e.g. *Visual Studio Code*),
which can provide a better coding experience.

If you have configured an external Python IDE for code editing, OVITO Pro automatically re-runs your code whenever you save the source file
in the external application and switch back to the OVITO Pro window.

.. _application_settings.python.package_installation:

Installing Python packages
""""""""""""""""""""""""""

.. image:: /images/app_settings/python_settings_install_package.*
  :width: 35%
  :align: right

OVITO Pro comes with an embedded Python interpreter, which manages its own set of Python packages that can be imported and used in your Python scripts.
The table in the screenshot above lists all Python packages currently installed in OVITO Pro's embedded interpreter.

The function :guilabel:`Install additional package` makes it easy to install additional Python packages from the PyPI repository or the conda-forge channel,
making them available within OVITO Pro. This function will run :command:`pip install` or :command:`conda install` commands in the background to download and install the selected third-party package(s).

.. note::

  If you are using the :ref:`conda version of OVITO Pro <installation.anaconda>`, the embedded Python interpreter is identical with the one used by the conda environment in which OVITO Pro is installed.
  Thus, Python packages you install in your conda environment become also available in OVITO Pro, and vice versa.

  For the *non-conda* version of OVITO Pro, the embedded Python interpreter is completely isolated from all other Python installations on your system.
  Thus, you can install additional Python packages in the embedded interpreter without affecting other Python environments on your system.
  Upgrading to a newer version of OVITO Pro will typically leave installed Python packages intact, because they are stored in a separate user site directory,
  not the OVITO Pro program directory.

Upgrading an already installed Python package to the latest version requires the same steps as installing a new package.

Uninstalling Python packages cannot be done from the GUI. Instead, you need to close OVITO Pro and run the following command in a terminal (if you are using a non-conda version of OVITO Pro):

.. code-block:: shell-session

   ovitos -m pip uninstall <package_name>

or, if you are using the conda version of OVITO Pro:

.. code-block:: shell-session

   conda remove <package_name>

The :program:`ovitos` command line tool is included in the OVITO Pro installation and can be used to execute Python commands and scripts from a system terminal
using the embedded Python interpreter. For more information, see :ref:`ovitos_interpreter`.

.. seealso:: :ref:`topics.python_extensions.gallery`