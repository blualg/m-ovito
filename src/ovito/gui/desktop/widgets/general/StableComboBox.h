////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
 * \brief A QComboBox that provides more stable behavior when the list of items is changed while
 *        the drop-down list is open. The standard QComboBox widget does not handle this situation
 *        well and won't let the user select an item, because the selection is reset when the list
 *        of items is changed.
 */
class OVITO_GUI_EXPORT StableComboBox : public QComboBox
{
    Q_OBJECT

public:

    /// Constructor.
    using QComboBox::QComboBox;

    /// Replaces the list of items.
    void setItems(const QList<std::pair<QString, QVariant>>& itemsWithData);

    /// Replaces the list of items.
    void setItems(std::vector<std::unique_ptr<QStandardItem>> items);

    /// Returns the standard warning icon.
    static const QIcon& warningIcon();
};

}   // End of namespace
