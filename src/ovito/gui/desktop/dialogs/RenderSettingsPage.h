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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>

namespace Ovito {

/**
 * Page of the application settings dialog, which hosts rendering options.
 */
class OVITO_GUI_EXPORT RenderSettingsPage : public ApplicationSettingsDialogPage
{
    Q_OBJECT
    OVITO_CLASS(RenderSettingsPage)

public:
    /// Default constructor.
    explicit RenderSettingsPage() = default;

    /// \brief Creates the widget.
    virtual void insertSettingsDialogPage(QTabWidget* tabWidget) override;

    /// \brief Lets the settings page to save all values entered by the user.
    virtual void saveValues(QTabWidget* tabWidget) override;

    /// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
    virtual int pageSortingKey() const override { return 0; }

Q_SIGNALS:
    void ffmpegPathValidated(bool isValid);

private:
    void validateFfmpegPath();

    void refreshCodecCombobox();

private:
    StatusWidget* _statusLabel;
    QLineEdit* _ffmpegPath;
    QComboBox* _ffmpegCodec;
    QComboBox* _ffmpegQualityBox;
    QByteArray _ffmpegCodecName;
    bool _externalffmpeg;
};

}  // namespace Ovito
