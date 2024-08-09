.. _modifier_templates:

Modifier templates
==================

.. image:: /images/app_settings/modifier_templates.*
  :width: 45%
  :align: right

Modifier templates are a way for you to define modifiers with pre-configured settings for quick access.
This is useful in situations where you need to apply the same modifier(s) with the exact same settings
to different datasets repeatedly. The modifier templates you define are preserved across program sessions
and can even be transferred between computers. Furthermore, a modifier template can consist of several modifiers, allowing
you to insert often-used sequences or combinations of modifiers into the data pipeline with a single click.

Modifier templates are managed in the :ref:`application settings dialog <application_settings>`.
Note that the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>` contains a button to quickly open that dialog. All modifier templates
you define are available application-wide, and OVITO will remember them across program sessions.

.. image:: /images/app_settings/modifier_templates_shortcut.png
  :width: 30%
  :align: right

New templates can be created from some existing modifier(s) in the active data pipeline.
That means you basically "save" one or more modifiers, including their current settings, to make them available
for future use. Once created, modifier templates appear in the drop-down list of available modifiers, after the built-in modifiers.
From there they can be quickly inserted into another pipeline. This feature is useful if you need to use the same modifier(s)
with the exact same settings over and over again.

Another typical use case for modifier templates are user-defined Python script modifiers. The
:ref:`Python script <particles.modifiers.python_script>` modifier type of OVITO Pro lets you write a script function performing some user-defined
data operations. After developing a new script function, you can save it as a modifier template to make it available
in future program sessions, just like the standard modifiers of OVITO. Since the Python source code will be part of the
stored template, it relieves you from saving the code in some other place such as an external .py file.

Transferring templates to a different computer
""""""""""""""""""""""""""""""""""""""""""""""

Note that OVITO stores the definition of all modifier templates in the application's settings file in a proprietary format.
If needed, they can be exported to a special modifier template file (:file:`*.ovmod`) to transfer them to another computer.
On the target computer, your can import them into another OVITO installation.
