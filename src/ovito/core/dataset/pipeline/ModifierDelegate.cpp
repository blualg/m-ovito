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

#include <ovito/core/Core.h>
#include "ModifierDelegate.h"
#include "DelegatingModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ModifierDelegate);
OVITO_CLASSINFO(ModifierDelegate, "ClassNameAlias", "AsynchronousModifierDelegate");  // For backward compatibility with OVITO 3.2.1
DEFINE_PROPERTY_FIELD(ModifierDelegate, isEnabled);
DEFINE_PROPERTY_FIELD(ModifierDelegate, inputDataObject);
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, isEnabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, inputDataObject, "Data object");

/******************************************************************************
* Returns the modifier to which this delegate belongs.
******************************************************************************/
Modifier* ModifierDelegate::modifier() const
{
    Modifier* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        if(DelegatingModifier* modifier = dynamic_object_cast<DelegatingModifier>(dependent)) {
            if(modifier->delegate() == this) result = modifier;
        }
        else if(MultiDelegatingModifier* modifier = dynamic_object_cast<MultiDelegatingModifier>(dependent)) {
            if(modifier->delegates().contains(const_cast<ModifierDelegate*>(this))) result = modifier;
        }
    });
    return result;
}

}   // End of namespace
