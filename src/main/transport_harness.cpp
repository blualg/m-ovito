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
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/particles/Particles.h>
#include <ovito/particles/modifier/analysis/transport/TransportModifier.h>
#include <ovito/stdobj/table/DataTable.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace Ovito;

struct HarnessConfig {
    QString dataDir;
    QString outputPath;
    double temperature = 1300.0;
    double deltaT = 1.0;
    bool computeMSD = true;
    bool computeVACF = true;
    bool computeConductivity = true;
    bool computeDistinctIonCorrelation = false;
    bool computeStronglyCorrelatedPairs = false;
    TransportModifier::StrongPairSamplingMode strongPairSamplingMode = TransportModifier::RandomPairSampling;
    int strongPairSampleCount = 1176;
    int strongPairFrameStep = 1;
    QString strongPairThresholds = QStringLiteral("0.75, 0.80, 0.85");
    int strongPairRandomSeed = 12345;
    bool strongPairDiscreteLagPoints = false;
    QString strongPairLagPoints = QStringLiteral("5, 50, 200, 500");
    int strongPairResampleCount = 8;
    bool computePerType = true;
};

QJsonValue toJsonValue(const QVariant& value)
{
    if(!value.isValid())
        return QJsonValue();

    if(value.metaType().id() == QMetaType::Double || value.canConvert<double>()) {
        const double number = value.toDouble();
        if(std::isfinite(number))
            return QJsonValue(number);
        return QJsonValue(QString::fromLatin1(std::isnan(number) ? "nan" : (number > 0 ? "inf" : "-inf")));
    }

    if(value.metaType().id() == QMetaType::Int || value.metaType().id() == QMetaType::LongLong)
        return QJsonValue(static_cast<qint64>(value.toLongLong()));

    if(value.metaType().id() == QMetaType::UInt || value.metaType().id() == QMetaType::ULongLong)
        return QJsonValue(static_cast<qint64>(value.toULongLong()));

    if(value.metaType().id() == QMetaType::Bool)
        return QJsonValue(value.toBool());

    return QJsonValue(value.toString());
}

void printUsage(const char* programName)
{
    std::cerr
        << "Usage: " << programName << " --data-dir <directory> [--output <file>] [--temperature <K>] [--dt <value>]\n"
        << "       [--msd on|off] [--vacf on|off] [--conductivity on|off] [--distinct-ion-correlation on|off] [--strong-pairs on|off] [--strong-pair-k <count>]\n"
        << "       [--strong-pair-mode deterministic|random] [--strong-pair-seed <int>] [--strong-pair-discrete on|off] [--strong-pair-lags <list>] [--strong-pair-resamples <count>]\n"
        << "       [--strong-pair-step <frames>] [--strong-pair-thresholds <list>] [--per-type on|off]\n"
        << "Expected files inside <directory>: log.lammps, mol.data, mol.lammpstrj\n";
}

bool parseOnOffOption(const QString& optionName, const QString& value)
{
    if(value.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0)
        return true;
    if(value.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0)
        return false;
    throw std::runtime_error(QStringLiteral("Invalid value for %1: expected 'on' or 'off'").arg(optionName).toStdString());
}

