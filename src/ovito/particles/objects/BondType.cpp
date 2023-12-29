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

#include <ovito/particles/Particles.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "BondType.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondType);
DEFINE_PROPERTY_FIELD(BondType, radius);
SET_PROPERTY_FIELD_LABEL(BondType, radius, "Radius");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(BondType, radius, WorldParameterUnit, 0);

/******************************************************************************
* Constructs a new BondType.
******************************************************************************/
BondType::BondType(ObjectInitializationFlags flags) : ElementType(flags), _radius(0.0)
{
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void BondType::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const
{
    ElementType::updateEditableProxies(state, dataPath);

    // Note: 'this' may no longer exist at this point, because the base method implementationmay
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

}   // End of namespace
