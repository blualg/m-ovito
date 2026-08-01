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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/denoise/ScoreBasedDenoisingModifier.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTextStream>
#include "ScoreBasedDenoisingModifierEditor.h"

namespace Ovito {

namespace {

class SummaryLabel : public QLabel
{
public:
    using QLabel::QLabel;

    bool hasHeightForWidth() const override
    {
        return wordWrap();
    }

    int heightForWidth(int width) const override
    {
        if(!wordWrap())
            return QLabel::heightForWidth(width);

        const QMargins margins = contentsMargins();
        const int horizontalPadding = margins.left() + margins.right() + frameWidth() * 2;
        const int verticalPadding = margins.top() + margins.bottom() + frameWidth() * 2;
        const int textWidth = std::max(1, width - horizontalPadding);
        const QRect bounds = fontMetrics().boundingRect(QRect(0, 0, textWidth, 0),
                                                        alignment() | Qt::TextWordWrap,
                                                        text());
        return bounds.height() + verticalPadding;
    }

    QSize sizeHint() const override
    {
        if(!wordWrap())
            return QLabel::sizeHint();
        const int widthHint = (width() > 0) ? width() : 360;
        return QSize(widthHint, heightForWidth(widthHint));
    }
};

QString quoteForCommandScript(QString value)
{
#if defined(Q_OS_WIN)
    value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(value);
#else
    value.replace(QStringLiteral("'"), QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(value);
#endif
}

QString scoreDenoisingEnvironmentDirectory()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(baseDir.isEmpty())
        baseDir = QDir::home().filePath(QStringLiteral(".m-ovito"));
    return QDir(baseDir).filePath(QStringLiteral("score-based-denoising-python"));
}

QString scoreDenoisingPythonExecutable(const QString& environmentDir)
{
#if defined(Q_OS_WIN)
    return QDir(environmentDir).filePath(QStringLiteral("Scripts/python.exe"));
#else
    return QDir(environmentDir).filePath(QStringLiteral("bin/python"));
#endif
}

QString tailText(const QString& text, int maxLines = 8)
{
    QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    if(lines.size() > maxLines)
        lines = lines.mid(lines.size() - maxLines);
    return lines.join(QStringLiteral("\n"));
}

QString torchWheelIndexUrl(const QString& runtimeKey)
{
    return QStringLiteral("https://download.pytorch.org/whl/%1").arg(runtimeKey.isEmpty() ? QStringLiteral("cpu") : runtimeKey);
}

void refreshWrappedLabelGeometry(QLabel* label)
{
    if(!label)
        return;

    label->updateGeometry();
    label->adjustSize();
    for(QWidget* widget = label->parentWidget(); widget; widget = widget->parentWidget()) {
        if(QLayout* layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        widget->updateGeometry();
    }
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(ScoreBasedDenoisingModifierEditor);
SET_OVITO_OBJECT_EDITOR(ScoreBasedDenoisingModifier, ScoreBasedDenoisingModifierEditor);

ScoreBasedDenoisingModifier* ScoreBasedDenoisingModifierEditor::modifier() const
{
    return static_object_cast<ScoreBasedDenoisingModifier>(editObject());
}

void ScoreBasedDenoisingModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Score-based denoising"), rolloutParams, "manual:particles.modifiers.score_based_denoising");

    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);

    auto* structureUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::structurePreset));
    structureUI->comboBox()->addItem(tr("None"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::NoDenoising)));
    structureUI->comboBox()->addItem(tr("FCC"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::FCC)));
    structureUI->comboBox()->addItem(tr("BCC"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::BCC)));
    structureUI->comboBox()->addItem(tr("HCP"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::HCP)));
    structureUI->comboBox()->addItem(tr("SiO2"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::SiO2)));
    structureUI->comboBox()->addItem(tr("Custom"),
                                     QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::Custom)));
    _structureCombo = structureUI->comboBox();
    grid->addWidget(new QLabel(tr("Structure/material:")), 0, 0);
    grid->addWidget(_structureCombo, 0, 1);

    IntegerParameterUI* stepsUI = createParamUI<IntegerParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::steps));
    grid->addWidget(stepsUI->label(), 1, 0);
    grid->addLayout(stepsUI->createFieldLayout(), 1, 1);

    FloatParameterUI* scaleUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::nearestNeighborDistance));
    grid->addWidget(scaleUI->label(), 2, 0);
    grid->addLayout(scaleUI->createFieldLayout(), 2, 1);

    StringParameterUI* modelPathUI = createParamUI<StringParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::modelPath));
    modelPathUI->lineEdit()->setPlaceholderText(tr("Optional for presets; required for Custom"));
    grid->addWidget(new QLabel(tr("Model path:")), 3, 0);
    grid->addWidget(modelPathUI->textBox(), 3, 1);

    StringParameterUI* pythonUI = createParamUI<StringParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::pythonExecutable));
    pythonUI->lineEdit()->setPlaceholderText(tr("python"));
    grid->addWidget(new QLabel(tr("Python executable:")), 4, 0);
    grid->addWidget(pythonUI->textBox(), 4, 1);

    layout->addLayout(grid);

    QGridLayout* installGrid = new QGridLayout();
    installGrid->setContentsMargins(0, 0, 0, 0);
    installGrid->setColumnStretch(1, 1);
    _runtimeInstallCombo = new QComboBox(rollout);
    _runtimeInstallCombo->addItem(tr("CPU"), QStringLiteral("cpu"));
    _runtimeInstallCombo->addItem(tr("CUDA 12.6"), QStringLiteral("cu126"));
    _runtimeInstallCombo->addItem(tr("CUDA 12.4"), QStringLiteral("cu124"));
    _runtimeInstallCombo->addItem(tr("CUDA 12.1"), QStringLiteral("cu121"));
    _runtimeInstallCombo->setToolTip(tr("Select which PyTorch runtime the installer should put into the denoising Python environment."));
    installGrid->addWidget(new QLabel(tr("Install runtime:")), 0, 0);
    installGrid->addWidget(_runtimeInstallCombo, 0, 1);
    layout->addLayout(installGrid);

    _installEnvironmentButton = new QPushButton(tr("Install/repair denoising Python environment"), rollout);
    _installEnvironmentButton->setMinimumHeight(24);
    _installEnvironmentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(_installEnvironmentButton);

    _installStatusLabel = new SummaryLabel(rollout);
    _installStatusLabel->setWordWrap(true);
    _installStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _installStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _installStatusLabel->setVisible(false);
    layout->addWidget(_installStatusLabel);

    QGridLayout* optionsGrid = new QGridLayout();
    optionsGrid->setContentsMargins(0, 0, 0, 0);
    optionsGrid->setColumnStretch(1, 1);

    auto* deviceUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::device));
    deviceUI->comboBox()->addItem(tr("CPU"),
                                  QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::Cpu)));
    deviceUI->comboBox()->addItem(tr("CUDA"),
                                  QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::Cuda)));
    deviceUI->comboBox()->addItem(tr("MPS"),
                                  QVariant::fromValue(static_cast<int>(ScoreBasedDenoisingModifier::Mps)));
    optionsGrid->addWidget(new QLabel(tr("Device:")), 0, 0);
    optionsGrid->addWidget(deviceUI->comboBox(), 0, 1);

    BooleanParameterUI* selectedUI = createParamUI<BooleanParameterUI>(
        PROPERTY_FIELD(ScoreBasedDenoisingModifier::onlySelected));
    optionsGrid->addWidget(selectedUI->checkBox(), 1, 0, 1, 2);
    layout->addLayout(optionsGrid);

    _scaleHintLabel = new SummaryLabel(rollout);
    _scaleHintLabel->setWordWrap(true);
    _scaleHintLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _scaleHintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_scaleHintLabel);

    _summaryLabel = new SummaryLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_summaryLabel);

    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Convergence:"), rollout));
    _convergencePlot = new DataTablePlotWidget();
    _convergencePlot->setMinimumHeight(240);
    _convergencePlot->setMaximumHeight(240);
    layout->addWidget(_convergencePlot);

    layout->addSpacing(28);
    layout->addStretch(1);
    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show convergence in data inspector"), ScoreBasedDenoisingModifier::ConvergenceTableIdentifier, 1));

    layout->addSpacing(6);
    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    statusWidget->setMinimumHeight(64);
    layout->addWidget(statusWidget);

    connect(structureUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ScoreBasedDenoisingModifierEditor::updateParameterVisibility);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &ScoreBasedDenoisingModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &ScoreBasedDenoisingModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsReplaced, this, &ScoreBasedDenoisingModifierEditor::updateParameterVisibility);
    connect(_installEnvironmentButton, &QPushButton::clicked,
            this, &ScoreBasedDenoisingModifierEditor::installOrRepairPythonEnvironment);

    updateParameterVisibility();
}

