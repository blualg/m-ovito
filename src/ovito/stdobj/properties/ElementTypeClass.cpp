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

#include <ovito/stdobj/StdObj.h>
#include "ElementTypeClass.h"
#include "ElementType.h"

namespace Ovito {

/******************************************************************************
* Returns a list of column names to be displayed in the data inspector for
* element types of this class.
******************************************************************************/
QStringList ElementTypeClass::dataInspectorColumns() const
{
    return {
        QStringLiteral("ID"),
        QStringLiteral("Type Name"),
    };
}

/******************************************************************************
* Returns the Qt table model data for the given element type to be displayed in the data inspector.
******************************************************************************/
QVariant ElementTypeClass::dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const
{
    if(role == Qt::DisplayRole) {
        if(columnIndex == 0)
            return elementType->numericId();
        else if(columnIndex == 1)
            return elementType->nameOrNumericId();
    }
    else if(role == Qt::DecorationRole) {
        if(columnIndex == 1)
            return static_cast<QColor>(elementType->color());
    }
    return {};
}

}   // End of namespace
