.. _application_settings.video_encoder:

Video Encoding
==============

On this tab of the :ref:`application settings dialog <application_settings>`, you can configure an external FFmpeg executable used for video encoding.
This is optional — OVITO comes packaged with a default FFmpeg library that will be used for video encoding if no external FFmpeg executable is configured.
External FFmpeg versions are supported on all platforms (Windows, Linux, macOS) and can give access to newer video encoding codecs, leading to smaller
output file sizes and improved quality.

FFmpeg releases can be downloaded from the official website `https://ffmpeg.org/download.html <https://ffmpeg.org/download.html>`__.

External encoder
""""""""""""""""

FFmpeg executable
  Here you can provide the path to the FFmpeg executable on your computer. You can either type in a full filesystem path to the executable or, if FFmpeg is in your ``PATH``,
  just the name of the executable. Alternatively, you can use the :menuselection:`...` button to select the FFmpeg executable using a file selection dialog.
  In any case, the executable must be named either ``ffmpeg`` or ``ffmpeg.exe`` (case insensitive).

  Once an FFmpeg executable is selected, OVITO will automatically validate it. If the selected executable is not valid, the built-in FFmpeg encoder of OVITO will be used.
  The outcome of the validation is displayed in the status field. If successful, the resolved path and FFmpeg version string will be displayed.
  The executable needs to have its executable flag set (e.g. by running ``chmod +x ffmpeg`` on macOS/Linux) to be considered valid.

  Leaving this field empty will automatically fall back to the built-in FFmpeg encoder of OVITO.

Codec settings
""""""""""""""

Codec
  You can select the video encoding codec here. OVITO checks the provided FFmpeg executable for supported codecs and lists them in the combo box.
  Currently, `h264` and `h265` codecs are supported. The `h264` codec offers widespread compatibility and good performance. The more modern `h265` (`HEVC`) codec
  is supported by newer players and can be used to further reduce file sizes at visually comparable quality.

  However, the `h265` codec does not work with ``.avi`` and ``.mov`` output file formats. OVITO will automatically fall back to the `h264` codec in this case.

  If the selected FFmpeg executable does not provide any of the supported codecs, the built-in FFmpeg encoder will be used.

Quality preset
  You can select a quality preset here. A higher quality preset will lead to larger file sizes and longer encoding times but increased visual fidelity of the
  resulting video.

  For external FFmpeg executables, the presets adjust the ``crf`` parameter of the ``ffmpeg`` command line interface. For
  `h264`, the ``crf`` value ranges from ``26`` (quality: low) to ``19`` (quality: high). For `h265`, the ``crf`` value ranges from ``24`` (quality: low) to ``17`` (quality: high).
  The built-in FFmpeg encoder of OVITO uses these presets to set FFmpeg's ``q_min`` and ``q_max`` parameters, ranging from ``3`` (quality: high) to ``8`` (quality: low).
  Note that previous versions of OVITO defaulted to the "high" quality preset.

Additional remarks
""""""""""""""""""

When rendering a video to a file, a small hint will be shown in the :ref:`core.render_settings` panel informing you whether the internal or external FFmpeg encoder will be used.