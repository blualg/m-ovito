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
#include "Property.h"
#include "OwnerPropertyRef.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Constructs a reference to a standard property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, int typeId) :
    _containerClass(pclass),
    _name(pclass->standardPropertyName(typeId))
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!_name.isEmpty());
}

/******************************************************************************
* Constructs a reference to a user-defined property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, const QString& name) :
    _containerClass(pclass),
    _name(name)
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(!name.isEmpty());
}

/******************************************************************************
* Constructs a reference based on an existing Property.
******************************************************************************/
OwnerPropertyRef::OwnerPropertyRef(PropertyContainerClassPtr pclass, const Property* property) :
    _containerClass(pclass),
    _name(property->name())
{
    OVITO_ASSERT(pclass);
}

/******************************************************************************
* Strict ordering function.
******************************************************************************/
bool OwnerPropertyRef::operator<(const OwnerPropertyRef& other) const
{
    if(containerClass() == other.containerClass())
        return name() < other.name();
    else
        return containerClass() < other.containerClass();
}

/******************************************************************************
* Writes a OwnerPropertyRef to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const OwnerPropertyRef& r)
{
    stream.beginChunk(0x04);
    stream << static_cast<const OvitoClassPtr&>(r.containerClass());
    stream << r.name();
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a OwnerPropertyRef from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, OwnerPropertyRef& r)
{
    int version = stream.expectChunkRange(0x02, 2);
    OvitoClassPtr clazz;
    stream >> clazz;
    r._containerClass = static_cast<PropertyContainerClassPtr>(clazz);
    if(version < 2) {
        int typeId;
        stream >> typeId;
    }
    stream >> r._name;
    if(version < 2) {
        int vectorComponentIndex;
        stream >> vectorComponentIndex;
    }
    if(version >= 1 && version < 2) {
        QString vectorComponentName;
        stream >> vectorComponentName;
    }
    if(!r._containerClass)
        r = OwnerPropertyRef();
    stream.closeChunk();
    return stream;
}

}   // End of namespace
