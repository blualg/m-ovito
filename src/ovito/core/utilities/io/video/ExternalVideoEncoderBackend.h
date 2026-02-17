////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>

namespace Ovito {

/**
 * \brief Wrapper class for the FFmpeg video encoding using an external process.
 */
class OVITO_CORE_EXPORT ExternalVideoEncoderBackend : public VideoEncoder::VideoEncoderBackend
{
public:
    /// Constructor.
    ExternalVideoEncoderBackend(QObject* parent = nullptr) : _parent(parent) {}

    /// Destructor.
    /// Calls closeFile() to ensure that the external process is terminated - will block until finished
    virtual ~ExternalVideoEncoderBackend();

    /// Opens a video file for writing.
    virtual void openFile(const QString& filename, int width, int height, float framesPerSecond) override;

    /// Writes a single frame into the video file / cache in the case of gif.
    virtual void writeFrame(const QImage& image) override;

    /// This closes the written video file.
    /// Blocks until the work is finished.
    virtual void closeFile() override;

    /// Returns the list of supported output formats.
    static QList<VideoEncoder::Format> supportedFormats(QStringView path);

    /// Returns the list of supported codecs.
    static QList<const VideoEncoder::CandidateCodec*> supportedCodecs(QStringView path);

    /// Clears the list of supported codecs.
    static void clearCodecs() { _supportedCodecs.clear(); }

private:
    /// Finishes the provided sub-process
    /// Blocks until the process is finished or the timeout is reached
    /// Negative timeout lead to blocking until the process is finished
    /// Throws an exception if the process does not finish successfully
    static void finishFFmpegProcess(QProcess* process, int timeout = 30 * 1000);

    /// Start subprocess with the path executable with a given command
    /// If path.isEmpty() the path is read from QSettings[VideoEncoder::FFMPEG_PATH_SETTING]
    /// Timeout is the maximum time for the process to start up
    static void startFFmpegProcess(QProcess* process, const QStringList& command, QStringView path = {}, int timeout = 30 * 1000);

    /// Subprocess running FFmpeg
    QProcess* _process = nullptr;

    /// Parent object for _process lifetime management
    QObject* _parent;

    /// Flag indicating that the encoder has been finalized - no more frames can be added
    bool _finalized = false;

    /// Cache images for 2 pass gif generation (gif encoder only)
    std::vector<QImage> _images;

    /// When rendering gif we need to cache all images for 2 pass encoding
    bool _gifMode = false;
    int _width = 0;
    int _height = 0;
    float _framesPerSecond;
    QString _outFilename;

    /// The list of supported video formats.
    inline static QList<VideoEncoder::Format> _supportedFormats;

    /// The list of supported video codecs.
    inline static QList<const VideoEncoder::CandidateCodec*> _supportedCodecs;
};

}  // namespace Ovito
