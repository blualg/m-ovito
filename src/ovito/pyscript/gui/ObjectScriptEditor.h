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

#pragma once

#include <ovito/pyscript/PyScript.h>
#include <ovito/core/oo/RefTargetListener.h>
#include <ovito/gui/desktop/GUI.h>
#include <QMainWindow>

class QPlainTextEdit;

namespace PyScript {

using namespace Ovito;

/**
 * Lightweight script editor used by the restored Python script modifier.
 */
class ObjectScriptEditor : public QMainWindow, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:
    ObjectScriptEditor(MainWindowUI& ui, QWidget* parentWidget, RefTarget* scriptableObject);

    static ObjectScriptEditor* findEditorForObject(RefTarget* scriptableObject);

public Q_SLOTS:
    void onCommitScript();
    void onLoadScriptFromFile();
    void onSaveScriptToFile();

protected Q_SLOTS:
    void onNotificationEvent(const ReferenceEvent& event);
    void updateEditorContents();
    void updateOutputWindow();

protected:
    virtual const QString& getObjectScript(RefTarget* obj) const = 0;
    virtual QString getOutputText(RefTarget* obj) const = 0;
    virtual void setObjectScript(RefTarget* obj, const QString& script) const = 0;
    virtual bool isObjectBusy(RefTarget* obj) const { Q_UNUSED(obj); return false; }
    virtual void cancelObjectScriptExecution(RefTarget* obj) const { Q_UNUSED(obj); }

    virtual void closeEvent(QCloseEvent* event) override;
    virtual void showEvent(QShowEvent* event) override;

    QPlainTextEdit* _codeEditor = nullptr;
    QPlainTextEdit* _outputWindow = nullptr;
    RefTargetListener<RefTarget> _scriptableObject;
    QAction* _undoAction = nullptr;
    QAction* _redoAction = nullptr;
    bool _commitInProgress = false;
    bool _closeRequestedWhileBusy = false;
};

}  // namespace PyScript
