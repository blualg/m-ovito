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
#include "PropertyParameterUI.h"

namespace Ovito {

/******************************************************************************
* This UI allows the user to select a filename as property value.
******************************************************************************/
class OVITO_GUI_EXPORT FilenameParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(FilenameParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const QStringList& fileFilter, bool existingFile);

    /// Destructor.
    ~FilenameParameterUI();

    /// This returns the button managed by this ParameterUI.
    QPushButton* selectorWidget() const { return _selectorButton; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the What's This helper text for the selector widget.
    void setWhatsThis(const QString& text) const {
        if(selectorWidget()) selectorWidget()->setWhatsThis(text);
    }

public:

    Q_PROPERTY(QPushButton selectorWidget READ selectorWidget)

private Q_SLOTS:

    /// Is called when the user presses the button.
    void onPickFilename();

protected:

    /// The selector control.
    QPointer<QPushButton> _selectorButton;

    /// List of file type filters.
    QStringList _fileFilter;

    /// Flag indicating whether the selected file must already exist.
    bool _existingFile;
};

}   // End of namespace