void ScoreBasedDenoisingModifierEditor::updateParameterVisibility()
{
    const int selectedMode = _structureCombo ? _structureCombo->currentData().toInt()
                                             : static_cast<int>(ScoreBasedDenoisingModifier::NoDenoising);
    if(!_scaleHintLabel)
        return;

    if(selectedMode == static_cast<int>(ScoreBasedDenoisingModifier::Custom)) {
        _scaleHintLabel->setText(tr("For Custom models, the nearest-neighbor distance field is used directly as the model scale and must be positive."));
    }
    else if(selectedMode == static_cast<int>(ScoreBasedDenoisingModifier::NoDenoising)) {
        _scaleHintLabel->setText(tr("Choose a preset or Custom model to run denoising."));
    }
    else {
        _scaleHintLabel->setText(tr("Set nearest-neighbor distance to 0 for automatic estimation. Presets use the default graphite denoiser model unless a model path is supplied."));
    }
    refreshWrappedLabelGeometry(_scaleHintLabel);
}

void ScoreBasedDenoisingModifierEditor::updatePlot()
{
    if(!_convergencePlot)
        return;

    handleExceptions([&]() {
        _convergencePlot->setTable(getPipelineOutput().getObjectBy<DataTable>(
            modificationNode(), ScoreBasedDenoisingModifier::ConvergenceTableIdentifier));
    });
}