HarnessConfig parseArguments(int argc, char** argv)
{
    HarnessConfig config;

    for(int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        auto requireValue = [&](const char* optionName) -> QString {
            if(i + 1 >= argc)
                throw std::runtime_error(QStringLiteral("Missing value for %1").arg(QString::fromLatin1(optionName)).toStdString());
            return QString::fromLocal8Bit(argv[++i]);
        };

        if(arg == QStringLiteral("--data-dir")) {
            config.dataDir = requireValue("--data-dir");
        }
        else if(arg == QStringLiteral("--output")) {
            config.outputPath = requireValue("--output");
        }
        else if(arg == QStringLiteral("--temperature")) {
            bool ok = false;
            config.temperature = requireValue("--temperature").toDouble(&ok);
            if(!ok)
                throw std::runtime_error("Invalid numeric value for --temperature");
        }
        else if(arg == QStringLiteral("--dt")) {
            bool ok = false;
            config.deltaT = requireValue("--dt").toDouble(&ok);
            if(!ok)
                throw std::runtime_error("Invalid numeric value for --dt");
        }
        else if(arg == QStringLiteral("--msd")) {
            config.computeMSD = parseOnOffOption(arg, requireValue("--msd"));
        }
        else if(arg == QStringLiteral("--vacf")) {
            config.computeVACF = parseOnOffOption(arg, requireValue("--vacf"));
        }
        else if(arg == QStringLiteral("--conductivity")) {
            config.computeConductivity = parseOnOffOption(arg, requireValue("--conductivity"));
        }
        else if(arg == QStringLiteral("--distinct-ion-correlation")) {
            config.computeDistinctIonCorrelation = parseOnOffOption(arg, requireValue("--distinct-ion-correlation"));
        }
        else if(arg == QStringLiteral("--strong-pairs")) {
            config.computeStronglyCorrelatedPairs = parseOnOffOption(arg, requireValue("--strong-pairs"));
        }
        else if(arg == QStringLiteral("--strong-pair-k")) {
            bool ok = false;
            config.strongPairSampleCount = requireValue("--strong-pair-k").toInt(&ok);
            if(!ok || config.strongPairSampleCount < 0)
                throw std::runtime_error("Invalid integer value for --strong-pair-k");
        }
        else if(arg == QStringLiteral("--strong-pair-mode")) {
            const QString value = requireValue("--strong-pair-mode");
            if(value.compare(QStringLiteral("deterministic"), Qt::CaseInsensitive) == 0)
                config.strongPairSamplingMode = TransportModifier::DeterministicPairSampling;
            else if(value.compare(QStringLiteral("random"), Qt::CaseInsensitive) == 0)
                config.strongPairSamplingMode = TransportModifier::RandomPairSampling;
            else
                throw std::runtime_error("Invalid value for --strong-pair-mode");
        }
        else if(arg == QStringLiteral("--strong-pair-seed")) {
            bool ok = false;
            config.strongPairRandomSeed = requireValue("--strong-pair-seed").toInt(&ok);
            if(!ok || config.strongPairRandomSeed < 0)
                throw std::runtime_error("Invalid integer value for --strong-pair-seed");
        }
        else if(arg == QStringLiteral("--strong-pair-discrete")) {
            config.strongPairDiscreteLagPoints = parseOnOffOption(arg, requireValue("--strong-pair-discrete"));
        }
        else if(arg == QStringLiteral("--strong-pair-lags")) {
            config.strongPairLagPoints = requireValue("--strong-pair-lags");
        }
        else if(arg == QStringLiteral("--strong-pair-resamples")) {
            bool ok = false;
            config.strongPairResampleCount = requireValue("--strong-pair-resamples").toInt(&ok);
            if(!ok || config.strongPairResampleCount < 1)
                throw std::runtime_error("Invalid integer value for --strong-pair-resamples");
        }
        else if(arg == QStringLiteral("--strong-pair-step")) {
            bool ok = false;
            config.strongPairFrameStep = requireValue("--strong-pair-step").toInt(&ok);
            if(!ok || config.strongPairFrameStep < 1)
                throw std::runtime_error("Invalid integer value for --strong-pair-step");
        }
        else if(arg == QStringLiteral("--strong-pair-thresholds")) {
            config.strongPairThresholds = requireValue("--strong-pair-thresholds");
        }
        else if(arg == QStringLiteral("--per-type")) {
            config.computePerType = parseOnOffOption(arg, requireValue("--per-type"));
        }
        else if(arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            printUsage(argv[0]);
            std::exit(0);
        }
        else {
            throw std::runtime_error(QStringLiteral("Unknown option: %1").arg(arg).toStdString());
        }
    }

    if(config.dataDir.isEmpty())
        throw std::runtime_error("Missing required --data-dir argument");

    QDir dir(config.dataDir);
    if(!dir.exists())
        throw std::runtime_error(QStringLiteral("Data directory does not exist: %1").arg(config.dataDir).toStdString());

    config.dataDir = dir.absolutePath();

    if(config.outputPath.isEmpty())
        config.outputPath = dir.filePath(QStringLiteral("ovito_transport_summary.json"));

    return config;
}

MainWindow* findMainWindow()
{
    MainWindow* mainWindow = nullptr;
    MainWindow::visitMainWindows([&](MainWindow* candidate) {
        if(!mainWindow)
            mainWindow = candidate;
    });
    return mainWindow;
}

