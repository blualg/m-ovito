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
 * \brief Wrapper class for the ffmpeg video encoding using an external process.
 */
class OVITO_CORE_EXPORT ExternalVideoEncoder : public VideoEncoder::VideoEncoderBackend
{
    Q_OBJECT

public:
    /// Constructor.
    ExternalVideoEncoder(QObject* parent = nullptr) : VideoEncoder::VideoEncoderBackend(parent) { qDebug() << "ExternalVideoEncoder()"; }

    /// Destructor.
    /// Calls closeFile() to ensure that the external process is terminated - might block for up to 30s;
    virtual ~ExternalVideoEncoder();

    /// Opens a video file for writing.
    virtual void openFile(
        const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format = nullptr) override;

    /// Writes a single frame into the video file.
    virtual void writeFrame(const QImage& image) override;

    /// This closes the written video file.
    /// Blocks up to 30s until the work is finished.
    virtual void closeFile() override;

    /// Returns the list of supported output formats filtered by the current codec.
    // static QList<VideoEncoder::Format> supportedFormatsFiltered();

    /// Returns the list of supported output formats.
    static QList<VideoEncoder::Format> supportedFormats();

    /// Returns the list of supported codecs.
    static QList<const VideoEncoder::CandidateCodec*> supportedCodecs();

    /// Clears the list of supported codecs.
    static void clearCodecs() { _supportedCodecs.clear(); }

    /// finish current process
    void finishCurrentProcess();

private:
    /// Subprocess running ffmpeg
    QProcess* _process;

    /// Flag indicating that the encoder has been finalized - no more frames can be added
    bool _finalized = false;

    /// Temporary file used for image storage and palette generation (gif encoder only)
    std::vector<QImage> _images;
    bool _gifmode = false;
    int _width = 0;
    int _height = 0;

    float _framesPerSecond;

    QString _outFilename;

    /// The list of supported video formats.
    inline static QList<VideoEncoder::Format> _supportedFormats;

    /// The filtered list of supported video codecs.
    // inline static QList<VideoEncoder::Format> _supportedFormatsFiltered;

    /// The list of supported video codecs.
    inline static QList<const VideoEncoder::CandidateCodec*> _supportedCodecs;
};

}  // namespace Ovito