void ScoreBasedDenoisingModifierEditor::updateSummary()
{
    if(!_summaryLabel)
        return;

    const PipelineFlowState& state = getPipelineOutput();
    const QVariant structure = state.getAttributeValue(QStringLiteral("ScoreDenoising.structure"));
    if(!structure.isValid()) {
        _summaryLabel->clear();
        refreshWrappedLabelGeometry(_summaryLabel);
        return;
    }

    QStringList lines;
    lines << tr("Structure: %1").arg(structure.toString());
    const QVariant steps = state.getAttributeValue(QStringLiteral("ScoreDenoising.steps"));
    if(steps.isValid())
        lines << tr("Steps: %1").arg(steps.toInt());
    const QVariant nnDistance = state.getAttributeValue(QStringLiteral("ScoreDenoising.nearest_neighbor_distance"));
    if(nnDistance.isValid())
        lines << tr("Nearest-neighbor distance: %1").arg(nnDistance.toDouble(), 0, 'g', 8);
    const QVariant modelScale = state.getAttributeValue(QStringLiteral("ScoreDenoising.model_scale"));
    if(modelScale.isValid())
        lines << tr("Model scale: %1").arg(modelScale.toDouble(), 0, 'g', 8);
    const QVariant written = state.getAttributeValue(QStringLiteral("ScoreDenoising.written_particle_count"));
    if(written.isValid())
        lines << tr("Updated particles: %1").arg(written.toLongLong());
    const QVariant finalConvergence = state.getAttributeValue(QStringLiteral("ScoreDenoising.final_convergence"));
    if(finalConvergence.isValid())
        lines << tr("Final convergence: %1").arg(finalConvergence.toDouble(), 0, 'g', 8);
    const QVariant modelPath = state.getAttributeValue(QStringLiteral("ScoreDenoising.model_path"));
    if(modelPath.isValid() && !modelPath.toString().isEmpty()) {
        const QString path = modelPath.toString();
        const QString modelName = QFileInfo(path).fileName();
        lines << tr("Model: %1").arg(modelName.isEmpty() ? path : modelName);
        _summaryLabel->setToolTip(path);
    }
    else {
        _summaryLabel->setToolTip(QString{});
    }

    _summaryLabel->setText(lines.join(QStringLiteral("\n")));
    refreshWrappedLabelGeometry(_summaryLabel);
}

