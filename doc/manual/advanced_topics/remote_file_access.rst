.. _usage.import.remote:

Remote file access
==================

OVITO comes with built-in SSH and HTTP clients for accessing files located on remote machines. This feature can save you from having to transfer
files stored in remote locations, for example on an HPC cluster, to your local desktop computer first.
To open a file stored on a remote host, select :menuselection:`File --> Load Remote File` from the menu.

The current version of OVITO does not provide a way to browse directories on remote machines. You have to directly specify
the full path to the remote file as an URL of the form::

  sftp://user@hostname/path/filename

Replace :command:`user` with your SSH login name for your remote machine, :command:`hostname` with the hostname of the remote machine,
and :command:`/path/filename` with the full path to the simulation data file to load. Note that the use of the "~" shortcut to a user's home directory is *not* supported; you have to specify the absolute directory path.

Furthermore, you can let OVITO download data from a web server location by specifying an URL of the form::

  https://www.website.org/path/filename

When connecting to the remote machine via SSH, OVITO will ask for the login password or the passphrase for the private key to be used for authentication.
Once established, the SSH connection is kept alive until the program session ends. OVITO creates a temporary copy of the remote file on the local computer before
loading the data into memory to speed up subsequent accesses to all simulation frames. The local data copies are cached until you close OVITO or
until you hit the :guilabel:`Reload` button in the :ref:`External File <scene_objects.file_source>` panel.

.. note::

  If it exists, OVITO parses the :file:`~/.ssh/config` `configuration file <https://www.ssh.com/ssh/config>`_ in your home directory to 
  configure the SSH connection.  

.. _usage.import.remote.troubleshooting:

Troubleshooting connection problems
-----------------------------------

Establishing a connection between OVITO and the SSH server may fail if the client and server cannot agree on a common authentication and encryption method. 
OVITO's built-in SSH client is based on the :program:`libssh` library, which supports a specific set of SSH key exchange methods, 
public key algorithms, ciphers, and authentication methods (`see here <https://www.libssh.org/features/>`__). During the handshaking process,
both parties need to agree on at least one common choice from each of these categories to successfully establish an SSH connection. 

.. note::

  The :program:`libssh` library is not identical to the `OpenSSH <https://www.openssh.com>`__ command line programs (:program:`ssh` & :program:`scp`), 
  which are installed on most Unix/Linux systems and which support a wider range of SSH connection methods. 
  These tools are, however, difficult to integrate into cross-platform software such as OVITO.

Which SSH connection methods the server side supports depends on the specific configuration of your SSH server. Please consult 
the documentation for your remote system or contact the administrator of your remote host if you are not sure. 

To diagnose possible connection problems, and to find out which set of SSH connection methods your server actually accepts, you can 
set the environment variable ``OVITO_SSH_LOG=1`` when you run OVITO. This will request the program to print verbose logging 
messages to the terminal during a connection attempt. The log output should tell you more about why the handshaking process 
fails.

On some platforms such as Windows, output sent to the terminal is not directly visible. In such a case you should additionally set the 
environment variable ``OVITO_LOG_FILE=<filename>`` to redirect all log output of the program into a text file. On Windows, for example, 
open the command prompt (:program:`cmd.exe`) and start OVITO by entering the following commands:

.. code-block:: winbatch

  set OVITO_LOG_FILE=%USERPROFILE%\ovito_log.txt
  set OVITO_SSH_LOG=1
  "C:\Program Files\OVITO Basic\ovito.exe"

Try loading a remote file via SSH. If the connection fails, have a look at the file :file:`ovito_log.txt`, which should have been created 
in your user directory. For further help, visit the `user forum <https://www.ovito.org/forum/>`__ or contact OVITO `technical support <https://www.ovito.org/contact/>`__.