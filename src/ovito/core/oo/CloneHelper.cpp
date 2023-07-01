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
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include "CloneHelper.h"

namespace Ovito {

/******************************************************************************
* This creates a copy of the RefTarget.
* If there has already a copy of this RefTarget
* created by this clone helper earlier, then it will be returned.
*    obj - The object to be cloned.
*    deepCopy - Controls whether sub-objects referenced by this RefTarget are copied too.
* Returns the copy of the source object.
******************************************************************************/
RefTarget* CloneHelper::cloneObjectImpl(const RefTarget* obj, bool deepCopy)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(obj == nullptr)
        return nullptr;

    OVITO_CHECK_OBJECT_POINTER(obj);

    if(RefTarget* clone = lookupCloneOf(obj))
        return clone;

    // Never generate undo records for a cloning operation.
    UndoSuspender noUndo;

    OORef<RefTarget> copy = obj->clone(deepCopy, *this);
    if(!copy)
        throw Exception(QString("Object of class %1 cannot be cloned. It does not implement the clone() method.").arg(obj->getOOClass().name()));

    OVITO_ASSERT_MSG(copy->getOOClass().isDerivedFrom(obj->getOOClass()), "CloneHelper::cloneObject", qPrintable(QString("The clone method of class %1 did not return a compatible class instance.").arg(obj->getOOClass().name())));

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    _cloneTable.emplace_back(obj, std::move(copy));
#else
    _cloneTable.push_back(std::make_pair(obj, std::move(copy)));
#endif
    return _cloneTable.back().second;
}

/******************************************************************************
* This creates a copy of a RefTarget.
******************************************************************************/
OORef<RefTarget> CloneHelper::cloneSingleObjectImpl(const RefTarget* obj, bool deepCopy)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(obj == nullptr)
        return nullptr;

    OVITO_CHECK_OBJECT_POINTER(obj);

    // Never generate undo records for a cloning operation.
    UndoSuspender noUndo;

    CloneHelper tempHelper;
    OORef<RefTarget> copy = obj->clone(deepCopy, tempHelper);
    if(!copy)
        throw Exception(QString("Object of class %1 cannot be cloned. It does not implement the clone() method.").arg(obj->getOOClass().name()));

    OVITO_ASSERT_MSG(copy->getOOClass().isDerivedFrom(obj->getOOClass()), "CloneHelper::cloneSingleObject", qPrintable(QString("The clone method of class %1 did not return a compatible class instance.").arg(obj->getOOClass().name())));

    return copy;
}

}   // End of namespace
