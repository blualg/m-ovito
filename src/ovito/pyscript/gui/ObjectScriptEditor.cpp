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
#include <ovito/core/app/Application.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFile>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QToolBar>
#include "ObjectScriptEditor.h"

namespace PyScript {

namespace {

} // namespace

/******************************************************************************
* Constructs the editor frame.
******************************************************************************/
ObjectScriptEditor::ObjectScriptEditor(MainWindowUI& ui, QWidget* parentWidget, RefTarget* scriptableObject) :
    QMainWindow(parentWidget, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    UserInterfaceComponent<MainWindowUI>(ui)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    _codeEditor = new QPlainTextEdit(this);
    _codeEditor->setFont(font);
    _codeEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    setCentralWidget(_codeEditor);

    _outputWindow = new QPlainTextEdit(this);
    _outputWindow->setFont(font);
    _outputWindow->setReadOnly(true);
    _outputWindow->setLineWrapMode(QPlainTextEdit::NoWrap);

    auto* outputDockWidget = new QDockWidget(tr("Script output:"), this);
    outputDockWidget->setObjectName(QStringLiteral("PythonScriptOutput"));
    outputDockWidget->setWidget(_outputWindow);
    outputDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, outputDockWidget);

    QToolBar* toolBar = addToolBar(tr("Script Editor"));
    toolBar->addAction(tr("Commit and run script"), this, &ObjectScriptEditor::onCommitScript);
    toolBar->addSeparator();
    toolBar->addAction(tr("Load script from disk"), this, &ObjectScriptEditor::onLoadScriptFromFile);
    toolBar->addAction(tr("Save script to disk"), this, &ObjectScriptEditor::onSaveScriptToFile);
    toolBar->addSeparator();
    _undoAction = toolBar->addAction(tr("Undo"));
    _redoAction = toolBar->addAction(tr("Redo"));
    _undoAction->setEnabled(false);
    _redoAction->setEnabled(false);

    connect(_undoAction, &QAction::triggered, _codeEditor, &QPlainTextEdit::undo);
    connect(_redoAction, &QAction::triggered, _codeEditor, &QPlainTextEdit::redo);
    connect(_codeEditor, &QPlainTextEdit::undoAvailable, _undoAction, &QAction::setEnabled);
    connect(_codeEditor, &QPlainTextEdit::redoAvailable, _redoAction, &QAction::setEnabled);
    connect(_codeEditor->document(), &QTextDocument::modificationChanged, this, [this](bool modified) {
        QString baseTitle = _scriptableObject.target() ? _scriptableObject.target()->objectTitle() : tr("Script editor");
        setWindowTitle(modified ? baseTitle + QStringLiteral(" *") : baseTitle);
    });

    setContextMenuPolicy(Qt::NoContextMenu);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 650);
    _codeEditor->setFocus();

    _scriptableObject.connect(this, [this](RefTarget*, const ReferenceEvent& event) {
        onNotificationEvent(event);
    });
    _scriptableObject.setTarget(scriptableObject);
    setWindowTitle(scriptableObject ? scriptableObject->objectTitle() : tr("Script editor"));
}

/******************************************************************************
* Returns an existing editor window for the given object if there is one.
******************************************************************************/
ObjectScriptEditor* ObjectScriptEditor::findEditorForObject(RefTarget* scriptableObject)
{
    for(QWidget* widget : QApplication::topLevelWidgets()) {
        if(ObjectScriptEditor* editor = qobject_cast<ObjectScriptEditor*>(widget)) {
            if(editor->_scriptableObject.target() == scriptableObject)
                return editor;
        }
    }
    return nullptr;
}

/******************************************************************************
* Is called when the scriptable object generates an event.
******************************************************************************/
void ObjectScriptEditor::onNotificationEvent(const ReferenceEvent& event)
{
    const bool busy = (_scriptableObject.target() && isObjectBusy(_scriptableObject.target()));

    if(event.type() == ReferenceEvent::TargetDeleted) {
        deleteLater();
    }
    else if(event.type() == ReferenceEvent::TargetChanged) {
        if(!busy) {
            updateEditorContents();
            updateOutputWindow();
        }
    }
    else if(event.type() == ReferenceEvent::ObjectStatusChanged) {
        if(!busy)
            updateOutputWindow();
    }

    if(_closeRequestedWhileBusy && (!_scriptableObject.target() || !busy)) {
        _closeRequestedWhileBusy = false;
        QMetaObject::invokeMethod(this, &QWidget::close, Qt::QueuedConnection);
    }
}

