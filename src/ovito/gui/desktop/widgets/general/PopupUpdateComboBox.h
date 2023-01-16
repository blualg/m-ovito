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

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QComboBox widget that emits a signal just before the drop-down popup list is shown.
 */
class OVITO_GUI_EXPORT PopupUpdateComboBox : public QComboBox
{
    Q_OBJECT

public:

    /// Initializes the widget.
    using QComboBox::QComboBox;

    /// Is called just before the drop-down box is activated.
    virtual void showPopup() override {
        Q_EMIT dropDownActivated();
        QComboBox::showPopup();
    }

Q_SIGNALS:

    /// This signal is emited right before the drop-down list of the combobox is displayed.
    void dropDownActivated();
};

}   // End of namespace