void ScoreBasedDenoisingModifierEditor::installOrRepairPythonEnvironment()
{
    if(_installProcess)
        return;

    const QString runtimeKey = _runtimeInstallCombo ? _runtimeInstallCombo->currentData().toString() : QStringLiteral("cpu");
    const QString runtimeName = _runtimeInstallCombo ? _runtimeInstallCombo->currentText() : tr("CPU");
    const bool cudaRuntime = runtimeKey.startsWith(QStringLiteral("cu"));
    const QString torchIndexUrl = torchWheelIndexUrl(runtimeKey);

    QString basePython = QStringLiteral("python");
    if(const ScoreBasedDenoisingModifier* mod = modifier()) {
        const QString configuredPython = mod->pythonExecutable().trimmed();
        if(!configuredPython.isEmpty() && (configuredPython == QStringLiteral("python") || QFileInfo::exists(configuredPython)))
            basePython = configuredPython;
    }

    const QString environmentDir = scoreDenoisingEnvironmentDirectory();
    _installPythonPath = scoreDenoisingPythonExecutable(environmentDir);
    const QFileInfo environmentInfo(environmentDir);
    QDir parentDir(environmentInfo.absolutePath());
    if(!parentDir.mkpath(QStringLiteral("."))) {
        if(_installStatusLabel)
            _installStatusLabel->setText(tr("Could not create denoising environment folder: %1").arg(environmentInfo.absolutePath()));
        return;
    }
    if(!QDir().mkpath(environmentDir)) {
        if(_installStatusLabel)
            _installStatusLabel->setText(tr("Could not create denoising environment folder: %1").arg(environmentDir));
        return;
    }

#if defined(Q_OS_WIN)
    const QString scriptPath = parentDir.filePath(QStringLiteral("install_score_based_denoising_runtime.cmd"));
#else
    const QString scriptPath = parentDir.filePath(QStringLiteral("install_score_based_denoising_runtime.sh"));
#endif

    QFile scriptFile(scriptPath);
    if(!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if(_installStatusLabel)
            _installStatusLabel->setText(tr("Could not write installer script: %1").arg(scriptPath));
        return;
    }

    QTextStream script(&scriptFile);
    const QString torchPackageSpec = QStringLiteral("torch>=2.6,<=2.10");
    const QString verificationCode = cudaRuntime
        ? QStringLiteral("from graphite.nn.utils.e3nn_initial_embedding import InitialEmbedding; import torch, torch_geometric; print('torch', torch.__version__, 'cuda_runtime', torch.version.cuda, 'cuda_available', torch.cuda.is_available()); raise SystemExit(0 if torch.version.cuda else 'Selected CUDA runtime but installed CPU-only torch')")
        : QStringLiteral("from graphite.nn.utils.e3nn_initial_embedding import InitialEmbedding; import torch, torch_geometric; print('torch', torch.__version__, 'cuda_runtime', torch.version.cuda, 'cuda_available', torch.cuda.is_available())");
#if defined(Q_OS_WIN)
    script << "@echo off\n";
    script << "setlocal\n";
    script << quoteForCommandScript(basePython) << " -m venv " << quoteForCommandScript(environmentDir) << "\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade pip setuptools wheel\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade --force-reinstall "
           << quoteForCommandScript(torchPackageSpec) << " --index-url " << quoteForCommandScript(torchIndexUrl) << "\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade "
           << quoteForCommandScript(QStringLiteral("numpy")) << " "
           << quoteForCommandScript(QStringLiteral("scikit-learn")) << " "
           << quoteForCommandScript(QStringLiteral("e3nn==0.4.4")) << " "
           << quoteForCommandScript(QStringLiteral("torch-geometric<2.7")) << " "
           << quoteForCommandScript(QStringLiteral("graphite @ git+https://github.com/LLNL/graphite.git@b7b182d")) << "\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade --no-deps git+https://github.com/ovito-org/ScoreBasedDenoising.git\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
    script << quoteForCommandScript(_installPythonPath) << " -c " << quoteForCommandScript(verificationCode) << "\n";
    script << "if errorlevel 1 exit /b %ERRORLEVEL%\n";
#else
    script << "#!/bin/sh\n";
    script << "set -e\n";
    script << quoteForCommandScript(basePython) << " -m venv " << quoteForCommandScript(environmentDir) << "\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade pip setuptools wheel\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade --force-reinstall "
           << quoteForCommandScript(torchPackageSpec) << " --index-url " << quoteForCommandScript(torchIndexUrl) << "\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade "
           << quoteForCommandScript(QStringLiteral("numpy")) << " "
           << quoteForCommandScript(QStringLiteral("scikit-learn")) << " "
           << quoteForCommandScript(QStringLiteral("e3nn==0.4.4")) << " "
           << quoteForCommandScript(QStringLiteral("torch-geometric<2.7")) << " "
           << quoteForCommandScript(QStringLiteral("graphite @ git+https://github.com/LLNL/graphite.git@b7b182d")) << "\n";
    script << quoteForCommandScript(_installPythonPath) << " -m pip install --upgrade --no-deps git+https://github.com/ovito-org/ScoreBasedDenoising.git\n";
    script << quoteForCommandScript(_installPythonPath) << " -c " << quoteForCommandScript(verificationCode) << "\n";
#endif
    scriptFile.close();
#if !defined(Q_OS_WIN)
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif

    _installLog.clear();
    if(_installStatusLabel)
        _installStatusLabel->setVisible(true);
    if(_installStatusLabel)
        _installStatusLabel->setText(tr("Installing %1 denoising Python runtime. This can take several minutes the first time.\nPyTorch index: %2\nEnvironment: %3")
            .arg(runtimeName, torchIndexUrl, environmentDir));
    refreshWrappedLabelGeometry(_installStatusLabel);
    if(_installEnvironmentButton)
        _installEnvironmentButton->setEnabled(false);
    if(_runtimeInstallCombo)
        _runtimeInstallCombo->setEnabled(false);

    _installProcess = new QProcess(this);
    _installProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(_installProcess, &QProcess::readyReadStandardOutput, this, &ScoreBasedDenoisingModifierEditor::readInstallOutput);
    connect(_installProcess, &QProcess::readyReadStandardError, this, &ScoreBasedDenoisingModifierEditor::readInstallOutput);
    connect(_installProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                handleInstallFinished(exitCode, exitStatus == QProcess::NormalExit);
            });

