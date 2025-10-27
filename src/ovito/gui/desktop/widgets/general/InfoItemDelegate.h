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
 * This Qt item delegate class that can display an additional info string or color next to each item.
 *
 * To use this delegate with a QAbstractItemView widget, subclass QAbstractItemModel and reimplement the data() method to return
 * the info data for the custom `infoRole` specified in the InfoItemDelegate constructor. The delegate will then render the info next to each item.
 */
class InfoItemDelegate : public QStyledItemDelegate
{
public:

    /// Constructor.
    explicit InfoItemDelegate(QObject* parent, int infoRole) : QStyledItemDelegate(parent), _infoRole(infoRole) {}

    /// Renders the item.
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Returns the Qt data role used to obtain the info data from the model.
    int infoRole() const { return _infoRole; }

private:

    /// Blend two RGB colors.
    static QColor blendColors(const QColor& color1, const QColor& color2, qreal ratio)
    {
        int r = color1.red() * (1 - ratio) + color2.red() * ratio;
        int g = color1.green() * (1 - ratio) + color2.green() * ratio;
        int b = color1.blue() * (1 - ratio) + color2.blue() * ratio;
        return QColor(r, g, b);
    }

private:

    /// The role used to obtain the info data from the model.
    int _infoRole;
};

}   // End of namespace
