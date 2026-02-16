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

#include "ExternalVideoEncoderBackend.h"

namespace Ovito {

/******************************************************************************
 * Destructor.
 * Calls closeFile() to ensure that the external process is terminated - will block until finished.
 ******************************************************************************/
ExternalVideoEncoderBackend::~ExternalVideoEncoderBackend()
{
    try {
        closeFile();
    }
    catch(const Exception& ex) {
        // Swallow exceptions in destructor
        qWarning() << VideoEncoder::tr("Warning: Unexpected exception in ExternalVideoEncoderBackend destructor:");
        ex.logError();
    }
}

/******************************************************************************
 * Returns the list of supported output formats.
 ******************************************************************************/
QList<VideoEncoder::Format> ExternalVideoEncoderBackend::supportedFormats(QStringView path)
{
    OVITO_ASSERT(this_task::isMainThread());

    if(!_supportedFormats.empty()) return _supportedFormats;

    // Run ffmpeg to get the list of supported formats
    QProcess process;
    startFFmpegProcess(&process, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-formats"), path);
    finishFFmpegProcess(&process, 5 * 1000);

#ifdef Q_OS_WIN
    static constexpr std::string_view sep = "\r\n";
#else
    static constexpr std::string_view sep = "\n";
#endif
    // Extract the list of supported formats from the output
    // Stored in the second column of the terminal output
    for(auto line : process.readAllStandardOutput() | std::views::split(sep)) {
        auto tokens =
            line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); }) | std::views::drop(1);
        if(tokens.begin() != tokens.end()) {
            if(const VideoEncoder::CandidateFormat* candidate = getCandidateFormat(std::string_view(*tokens.begin()))) {
                _supportedFormats.push_back({.candidate = candidate, .avformat = nullptr});
            }
        }
    }

    return _supportedFormats;
}

/******************************************************************************
 * Returns the list of supported output codecs.
 ******************************************************************************/
QList<const VideoEncoder::CandidateCodec*> ExternalVideoEncoderBackend::supportedCodecs(QStringView path)
{
    OVITO_ASSERT(this_task::isMainThread());

    if(!_supportedCodecs.empty()) return _supportedCodecs;

    // Run ffmpeg to get the list of supported encoders
    QProcess process;
    startFFmpegProcess(&process, QStringList() << QStringLiteral("-hide_banner") << QStringLiteral("-encoders"), path);
    finishFFmpegProcess(&process, 5 * 1000);

#ifdef Q_OS_WIN
    static constexpr std::string_view sep = "\r\n";
#else
    static constexpr std::string_view sep = "\n";
#endif

    // Extract the list of supported formats from the encoders
    // Stored in the second column of the terminal output
    for(auto line : process.readAllStandardOutput() | std::views::split(sep)) {
        auto tokens =
            line | std::views::split(' ') | std::views::filter([](auto&& r) { return !std::ranges::empty(r); }) | std::views::drop(1);
        if(tokens.begin() != tokens.end()) {
            const VideoEncoder::CandidateCodec* candidate = getCandidateCodec(std::string_view(*tokens.begin()));
            // Only add unique codecs
            if(candidate != nullptr &&
               std::ranges::none_of(_supportedCodecs, [&](const auto* c) { return c->libName == candidate->libName; })) {
                _supportedCodecs.push_back(candidate);
            }
        }
    }
    return _supportedCodecs;
}

/******************************************************************************
 * Start subprocess with the path executable with a given command
 * If path.isEmpty() the path is read from QSettings[VideoEncoder::FFMPEG_PATH_SETTING]
 * Timeout is the maximum time for the process to start up
 * Throws an exception if the process does not start successfully
 ******************************************************************************/
void ExternalVideoEncoderBackend::startFFmpegProcess(QProcess* process, const QStringList& command, QStringView path, const int timeout)
{
    QSettings settings;
    const QString& executable = path.isEmpty() ? settings.value(VideoEncoder::FFMPEG_PATH_SETTING, {}).toString() : path.toString();
    if(executable.isEmpty()) {
        throw Exception(VideoEncoder::tr("FFmpeg path is not set."));
    }
    // Start the process
    process->start(executable, command);

    // Wait for the process to start.
    if(process->state() == QProcess::Starting || process->state() == QProcess::NotRunning) {
        if(!process->waitForStarted(timeout)) {
            Exception ex(VideoEncoder::tr("Failed to start FFmpeg process: %1 %2.").arg(executable).arg(command.join(" ")));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
            throw ex;
        }
    }
}

/******************************************************************************
 * Finishes the provided sub-process
 * Blocks until the process is finished or the timeout is reached
 * Negative timeout lead to blocking until the process is finished
 * Throws an exception if the process does not finish successfully
 ******************************************************************************/
