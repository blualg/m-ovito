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
    tabWidget->addTab(page, tr("Video Encoding"));
    auto* layout = new QVBoxLayout(page);
    int row = 0;

    // Load current settings
    QSettings settings;
    _ffmpegCodecName = settings.value(VideoEncoder::FFMPEG_CODEC_SETTING, {}).value<QByteArray>();
    const QString& ffmpegPath = settings.value(VideoEncoder::FFMPEG_PATH_SETTING, {}).toString();
    _externalFFmpeg = settings.value(VideoEncoder::FFMPEG_USE_EXT_SETTING, false).toBool();
    const int ffmpegQuality = settings.value(VideoEncoder::FFMPEG_QUALITY_SETTING, (int)VideoEncoder::Quality::Medium).value<int>();

    {
        auto* exeGroupBox = new QGroupBox(tr("External encoder (optional)"), page);
        layout->addWidget(exeGroupBox);
        auto* layout1 = new QGridLayout(exeGroupBox);

        auto* infoLabel =
            new QLabel(tr("Please provide the path to the FFmpeg executable on your computer or leave it empty to use the "
                          "built-in encoder of OVITO, which does not support all high-quality codecs. You can obtain FFmpeg from <a "
                          "href=\"https://ffmpeg.org/download.html\">ffmpeg.org/download</a>."));
        infoLabel->setWordWrap(true);
        infoLabel->setOpenExternalLinks(true);
        layout1->addWidget(infoLabel, row++, 0, 1, 3);

        layout1->addWidget(new QLabel(tr("FFmpeg executable:")), row, 0);
        _ffmpegPath = new EnterLineEdit(page);
        _ffmpegPath->setText(ffmpegPath);
        layout1->addWidget(_ffmpegPath, row, 1);

        // Select the FFmpeg executable using a file selection dialog.
        auto* selectExecutablePathButton = new QPushButton(QStringLiteral("..."));
        connect(selectExecutablePathButton, &QPushButton::clicked, this, [this, page]() {
            TaskManager::setNativeDialogActive(true);
            const QString path = QFileDialog::getOpenFileName(page, tr("Select FFmpeg Executable"), _ffmpegPath->text().trimmed());
            TaskManager::setNativeDialogActive(false);
            if(!path.isEmpty()) {
                _ffmpegPath->setText(path);
                validateFfmpegPath();
            }
        });
        selectExecutablePathButton->setToolTip(tr("Pick FFmpeg executable..."));
        layout1->addWidget(selectExecutablePathButton, row++, 2);

        // Validate ffmpeg on when path is changed
        connect(_ffmpegPath, &EnterLineEdit::editingFinished, this, &FFmpegSettingsPage::validateFfmpegPath);
        connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, [this](bool valid) { _externalFFmpeg = valid; });

        // Status label.
        _statusLabel = new StatusWidget(page);
        _statusLabel->setWordWrap(true);
        layout1->addWidget(_statusLabel, row++, 0, 1, 3);
    }

    {
        // Setup codecs:
        auto* codecSettingsGroupBox = new QGroupBox(tr("Encoding settings"), page);
        layout->addWidget(codecSettingsGroupBox);
        auto* layout1 = new QGridLayout(codecSettingsGroupBox);
        // Have the combo boxes (col 1) take up as much space as possible.
        layout1->setColumnStretch(0, 0);
        layout1->setColumnStretch(1, 1);
        int row = 0;

        _ffmpegQualityBox = new QComboBox(page);
        QMetaEnum metaEnum = QMetaEnum::fromType<VideoEncoder::Quality>();
        for(int i = 0; i < metaEnum.keyCount(); ++i) {
            _ffmpegQualityBox->addItem(metaEnum.key(i), metaEnum.value(i));
        }
        _ffmpegQualityBox->setCurrentIndex(_ffmpegQualityBox->findData(ffmpegQuality));

        layout1->addWidget(new QLabel(tr("Quality preset:")), row, 0);
        layout1->addWidget(_ffmpegQualityBox, row++, 1, 1, 2);

        _ffmpegCodec = new QComboBox(page);
        layout1->addWidget(new QLabel(tr("Codec:")), row, 0);
        layout1->addWidget(_ffmpegCodec, row++, 1, 1, 2);
        // Refresh combobox when ffmpeg path is changed
        connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, &FFmpegSettingsPage::refreshCodecCombobox);

        // Codec / format warning
        auto* codecInfoLabel = new QLabel(page);
        codecInfoLabel->setText(tr("Note: The H.265 codec does not work with .avi and .mov file formats; fallback is H.264."));
        codecInfoLabel->setWordWrap(true);
        codecInfoLabel->setVisible(false);
        connect(this, &FFmpegSettingsPage::ffmpegPathValidated, this, [this, codecInfoLabel](bool valid) {
            codecInfoLabel->setVisible(valid && _ffmpegCodecName == QByteArrayLiteral("libx265"));
        });

        // Keep space for the label
        QSizePolicy sp = codecInfoLabel->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        codecInfoLabel->setSizePolicy(sp);

        layout1->addWidget(codecInfoLabel, row++, 1, 1, 2);

        // Store selection
        connect(_ffmpegCodec, &QComboBox::activated, this, [this, codecInfoLabel](int index) {
            const VideoEncoder::Backend backend = _externalFFmpeg ? VideoEncoder::Backend::EXTERNAL : VideoEncoder::Backend::INTERNAL;
            const QString& path = _externalFFmpeg ? _ffmpegPath->text().trimmed() : QString();
            const QList<const VideoEncoder::CandidateCodec*>& codecs = VideoEncoder::supportedCodecs(backend, path);
            if(index < 0 || index >= codecs.size()) return;
            const VideoEncoder::CandidateCodec* codec = codecs[index];
            _ffmpegCodecName = codec->libName;
            codecInfoLabel->setVisible(_ffmpegCodecName == QByteArrayLiteral("libx265"));
        });
    }

    layout->addStretch();

    // Validate path on startup
    // Use a single shot timer to avoid blocking during opening of the application settings dialog.
    QTimer::singleShot(0, this, &FFmpegSettingsPage::validateFfmpegPath);
}

