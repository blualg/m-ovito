////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/gui/qml/GUI.h>
#include <ovito/gui/qml/dataset/WasmFileManager.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "MainWindow.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
MainWindow::MainWindow() : MainWindowInterface(_datasetContainer), _datasetContainer(this)
{
    // Create the object that manages the input modes of the viewports.
    setViewportInputManager(new ViewportInputManager(this, _datasetContainer, this));

    // Create actions.
    setActionManager(new ActionManager(this, this));

    // For timed display of texts in the status bar:
    connect(&_statusBarTimer, &QTimer::timeout, this, &MainWindow::clearStatusBarMessage);

    // Create list model for the items in the selected data pipeline.
    _pipelineListModel = new PipelineListModel(_datasetContainer, actionManager(), this);

    // Create list of available modifiers.
    _modifierListModel = new ModifierListModel(this, this, _pipelineListModel);
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainWindow::~MainWindow()
{
    datasetContainer()->setCurrentSet(nullptr);
    taskManager().cancelAllAndWait();
}

/******************************************************************************
* Displays a message string in the window's status bar.
******************************************************************************/
void MainWindow::showStatusBarMessage(const QString& message, int timeout)
{
    if(message != _statusBarText) {
        _statusBarText = message;

        static const QString separatorMarker = QStringLiteral("<sep>");
        static const QString separatorText = QStringLiteral(" | ");
        static const QString separatorTextColored = QStringLiteral(" <font color=\"gray\">|</font> ");
        static const QString keyBeginMarker = QStringLiteral("<key>");
        static const QString keyBeginText = QStringLiteral("<font color=\"#CCF\">");
        static const QString keyEndMarker = QStringLiteral("</key>");
        static const QString keyEndText = QStringLiteral("</font>");
        static const QString valueBeginMarker = QStringLiteral("<val>");
        static const QString valueBeginText = QStringLiteral("");
        static const QString valueEndMarker = QStringLiteral("</val>");
        static const QString valueEndText = QStringLiteral("");

        // Create a version of the message string that does not contain any markup.
        _statusBarTextMarkup = message;
        _statusBarTextMarkup.replace(separatorMarker, separatorTextColored);
        _statusBarTextMarkup.replace(keyBeginMarker, keyBeginText);
        _statusBarTextMarkup.replace(keyEndMarker, keyEndText);
        _statusBarTextMarkup.replace(valueBeginMarker, valueBeginText);
        _statusBarTextMarkup.replace(valueEndMarker, valueEndText);

        Q_EMIT statusBarTextChanged(_statusBarTextMarkup);
        if(timeout > 0) {
            _statusBarTimer.start(timeout);
        }
        else {
            _statusBarTimer.stop();
        }
    }
}

/******************************************************************************
* Executes the user-provided function and records all actions on the undo stack.
******************************************************************************/
void MainWindow::undoableOperation(const QString& actionDescription, const QJSValue& callbackFunction)
{
    OVITO_ASSERT(callbackFunction.isCallable());

    try {
        UndoableTransaction transaction(datasetContainer()->currentSet()->undoStack(), actionDescription);
        QJSValue result = callbackFunction.call();
        if(result.isError()) {
            throw Exception(tr("Uncaught script exception at line %1 in file %2: %3")
                .arg(result.property("lineNumber").toInt())
                .arg(result.property("fileName").toString())
                .arg(result.toString()));
        }
        transaction.commit();
    }
    catch(Exception& ex) {
        ex.setContext(this);
        ex.reportError();
    }
}

/******************************************************************************
* Lets the user select a file on the local computer to be imported into the scene.
******************************************************************************/
void MainWindow::importDataFile()
{
    WasmFileManager::importFileIntoMemory(this, QStringLiteral("*"), [this](const QUrl& url) {
        try {
            if(url.isValid())
                datasetContainer()->importFile(url);
        }
        catch(const Exception& ex) {
            ex.reportError();
        }
    });
}

}   // End of namespace