#if defined(Q_OS_WIN)
    _installProcess->start(QStringLiteral("cmd.exe"), {QStringLiteral("/C"), scriptPath});
#else
    _installProcess->start(QStringLiteral("/bin/sh"), {scriptPath});
#endif
}

void ScoreBasedDenoisingModifierEditor::readInstallOutput()
{
    if(!_installProcess)
        return;

    _installLog += QString::fromLocal8Bit(_installProcess->readAllStandardOutput());
    _installLog += QString::fromLocal8Bit(_installProcess->readAllStandardError());
    if(_installStatusLabel) {
        _installStatusLabel->setVisible(true);
        const QString tail = tailText(_installLog);
        _installStatusLabel->setText(tail.isEmpty()
            ? tr("Installing ScoreBasedDenoising/graphite runtime...")
            : tr("Installing ScoreBasedDenoising/graphite runtime...\n%1").arg(tail));
        refreshWrappedLabelGeometry(_installStatusLabel);
    }
}

void ScoreBasedDenoisingModifierEditor::handleInstallFinished(int exitCode, bool normalExit)
{
    readInstallOutput();

    QProcess* process = _installProcess;
    _installProcess = nullptr;
    if(process)
        process->deleteLater();
    if(_installEnvironmentButton)
        _installEnvironmentButton->setEnabled(true);
    if(_runtimeInstallCombo)
        _runtimeInstallCombo->setEnabled(true);

    if(normalExit && exitCode == 0 && QFileInfo::exists(_installPythonPath)) {
        if(ScoreBasedDenoisingModifier* mod = modifier()) {
            const QString pythonPath = _installPythonPath;
            performTransaction(tr("Set denoising Python executable"), [mod, pythonPath]() {
                mod->setPythonExecutable(pythonPath);
            });
        }
        if(_installStatusLabel)
            _installStatusLabel->setVisible(true);
        if(_installStatusLabel)
            _installStatusLabel->setText(tr("Denoising Python environment is ready.\nPython executable: %1").arg(_installPythonPath));
        refreshWrappedLabelGeometry(_installStatusLabel);
    }
    else {
        const QString tail = tailText(_installLog, 12);
        if(_installStatusLabel)
            _installStatusLabel->setVisible(true);
        if(_installStatusLabel)
            _installStatusLabel->setText(tr("Denoising Python environment installation failed with exit code %1.%2")
                .arg(exitCode)
                .arg(tail.isEmpty() ? QString() : QStringLiteral("\n") + tail));
        refreshWrappedLabelGeometry(_installStatusLabel);
    }
}

}  // namespace Ovito
