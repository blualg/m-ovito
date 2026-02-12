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
QList<VideoEncoder::Format> ExternalVideoEncoder::supportedFormats(const QString& path)
{
    OVITO_ASSERT(this_task::isMainThread());

    if(!_supportedFormats.empty()) return _supportedFormats;

    QProcess process;
    startFFmpegProcess(&process, QStringList() << "-hide_banner" << "-formats", path);
    finishFFmpegProcess(&process, 5 * 1000);

    for(auto line : process.readAllStandardOutput() | std::views::split('\n')) { // VIDTODO: Wieso hier stets '\n' und unten in supportedCodecs() plattformabhängig "sep"?
#if 0
        // VIDTODO: Statt for-loop lieber folgendes?
        // Tokenize line, skip first token (contains flags), and check second token for format name.
        auto tokens = line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); }) | std::views::drop(1);
        if(tokens.begin() != tokens.end()) {
            if(const VideoEncoder::CandidateFormat* candidate = getCandidateFormat(std::string_view(*tokens.begin()))) {
                _supportedFormats.push_back({.candidate = candidate, .avformat = nullptr});
            }
        }
#endif
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

/******************************************************************************
 * Returns the list of supported output codecs.
 ******************************************************************************/
QList<const VideoEncoder::CandidateCodec*> ExternalVideoEncoder::supportedCodecs(const QString& path)
{
    OVITO_ASSERT(this_task::isMainThread());

    if(!_supportedCodecs.empty()) return _supportedCodecs;

    QProcess process;
    startFFmpegProcess(&process, QStringList() << "-hide_banner" << "-encoders", path);
    finishFFmpegProcess(&process, 5 * 1000);

#ifdef Q_OS_WIN
    static constexpr std::string_view sep = "\r\n";
#else
    static constexpr std::string_view sep = "\n";
#endif

    for(auto line : process.readAllStandardOutput() | std::views::split(sep)) {
        size_t tokenIndex = 0;
        for(auto token : line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); })) {
            // Second token is the name of the format
            if(tokenIndex == 1) {
                const VideoEncoder::CandidateCodec* candidate = getCandidateCodec(std::string_view(token));
                if(candidate != nullptr) {
                    _supportedCodecs.push_back(candidate);
                    qDebug() << "Supported codec:" << std::string_view(token) << candidate->name << candidate->longName
                             << candidate->libName;
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

void ExternalVideoEncoder::startFFmpegProcess(QProcess* process, const QStringList& command, const QString& path, const int timeout)
{
    QSettings settings;
    const QString& executable = path.isEmpty() ? settings.value(VideoEncoder::FFMPEG_PATH_SETTING, "ffmpeg").toString() : path;
    qDebug() << executable << command;
    if(executable.isEmpty()) {
        throw Exception(tr("FFmpeg (%1) path is not set.").arg(executable));
    }
    // Start the process
    process->start(executable, command);

    // Wait for the process to start.
    if(process->state() == QProcess::Starting || process->state() == QProcess::NotRunning) {
        if(!process->waitForStarted(timeout)) {
            Exception ex(tr("Failed to start FFmpeg process: %1 %2.").arg(executable).arg(command.join(" ")));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
            throw ex;
        }
    }
}

/// finish current process
void ExternalVideoEncoder::finishFFmpegProcess(QProcess* process, const int timeout)
{
    if(!process) {
        return;
    }

    int time = 0;
    constexpr int pollingInterval = 100;
    do {
        process->waitForFinished(pollingInterval);
        time += pollingInterval;
        if(this_task::get() && this_task::get()->isCanceled()) {
            qDebug() << "Canceled";
            process->terminate();
        }
        if(timeout > 0 && time > timeout) {
            Exception ex(tr("Failed to finish FFmpeg process: Timeout reached"));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
            throw ex;
        }
    } while(process->state() == QProcess::Running);

    if(process->exitStatus() == QProcess::CrashExit) {
        Exception ex(tr("FFmpeg crashed"));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
        throw ex;
    }

    if(process->exitCode() != 0) {
        Exception ex(tr("FFmpeg failed with exit code:") + QString::number(process->exitCode()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
        throw ex;
    }
}

void ExternalVideoEncoder::openFile(const QString& filename, int width, int height, float framesPerSecond, VideoEncoder::Format* format)
{
    _process = new QProcess(this);

    // Special case for gif encoder -> stores all images and processes them later
    if((format && format->candidate->name == "gif") || filename.endsWith(".gif", Qt::CaseInsensitive)) {
        _gifmode = true;
        _framesPerSecond = framesPerSecond;
        _width = width;
        _height = height;
        _outFilename = filename;
        return;
    }

    // Grab settings
    QSettings settings;
    const QByteArray& codecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    qDebug() << "codecName" << codecName;
    const VideoEncoder::CandidateCodec* codecPtr = VideoEncoderBackend::getCandidateCodec(codecName);
    if(!codecPtr) {
        throw Exception(tr("Video encoder (%1) not found.").arg(codecName));
    }
    const QByteArray& codecLib = codecPtr->libName;

    const auto quality =
        (VideoEncoder::Quality)settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    // Default streaming mode for video encoding
    QStringList args;
    int crf = 23;
    if(codecLib == "libx264") {
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
    else if(codecLib == "libx265") {
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
    else {
        throw Exception(tr("Unsupported codec: %1").arg(codecLib));
    }
    args << "-hide_banner"
         << "-f" << "rawvideo"
         << "-pixel_format" << "rgb32"
         << "-video_size" << QStringLiteral("%1x%2").arg(width).arg(height) << "-framerate" << QString::number(framesPerSecond) << "-i"
         << "-"
         << "-c:v" << codecLib << "-pix_fmt" << "yuv420p"
         << "-crf" << QString::number(crf) << "-preset" << "slow"
         << "-y" << filename;

    startFFmpegProcess(_process, args);
}

void ExternalVideoEncoder::writeFrame(const QImage& image)
{
    // Special case for gif encoder -> stores all images and processes them later
    if(_gifmode) {
        _images.push_back(image.convertToFormat(QImage::Format_RGB32));
        return;
    }

    // Video mode - store each image individually
    if(image.width() % 2 != 0) {
        throw Exception(tr("Image width must be even: %1").arg(image.width())); // VIDTODO: Hier sollte noch eine Begründung für den Nutzer folgen
    }
    if(image.height() % 2 != 0) {
        throw Exception(tr("Image height must be even: %1").arg(image.height()));
    }
    if(!_process) {
        throw Exception(tr("FFmpeg process not found!"));
    }

    if(_finalized) {
        throw Exception(tr("Cannot write frame after finalize"));
    }

    // Convert image to RGB24 format
    QImage rgb = image.convertToFormat(QImage::Format_RGB32);

    // Write raw pixel data
    const uchar* bits = rgb.constBits();
    const qint64 size = rgb.sizeInBytes();
    _process->write((const char*)bits, size);
}

void ExternalVideoEncoder::closeFile()
{
    if(_finalized) {
        return;
    }

    _finalized = true;

    if(_gifmode) {
        // Process all images in the temporary directory
        QSettings settings;
        const QString& executable = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, "FFmpeg").toString();

        QStringList args;

        // Temporary file for palette generation
        QTemporaryFile paletteFile("palette.XXXXXX.png");
        if(!paletteFile.open()) {
            throw Exception(tr("Failed to open temporary file for palette generation."));
        }
        paletteFile.close();
        qDebug() << "Writing palette to" << paletteFile.fileName();

        // Run palette generation
        args << "-hide_banner" << "-f" << "rawvideo"
             << "-pixel_format" << "rgb32" << "-video_size" << QStringLiteral("%1x%2").arg(_width).arg(_height) << "-framerate"
             << QString::number(_framesPerSecond) << "-i" << "-" << "-vf"
             << "palettegen=stats_mode=full" << "-y" << paletteFile.fileName();

        startFFmpegProcess(_process, args);

        for(const QImage& image : _images) {
            // Write raw pixel data
            const uchar* bits = image.constBits();
            const qint64 size = image.sizeInBytes();
            _process->write((const char*)bits, size);
        }

        // Close the input pipe
        _process->closeWriteChannel();
        finishFFmpegProcess(_process, -1);

        // Confirm the palette file exists and has content
        const QFileInfo paletteInfo(paletteFile.fileName());
        if(!paletteInfo.exists() || paletteInfo.size() == 0) {
            throw Exception(tr("Palette file was not created properly"));
        }

        args.clear();

        // Render the gif
        args << "-hide_banner" << "-f" << "rawvideo"
             << "-pixel_format" << "rgb32" << "-video_size" << QStringLiteral("%1x%2").arg(_width).arg(_height) << "-framerate"
             << QString::number(_framesPerSecond) << "-i" << "-" << "-i" << paletteFile.fileName() << "-lavfi"
             << "paletteuse=dither=floyd_steinberg" << "-y" << _outFilename;

        startFFmpegProcess(_process, args);

        for(const QImage& image : _images) {
            // Write raw pixel data
            const uchar* bits = image.constBits();
            const qint64 size = image.sizeInBytes();
            _process->write((const char*)bits, size);
        }

        // Close the input pipe and wait for the process to finish
        _process->closeWriteChannel();
        finishFFmpegProcess(_process, -1);
    }
    else {
        // Close the input pipe (non-gif mode)
        _process->closeWriteChannel();
        finishFFmpegProcess(_process, -1);
    }
}

}  // namespace Ovito
