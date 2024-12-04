////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>
#include <ovito/gui/desktop/widgets/general/EnterLineEdit.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/desktop/properties/DefaultPropertiesEditor.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/io/FileExporter.h>
#include "FileExporterSettingsDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FileExporterSettingsDialog::FileExporterSettingsDialog(MainWindow& mainWindow, Scene& scene, FileExporter* exporter, QWidget* parent)
    : QDialog(parent), _mainWindow(mainWindow), _exporter(exporter)
{
    OVITO_ASSERT(exporter->sceneToExport() == &scene);

    setWindowTitle(tr("Data Export Settings"));

    _mainLayout = new QVBoxLayout(this);
    QGroupBox* groupBox;
    QGridLayout* groupLayout;

    // Create "Export sequence" group box, which lets the user select the trajectory interval to be exported.
    groupBox = new QGroupBox(tr("Export frame sequence"), this);
    _mainLayout->addWidget(groupBox);

    groupLayout = new QGridLayout(groupBox);
    groupLayout->setColumnStretch(0, 5);
    groupLayout->setColumnStretch(1, 95);
    _rangeButtonGroup = new QButtonGroup(this);

    bool exportTrajectory = _exporter->exportTrajectory();
    if(!exportTrajectory && scene.animationSettings()->lastFrame() <= scene.animationSettings()->firstFrame())
        groupBox->setEnabled(false);
    else
        _skipDialog = false;
    QRadioButton* singleFrameBtn = new QRadioButton(tr("Current frame only"));
    _rangeButtonGroup->addButton(singleFrameBtn, 0);
    groupLayout->addWidget(singleFrameBtn, 0, 0, 1, 2);
    singleFrameBtn->setChecked(!exportTrajectory);

    QHBoxLayout* frameRangeLayout = new QHBoxLayout();
    frameRangeLayout->setSpacing(0);
    groupLayout->addLayout(frameRangeLayout, 1, 0, 1, 2);

    QRadioButton* frameSequenceBtn = new QRadioButton(tr("Range:"));
    _rangeButtonGroup->addButton(frameSequenceBtn, 1);
    frameRangeLayout->addWidget(frameSequenceBtn);
    frameRangeLayout->addSpacing(10);
    frameSequenceBtn->setChecked(exportTrajectory);
    frameSequenceBtn->setEnabled(scene.animationSettings()->lastFrame() > scene.animationSettings()->firstFrame());

    class ShortLineEdit : public QLineEdit {
    public:
        virtual QSize sizeHint() const override {
            QSize s = QLineEdit::sizeHint();
            return QSize(s.width() / 2, s.height());
        }
    };

    frameRangeLayout->addWidget(new QLabel(tr("From frame:")));
    _startTimeSpinner = new SpinnerWidget();
    _startTimeSpinner->setUnit(mainWindow.unitsManager().integerIdentityUnit());
    _startTimeSpinner->setIntValue(scene.animationSettings()->firstFrame());
    _startTimeSpinner->setTextBox(new ShortLineEdit());
    _startTimeSpinner->setMinValue(scene.animationSettings()->firstFrame());
    _startTimeSpinner->setMaxValue(scene.animationSettings()->lastFrame());
    frameRangeLayout->addWidget(_startTimeSpinner->textBox());
    frameRangeLayout->addWidget(_startTimeSpinner);
    frameRangeLayout->addSpacing(8);
    frameRangeLayout->addWidget(new QLabel(tr("To:")));
    _endTimeSpinner = new SpinnerWidget();
    _endTimeSpinner->setUnit(mainWindow.unitsManager().integerIdentityUnit());
    _endTimeSpinner->setIntValue(scene.animationSettings()->lastFrame());
    _endTimeSpinner->setTextBox(new ShortLineEdit());
    _endTimeSpinner->setMinValue(scene.animationSettings()->firstFrame());
    _endTimeSpinner->setMaxValue(scene.animationSettings()->lastFrame());
    frameRangeLayout->addWidget(_endTimeSpinner->textBox());
    frameRangeLayout->addWidget(_endTimeSpinner);
    frameRangeLayout->addSpacing(8);
    frameRangeLayout->addWidget(new QLabel(tr("Every Nth frame:")));
    _nthFrameSpinner = new SpinnerWidget();
    _nthFrameSpinner->setUnit(mainWindow.unitsManager().integerIdentityUnit());
    _nthFrameSpinner->setIntValue(_exporter->everyNthFrame());
    _nthFrameSpinner->setTextBox(new ShortLineEdit());
    _nthFrameSpinner->setMinValue(1);
    frameRangeLayout->addWidget(_nthFrameSpinner->textBox());
    frameRangeLayout->addWidget(_nthFrameSpinner);

    _startTimeSpinner->setEnabled(frameSequenceBtn->isChecked());
    _endTimeSpinner->setEnabled(frameSequenceBtn->isChecked());
    _nthFrameSpinner->setEnabled(frameSequenceBtn->isChecked());
    connect(frameSequenceBtn, &QRadioButton::toggled, _startTimeSpinner, &SpinnerWidget::setEnabled);
    connect(frameSequenceBtn, &QRadioButton::toggled, _endTimeSpinner, &SpinnerWidget::setEnabled);
    connect(frameSequenceBtn, &QRadioButton::toggled, _nthFrameSpinner, &SpinnerWidget::setEnabled);

    QGridLayout* fileGroupLayout = new QGridLayout();
    fileGroupLayout->setColumnStretch(1, 1);
    groupLayout->addLayout(fileGroupLayout, 2, 1, 1, 1);

    if(_exporter->supportsMultiFrameFiles()) {
        _fileGroupButtonGroup = new QButtonGroup(this);
        QRadioButton* singleOutputFileBtn = new QRadioButton(tr("Single output file:   %1").arg(QFileInfo(_exporter->outputFilename()).fileName()));
        _fileGroupButtonGroup->addButton(singleOutputFileBtn, 0);
        fileGroupLayout->addWidget(singleOutputFileBtn, 0, 0, 1, 2);
        singleOutputFileBtn->setChecked(!_exporter->useWildcardFilename());

        QRadioButton* multipleFilesBtn = new QRadioButton(tr("File sequence:"));
        _fileGroupButtonGroup->addButton(multipleFilesBtn, 1);
        fileGroupLayout->addWidget(multipleFilesBtn, 1, 0, 1, 1);
        multipleFilesBtn->setChecked(_exporter->useWildcardFilename());

        _wildcardTextbox = new EnterLineEdit(_exporter->wildcardFilename());
        fileGroupLayout->addWidget(_wildcardTextbox, 1, 1, 1, 1);
        _wildcardTextbox->setEnabled(multipleFilesBtn->isChecked());
        connect(multipleFilesBtn, &QRadioButton::toggled, _wildcardTextbox, &QLineEdit::setEnabled);

        singleOutputFileBtn->setEnabled(frameSequenceBtn->isChecked());
        multipleFilesBtn->setEnabled(frameSequenceBtn->isChecked());
        connect(frameSequenceBtn, &QRadioButton::toggled, singleOutputFileBtn, &QWidget::setEnabled);
        connect(frameSequenceBtn, &QRadioButton::toggled, multipleFilesBtn, &QWidget::setEnabled);
    }
    else {
        _wildcardTextbox = new EnterLineEdit(_exporter->wildcardFilename());
        fileGroupLayout->addWidget(new QLabel(tr("Filename pattern:")), 0, 0, 1, 1);
        fileGroupLayout->addWidget(_wildcardTextbox, 0, 1, 1, 1);
    }
    _wildcardTextbox->setEnabled(frameSequenceBtn->isChecked());
    connect(frameSequenceBtn, &QRadioButton::toggled, _wildcardTextbox, &QWidget::setEnabled);

    // Create "Data" group box for selection of the source data pipeline and source data object.
    groupBox = new QGroupBox(tr("Data to export"), this);
    _mainLayout->addWidget(groupBox);

    groupLayout = new QGridLayout(groupBox);
    _sceneNodeBox = new QComboBox();
    _dataObjectBox = new QComboBox();
    groupLayout->addWidget(new QLabel(tr("Pipeline:")), 0, 0);
    groupLayout->setColumnStretch(1, 1);
    groupLayout->addWidget(_sceneNodeBox, 0, 1);
    groupLayout->setColumnMinimumWidth(2, 10);
    groupLayout->addWidget(new QLabel(tr("Data object:")), 0, 3);
    groupLayout->setColumnStretch(4, 1);
    groupLayout->addWidget(_dataObjectBox, 0, 4);

    scene.visitChildren([this, exporter](SceneNode* node) {
        if(exporter->isSuitableSceneNode(node)) {
            _sceneNodeBox->addItem(node->objectTitle(), QVariant::fromValue(OORef<OvitoObject>(node)));
            if(node == exporter->sceneNodeToExport())
                _sceneNodeBox->setCurrentIndex(_sceneNodeBox->count() - 1);
        }
        return true;
    });
    updateDataObjectList();
    if(_sceneNodeBox->count() <= 1)
        _sceneNodeBox->setEnabled(false);
    if(_dataObjectBox->count() <= 1)
        _dataObjectBox->setEnabled(false);
    if(_sceneNodeBox->isEnabled() == false && _dataObjectBox->isEnabled() == false)
        groupBox->setEnabled(false);
    else
        _skipDialog = false;

    // Update exporter whenever a new source pipeline has been selected by the user.
    connect(_sceneNodeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        _mainWindow.handleExceptions([&] {
            _exporter->setSceneNodeToExport(static_object_cast<SceneNode>(_sceneNodeBox->currentData().value<OORef<OvitoObject>>()));
        });
    });

    // Update the list of available data objects whenever the user selects a different source pipeline.
    connect(_sceneNodeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FileExporterSettingsDialog::updateDataObjectList);

    // Update the exporter whenever the user selects a new data object.
    connect(_dataObjectBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        _mainWindow.handleExceptions([&] {
            _exporter->setDataObjectToExport(_dataObjectBox->currentData().value<DataObjectReference>());
        });
    });

    // Show the optional UI of the exporter.
    if(OORef<PropertiesEditor> editor = PropertiesEditor::create(mainWindow, exporter)) {
        if(editor->getOOMetaClass() != DefaultPropertiesEditor::OOClass()) {
            PropertiesPanel* propPanel = new PropertiesPanel(mainWindow, this);
            propPanel->setFrameStyle(QFrame::NoFrame);
            _mainLayout->addWidget(propPanel);
            propPanel->setEditObject(exporter);
            _skipDialog = false;
        }
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &FileExporterSettingsDialog::onOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &FileExporterSettingsDialog::reject);
    _mainLayout->addWidget(buttonBox);
}