void ExternalVideoEncoderBackend::finishFFmpegProcess(QProcess* process, const int timeout)
{
    if(!process) {
        return;
    }

    int time = 0;
    constexpr int pollingInterval = 20; // ms
    do {
        process->waitForFinished(pollingInterval);
        time += pollingInterval;
        if(this_task::get() && this_task::get()->isCanceled()) {
            process->terminate();
        }
        if(timeout > 0 && time > timeout) {
            Exception ex(VideoEncoder::tr("FFmpeg video encoding process did not finish: Timeout reached"));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
            ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
            throw ex;
        }
    }
    while(process->state() == QProcess::Running);

    if(process->exitStatus() == QProcess::CrashExit) {
        Exception ex(VideoEncoder::tr("External FFmpeg video encoding process crashed."));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
        throw ex;
    }

    if(process->exitCode() != 0) {
        Exception ex(VideoEncoder::tr("External FFmpeg video encoding process failed with exit code: %1").arg(process->exitCode()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardOutput()));
        ex.appendDetailMessage(QString::fromUtf8(process->readAllStandardError()));
        throw ex;
    }
}

namespace {
inline bool isGif(const QString& filename, VideoEncoder::Format* format)
{
    return (format && format->candidate->name == QByteArrayLiteral("gif")) ||
           filename.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive);
}

inline bool isMov(const QString& filename, VideoEncoder::Format* format)
{
    return (format && format->candidate->name == QByteArrayLiteral("mov")) ||
           filename.endsWith(QStringLiteral(".mov"), Qt::CaseInsensitive);
}

inline bool isAvi(const QString& filename, VideoEncoder::Format* format)
{
    return (format && format->candidate->name == QByteArrayLiteral("avi")) ||
           filename.endsWith(QStringLiteral(".avi"), Qt::CaseInsensitive);
}
}  // namespace

/******************************************************************************
 * Opens a video file for writing.
 ******************************************************************************/
void ExternalVideoEncoderBackend::openFile(const QString& filename, int width, int height, float framesPerSecond)
{
    _process = new QProcess(_parent);

    // Special case for gif encoder -> stores all images and processes them later
    if(isGif(filename, nullptr)) {
        _gifMode = true;
        _framesPerSecond = framesPerSecond;
        _width = width;
        _height = height;
        _outFilename = filename;
        return;
    }

    // Default streaming mode for video encoding

    // Grab settings
    QSettings settings;
    const QByteArray& codecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    const VideoEncoder::CandidateCodec* codecPtr = VideoEncoderBackend::getCandidateCodec(codecName);
    if(!codecPtr) {
        throw Exception(VideoEncoder::tr("Video encoder '%1' not found. It may not be supported by the current FFmpeg backend.").arg(codecName));
    }

    // Fallback to libx264 for avi and mov containers - no support for libx265
    const QByteArray& codecLib =
        ((isMov(filename, nullptr) || isAvi(filename, nullptr)) && codecPtr->libName == QByteArrayLiteral("libx265"))
            ? QByteArrayLiteral("libx264")
            : codecPtr->libName;

    // Read quality
    const auto quality =
        (VideoEncoder::Quality)settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    // Configure codec quality / bitrate
    QStringList qualityArgs;
    if(codecLib == QByteArrayLiteral("libx264")) {
        qualityArgs << QStringLiteral("-crf");
        if(quality == VideoEncoder::Quality::High) {
            qualityArgs << QStringLiteral("19");
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            qualityArgs << QStringLiteral("22");
        }
        else if(quality == VideoEncoder::Quality::Low) {
            qualityArgs << QStringLiteral("26");
        }
    }
    else if(codecLib == QByteArrayLiteral("libx265")) {
        qualityArgs << QStringLiteral("-crf");
        if(quality == VideoEncoder::Quality::High) {
            qualityArgs << QStringLiteral("17");
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            qualityArgs << QStringLiteral("20");
        }
        else if(quality == VideoEncoder::Quality::Low) {
            qualityArgs << QStringLiteral("24");
        }
    }
    else if(codecLib == QByteArrayLiteral("mpeg4")) {
        if(quality == VideoEncoder::Quality::High) {
            qualityArgs << QStringLiteral("-qmin") << QStringLiteral("3") << QStringLiteral("-qmax") << QStringLiteral("3");
        }
        else if(quality == VideoEncoder::Quality::Medium) {
            qualityArgs << QStringLiteral("-qmin") << QStringLiteral("4") << QStringLiteral("-qmax") << QStringLiteral("5");
        }
        else if(quality == VideoEncoder::Quality::Low) {
            qualityArgs << QStringLiteral("-qmin") << QStringLiteral("6") << QStringLiteral("-qmax") << QStringLiteral("8");
        }
    }
    else {
        throw Exception(VideoEncoder::tr("Selected codec is not supported by this OVITO version: %1").arg(codecLib));
    }
    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-f") << QStringLiteral("rawvideo") << QStringLiteral("-pix_fmt")
         << QStringLiteral("rgb32") << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(width).arg(height)
         << QStringLiteral("-framerate") << QString::number(framesPerSecond) << QStringLiteral("-i") << QStringLiteral("-")
         << QStringLiteral("-c:v") << codecLib << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p") << qualityArgs
         << QStringLiteral("-preset") << QStringLiteral("slow") << QStringLiteral("-y") << filename;

    qDebug() << args;
    startFFmpegProcess(_process, args);
}

