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
#include "RenderSettingsPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RenderSettingsPage);

/******************************************************************************
 * Creates the widget that contains the plugin specific setting controls.
 ******************************************************************************/
void RenderSettingsPage::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, tr("Render Settings"));
    QVBoxLayout* layout1 = new QVBoxLayout(page);

    // Load default from settings
    QSettings settings;
    _ffmpegCodecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    const QString& ffmpegPath = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, {}).toString();
    _externalffmpeg = settings.value(VideoEncoder::FFMPEG_USE_EXT_SETTING, false).toBool();
    int ffmpegQuality = settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    // Group "ffmpeg":
    auto* ffmpegGroupBox = new QGroupBox(tr("ffmpeg options"), page);
    layout1->addWidget(ffmpegGroupBox);
    QGridLayout* innerLayout = new QGridLayout(ffmpegGroupBox);

    innerLayout->addWidget(new QLabel(tr("Path to the ffmpeg executable:")), 0, 0);

    int row = 0;
    _ffmpegPath = new EnterLineEdit(ffmpegGroupBox);
    innerLayout->addWidget(_ffmpegPath, row++, 1);
    // Load default from settings
    _ffmpegPath->setText(ffmpegPath);
    // Validate ffmpeg on when path is changed
    connect(_ffmpegPath, &EnterLineEdit::editingFinished, this, &RenderSettingsPage::validateFfmpegPath);
    connect(this, &RenderSettingsPage::ffmpegPathValidated, this, [this](bool valid) { _externalffmpeg = valid; });

    // Status label.
    _statusLabel = new StatusWidget(ffmpegGroupBox);
    _statusLabel->setWordWrap(true);
    innerLayout->addWidget(_statusLabel, row++, 0, 1, 2);

    // Setup codecs:
    auto* codecGroupBox = new QGroupBox(tr("Codecs"), page);
    layout1->addWidget(codecGroupBox);
    innerLayout = new QGridLayout(codecGroupBox);

    _ffmpegCodec = new QComboBox(codecGroupBox);
    innerLayout->addWidget(_ffmpegCodec);
    // Refresh combobox when ffmpeg path is changed
    connect(this, &RenderSettingsPage::ffmpegPathValidated, this, &RenderSettingsPage::refreshCodecCombobox);

    auto* statusWidget = new StatusWidget(codecGroupBox);
    innerLayout->addWidget(statusWidget);
    // Store selection
    connect(_ffmpegCodec, &QComboBox::currentIndexChanged, this, [this, statusWidget](int index) {
        const VideoEncoder::Backend backend = _externalffmpeg ? VideoEncoder::Backend::EXTERN : VideoEncoder::Backend::OVITO;
        const QList<const VideoEncoder::CandidateCodec*>& codecs = VideoEncoder::supportedCodecs(backend);
        if(index < 0 || index > codecs.size()) return;
        const VideoEncoder::CandidateCodec* codec = codecs[index];
        _ffmpegCodecName = QByteArray::fromRawData(codec->name.data(), codec->name.size());
        if(codec->name == "hevc" || codec->name == "av1") {
            statusWidget->setStatus(
                PipelineStatus(tr(".avi and .mov files do not support 'H.265' or 'AV1' codecs. Please use .mkv or .mp4. Otherwise, a "
                                  "fallback codec (H.264) will be used.")));
        }
        else {
            statusWidget->clearStatus();
        }
    });

    row = 0;
    auto* qualityGroupBox = new QGroupBox(tr("Quality"), page);

    layout1->addWidget(qualityGroupBox);
    innerLayout = new QGridLayout(qualityGroupBox);

    _ffmpegQualityBox = new QComboBox(qualityGroupBox);
    QMetaEnum metaEnum = QMetaEnum::fromType<VideoEncoder::Quality>();
    for(int i = 0; i < metaEnum.keyCount(); ++i) {
        _ffmpegQualityBox->addItem(metaEnum.key(i), metaEnum.value(i));
    }
    _ffmpegQualityBox->setCurrentIndex(ffmpegQuality);

    innerLayout->addWidget(new QLabel(tr("Quality preset:")), row, 0);
    innerLayout->addWidget(_ffmpegQualityBox, row, 1);

    layout1->addStretch();

    // Validate path on startup
    validateFfmpegPath();
}

