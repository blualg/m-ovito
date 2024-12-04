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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/io/FileImporter.h>

namespace Ovito {

/**
 * This dialog lets the user select a remote file to be imported.
 */
class OVITO_GUI_EXPORT ImportRemoteFileDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    ImportRemoteFileDialog(MainWindow& mainWindow, const std::vector<const FileImporterClass*>& importerTypes, QWidget* parent = nullptr, const QString& caption = QString());

    /// Sets the current URL in the dialog.
    void selectFile(const QUrl& url);

    /// Returns the file to import after the dialog has been closed with "OK".
    QUrl urlToImport() const;

    /// Returns the selected importer class and sub-format name.
    const std::pair<const FileImporterClass*, QString>& selectedFileImporter() const;

    virtual QSize sizeHint() const override {
        return QDialog::sizeHint().expandedTo(QSize(700, 0));
    }

protected Q_SLOTS:

    /// This is called when the user has pressed the OK button of the dialog.
    /// Validates and saves all input made by the user and closes the dialog box.
    void onOk();

    /// This is called when the user presses the help button of the dialog.
    void onHelp();

private:

    MainWindow& _mainWindow;
    std::vector<std::pair<const FileImporterClass*, QString>> _importerFormats;
    QComboBox* _urlEdit;
    QComboBox* _formatSelector;
    QRadioButton* _libsshMethod;
    QRadioButton* _opensshMethod;
    QLineEdit* _sftpPath;
};

}   // End of namespace
