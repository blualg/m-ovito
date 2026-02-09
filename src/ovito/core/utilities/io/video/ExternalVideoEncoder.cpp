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

#include "ExternalVideoEncoder.h"

namespace Ovito {

ExternalVideoEncoder::~ExternalVideoEncoder()
{
    qDebug() << "~ExternalVideoEncoder()";
    try {
        closeFile();
    }
    catch(const Exception& ex) {
        // Swallow exceptions in destructor
        qWarning() << "Warning: Unexpected exception in ExternalVideoEncoder destructor:";
        ex.logError();
    }
}

/******************************************************************************
 * Returns the list of supported output formats.
 ******************************************************************************/
QList<VideoEncoder::Format> ExternalVideoEncoder::supportedFormats()
{
    if(!_supportedFormats.empty()) return _supportedFormats;

    QProcess process;
    process.start("ffmpeg", QStringList() << "-hide_banner" << "-formats");

    if(!process.waitForStarted(5 * 1000)) {
        Exception ex(tr("Failed to start 'ffmpeg -formats'"));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    if(!process.waitForFinished(5 * 1000)) {
        Exception ex(tr("Failed to finish 'ffmpeg -formats'"));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    if(process.exitCode() != 0) {
        Exception ex(tr("'ffmpeg -formats' failed with exit code:") + QString::number(process.exitCode()));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    for(auto line : process.readAllStandardOutput() | std::views::split('\n')) {
        size_t tokenIndex = 0;
        for(auto token : line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); })) {
            // Second token is the name of the format
            if(tokenIndex == 1) {
                const VideoEncoder::CandidateFormat* candidate = getCandidateFormat(std::string_view(token));
                if(candidate != nullptr) {
                    _supportedFormats.push_back({.candidate = candidate, .avformat = nullptr});
                }
            }
            else if(tokenIndex > 1) {
                break;
            }
            tokenIndex++;
        }
    }

    return _supportedFormats;
}

#if 0
/******************************************************************************
 * Returns the list of supported output formats.
 ******************************************************************************/
QList<VideoEncoder::Format> ExternalVideoEncoder::supportedFormatsFiltered()
{
    const QList<VideoEncoder::Format>& formats = ExternalVideoEncoder::supportedFormats();

    QSettings settings;
    const QByteArray& codecName = settings.value(FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    qDebug() << "codecName" << codecName;
    if(codecName.isEmpty()) {
        return formats;
    }

    const VideoEncoder::CandidateCodec* codec = VideoEncoderBackend::getCandidateCodec(codecName);

    if(!codec) {
        return formats;
    }

    _supportedFormatsFiltered.clear();
    for(const auto& format : formats) {
        if(format.candidate->name == "avi" && codec->name == "hevc") {
            qDebug() << "avi" << "hevc" << "skip";
            continue;
        }
        if(format.candidate->name == "mov" && codec->name == "hevc") {
            qDebug() << "mov" << "hevc" << "skip";
            continue;
        }
        if(format.candidate->name == "gif" && codec->name == "hevc") {
            qDebug() << "mov" << "hevc" << "skip";
            continue;
        }
        _supportedFormatsFiltered.push_back(format);
    }
    return _supportedFormatsFiltered;
}
#endif
/******************************************************************************
 * Returns the list of supported output codecs.
 ******************************************************************************/
QList<const VideoEncoder::CandidateCodec*> ExternalVideoEncoder::supportedCodecs()
{
    if(!_supportedCodecs.empty()) return _supportedCodecs;

    QProcess process;
    process.start("ffmpeg", QStringList() << "-hide_banner" << "-codecs");

    if(!process.waitForStarted(5 * 1000)) {
        Exception ex(tr("Failed to start 'ffmpeg -codecs'"));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    if(!process.waitForFinished(5 * 1000)) {
        Exception ex(tr("Failed to finish 'ffmpeg -codecs'"));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    if(process.exitCode() != 0) {
        Exception ex(tr("'ffmpeg -codecs' failed with exit code:") + QString::number(process.exitCode()));
        ex.appendDetailMessage(QString::fromUtf8(process.readAllStandardError()));
        throw ex;
    }

    for(auto line : process.readAllStandardOutput() | std::views::split('\n')) {
        size_t tokenIndex = 0;
        for(auto token : line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); })) {
            // Second token is the name of the format
            if(tokenIndex == 1) {
                const VideoEncoder::CandidateCodec* candidate = getCandidateCodec(std::string_view(token));
                if(candidate != nullptr) {
                    _supportedCodecs.push_back(candidate);
                    qDebug() << "Supported codec:" << candidate->name << candidate->longName << candidate->libName;
                }
            }
            else if(tokenIndex > 1) {
                break;
            }
            tokenIndex++;
        }
    }

    return _supportedCodecs;
}

void ExternalVideoEncoder::openFile(const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format)
{
    _process = new QProcess(this);

    // Grab settings
    QSettings settings;
    const QByteArray& codecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    qDebug() << "codecName" << codecName;
    const VideoEncoder::CandidateCodec* codecPtr = VideoEncoderBackend::getCandidateCodec(codecName);
    const char* codecLib = codecPtr ? codecPtr->libName.data() : "libx264";

    const auto quality =
        (VideoEncoder::Quality)settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    int crf = 23;
    if(std::strcmp(codecLib, "libx264") == 0) {
        if(quality == VideoEncoder::Quality::High) {
            crf = 19;
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            crf = 22;
        }
        else if(quality == VideoEncoder::Quality::Low) {
            crf = 26;
        }
    }
    else if(std::strcmp(codecLib, "libx265") == 0) {
        if(quality == VideoEncoder::Quality::High) {
            crf = 17;
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            crf = 20;
        }
        else if(quality == VideoEncoder::Quality::Low) {
            crf = 24;
        }
    }
    else if(std::strcmp(codecLib, "av1") == 0) {
        if(quality == VideoEncoder::Quality::High) {
            crf = 20;
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            crf = 27;
        }
        else if(quality == VideoEncoder::Quality::Low) {
            crf = 35;
        }
    }
    else {
        throw Exception(tr("Unsupported codec: %1").arg(codecLib));
    }

    QStringList args;
    args << "-hide_banner"
         << "-f" << "rawvideo"
         << "-pixel_format" << "rgb32"
         << "-video_size" << QStringLiteral("%1x%2").arg(width).arg(height) << "-framerate" << QString::number(framesPerSecond) << "-i"
         << "-"
         << "-c:v" << codecLib << "-pix_fmt" << "yuv420p"
         << "-crf" << QString::number(crf) << "-preset" << "slow"
         << "-y" << filename;

    const QString& executable = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, "ffmpeg").toString();
    qDebug() << executable << args;
    if(executable.isEmpty()) {
        throw Exception(tr("ffmpeg (%1) path is not set.").arg(executable));
    }
    _process->start(executable, args);
}