/******************************************************************************
* Updates the displayed list of data object available for export.
******************************************************************************/
void FileExporterSettingsDialog::updateDataObjectList()
{
    // Update the data objects list.
    _dataObjectBox->clear();

    _mainWindow.handleExceptions([&] {
        std::vector<DataObjectClassPtr> objClasses = _exporter->exportableDataObjectClass();
        if(!objClasses.empty() && _exporter->sceneToExport()) {
            if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(_exporter->sceneNodeToExport())) {
                if(const PipelineFlowState& state = pipeline->getCachedPipelineOutput(_exporter->sceneToExport()->animationSettings()->currentTime())) {
                    for(DataObjectClassPtr clazz : objClasses) {
                        OVITO_ASSERT(clazz != nullptr);
                        for(const ConstDataObjectPath& dataPath : state.data()->getObjectsRecursive(*clazz)) {
                            if(_exporter->isSuitableDataObject(dataPath)) {
                                QString title = dataPath.back()->objectTitle();
                                DataObjectReference dataRef(clazz, dataPath.toString(), title);
                                _dataObjectBox->addItem(title, QVariant::fromValue(dataRef));
                                if(dataRef == _exporter->dataObjectToExport())
                                    _dataObjectBox->setCurrentIndex(_dataObjectBox->count() - 1);
                            }
                        }
                    }
                }
            }
            if(_dataObjectBox->count() > 1) {
                _dataObjectBox->setEnabled(true);
                _exporter->setDataObjectToExport(_dataObjectBox->currentData().value<DataObjectReference>());
            }
            else if(_dataObjectBox->count() == 1) {
                _dataObjectBox->setEnabled(false);
                _exporter->setDataObjectToExport(_dataObjectBox->currentData().value<DataObjectReference>());
            }
            else {
                _dataObjectBox->setEnabled(false);
                _dataObjectBox->addItem(tr("<no exportable data>"));
            }
        }
        else {
            _dataObjectBox->setEnabled(false);
        }
    });
}

/******************************************************************************
* This is called when the user has pressed the OK button.
******************************************************************************/
void FileExporterSettingsDialog::onOk()
{
    setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.

    _mainWindow.handleExceptions([&] {
        _exporter->setExportTrajectory(_rangeButtonGroup->checkedId() == 1);
        _exporter->setUseWildcardFilename(_fileGroupButtonGroup ? (_fileGroupButtonGroup->checkedId() == 1) : _exporter->exportTrajectory());
        _exporter->setWildcardFilename(_wildcardTextbox->text());
        _exporter->setStartFrame(_startTimeSpinner->intValue());
        _exporter->setEndFrame(std::max(_endTimeSpinner->intValue(), _startTimeSpinner->intValue()));
        _exporter->setEveryNthFrame(_nthFrameSpinner->intValue());

        accept();
    });
}

}   // End of namespace