void FFmpegSettingsPage::refreshCodecCombobox()
{
    // Clear box
    _ffmpegCodec->clear();

    // Get backend and path
    VideoEncoder::Backend backend = _externalFFmpeg ? VideoEncoder::Backend::EXTERNAL : VideoEncoder::Backend::INTERNAL;
    const QString& path = _externalFFmpeg ? _ffmpegPath->text().trimmed() : QString();
    if(path.isEmpty()) {
        backend = VideoEncoder::Backend::INTERNAL;
    }

    // Get codec list
    const QList<const VideoEncoder::CandidateCodec*>& codecs = VideoEncoder::supportedCodecs(backend, path);
    if(codecs.empty()) {
        _statusLabel->setStatus(PipelineStatus(
            PipelineStatus::Error, tr("External FFmpeg does not support the required codecs; the built-in encoder will be used.")));
        _externalFFmpeg = false;
    }

    // Tracks whether the currently set codec was found
    bool codecFound = false;
    // Fill combobox from codecs and select current codec
    for(const auto [i, codec] : Ovito::enumerate(codecs)) {
        _ffmpegCodec->addItem(codec->longName);
        if(codec->libName == _ffmpegCodecName) {
            _ffmpegCodec->setCurrentIndex((int)i);
            codecFound = true;
        }
    }

    // Disable combobox if no codecs are available.
    _ffmpegCodec->setDisabled(codecs.size() < 2);
    OVITO_ASSERT(codecs.size() > 0);
    if(!codecFound) {
        _ffmpegCodec->setCurrentIndex(0);
        _ffmpegCodecName = codecs[0]->libName;
    }
}

/******************************************************************************
 * Validates the user provided FFmpeg executable.
 * Falls back to the built-in FFmpeg library if the path or executable is invalid.
 ******************************************************************************/
