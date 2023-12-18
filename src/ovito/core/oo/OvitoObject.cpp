////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/oo/OvitoClass.h>
#include "OvitoObject.h"

namespace Ovito {

// The class descriptor instance for the OvitoObject class.
const OvitoClass OvitoObject::__OOClass_instance{QStringLiteral("OvitoObject"), nullptr, OVITO_PLUGIN_NAME, nullptr, &__OOClass_metadata_head};

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
OvitoObject::~OvitoObject()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(!isBeingConstructed());
    OVITO_ASSERT(isBeingDeleted());
    _magicAliveCode = 0xFEDCBA87;
}
#endif

/******************************************************************************
* Internal method that calls this object's aboutToBeDeleted() routine.
* It is automatically called when the object's reference counter reaches zero.
******************************************************************************/
void OvitoObject::deleteObjectInternal() noexcept
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT(!isBeingDeleted());
    OVITO_ASSERT(!isBeingConstructed());

#if 0 // TODO
    // Delete the object in the main thread only.
    if(QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "deleteObjectInternal", Qt::QueuedConnection);
        return;
    }
#endif

    // Mark this object as being deleted.
    _flags.setFlag(BeingDeleted);
    aboutToBeDeleted();
}

/******************************************************************************
* Prints an object to Qt debug stream.
******************************************************************************/
QDebug operator<<(QDebug dbg, const OvitoObject* o)
{
    QDebugStateSaver saver(dbg);
    if(!o)
        return dbg << "OvitoObject(0x0)";
    dbg.nospace() << o->getOOClass().className() << '(' << (const void *)o << ')';
    return dbg;
}

}   // End of namespace
