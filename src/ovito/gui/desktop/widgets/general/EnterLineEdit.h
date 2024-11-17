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

namespace Ovito {

/**
 * \brief A variant of the QLineEdit standard widget which intercepts 'Enter' key-press events instead of forwarding them to the parent widget.
 *        This is used in places where the edit widget is a child of a checkable QGroupBox, in which
 *        the enter key otherwise would toggle the group widget's checkbox.
 */
class OVITO_GUI_EXPORT EnterLineEdit : public QLineEdit
{
    Q_OBJECT

public:

    /// Constructor.
    using QLineEdit::QLineEdit;

protected:

    /// Handles key-press events.
    virtual void keyPressEvent(QKeyEvent* event) override;
};

}   // End of namespace
