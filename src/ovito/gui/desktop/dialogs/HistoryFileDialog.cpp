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
#include "HistoryFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
HistoryFileDialog::HistoryFileDialog(MainWindowUI& ui, const QString& dialogClass, QWidget* parent, const QString& caption, const QString& directory, const QString& filter)
    : QFileDialog(parent, caption, directory.isEmpty() ? QDir::currentPath() : directory, filter)
    , UserInterfaceComponent<MainWindowUI>(ui)
    , _dialogClass(dialogClass)
{
    connect(this, &QFileDialog::fileSelected, this, &HistoryFileDialog::onFileSelected);
    connect(this, &QFileDialog::filesSelected, this, [&](const QStringList& selected) {
        if(!selected.empty()) onFileSelected(selected.front());
    });

    // This legacy option has been removed from the GUI since OVITO 3.7:
    // The user can request Qt's widget-based file dialog instead of the native OS dialog by settings the corresponding option in the application settings.
    // The native dialogs of some platforms don't provide the directory history function but is typically faster than the Qt implementation.
    QSettings settings;
    if(settings.value("file/dialog_type").toString() == "qt")
        setOption(QFileDialog::DontUseNativeDialog);

    if(keepWorkingDirectoryHistoryEnabled()) {
        QStringList history = ui.getRecentlyUsedDirectories(_dialogClass);
        if(history.isEmpty() == false) {
            if(directory.isEmpty()) {
                setDirectory(history.front());
            }
            setHistory(history);
        }
    }
}

/******************************************************************************
* This is called when the user has pressed the OK button of the dialog.
******************************************************************************/
void HistoryFileDialog::onFileSelected(const QString& file)
{
    if(file.isEmpty())
        return;

    if(keepWorkingDirectoryHistoryEnabled()) {
        QString currentDir = QFileInfo(file).absolutePath();
        ui().updateMostRecentlyUsedDirectory(_dialogClass, currentDir);
    }
}

}   // End of namespace