OORef<Pipeline> importLammpsDataset(Scene* scene, const QString& dataDir)
{
    const QString dataFile = QDir(dataDir).filePath(QStringLiteral("mol.data"));
    const QString trajectoryFile = QDir(dataDir).filePath(QStringLiteral("mol.lammpstrj"));

    if(!QFileInfo::exists(trajectoryFile))
        throw Exception(QStringLiteral("Required input file not found: %1").arg(trajectoryFile));

    std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters;
    const std::vector<QString> importPaths = QFileInfo::exists(dataFile)
        ? std::vector<QString>{dataFile, trajectoryFile}
        : std::vector<QString>{trajectoryFile};

    for(const QString& path : importPaths) {
        const QUrl url = QUrl::fromLocalFile(path);
        OORef<FileImporter> importer = FileImporter::autodetectFileFormat(url).blockForResult();
        if(!importer)
            throw Exception(QStringLiteral("Could not auto-detect an importer for %1").arg(path));

        if(path.endsWith(QStringLiteral(".lammpstrj"), Qt::CaseInsensitive)) {
            if(FileSourceImporter* fileSourceImporter = dynamic_object_cast<FileSourceImporter>(importer.get())) {
                fileSourceImporter->setMultiTimestepFile(true);
            }
        }

        urlImporters.emplace_back(url, std::move(importer));
    }

    std::stable_sort(urlImporters.begin(), urlImporters.end(), [](const auto& a, const auto& b) {
        const int pa = a.second->importerPriority();
        const int pb = b.second->importerPriority();
        if(pa > pb) return true;
        if(pa < pb) return false;
        return a.second->getOOClass().name() < b.second->getOOClass().name();
    });

    OORef<FileImporter> importer = urlImporters.front().second;
    OORef<Pipeline> pipeline = importer->importFileSet(scene, std::move(urlImporters),
        FileImporter::ResetScene, true, FileImporter::ImportAsTrajectory).blockForResult();

    if(!pipeline)
        throw Exception(QStringLiteral("Import did not create a pipeline."));

    return pipeline;
}

QJsonObject collectSummary(const DataCollection* collection)
{
    static const char* keys[] = {
        "Transport.sampled_particle_count",
        "Transport.sampled_frame_count",
        "Transport.dimensionality",
        "Transport.pylat_compatibility_enabled",
        "Transport.pylat_msd_fit_start_lag",
        "Transport.pylat_gk_fit_start_lag",
        "Transport.pylat_gk_fit_end_lag",
        "Transport.D_MSD",
        "Transport.D_MSD_SI",
        "Transport.D_VACF",
        "Transport.D_VACF_SI",
        "Transport.sigma_green_kubo_vavg_raw",
        "Transport.sigma_green_kubo_vavg",
        "Transport.sigma_correlated_einstein_vavg_raw",
        "Transport.sigma_correlated_einstein_vavg",
        "Transport.sigma_nernst_einstein_vavg_raw",
        "Transport.sigma_nernst_einstein_vavg",
        "Transport.haven_ratio_vavg"
    };

    QJsonObject summary;
    for(const char* key : keys) {
        const QVariant value = collection->getAttributeValue(QString::fromLatin1(key));
        if(value.isValid())
            summary.insert(QString::fromLatin1(key), toJsonValue(value));
    }
    return summary;
}

