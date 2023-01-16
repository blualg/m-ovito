////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
const OvitoClass OvitoObject::__OOClass_instance{QStringLiteral("OvitoObject"), nullptr, OVITO_PLUGIN_NAME, &OvitoObject::staticMetaObject};

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
OvitoObject::~OvitoObject()
{
    OVITO_CHECK_OBJECT_POINTER(this);
    OVITO_ASSERT_MSG(objectReferenceCount() == 0, "~OvitoObject()", "Destroying an object whose reference counter is non-zero.");
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
    OVITO_ASSERT_MSG(_referenceCount.loadAcquire() == 0, "OvitoObject::deleteObjectInternal()", "Object is still referenced while being deleted.");

    // Delete the object in the main thread only.
    if(QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "deleteObjectInternal", Qt::QueuedConnection);
        return;
    }

    // Set the reference counter to a positive value to prevent the object
    // from being deleted a second time during the call to aboutToBeDeleted().
    _referenceCount.storeRelease(INVALID_REFERENCE_COUNT);
    aboutToBeDeleted();

    // After returning from aboutToBeDeleted(), the reference count should be back at the
    // original value (no new references).
    OVITO_ASSERT(_referenceCount.loadAcquire() == INVALID_REFERENCE_COUNT);
    _referenceCount.storeRelease(0);

    // Delete the object itself.
    delete this;
}

/******************************************************************************
* Returns true if this object is currently being loaded from an ObjectLoadStream.
******************************************************************************/
bool OvitoObject::isBeingLoaded() const
{
    return (qobject_cast<ObjectLoadStream*>(parent()) != nullptr);
}

}   // End of namespace
