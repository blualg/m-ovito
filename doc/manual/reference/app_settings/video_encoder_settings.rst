.. _application_settings.video_encoder:

Video encoding
==============

On this tab of the :ref:`application settings dialog <application_settings>`, you can configure an external FFmpeg executable used for video encoding.
This is optional — OVITO comes packaged with a default FFmpeg library that will be used for video encoding if no external FFmpeg executable is configured.
External FFmpeg versions are supported on all platforms (Windows, Linux, macOS) and can give access to newer video encoding codecs, leading to smaller
output file sizes and improved quality.

.. VIDTODO: Ich würde evtl die Aussage zu "supported on all platforms (Windows, Linux, macOS)" streichen. Ist nicht notwendig die Plattformen alle explizit aufzulisten in meinen Augen. Ist ja eher die Erwartungshaltung, dass es überall geht, wenn nichts explizit ausgeschlossen wird.
            Was aber vielleicht begründet werden könnte, wären die lizenzrechtlichen Einschränkungen des internen Encoder. Zum Beispiel "Note that the built-in FFmpeg encoder of OVITO does not include support for the H.264 and H.265 codecs due to licensing restrictions, while these codecs are typically available in external FFmpeg executables." Oder so ähnlich.

FFmpeg releases can be downloaded from the official website at `https://ffmpeg.org/download.html <https://ffmpeg.org/download.html>`__.

External encoder
""""""""""""""""

FFmpeg executable
  Here you can provide the path to an FFmpeg executable installed on your computer. You can either type in a full filesystem path to the executable or, if FFmpeg is in your ``PATH``,
  just the name of the executable ("ffmpeg"). Click the :guilabel:`...` button to select the FFmpeg executable using a file selection dialog.
  The executable must be named either :file:`ffmpeg` or :file:`ffmpeg.exe` (case insensitive).

  Once you specify the FFmpeg executable, OVITO will examine it first. If the selected executable is invalid, OVITO will fall back to the internal FFmpeg encoder.
  The outcome of the validation is displayed in the status field.

  You can clear the path to use the internal video encoder of OVITO again.

Encoding settings
"""""""""""""""""

.. VIDTODO: Wir die Schreibweise "h264" genutzt, um GUI dagegen die offizielle Schreibweise "H.264". Sollten wir das angleichen?

Quality preset
  You can select a quality preset here. A higher quality preset will lead to larger file sizes and longer encoding times but increased visual fidelity of the
  resulting video.

  .. VIDTODO: "You can select a quality preset here." scheint überflüssige Aussage zu sein.

  For external FFmpeg executables, the presets adjust the ``crf`` parameter of the ``ffmpeg`` command line interface. For
  `h264`, the ``crf`` value ranges from ``26`` (quality: low) to ``19`` (quality: high). For `h265`, the ``crf`` value ranges from ``24`` (quality: low) to ``17`` (quality: high).
  The built-in FFmpeg encoder of OVITO uses these presets to set FFmpeg's ``q_min`` and ``q_max`` parameters, ranging from ``3`` (quality: high) to ``8`` (quality: low).
  Note that previous versions of OVITO defaulted to the "high" quality preset.

Codec
  Select the video encoding algorithm (codec). OVITO checks the provided FFmpeg executable for supported codecs and lists them in the combo box.
  Currently, `h264` and `h265` codecs are supported by OVITO. The `h264` codec offers widespread compatibility and good performance. The more modern `h265` (`HEVC`) codec
  is supported by newer players and can be used to further reduce file sizes at visually comparable quality.

  However, the `h265` codec does not work with ``.avi`` and ``.mov`` output file formats. OVITO will automatically fall back to the `h264` codec in this case.

  If the selected FFmpeg executable does not provide any of the supported codecs, the built-in FFmpeg encoder will be used.

Additional remarks
""""""""""""""""""

.. VIDTODO: Ich glaube diese Sache ist nicht dokumentationswürdig. Es springt ja von alleine ins Auge und man muss es nicht erläutern.

When rendering a video to a file, a small hint will be shown in the :ref:`core.render_settings` panel informing you whether the internal or external FFmpeg encoder will be used.