QJsonObject collectLineTable(const DataCollection* collection, const QStringView identifier)
{
    const DataTable* table = static_object_cast<DataTable>(collection->getLeafObject(DataTable::OOClass(), identifier));
    if(!table || !table->y())
        return {};

    ConstPropertyPtr x = table->getXValues();
    if(!x)
        return {};

    BufferReadAccessAndRef<FloatType> xAcc(x);
    BufferReadAccessAndRef<FloatType*> yAcc(table->y());

    QJsonArray time;
    std::vector<QJsonArray> componentArrays(yAcc.componentCount());
    const QStringList componentNames = table->y()->componentNames();

    for(size_t row = 0; row < yAcc.size(); ++row) {
        time.append(toJsonValue(QVariant::fromValue(static_cast<double>(xAcc[row]))));
        for(size_t component = 0; component < yAcc.componentCount(); ++component)
            componentArrays[component].append(toJsonValue(QVariant::fromValue(static_cast<double>(yAcc.get(row, component)))));
    }

    QJsonObject components;
    for(size_t component = 0; component < yAcc.componentCount(); ++component) {
        const QString componentName = (component < static_cast<size_t>(componentNames.size()) && !componentNames[static_cast<int>(component)].isEmpty())
            ? componentNames[static_cast<int>(component)]
            : QStringLiteral("Component %1").arg(component + 1);
        components.insert(componentName, componentArrays[component]);
    }

    QJsonObject result;
    result.insert(QStringLiteral("time"), time);
    result.insert(QStringLiteral("components"), components);
    if(const Property* errorProperty = table->getProperty(QStringLiteral("Error"));
       errorProperty && errorProperty->size() == table->y()->size() && errorProperty->componentCount() == table->y()->componentCount()) {
        BufferReadAccess<FloatType*> errorAcc(errorProperty);
        std::vector<QJsonArray> errorArrays(errorAcc.componentCount());
        for(size_t row = 0; row < errorAcc.size(); ++row) {
            for(size_t component = 0; component < errorAcc.componentCount(); ++component)
                errorArrays[component].append(toJsonValue(QVariant::fromValue(static_cast<double>(errorAcc.get(row, component)))));
        }

        QJsonObject errors;
        for(size_t component = 0; component < errorAcc.componentCount(); ++component) {
            const QString componentName = (component < static_cast<size_t>(componentNames.size()) && !componentNames[static_cast<int>(component)].isEmpty())
                ? componentNames[static_cast<int>(component)]
                : QStringLiteral("Component %1").arg(component + 1);
            errors.insert(componentName, errorArrays[component]);
        }
        result.insert(QStringLiteral("errors"), errors);
    }
    return result;
}

