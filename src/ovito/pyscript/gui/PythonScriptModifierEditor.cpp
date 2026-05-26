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

#include <ovito/pyscript/PyScript.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/pyscript/extensions/PythonScriptModifier.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <limits>
#include "PythonScriptModifierEditor.h"
#include "ObjectScriptEditor.h"

namespace PyScript {
namespace {

DataSet* currentApplicationDataset()
{
    if(Application::instance() == nullptr)
        return nullptr;
    return Application::instance()->datasetContainer().currentSet();
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(PythonScriptModifierEditor);
SET_OVITO_OBJECT_EDITOR(PythonScriptModifier, PythonScriptModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void PythonScriptModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Python script"), rolloutParams, "");

    auto* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* titleUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(Modifier::title));
    layout->addWidget(new QLabel(tr("User-defined modifier name:")), 0, 0);
    if(QLineEdit* lineEdit = titleUI->lineEdit())
        lineEdit->setPlaceholderText(QStringLiteral("<%1>").arg(PythonScriptModifier::OOClass().displayName()));
    layout->addWidget(titleUI->lineEdit(), 0, 1);

    _editScriptButton = new QPushButton(tr("Edit script..."), rollout);
    layout->addWidget(_editScriptButton, 1, 0, 1, 2);
    connect(_editScriptButton, &QPushButton::clicked, this, &PythonScriptModifierEditor::onOpenEditor);

    _parametersGroup = new QGroupBox(tr("Parameters"), rollout);
    _parametersLayout = new QFormLayout(_parametersGroup);
    _parametersLayout->setContentsMargins(6, 6, 6, 6);
    _parametersLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addWidget(_parametersGroup, 2, 0, 1, 2);

    layout->addWidget(new QLabel(tr("Script output:")), 3, 0, 1, 2);
    _outputDisplay = new QTextEdit(rollout);
    _outputDisplay->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _outputDisplay->setReadOnly(true);
    _outputDisplay->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(_outputDisplay, 4, 0, 1, 2);

    _openInspectorButton = new QPushButton(tr("Show in data inspector"), rollout);
    layout->addWidget(_openInspectorButton, 5, 0, 1, 2);
    connect(_openInspectorButton, &QPushButton::clicked, this, &PythonScriptModifierEditor::onOpenDataInspector);

    connect(this, &PropertiesEditor::contentsChanged, this, &PythonScriptModifierEditor::updateUserInterface);
    connect(this, &PropertiesEditor::contentsReplaced, this, &PythonScriptModifierEditor::updateUserInterface);
    updateUserInterface();
}

/******************************************************************************
* Tracks status changes to refresh the output pane.
******************************************************************************/
bool PythonScriptModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::ObjectStatusChanged) {
        if(const auto* modifier = dynamic_object_cast<PythonScriptModifier>(editObject())) {
            if(!modifier->isScriptBusy())
                updateUserInterface();
        }
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Refreshes the editor widgets.
******************************************************************************/
void PythonScriptModifierEditor::updateUserInterface()
{
    PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(editObject());
    if(modifier) {
        const QString pendingText = tr("<Script compilation pending>\n");
        if(modifier->scriptCompilationOutput() == pendingText)
            modifier->ensureScriptCompiled();
        _editScriptButton->setEnabled(true);
        _openInspectorButton->setEnabled(true);
        const QString executionOutput = modifier->scriptExecutionOutput();
        QString text = modifier->scriptCompilationOutput();
        if(!executionOutput.isEmpty()) {
            if(!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
                text += QLatin1Char('\n');
            text += executionOutput;
        }
        _outputDisplay->setPlainText(text);
        if(_displayedSchema != modifier->parameterSchema())
            rebuildParameterWidgets(modifier);
        syncParameterWidgetValues(modifier);
        _parametersGroup->setVisible(!modifier->parameterSchema().isEmpty());

        if(!executionOutput.contains(QStringLiteral("Script finished successfully.")))
            _lastAutoOpenedExecutionOutput.clear();

        // Automatically open the generated table output once after a successful table-producing run.
        if(executionOutput != _lastAutoOpenedExecutionOutput
                && executionOutput.contains(QStringLiteral("Generated table(s):"))
                && executionOutput.contains(QStringLiteral("Script finished successfully."))) {
            _lastAutoOpenedExecutionOutput = executionOutput;
            QMetaObject::invokeMethod(this, [this]() { onOpenDataInspector(); }, Qt::QueuedConnection);
        }
    }
    else {
        _editScriptButton->setEnabled(false);
        _openInspectorButton->setEnabled(false);
        _outputDisplay->clear();
        _parametersGroup->setVisible(false);
        _lastAutoOpenedExecutionOutput.clear();
    }
}

/******************************************************************************
* Rebuilds the dynamic parameter widget set for the current script schema.
******************************************************************************/
void PythonScriptModifierEditor::rebuildParameterWidgets(PythonScriptModifier* modifier)
{
    while(_parametersLayout->rowCount() > 0)
        _parametersLayout->removeRow(0);
    _parameterBindings.clear();
    _displayedSchema = modifier ? modifier->parameterSchema() : QVariantList{};

    if(!modifier)
        return;

    for(const QVariant& schemaEntryVar : modifier->parameterSchema()) {
        const QVariantMap schemaEntry = schemaEntryVar.toMap();
        const QString name = schemaEntry.value(QStringLiteral("name")).toString();
        const QString kind = schemaEntry.value(QStringLiteral("kind")).toString();
        const QString label = schemaEntry.value(QStringLiteral("label")).toString();

        ParameterWidgetBinding binding;
        binding.name = name;
        binding.kind = kind;

        if(kind == QStringLiteral("bool")) {
            auto* checkbox = new QCheckBox(label, _parametersGroup);
            connect(checkbox, &QCheckBox::toggled, this, [this, modifier, name](bool checked) {
                commitParameterValue(modifier, name, checked);
            });
            _parametersLayout->addRow(QString(), checkbox);
            binding.widget = checkbox;
        }
        else if(kind == QStringLiteral("int")) {
            auto* spinBox = new QSpinBox(_parametersGroup);
            spinBox->setRange(schemaEntry.value(QStringLiteral("min"), std::numeric_limits<int>::lowest()).toInt(),
                              schemaEntry.value(QStringLiteral("max"), std::numeric_limits<int>::max()).toInt());
            connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this, modifier, name](int value) {
                commitParameterValue(modifier, name, value);
            });
            _parametersLayout->addRow(label + QStringLiteral(":"), spinBox);
            binding.widget = spinBox;
        }
        else if(kind == QStringLiteral("float")) {
            auto* spinBox = new QDoubleSpinBox(_parametersGroup);
            spinBox->setDecimals(6);
            spinBox->setRange(schemaEntry.value(QStringLiteral("min"), -1.0e12).toDouble(),
                              schemaEntry.value(QStringLiteral("max"), 1.0e12).toDouble());
            connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, modifier, name](double value) {
                commitParameterValue(modifier, name, value);
            });
            _parametersLayout->addRow(label + QStringLiteral(":"), spinBox);
            binding.widget = spinBox;
        }
        else if(kind == QStringLiteral("string")) {
            auto* lineEdit = new QLineEdit(_parametersGroup);
            connect(lineEdit, &QLineEdit::editingFinished, this, [this, modifier, name, lineEdit]() {
                commitParameterValue(modifier, name, lineEdit->text());
            });
            _parametersLayout->addRow(label + QStringLiteral(":"), lineEdit);
            binding.widget = lineEdit;
        }
        else if(kind == QStringLiteral("choice")) {
            auto* comboBox = new QComboBox(_parametersGroup);
            const QVariantList choices = schemaEntry.value(QStringLiteral("choices")).toList();
            for(const QVariant& choiceVar : choices) {
                const QVariantMap choice = choiceVar.toMap();
                comboBox->addItem(choice.value(QStringLiteral("label")).toString(), choice.value(QStringLiteral("value")));
            }
            connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, modifier, name, comboBox](int index) {
                if(index >= 0)
                    commitParameterValue(modifier, name, comboBox->itemData(index));
            });
            _parametersLayout->addRow(label + QStringLiteral(":"), comboBox);
            binding.widget = comboBox;
        }

        if(binding.widget)
            _parameterBindings.push_back(binding);
    }
}