void ExternalVideoEncoder::writeFrame(const QImage& image)
{
    if(image.width() % 2 != 0) {
        throw Exception(tr("Image width must be even: %1").arg(image.width()));
    }
    if(image.height() % 2 != 0) {
        throw Exception(tr("Image height must be even: %1").arg(image.height()));
    }
    if(!_process) {
        throw Exception(tr("ffmpeg process not found!"));
    }

    if(_finalized) {
        throw Exception(tr("Cannot write frame after finalize"));
    }

    // Wait for the process to start.
    if(_process->state() == QProcess::Starting || _process->state() == QProcess::NotRunning) {
        if(!_process->waitForStarted()) {
            throw Exception(tr("Failed to start ffmpeg process."));
        }
    }

    // Convert image to RGB24 format
    QImage rgb = image.convertToFormat(QImage::Format_RGB32);

    // Write raw pixel data
    const uchar* bits = rgb.constBits();
    const qint64 size = rgb.sizeInBytes();

    _process->write((const char*)bits, size);
}

void ExternalVideoEncoder::finalize()
{
    _finalized = true;
    _process->closeWriteChannel();
}

void ExternalVideoEncoder::closeFile()
{
    finalize();

    if(!_process->waitForFinished(30 * 1000)) {
        Exception ex(tr("Failed to finish ffmpeg process"));
        ex.appendDetailMessage(QString::fromUtf8(_process->readAllStandardError()));
        throw ex;
    }

    if(_process->exitCode() != 0) {
        Exception ex(tr("ffmpeg failed with exit code:") + QString::number(_process->exitCode()));
        ex.appendDetailMessage(QString::fromUtf8(_process->readAllStandardError()));
        throw ex;
    }
}

}  // namespace Ovito