void FFmpegSettingsPage::validateFfmpegPath()
{
    VideoEncoder::clearCodecs();
    // User input
    const QString userInput = QDir::fromNativeSeparators(_ffmpegPath->text().trimmed());

    // Check path is empty
    if(userInput.isEmpty()) {
        _statusLabel->setStatus(PipelineStatus(tr("No FFmpeg executable specified; the built-in video encoder of OVITO will be used.")));
        Q_EMIT ffmpegPathValidated(false);
        return;
    }

    // Resolve the path
    const QString path = QStandardPaths::findExecutable(userInput);

    // Check path is empty
    if(path.isEmpty()) {
        _statusLabel->setStatus(PipelineStatus(tr("%1 not found; the built-in video encoder of OVITO will be used.").arg(userInput)));
        Q_EMIT ffmpegPathValidated(false);
        return;
    }

    // Check if file exists
    QFileInfo fileInfo(path);
    if(!fileInfo.exists()) {
        _statusLabel->setStatus(
            PipelineStatus(PipelineStatus::Error, tr("%1 does not exist. The built-in video encoder will be used.").arg(path)));
        Q_EMIT ffmpegPathValidated(false);
        return;
    }

    // Check if file is executable.
    if(!fileInfo.isExecutable()) {
        _statusLabel->setStatus(
            PipelineStatus(PipelineStatus::Error, tr("%1 is not executable. The built-in video encoder will be used.").arg(path)));
        Q_EMIT ffmpegPathValidated(false);
        return;
    }

    // Match the name before attempting to run the executable.
    if(!(path.endsWith("ffmpeg", Qt::CaseInsensitive) || path.endsWith("ffmpeg.exe", Qt::CaseInsensitive))) {
        _statusLabel->setStatus(
            PipelineStatus(PipelineStatus::Error,
                           tr("Path needs to end with 'ffmpeg' or 'ffmpeg.exe' to be valid. The built-in video encoder will be used.")));
        Q_EMIT ffmpegPathValidated(false);
        return;
    }

    _statusLabel->setStatus(PipelineStatus(tr("Checking FFmpeg executable...")));

    // Background process used for checking if ffmpeg is executable
    QProcess* process = new QProcess(this);

    // Setup timeout timer (ms)
    constexpr int timeout = 3 * 1000;
    QTimer::singleShot(timeout, process, &QProcess::kill);

    // Handle process finished
    connect(
        process, &QProcess::finished, this, [process, userInput, path, this](int exitCode, QProcess::ExitStatus exitStatus) {
            if(exitStatus != QProcess::NormalExit) {
                _statusLabel->setStatus(PipelineStatus(
                    PipelineStatus::Error, tr("%1 did not exit normally. The built-in video encoder will be used.").arg(path)));
                Q_EMIT ffmpegPathValidated(false);
                process->deleteLater();
                return;
            }

            const QByteArray output = process->readAllStandardOutput();
            if(exitCode == 0 && output.contains(QByteArrayLiteral("ffmpeg version"))) {
                const qsizetype newlinePos = output.indexOf('\n');
                const QByteArray firstLine = (newlinePos != -1) ? output.left(newlinePos).trimmed() : output.trimmed();
                const QString versionString = QString::fromUtf8(firstLine);
                _statusLabel->setStatus(
                    PipelineStatus(PipelineStatus::Success, tr("Found %1:\n%2").arg(QDir::toNativeSeparators(path)).arg(versionString)));
                Q_EMIT ffmpegPathValidated(true);
                process->deleteLater();
                return;
            }
            else {
                _statusLabel->setStatus(PipelineStatus(
                    PipelineStatus::Error, tr("%1 is not a valid FFmpeg executable. The built-in video encoder will be used.").arg(path)));
                Q_EMIT ffmpegPathValidated(false);
                process->deleteLater();
                return;
            }
        });

    // Start the provided process
    process->start(path, QStringList() << "-version");
}

/******************************************************************************
 * Lets the page save all changed settings.
 ******************************************************************************/
void FFmpegSettingsPage::saveValues(QTabWidget* tabWidget)
{
    QSettings settings;
    settings.setValue(VideoEncoder::FFMPEG_PATH_SETTING, _ffmpegPath->text().trimmed());
    settings.setValue(VideoEncoder::FFMPEG_CODEC_SETTING, _ffmpegCodecName);
    settings.setValue(VideoEncoder::FFMPEG_USE_EXT_SETTING, _externalFFmpeg);
    settings.setValue(VideoEncoder::FFMPEG_QUALITY_SETTING, _ffmpegQualityBox->currentData().toInt());
}

}  // namespace Ovito
