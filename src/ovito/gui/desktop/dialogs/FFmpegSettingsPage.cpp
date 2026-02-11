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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/core/app/Application.h>
#include <ovito/gui/desktop/widgets/display/StatusWidget.h>
#include <ovito/core/utilities/io/video/VideoEncoder.h>
#include <ovito/gui/desktop/widgets/general/EnterLineEdit.h>
#include "FFmpegSettingsPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FFmpegSettingsPage);

/******************************************************************************
 * Creates the widget that contains the plugin specific setting controls.
 ******************************************************************************/
void FFmpegSettingsPage::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, tr("FFmpeg"));
    auto* layout = new QVBoxLayout(page);
    auto* layout1 = new QGridLayout();
    int row = 0;

    // Load default from settings
    QSettings settings;
    _ffmpegCodecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    qDebug() << "Current codec name" << _ffmpegCodecName;
    const QString& ffmpegPath = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, {}).toString();
    _externalffmpeg = settings.value(VideoEncoder::FFMPEG_USE_EXT_SETTING, false).toBool();
    int ffmpegQuality = settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    layout1->addWidget(new QLabel(tr("FFmpeg executable:")), row, 0);
    _ffmpegPath = new EnterLineEdit(page);
    layout1->addWidget(_ffmpegPath, row++, 1, 1, 2);
    // Load default from settings
    _ffmpegPath->setText(ffmpegPath);
    // Validate ffmpeg on when path is changed
    connect(_ffmpegPath, &EnterLineEdit::editingFinished, this, &FFmpegSettingsPage::validateFfmpegPath);
    connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, [this](bool valid) { _externalffmpeg = valid; });

    // Status label.
    _statusLabel = new StatusWidget(page);
    _statusLabel->setWordWrap(true);
    layout1->addWidget(_statusLabel, row++, 0, 1, 3);

    // Setup codecs:
    _ffmpegCodec = new QComboBox(page);
    layout1->addWidget(new QLabel(tr("Codec:")), row, 0);
    layout1->addWidget(_ffmpegCodec, row, 1);
    // Refresh combobox when ffmpeg path is changed
    connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, &FFmpegSettingsPage::refreshCodecCombobox);
    layout1->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), row++, 2);

    auto* codecInfoLabel = new QLabel(page);
    codecInfoLabel->setText(
        tr(".avi and .mov files do not support H.265 codec. Please use .mkv or .mp4. Otherwise, a fallback codec (H.264) will be used."));
    codecInfoLabel->setWordWrap(true);
    codecInfoLabel->setVisible(_ffmpegCodecName == "hevc");
    connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, [this, codecInfoLabel](bool valid) {
        codecInfoLabel->setVisible(valid && _ffmpegCodecName == "hevc");
    });

    // Keep sapce for the label
    QSizePolicy sp = codecInfoLabel->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    codecInfoLabel->setSizePolicy(sp);

    layout1->addWidget(codecInfoLabel, row++, 0, 1, 3);
    // Store selection
    connect(_ffmpegCodec, &QComboBox::activated, this, [this, codecInfoLabel](int index) {
        const VideoEncoder::Backend backend = _externalffmpeg ? VideoEncoder::Backend::EXTERN : VideoEncoder::Backend::OVITO;
        const QList<const VideoEncoder::CandidateCodec*>& codecs = VideoEncoder::supportedCodecs(backend);
        if(index < 0 || index > codecs.size()) return;
        const VideoEncoder::CandidateCodec* codec = codecs[index];
        _ffmpegCodecName = codec->name;
        codecInfoLabel->setVisible(_ffmpegCodecName == "hevc");
    });

    _ffmpegQualityBox = new QComboBox(page);
    QMetaEnum metaEnum = QMetaEnum::fromType<VideoEncoder::Quality>();
    for(int i = 0; i < metaEnum.keyCount(); ++i) {
        _ffmpegQualityBox->addItem(metaEnum.key(i), metaEnum.value(i));
    }
    _ffmpegQualityBox->setCurrentIndex(ffmpegQuality);

    layout1->addWidget(new QLabel(tr("Quality preset:")), row, 0);
    layout1->addWidget(_ffmpegQualityBox, row, 1);
    layout1->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), row++, 2);

    layout->addLayout(layout1);
    layout->addStretch();

    // Validate path on startup
    validateFfmpegPath();
    // Select the current codec
    refreshCodecCombobox();
}

void FFmpegSettingsPage::refreshCodecCombobox()
{
    _ffmpegCodec->clear();
    qDebug() << "Current codec name" << _ffmpegCodecName;
    const VideoEncoder::Backend backend = _externalffmpeg ? VideoEncoder::Backend::EXTERN : VideoEncoder::Backend::OVITO;
    for(const auto [i, codec] : Ovito::enumerate(VideoEncoder::supportedCodecs(backend))) {
        _ffmpegCodec->addItem(codec->longName);
        qDebug() << "codec name" << _ffmpegCodecName << codec->name << (codec->name == _ffmpegCodecName);
        if(codec->name == _ffmpegCodecName) {
            _ffmpegCodec->setCurrentIndex((int)i);
        }
    }
}

