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
#include <ovito/core/oo/PropertyFieldDescriptor.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/dataset/DataSet.h>
#include "RefMakerClass.h"

namespace Ovito {

/******************************************************************************
* Is called by the system after construction of the meta-class instance.
******************************************************************************/
void RefMakerClass::initialize()
{
    OvitoClass::initialize();

    // Collect all property fields of the class hierarchy in one array.
    for(const RefMakerClass* clazz = this; clazz != &RefMaker::OOClass(); clazz = static_cast<const RefMakerClass*>(clazz->superClass())) {
        for(const PropertyFieldDescriptor* field = clazz->firstPropertyField(); field != nullptr; field = field->next()) {
            _propertyFields.push_back(field);
        }
    }
}

/******************************************************************************
* Searches for a property field defined in this class or one of its super classes.
******************************************************************************/
const PropertyFieldDescriptor* RefMakerClass::findPropertyField(const char* identifier, bool searchSuperClasses) const
{
    OVITO_ASSERT(identifier != nullptr);

    if(!searchSuperClasses) {
        for(const PropertyFieldDescriptor* field = firstPropertyField(); field; field = field->next()) {
            if(qstrcmp(field->identifier(), identifier) == 0 || qstrcmp(field->identifierAlias(), identifier) == 0)
                return field;
        }
    }
    else {
        for(const PropertyFieldDescriptor* field : _propertyFields) {
            if(qstrcmp(field->identifier(), identifier) == 0 || qstrcmp(field->identifierAlias(), identifier) == 0)
                return field;
        }
    }

    return nullptr;
}

}   // End of namespace
