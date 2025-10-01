.. _usage.import.remote:

Remote file access (SSH)
========================

OVITO includes built-in SSH and HTTP(S) clients, allowing direct access to data files stored on remote machines.
This feature eliminates the need to manually transfer files, such as those on an HPC cluster, to your local desktop.

To open a remote file, select :menuselection:`File --> Load Remote File` from the menu.

Currently, OVITO does not support browsing directories on remote machines.
Instead, you must provide the full file path using a URL in the following format::

  sftp://user@hostname/path/filename

Replace:

- ``user`` with your SSH username.
- ``hostname`` with the remote machine's hostname.
- ``/path/filename`` with the full file path.

Note that the "~" shortcut for a home directory is *not* supported; use an absolute path instead.

You can also download files from a web location using a URL like::

  https://www.website.org/path/filename

When connecting via SSH, OVITO will prompt for your password or private key passphrase.
Once established, the SSH connection remains active until the program session ends.
OVITO temporarily stores downloaded trajectory files locally to speed up access to subsequent frames.
These cached files persist until you close OVITO or click :guilabel:`Reload` in the :ref:`External File <scene_objects.file_source>` panel.

.. note::

  If present, OVITO's SSH client reads the :file:`~/.ssh/config` `configuration file <https://www.ssh.com/ssh/config>`_ to apply connection settings.

.. _usage.import.remote.openssh_connection_method:

OpenSSH client |ovito-pro|
--------------------------

.. image:: /images/io/remote_file_import_dialog.png
  :width: 50%
  :align: right

OVITO's integrated standard SSH client is based on `Libssh <https://www.libssh.org>`__, a separate implementation of
the SSH protocol distinct from the commonly used OpenSSH command-line tools (:program:`ssh` and :program:`scp`), which are present on most systems.

However, Libssh does not support all authentication methods available in OpenSSH. In particular, it does *not* work with
smartcards or two-factor authentication systems requiring PKCS#11 extensions.

For these cases, OVITO Pro offers an alternative connection method that uses the external :program:`sftp` tool
from `OpenSSH <https://www.openssh.com>`__. This method ensures full compatibility with authentication methods
and settings configured in :file:`~/.ssh/config`.

You may need to specify its location on your computer if the :program:`sftp` tool is not in the system's `PATH`.

.. note::

  Requires OpenSSH version 8.4 or later.

.. _usage.import.remote.troubleshooting:

Troubleshooting connection problems
-----------------------------------

SSH connections may fail if the client and server do not share a common authentication or encryption method.
OVITO's built-in SSH client, based on the :program:`libssh` library, supports a specific set of key exchange methods,
public key algorithms, ciphers, and authentication mechanisms (`see details <https://www.libssh.org/features/>`__).
The client and server must agree on at least one method from each category to establish a connection.

The available SSH methods depend on the remote server's configuration. If unsure, consult your system administrator or server documentation.

Diagnosing connection issues
""""""""""""""""""""""""""""

To troubleshoot, set the environment variable ``OVITO_SSH_LOG=1`` before running OVITO.
This enables detailed SSH connection logs in the terminal. If the connection or the client-server handshake fail,
these logs can help identify the issue.

On platforms where terminal output is not visible (e.g., Windows), redirect logs to a file by setting the
``OVITO_LOG_FILE`` environment variable. For example, in Windows Command Prompt (:program:`cmd.exe`):

.. code-block:: winbatch

  set OVITO_LOG_FILE=%USERPROFILE%\ovito_log.txt
  set OVITO_SSH_LOG=1
  "C:\Program Files\OVITO Basic\ovito.exe"

Then try loading a remote file via SSH in OVITO. If the connection fails, check :file:`ovito_log.txt` in your user directory for error details.

For further assistance, visit the `online user forum <https://matsci.org/c/ovito/>`__ or contact `OVITO technical support <https://www.ovito.org/contact/>`__.