/******************************************************************************
* Commits the current script to the owner object.
******************************************************************************/
void ObjectScriptEditor::onCommitScript()
{
    if(_scriptableObject.target()) {
        _commitInProgress = true;
        if(ui().datasetContainer().currentSet()) {
            Application::instance()->datasetContainer().setCurrentSet(ui().datasetContainer().currentSet());
        }
        setObjectScript(_scriptableObject.target(), _codeEditor->toPlainText());
        updateOutputWindow();
        ui().updateViewports();
        for(int i = 0; i < 20; ++i) {
            QCoreApplication::sendPostedEvents(nullptr, 0);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            updateOutputWindow();
            if(!_outputWindow->toPlainText().contains(tr("Running script...")))
                break;
        }
        _commitInProgress = false;
        if(_closeRequestedWhileBusy) {
            _closeRequestedWhileBusy = false;
            QMetaObject::invokeMethod(this, &QWidget::close, Qt::QueuedConnection);
        }
    }
}

/******************************************************************************
* Replaces the editor contents with the script from the owning object.
******************************************************************************/
void ObjectScriptEditor::updateEditorContents()
{
    if(_scriptableObject.target()) {
        _codeEditor->setEnabled(true);
        const QString& script = getObjectScript(_scriptableObject.target());
        if(script != _codeEditor->toPlainText())
            _codeEditor->setPlainText(script);
        _codeEditor->document()->setModified(false);
    }
    else {
        _codeEditor->document()->setModified(false);
        _codeEditor->setEnabled(false);
        _codeEditor->clear();
    }
}

/******************************************************************************
* Replaces the output window contents with the script output cached by the owner object.
******************************************************************************/
void ObjectScriptEditor::updateOutputWindow()
{
    _outputWindow->setPlainText(_scriptableObject.target() ? getOutputText(_scriptableObject.target()) : QString());
}

/******************************************************************************
* Is called when the window is shown.
******************************************************************************/
void ObjectScriptEditor::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    updateEditorContents();
    updateOutputWindow();
}

/******************************************************************************
* Is called when the user closes the window.
******************************************************************************/
void ObjectScriptEditor::closeEvent(QCloseEvent* event)
{
    if(_scriptableObject.target()) {
        cancelObjectScriptExecution(_scriptableObject.target());
        if(_commitInProgress || isObjectBusy(_scriptableObject.target())) {
            _closeRequestedWhileBusy = true;
            hide();
            event->ignore();
            return;
        }
    }

    if(_scriptableObject.target() && _codeEditor->document()->isModified()) {
        QMessageBox::StandardButton button = QMessageBox::question(
            this,
            tr("Save changes"),
            tr("The script has been modified. Do you want to commit the changes before closing the editor window?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if(button == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if(button == QMessageBox::Yes)
            onCommitScript();
    }

    QMainWindow::closeEvent(event);
}

/******************************************************************************
* Lets the user load a script file into the editor.
******************************************************************************/
void ObjectScriptEditor::onLoadScriptFromFile()
{
    if(!_scriptableObject.target())
        return;

    ui().handleExceptions([this]() {
        QFileDialog dialog(this, tr("Load script file"), QDir::currentPath(), tr("Python scripts (*.py);;Any files (*)"));
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        if(dialog.exec() != QDialog::Accepted)
            return;

        const QStringList selectedFiles = dialog.selectedFiles();
        if(selectedFiles.isEmpty())
            return;
        const QString fileName = selectedFiles.front();

        QFile file(fileName);
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            throw Exception(tr("Failed to open file '%1' for reading: %2").arg(file.fileName(), file.errorString()));
        const QString loadedScript = QString::fromUtf8(file.readAll());
        _codeEditor->setPlainText(loadedScript);
        _codeEditor->document()->setModified(true);
        updateOutputWindow();
    });
}

/******************************************************************************
* Lets the user save the current script to a file.
******************************************************************************/
void ObjectScriptEditor::onSaveScriptToFile()
{
    if(!_scriptableObject.target())
        return;

    try {
        QFileDialog dialog(this, tr("Save script file"), QStringLiteral("script.py"), tr("Python scripts (*.py);;Any files (*)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        if(dialog.exec() != QDialog::Accepted)
            return;
        const QStringList selectedFiles = dialog.selectedFiles();
        if(selectedFiles.isEmpty())
            return;
        const QString fileName = selectedFiles.front();

        QFile file(fileName);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw Exception(tr("Failed to open file '%1' for writing: %2").arg(file.fileName(), file.errorString()));
        file.write(_codeEditor->toPlainText().toUtf8());
    }
    catch(const Exception& ex) {
        ui().reportError(ex);
    }
}

}  // namespace PyScript
