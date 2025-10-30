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

#include <ovito/particles/Particles.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "BondType.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondType);
OVITO_CLASSINFO(BondType, "DisplayName", "Bond type");
DEFINE_PROPERTY_FIELD(BondType, radius);
SET_PROPERTY_FIELD_LABEL(BondType, radius, "Radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondType, radius, WorldParameterUnit, 0);

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void BondType::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath, bool forceProxyReplacement) const
{
    ElementType::updateEditableProxies(state, dataPath, forceProxyReplacement);

    // Note: 'this' may no longer exist at this point, because the base method implementation may
    // have already replaced it with a mutable copy.
    const BondType* self = static_object_cast<BondType>(dataPath.back());

    if(const BondType* proxy = static_object_cast<BondType>(self->editableProxy())) {
        if(proxy->radius() != self->radius()) {
            // Make this data object mutable first.
            BondType* mutableSelf = static_object_cast<BondType>(state.makeMutableInplace(dataPath));
            mutableSelf->setRadius(proxy->radius());
        }
    }
}

/******************************************************************************
* Returns a list of column names to be displayed in the data inspector for
* element types of this class.
******************************************************************************/
QStringList BondType::OOMetaClass::dataInspectorColumns() const
{
    QStringList columns = ElementTypeClass::dataInspectorColumns();
    columns << QStringLiteral("Radius");
    return columns;
}

/******************************************************************************
* Returns the Qt table model data for the given element type to be displayed in the data inspector.
******************************************************************************/
QVariant BondType::OOMetaClass::dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const
{
    if(role == Qt::DisplayRole) {
        if(const BondType* btype = dynamic_object_cast<BondType>(elementType)) {
            if(columnName == QStringLiteral("Radius")) {
                if(btype->radius() != 0)
                    return btype->radius();
                else
                    return {};
            }
        }
    }
    return ElementTypeClass::dataInspectorModelData(columnIndex, columnName, elementType, role);
}

}   // End of namespace