void RenderSettingsPage::refreshCodecCombobox()
{
    _ffmpegCodec->clear();
    const VideoEncoder::Backend backend = _externalffmpeg ? VideoEncoder::Backend::EXTERN : VideoEncoder::Backend::OVITO;
    for(const auto [i, codec] : Ovito::enumerate(VideoEncoder::supportedCodecs(backend))) {
        const QByteArray codecShortName = QByteArray::fromRawData(codec->name.data(), codec->name.size());
        _ffmpegCodec->addItem(QByteArray::fromRawData(codec->longName.data(), codec->longName.size()));
        if(codecShortName == _ffmpegCodecName) {
            _ffmpegCodec->setCurrentIndex((int)i);
        }
    }
}

void RenderSettingsPage::validateFfmpegPath()
{
    // User input
    const QString userInput = _ffmpegPath->text().trimmed();
    // qDebug() << "userInput" << userInput;

    // Check path is empty
    if(userInput.isEmpty()) {
        qDebug() << "ffmpeg path is empty, the built-in ffmpeg library will be used.";
        _statusLabel->setStatus(PipelineStatus(tr("ffmpeg path is empty, the built-in ffmpeg library will be used.")));
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
            PipelineStatus::Error, tr("'%1 (%2)' does not exist, the built-in ffmpeg library will be used.").arg(userInput).arg(path)));
        ffmpegPathValidated(false);
        return;
    }

    // Check if file is executable.
    if(!fileInfo.isExecutable()) {
        // qDebug() << tr("'%1 (%2)' is not executable.").arg(userInput).arg(path);
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error, tr("%1 (%2) is not executable, the built-in ffmpeg library will be used.").arg(userInput).arg(path)));
        ffmpegPathValidated(false);
        return;
    }

    // Match the name before attempting to run the executable.
    if(!(path.endsWith("ffmpeg", Qt::CaseInsensitive) || path.endsWith("ffmpeg.exe", Qt::CaseInsensitive))) {
        // qDebug() << tr("Path needs to end with 'ffmpeg' or 'ffmpeg.exe' to be valid, the built-in ffmpeg library will be used.");
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error,
            tr("Provided path needs to end with 'ffmpeg' or 'ffmpeg.exe' to be valid, the built-in ffmpeg library will be used.")));
        ffmpegPathValidated(false);
        return;
    }

    _statusLabel->setStatus(PipelineStatus(tr("Checking ffmpeg executable...")));

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
                           tr("Timeout while checking: %1 (%2), the built-in ffmpeg library will be used.").arg(userInput).arg(path)));
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
                    tr("%1 (%2) did not exit normally, the built-in ffmpeg library will be used..").arg(userInput).arg(path)));
                ffmpegPathValidated(false);
                process->deleteLater();
                return;
            }

            const QString output = process->readAllStandardOutput();
            if(exitCode == 0 && output.contains("ffmpeg version", Qt::CaseInsensitive)) {
                const QString versionString = output.section('\n', 0, 0).trimmed();
                // qDebug() << tr("'%1 (%2)' found:\n%3").arg(userInput).arg(path).arg(versionString);
                _statusLabel->setStatus(
                    PipelineStatus(PipelineStatus::Success, tr("'%1 (%2)' found:\n%3").arg(userInput).arg(path).arg(versionString)));
                ffmpegPathValidated(true);
                process->deleteLater();
                return;
            }
            else {
                // qDebug() << tr("'%1 (%2)' is not a valid ffmpeg executable.").arg(userInput).arg(path);
                _statusLabel->setStatus(PipelineStatus(
                    PipelineStatus::Error,
                    tr("'%1 (%2)' is not a valid ffmpeg executable, the built-in ffmpeg library will be used..").arg(userInput).arg(path)));
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
void RenderSettingsPage::saveValues(QTabWidget* tabWidget)
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