/******************************************************************************
* Synchronizes dynamic parameter widget values from the current modifier state.
******************************************************************************/
void PythonScriptModifierEditor::syncParameterWidgetValues(PythonScriptModifier* modifier)
{
    if(!modifier)
        return;

    const QVariantMap values = modifier->parameterValues();
    for(const ParameterWidgetBinding& binding : _parameterBindings) {
        const QVariant currentValue = values.value(binding.name);
        if(binding.kind == QStringLiteral("bool")) {
            auto* checkbox = qobject_cast<QCheckBox*>(binding.widget);
            if(!checkbox)
                continue;
            QSignalBlocker blocker(checkbox);
            checkbox->setChecked(currentValue.toBool());
        }
        else if(binding.kind == QStringLiteral("int")) {
            auto* spinBox = qobject_cast<QSpinBox*>(binding.widget);
            if(!spinBox)
                continue;
            QSignalBlocker blocker(spinBox);
            spinBox->setValue(currentValue.toInt());
        }
        else if(binding.kind == QStringLiteral("float")) {
            auto* spinBox = qobject_cast<QDoubleSpinBox*>(binding.widget);
            if(!spinBox)
                continue;
            QSignalBlocker blocker(spinBox);
            spinBox->setValue(currentValue.toDouble());
        }
        else if(binding.kind == QStringLiteral("string")) {
            auto* lineEdit = qobject_cast<QLineEdit*>(binding.widget);
            if(!lineEdit)
                continue;
            QSignalBlocker blocker(lineEdit);
            lineEdit->setText(currentValue.toString());
        }
        else if(binding.kind == QStringLiteral("choice")) {
            auto* comboBox = qobject_cast<QComboBox*>(binding.widget);
            if(!comboBox)
                continue;
            QSignalBlocker blocker(comboBox);
            for(int index = 0; index < comboBox->count(); ++index) {
                if(comboBox->itemData(index) == currentValue) {
                    comboBox->setCurrentIndex(index);
                    break;
                }
            }
        }
    }
}