void FFmpegSettingsPage::validateFfmpegPath()
{
    // User input
    const QString userInput = _ffmpegPath->text().trimmed();
    // qDebug() << "userInput" << userInput;

    // Check path is empty
    if(userInput.isEmpty()) {
        _statusLabel->setStatus(PipelineStatus(tr("FFmpeg path is empty, the built-in FFmpeg library will be used.")));
        ffmpegPathValidated(false);
        return;
    }

    // Resolve the path
    const QString path = QStandardPaths::findExecutable(userInput);
    // qDebug() << "path" << path;

    // Check if file exists
    QFileInfo fileInfo(path);
    if(!fileInfo.exists()) {
        // qDebug() << tr("'%1 (%2)' does not exist.").arg(userInput).arg(path);
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error, tr("%1 does not exist, the built-in FFmpeg library will be used.").arg(userInput).arg(path)));
        ffmpegPathValidated(false);
        return;
    }

    // Check if file is executable.
    if(!fileInfo.isExecutable()) {
        // qDebug() << tr("'%1 (%2)' is not executable.").arg(userInput).arg(path);
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error, tr("%1 (%2) is not executable, the built-in FFmpeg library will be used.").arg(userInput).arg(path)));
        ffmpegPathValidated(false);
        return;
    }

    // Match the name before attempting to run the executable.
    if(!(path.endsWith("ffmpeg", Qt::CaseInsensitive) || path.endsWith("ffmpeg.exe", Qt::CaseInsensitive))) {
        // qDebug() << tr("Path needs to end with 'ffmpeg' or 'ffmpeg.exe' to be valid, the built-in ffmpeg library will be used.");
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error,
            tr("Provided path needs to end with 'ffmpeg' or 'ffmpeg.exe' to be valid, the built-in FFmpeg library will be used.")));
        ffmpegPathValidated(false);
        return;
    }

    _statusLabel->setStatus(PipelineStatus(tr("Checking FFmpeg executable...")));

    // Background process used for checking if ffmpeg is executable
    QProcess* process = new QProcess();

    // Setup timeout timer (ms)
    constexpr int timeout = 3 * 1000;
    QTimer* timeoutTimer = new QTimer();
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(timeout);

    // Handle timeout
    connect(timeoutTimer, &QTimer::timeout, this, [process, userInput, path, timeoutTimer, this]() {
        process->kill();
        process->deleteLater();
        timeoutTimer->deleteLater();
        // qDebug() << tr("Timeout while checking: '%1 (%2)'.").arg(userInput).arg(path);
        _statusLabel->setStatus(
            PipelineStatus(PipelineStatus::Error,
                           tr("Timeout while checking: %1 (%2), the built-in FFmpeg library will be used.").arg(userInput).arg(path)));
        ffmpegPathValidated(false);
        return;
    });

    // Handle process finished
    connect(
        process,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        [process, timeoutTimer, userInput, path, this](int exitCode, QProcess::ExitStatus exitStatus) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();

            if(exitStatus != QProcess::NormalExit) {
                // qDebug() << tr("%1 (%2) did not exit normally.").arg(userInput).arg(path);
                _statusLabel->setStatus(PipelineStatus(
                    PipelineStatus::Error,
                    tr("%1 (%2) did not exit normally, the built-in FFmpeg library will be used.").arg(userInput).arg(path)));
                ffmpegPathValidated(false);
                process->deleteLater();
                return;
            }

            const QString output = process->readAllStandardOutput();
            if(exitCode == 0 && output.contains("FFmpeg version", Qt::CaseInsensitive)) {
                const QString versionString = output.section('\n', 0, 0).trimmed();
                // qDebug() << tr("'%1 (%2)' found:\n%3").arg(userInput).arg(path).arg(versionString);
                _statusLabel->setStatus(
                    PipelineStatus(PipelineStatus::Success, tr("%1 (%2) found:\n%3").arg(userInput).arg(path).arg(versionString)));
                ffmpegPathValidated(true);
                process->deleteLater();
                return;
            }
            else {
                // qDebug() << tr("'%1 (%2)' is not a valid ffmpeg executable.").arg(userInput).arg(path);
                _statusLabel->setStatus(PipelineStatus(
                    PipelineStatus::Error,
                    tr("%1 (%2) is not a valid FFmpeg executable, the built-in FFmpeg library will be used.").arg(userInput).arg(path)));
                ffmpegPathValidated(false);
                process->deleteLater();
                return;
            }
        });

    // Start the provided process
    process->start(path, QStringList() << "-version");
    timeoutTimer->start();
}

/******************************************************************************
 * Lets the page save all changed settings.
 ******************************************************************************/
void FFmpegSettingsPage::saveValues(QTabWidget* tabWidget)
{
    QSettings settings;
    qDebug() << VideoEncoder::FFMPEG_PATH_SETTING << _ffmpegPath->text().trimmed();
    qDebug() << VideoEncoder::FFMPEG_CODEC_SETTING << _ffmpegCodecName;
    qDebug() << VideoEncoder::FFMPEG_USE_EXT_SETTING << _externalffmpeg;
    qDebug() << VideoEncoder::FFMPEG_QUALITY_SETTING << _ffmpegQualityBox->currentData().toInt();
    settings.setValue(VideoEncoder::FFMPEG_PATH_SETTING, _ffmpegPath->text().trimmed());
    settings.setValue(VideoEncoder::FFMPEG_CODEC_SETTING, _ffmpegCodecName);
    settings.setValue(VideoEncoder::FFMPEG_USE_EXT_SETTING, _externalffmpeg);
    settings.setValue(VideoEncoder::FFMPEG_QUALITY_SETTING, _ffmpegQualityBox->currentData().toInt());
}

}  // namespace Ovito
