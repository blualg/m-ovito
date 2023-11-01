////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * A custom version of the QMessageBox dialog class.
 *
 * On macOS, this wrapper class prevents QMessageBox from using the native dialog window,
 * which shows various issues since Qt 6.4.
 */
class MessageDialog : public QMessageBox
{
public:

    /// Constructor.
    MessageDialog(QWidget* parent = nullptr) : QMessageBox(parent) {
#ifdef Q_OS_MACOS
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
#endif
    }

    /// Constructor.
    MessageDialog(QMessageBox::Icon icon, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = NoButton, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint)
        : QMessageBox(icon, title, text, buttons, parent, f) {
#ifdef Q_OS_MACOS
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
#endif
    }

#ifdef Q_OS_MACOS
    /// Destructor.
    ~MessageDialog() {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
    }
#endif

#ifdef Q_OS_MACOS
    static QMessageBox::StandardButton critical(QWidget* parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = Ok, QMessageBox::StandardButton defaultButton = NoButton) {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        auto result = QMessageBox::critical(parent, title, text, buttons, defaultButton);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
        return result;
    }

    static QMessageBox::StandardButton question(QWidget* parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons = StandardButtons(Yes | No), QMessageBox::StandardButton defaultButton = NoButton) {
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
        auto result = QMessageBox::question(parent, title, text, buttons, defaultButton);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
        return result;
    }
#endif
};

}   // End of namespace