/******************************************************************************
* Commits a class-parameter change to the modifier object.
******************************************************************************/
void PythonScriptModifierEditor::commitParameterValue(PythonScriptModifier* modifier, const QString& name, const QVariant& value)
{
    if(!modifier || Application::instance() == nullptr)
        return;

    Application::instance()->handleExceptions([modifier, name, value]() {
        UndoableTransaction transaction(*Application::instance(), QObject::tr("Change Python modifier parameter"));
        modifier->setParameterValue(name, value);
        transaction.commit();
    });
}

/******************************************************************************
* Opens the script editor window.
******************************************************************************/
void PythonScriptModifierEditor::onOpenEditor()
{
    PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(editObject());
    if(!modifier)
        return;

    class ScriptEditor : public ObjectScriptEditor {
    public:
        ScriptEditor(MainWindowUI& ui, QWidget* parentWidget, RefTarget* scriptableObject) : ObjectScriptEditor(ui, parentWidget, scriptableObject) {}

    protected:
        virtual const QString& getObjectScript(RefTarget* obj) const override {
            PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(obj);
            if(!modifier->script().isEmpty())
                return modifier->script();

            static const QString message = tr("# Modifier function was defined in an external Python file. Source code is not available here.\n");
            return message;
        }

        virtual QString getOutputText(RefTarget* obj) const override {
            PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(obj);
            QString text = modifier->scriptCompilationOutput();
            if(!modifier->scriptExecutionOutput().isEmpty()) {
                if(!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
                    text += QLatin1Char('\n');
                text += modifier->scriptExecutionOutput();
            }
            return text;
        }

        virtual void setObjectScript(RefTarget* obj, const QString& script) const override {
            if(Application::instance() == nullptr)
                return;
            Application::instance()->handleExceptions([obj, &script]() {
                UndoableTransaction transaction(*Application::instance(), tr("Commit script"));
                PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(obj);
                modifier->commitScript(script);
                transaction.commit();
            });
        }

        virtual bool isObjectBusy(RefTarget* obj) const override {
            if(const auto* modifier = dynamic_object_cast<PythonScriptModifier>(obj))
                return modifier->isScriptBusy();
            return false;
        }

        virtual void cancelObjectScriptExecution(RefTarget* obj) const override {
            if(auto* modifier = dynamic_object_cast<PythonScriptModifier>(obj))
                modifier->cancelRunningScripts(false);
        }
    };

    if(ObjectScriptEditor* editor = ObjectScriptEditor::findEditorForObject(modifier)) {
        editor->show();
        editor->activateWindow();
        return;
    }

    auto* editor = new ScriptEditor(ui(), ui().mainWindow(), modifier);
    editor->show();
}

/******************************************************************************
* Opens the Python modifier results in the Data Inspector.
******************************************************************************/
void PythonScriptModifierEditor::onOpenDataInspector()
{
    PythonScriptModifier* modifier = static_object_cast<PythonScriptModifier>(editObject());
    ModificationNode* node = modificationNode();
    if(!modifier || !node)
        return;
    PipelineNode* pipelineNode = static_cast<PipelineNode*>(node);

    // The Data Inspector chooses its content based on the currently selected scene pipeline.
    // Make sure the owning pipeline scene node is selected before trying to open a specific table.
    if(Application::instance()) {
        DataSet* dataset = Application::instance()->datasetContainer().currentSet();
        if(!dataset)
            dataset = ui().datasetContainer().currentSet();
        if(dataset) {
            if(SelectionSet* selection = dataset->findGlobalObject<SelectionSet>()) {
                const QSet<Pipeline*> scenePipelines = pipelineNode->pipelines(true);
                if(!scenePipelines.isEmpty()) {
                    if(SceneNode* sceneNode = (*scenePipelines.begin())->someSceneNode())
                        selection->setNode(sceneNode);
                }
            }
        }
    }

    if(const DataCollection* data = getPipelineOutput().data()) {
        QString tableIdentifier;
        data->visitObjectsOfType<DataTable>([&](const DataTable* table) {
            if(!tableIdentifier.isEmpty())
                return;
            if(table->createdByNode().lock().get() != pipelineNode)
                return;
            tableIdentifier = table->identifier();
            if(tableIdentifier.isEmpty())
                tableIdentifier = table->title();
            if(tableIdentifier.isEmpty())
                tableIdentifier = table->objectTitle();
        });

        if(!tableIdentifier.isEmpty()) {
            if(ui().mainWindow()->openDataInspector(pipelineNode, tableIdentifier, 0))
                return;
        }
    }

    ui().mainWindow()->openDataInspector(pipelineNode);
}

}  // namespace PyScript
