.. _application_settings.python:

Python settings |ovito-pro|
===========================

.. image:: /images/app_settings/python_settings.*
  :width: 45%
  :align: right

This page of the :ref:`application settings dialog <application_settings>`
lets you configure settings related to the embedded Python interpreter of OVITO Pro.

Code editor
"""""""""""

OVITO Pro can execute Python code that you write, e.g., as part of a :ref:`user-defined modifier <particles.modifiers.python_script>`.
You can choose between a simple integrated code editor window and an external application or Python IDE (e.g. *Visual Studio Code*) for
editing your Python code, which can provide a better coding experience.

OVITO Pro automatically re-runs your code whenever you save it in the external application and switch back to the OVITO Pro window.

Python packages
"""""""""""""""

The table lists all Python packages that are currently installed in the OVITO Pro environment and which can be imported
and used in your Python scripts that run within OVITO Pro.

Note that regular installations of OVITO Pro use :ref:`an embedded Python interpreter <ovitos_interpreter>`, which
manages its own set of Python packages independent from your system's Python interpreter. Thus, :ref:`installing additional packages from the PyPI repository <ovitos_install_modules>` requires a
special :command:`pip` command, which is displayed below the package list for your convenience. If you are working with the :ref:`Conda version of OVITO Pro <installation.anaconda>`, the program
uses Conda's Python interpreter and you can make additional Python packages available simply by installing them in the same Conda environment as OVITO Pro.