/******************************************************************************
 * Writes a single frame into the video file / cache in the case of gif.
 ******************************************************************************/
void ExternalVideoEncoderBackend::writeFrame(const QImage& image)
{
    // Special case for gif encoder -> stores all images and processes them later
    if(_gifMode) {
        _images.push_back(image.convertToFormat(QImage::Format_ARGB32));
        return;
    }

    // Video mode - store each image individually
    if(image.width() % 2 != 0) {
        throw Exception(VideoEncoder::tr("The selected video codec requires the image width to be even. Current width: %1").arg(image.width()));
    }
    if(image.height() % 2 != 0) {
        throw Exception(
            VideoEncoder::tr("The selected video codec requires the image height to be even. Current height: %1").arg(image.height()));
    }

    if(_finalized) {
        throw Exception(VideoEncoder::tr("Cannot write another video frame after finalize has been called."));
    }

    // Convert image to RGB32 format
    QImage rgb = image.convertToFormat(QImage::Format_RGB32);

    // Send raw pixel data to FFmpeg process
    const uchar* bits = rgb.constBits();
    const qint64 size = rgb.sizeInBytes();

    OVITO_ASSERT(_process);
    OVITO_ASSERT(_process->state() == QProcess::Running);
    _process->write((const char*)bits, size);
}

/******************************************************************************
 * Closes the written video file.
 * Blocks until the work is finished.
 ******************************************************************************/
void ExternalVideoEncoderBackend::closeFile()
{
    if(_finalized) {
        return;
    }

    _finalized = true;

    if(_gifMode) {
        // Generate gif from the cached images
        QSettings settings;
        const QString& executable = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, "FFmpeg").toString();

        QStringList args;

        // Temporary file for palette generation
        QTemporaryFile paletteFile("palette.XXXXXX.png");
        if(!paletteFile.open()) {
            throw Exception(VideoEncoder::tr("Failed to open temporary file for GIF color palette generation."));
        }
        paletteFile.close();

        // Run palette generation
        args << QStringLiteral("-hide_banner") << QStringLiteral("-f") << QStringLiteral("rawvideo") << QStringLiteral("-pix_fmt")
             << QStringLiteral("rgba") << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(_width).arg(_height)
             << QStringLiteral("-framerate") << QString::number(_framesPerSecond) << QStringLiteral("-i") << QStringLiteral("-")
             << QStringLiteral("-vf") << QStringLiteral("palettegen=stats_mode=full") << QStringLiteral("-y") << paletteFile.fileName();

        startFFmpegProcess(_process, args);

        for(const QImage& image : _images) {
            // Send raw pixel data to FFmpeg process
            const uchar* bits = image.constBits();
            const qint64 size = image.sizeInBytes();
            _process->write((const char*)bits, size);
        }

        // Close the input pipe -> signal ffmpeg to finish
        _process->closeWriteChannel();
        finishFFmpegProcess(_process, -1);

        // Confirm the palette file exists and has content
        const QFileInfo paletteInfo(paletteFile.fileName());
        if(!paletteInfo.exists() || paletteInfo.size() == 0) {
            throw Exception(VideoEncoder::tr("Something went wrong. GIF color palette file was not created by FFmpeg. Expected path: %1").arg(paletteFile.fileName()));
        }

        args.clear();

        // Render the gif
        args << QStringLiteral("-hide_banner") << QStringLiteral("-f") << QStringLiteral("rawvideo") << QStringLiteral("-pix_fmt")
             << QStringLiteral("rgba") << QStringLiteral("-video_size") << QStringLiteral("%1x%2").arg(_width).arg(_height)
             << QStringLiteral("-framerate") << QString::number(_framesPerSecond) << QStringLiteral("-i") << QStringLiteral("-")
             << QStringLiteral("-i") << paletteFile.fileName() << QStringLiteral("-lavfi")
             << QStringLiteral("paletteuse=dither=floyd_steinberg") << QStringLiteral("-y") << _outFilename;

        startFFmpegProcess(_process, args);

        for(const QImage& image : _images) {
            // Send raw pixel data to FFmpeg process
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
        qDebug() << QString::fromUtf8(_process->readAllStandardOutput());
    }
}

}  // namespace Ovito
