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

extern "C" {
struct AVOutputFormat;
};

namespace Ovito {

class VideoEncoderBackend;

/**
 * \brief Wrapper class for the ffmpeg video encoding library.
 */
class OVITO_CORE_EXPORT VideoEncoder : public QObject
{
    Q_OBJECT

public:
    /// List of supported video formats (containers, i.e. ffmpeg -demuxers)
    struct CandidateFormat {
        const QByteArray name;
        const QString longName;
        const QStringList extensions;
    };

    /// List of supported video codecs (encoders)
    struct CandidateCodec {
        const QByteArray name;
        const QByteArray longName;
        const QByteArray libName;
    };

    /**
     * Describes an output format supported by the video encoding engine.
     */
    struct Format {
        const CandidateFormat* candidate;
        const AVOutputFormat* avformat;
    };

    // Available video encoding quality presets.
    enum class Quality : uint8_t
    {
        Low,
        Medium,
        High
    };
    Q_ENUM(Quality)

    /**
     * \brief Abstract base class for the ffmpeg video encoding backend.
     */
    class VideoEncoderBackend : public QObject
    {
    public:
        VideoEncoderBackend(QObject* parent = nullptr) : QObject(parent) {}

        /// Opens a video file for writing.
        virtual void openFile(
            const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format = nullptr) = 0;

        /// Writes a single frame into the video file.
        virtual void writeFrame(const QImage& image) = 0;

        /// This closes the written video file.
        virtual void closeFile() = 0;

    protected:
        /// Compares a format name to the list of supported formats and returns the corresponding CandidateFormat.
        static const CandidateFormat* getCandidateFormat(std::string_view name);
        /// Compares a codec name to the list of supported codecs and returns the corresponding CandidateCodec.
        static const CandidateCodec* getCandidateCodec(std::string_view name);
    };

    enum class Backend : uint8_t
    {
        EXTERN,
        OVITO
    };
    /// Constructor.
    VideoEncoder(QObject* parent = nullptr);

    /// Opens a video file for writing.
    void openFile(const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format = nullptr);

    /// Writes a single frame into the video file.
    void writeFrame(const QImage& image);

    /// This closes the written video file.
    void closeFile();

    /// Returns the list of supported output formats.
    static QList<Format> supportedFormats(std::optional<Backend> backend = std::nullopt, const QString& path = "");

    /// Returns the list of supported output codecs.
    static QList<const VideoEncoder::CandidateCodec*> supportedCodecs(std::optional<Backend> backend = std::nullopt,
                                                                      const QString& path = "");

    constexpr static const char* FFMPEG_PATH_SETTING = "renderer/ffmpeg_path";
    constexpr static const char* FFMPEG_CODEC_SETTING = "renderer/ffmpeg_codec";
    constexpr static const char* FFMPEG_USE_EXT_SETTING = "renderer/ffmpeg_use_external";
    constexpr static const char* FFMPEG_QUALITY_SETTING = "renderer/ffmpeg_quality";

private:
    static Backend getBackend();

    std::unique_ptr<VideoEncoderBackend> _encoder;
};

}   // End of namespace