void writeSummaryFile(const HarnessConfig& config, const QJsonObject& summary, const QJsonObject& curves)
{
    QJsonObject root;
    root.insert(QStringLiteral("data_dir"), QDir::toNativeSeparators(config.dataDir));
    root.insert(QStringLiteral("temperature_K"), config.temperature);
    root.insert(QStringLiteral("delta_t_fs"), config.deltaT);
    root.insert(QStringLiteral("summary"), summary);
    if(!curves.isEmpty())
        root.insert(QStringLiteral("curves"), curves);

    QFile outputFile(config.outputPath);
    if(!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw Exception(QStringLiteral("Failed to open output file for writing: %1").arg(config.outputPath));

    outputFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    outputFile.close();
}

void runHarness(const HarnessConfig& config)
{
    MainWindow* mainWindow = findMainWindow();
    if(!mainWindow)
        throw Exception(QStringLiteral("Could not locate the OVITO main window."));

    mainWindow->hide();
    MainWindowUI& ui = mainWindow->ui();
    MainThreadOperation operation(ui, MainThreadOperation::Isolated, true);

    OORef<Scene> scene = ui.datasetContainer().activeScene();
    if(!scene)
        throw Exception(QStringLiteral("No active scene is available."));

    OORef<Pipeline> pipeline = importLammpsDataset(scene, config.dataDir);

    OORef<TransportModifier> modifier = OORef<TransportModifier>::create();
    modifier->setComputeMSD(config.computeMSD);
    modifier->setComputeVACF(config.computeVACF);
    modifier->setComputeConductivity(config.computeConductivity);
    modifier->setComputeDistinctIonCorrelation(config.computeDistinctIonCorrelation);
    modifier->setComputeStronglyCorrelatedPairs(config.computeStronglyCorrelatedPairs);
    modifier->setStrongPairSamplingMode(config.strongPairSamplingMode);
    modifier->setStrongPairSampleCount(config.strongPairSampleCount);
    modifier->setStrongPairFrameStep(config.strongPairFrameStep);
    modifier->setStrongPairThresholds(config.strongPairThresholds);
    modifier->setStrongPairRandomSeed(config.strongPairRandomSeed);
    modifier->setStrongPairDiscreteLagPoints(config.strongPairDiscreteLagPoints);
    modifier->setStrongPairLagPoints(config.strongPairLagPoints);
    modifier->setStrongPairResampleCount(config.strongPairResampleCount);
    modifier->setComputePerType(config.computePerType);
    modifier->setUseOnlySelectedParticles(false);
    modifier->setDeltaT(config.deltaT);
    modifier->setTemperature(config.temperature);
    modifier->setTimeUnit(TransportModifier::Femtoseconds);
    modifier->setLengthUnit(TransportModifier::Angstroms);
    modifier->setChargeUnit(TransportModifier::ElementaryCharges);

    pipeline->applyModifier(scene->animationSettings()->currentTime(), true, modifier);
    modifier->setRunRequestId(modifier->runRequestId() + 1);

    const PipelineFlowState state = pipeline->evaluatePipeline(
        PipelineEvaluationRequest(scene->animationSettings()->currentTime(), true, false)).blockForResult();

    if(!state.data())
        throw Exception(QStringLiteral("Transport evaluation did not return a data collection."));

    const QJsonObject summary = collectSummary(state.data());
    QJsonObject curves;
    if(const QJsonObject currentCorrelation = collectLineTable(state.data(), TransportModifier::CurrentCorrelationTableId); !currentCorrelation.isEmpty())
        curves.insert(QStringLiteral("current_autocorrelation"), currentCorrelation);
    if(const QJsonObject conductivity = collectLineTable(state.data(), TransportModifier::ConductivityTableId); !conductivity.isEmpty())
        curves.insert(QStringLiteral("ionic_conductivity"), conductivity);
    if(const QJsonObject chargeDisplacementContributions =
           collectLineTable(state.data(), TransportModifier::ChargeDisplacementContributionsTableId);
       !chargeDisplacementContributions.isEmpty())
        curves.insert(QStringLiteral("charge_displacement_contributions"), chargeDisplacementContributions);
    if(const QJsonObject conductivityContributions =
           collectLineTable(state.data(), TransportModifier::ConductivityContributionsTableId);
       !conductivityContributions.isEmpty())
        curves.insert(QStringLiteral("einstein_conductivity_contributions"), conductivityContributions);
    if(const QJsonObject distinctIonCorrelation =
           collectLineTable(state.data(), TransportModifier::DistinctIonCorrelationTableId);
       !distinctIonCorrelation.isEmpty())
        curves.insert(QStringLiteral("distinct_ion_correlation"), distinctIonCorrelation);
    if(const QJsonObject stronglyCorrelatedPairs =
           collectLineTable(state.data(), TransportModifier::StronglyCorrelatedPairsTableId);
       !stronglyCorrelatedPairs.isEmpty())
        curves.insert(QStringLiteral("strongly_correlated_pairs"), stronglyCorrelatedPairs);
    writeSummaryFile(config, summary, curves);

    std::cout << "Wrote OVITO transport summary to " << QDir::toNativeSeparators(config.outputPath).toStdString() << std::endl;
}

void scheduleHarness(const HarnessConfig& config)
{
    if(MainWindow* mainWindow = findMainWindow()) {
        if(mainWindow->ui().datasetContainer().currentSet()) {
            try {
                runHarness(config);
                QCoreApplication::exit(0);
            }
            catch(const Exception& ex) {
                ex.logError();
                std::cerr << ex.message().toStdString() << std::endl;
                QCoreApplication::exit(1);
            }
            catch(const std::exception& ex) {
                std::cerr << ex.what() << std::endl;
                QCoreApplication::exit(1);
            }
            return;
        }
    }

    QTimer::singleShot(50, QCoreApplication::instance(), [config]() {
        scheduleHarness(config);
    });
}

} // namespace

int main(int argc, char** argv)
{
    HarnessConfig config;
    try {
        config = parseArguments(argc, argv);
    }
    catch(const std::exception& ex) {
        printUsage(argv[0]);
        std::cerr << ex.what() << std::endl;
        return 2;
    }

    Ovito::OORef<Ovito::GuiApplication> app = Ovito::OORef<Ovito::GuiApplication>::create();

    int appArgc = 1;
    char* appArgv[] = { argv[0] };

    if(!app->initialize(appArgc, appArgv)) {
        app->shutdown();
        return 1;
    }

    if(!QCoreApplication::instance()) {
        app->taskManager().executePendingWork();
        const int exitCode = app->taskManager().isShuttingDown() ? 1 : 0;
        app->shutdown();
        return exitCode;
    }

    QTimer::singleShot(0, QCoreApplication::instance(), [config]() {
        scheduleHarness(config);
    });

    const int exitCode = QCoreApplication::exec();
    app->shutdown();
    return exitCode;
}
