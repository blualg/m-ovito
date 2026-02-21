.. _application_settings.video_encoding:

Video encoding
==============

.. image:: /images/app_settings/video_encoder_settings.*
  :width: 45%
  :align: right

On this tab of the :ref:`application settings dialog <application_settings>`, you can configure an external FFmpeg executable used for video encoding.
This is optional — OVITO comes packaged with an integrated FFmpeg video encoding engine that will be used for video encoding if no external FFmpeg executable is configured.

However, the integrated encoder of OVITO cannot produce videos using the high-quality `H.264` and `H.265` codecs due to licensing and patent restrictions.
By specifying an external FFmpeg executable you can let OVITO produce video files using these newer video encoding codecs, leading to smaller
file sizes and improved quality.

The FFmpeg software can be downloaded from the official website at `https://ffmpeg.org/download.html <https://ffmpeg.org/download.html>`__.

.. _application_settings.video_encoding.external_encoder:

External encoder
""""""""""""""""

FFmpeg executable
  Here you can provide the path to an FFmpeg executable installed on your computer. You can either type in a full filesystem path to the executable or, if FFmpeg is in your ``PATH``,
  just the name of the executable ("ffmpeg"), see the screenshot. Click the :guilabel:`...` button to select the FFmpeg executable using a file selection dialog.
  The executable must be named either :file:`ffmpeg` or :file:`ffmpeg.exe` (case insensitive).

  Once you specify the FFmpeg executable, OVITO will examine it first. If the selected executable is invalid, OVITO will fall back to the internal FFmpeg encoder.
  The outcome of the validation is displayed in the status field.

  You can clear the path to use the internal video encoder of OVITO again.

.. _application_settings.video_encoding.encoding_settings:

Encoding settings
"""""""""""""""""

Quality preset
  A higher quality preset gives increased visual fidelity of the resulting video but also leads to a larger file size.

  When using an external FFmpeg, the selected preset determines the ``crf`` parameter on the command line interface.
  For `H.264`, the ``crf`` value ranges from 26 (low quality) to 19 (high quality).
  For `H.265`, the ``crf`` value ranges from 24 (low quality) to 17 (high quality).
  The built-in encoder of OVITO (`MPEG-4` codec) chooses the ``q_min`` and ``q_max`` encoder parameters depending on the selected quality preset,
  ranging from 3 (high quality) to 8 (low quality). Previous versions of OVITO always used the high quality preset for the `MPEG-4` codec.

Codec
  Select the video encoding algorithm (codec). OVITO checks the provided FFmpeg executable for supported codecs.
  Currently, `H.264`, `H.265`, and `MPEG-4` codecs are supported by OVITO. The `H.264` codec offers widespread compatibility and good performance. The more modern `H.265` (`HEVC`) codec
  is supported by newer players and can be used to further reduce file sizes at visually comparable quality. The `MPEG-4` codec should only be
  used for playback on legacy devices that do not support newer codecs.

  The `H.265` codec does not work with ``.avi`` and ``.mov`` container file formats. OVITO will automatically fall back to the `H.264` codec in these cases.