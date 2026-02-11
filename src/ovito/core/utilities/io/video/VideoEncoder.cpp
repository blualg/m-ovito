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

#include <ovito/core/Core.h>
#include "VideoEncoder.h"
#include "OvitoVideoEncoder.h"
#include "ExternalVideoEncoder.h"

namespace Ovito {

/******************************************************************************
* Constructor
******************************************************************************/
VideoEncoder::VideoEncoder(const Task* task, QObject* parent) : QObject(parent)
{
    if(getBackend() == Backend::OVITO) {
        _encoder = std::make_unique<OvitoVideoEncoder>(task, parent);
    }
    else {
        _encoder = std::make_unique<ExternalVideoEncoder>(task, parent);
    }
}

VideoEncoder::Backend VideoEncoder::getBackend()
{
    QSettings settings;
    qDebug() << "VideoEncoder::getBackend():" << settings.value(FFMPEG_USE_EXT_SETTING, false).toBool()
             << ((settings.value(FFMPEG_USE_EXT_SETTING, false).toBool()) ? "Backend::EXTERN" : "Backend::OVITO");
    return (settings.value(FFMPEG_USE_EXT_SETTING, false).toBool()) ? Backend::EXTERN : Backend::OVITO;
}

/******************************************************************************
 * Returns the list of supported output formats.
 ******************************************************************************/
QList<VideoEncoder::Format> VideoEncoder::supportedFormats(std::optional<Backend> backend)
{
    const Backend b = backend ? *backend : getBackend();
    if(b == Backend::OVITO) {
        return OvitoVideoEncoder::supportedFormats();
    }
    else if(b == Backend::EXTERN) {
        return ExternalVideoEncoder::supportedFormats();
    }
    else {
        OVITO_ASSERT(false);
    }
    return {};
}

/******************************************************************************
 * Returns the list of supported output codecs.
 ******************************************************************************/
QList<const VideoEncoder::CandidateCodec*> VideoEncoder::supportedCodecs(std::optional<Backend> backend)
{
    const Backend b = backend ? *backend : getBackend();
    if(b == Backend::OVITO) {
        return {};
    }
    else if(b == Backend::EXTERN) {
        return ExternalVideoEncoder::supportedCodecs();
    }
    else {
        OVITO_ASSERT(false);
    }
    return {};
}
/******************************************************************************
* Opens a video file for writing.
******************************************************************************/
void VideoEncoder::openFile(const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format)
{
    _encoder->openFile(filename, width, height, framesPerSecond);
}

/******************************************************************************
* This closes the written video file.
******************************************************************************/
void VideoEncoder::closeFile() { _encoder->closeFile(); }

/******************************************************************************
* Writes a single frame into the video file.
******************************************************************************/
void VideoEncoder::writeFrame(const QImage& image) { _encoder->writeFrame(image); }

const VideoEncoder::CandidateFormat* VideoEncoder::VideoEncoderBackend::getCandidateFormat(std::string_view name)
{
    static const std::array<const CandidateFormat, 5> CandidateFormats{
        {{
             .name = "avi",
             .longName = QStringLiteral("AVI (Audio Video Interleaved)"),
             .extensions = QStringList{QStringLiteral("avi")},
         },
         {
             .name = "mp4",
             .longName = QStringLiteral("MP4 (MPEG-4 Part 14)"),
             .extensions = QStringList{QStringLiteral("mp4")},
         },
         {
             .name = "mov",
             .longName = QStringLiteral("QuickTime / MOV"),
             .extensions = QStringList{QStringLiteral("mov")},
         },
         {
             .name = "matroska",
             .longName = QStringLiteral("Matroska"),
             .extensions = QStringList{QStringLiteral("mkv")},
         },
         {
             .name = "gif",
             .longName = QStringLiteral("CompuServe Graphics Interchange Format (GIF)"),
             .extensions = QStringList{QStringLiteral("gif")},
         }}};

    const auto* it = std::ranges::find(CandidateFormats, name, &CandidateFormat::name);

    return (it == CandidateFormats.end()) ? nullptr : &(*it);
}

const VideoEncoder::CandidateCodec* VideoEncoder::VideoEncoderBackend::getCandidateCodec(std::string_view name)
{
    static const std::array<const CandidateCodec, 2> candidateCodecs{{
        // {.name = "av1", .longName = "Alliance for Open Media AV1", .libName = "libsvtav1"},
        {.name = "h264", .longName = "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10", .libName = "libx264"},
        {.name = "hevc", .longName = "H.265 / HEVC (High Efficiency Video Coding)", .libName = "libx265"},
    }};

    const auto* it = std::ranges::find(candidateCodecs, name, &CandidateCodec::name);

    return (it == candidateCodecs.end()) ? nullptr : &(*it);
}

}   // End of namespace
