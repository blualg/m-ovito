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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "PeriodicDomainObject.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PeriodicDomainObject);
DEFINE_REFERENCE_FIELD(PeriodicDomainObject, domain);
DEFINE_PROPERTY_FIELD(PeriodicDomainObject, cuttingPlanes);
DEFINE_PROPERTY_FIELD(PeriodicDomainObject, title);
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, domain, "Domain");
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, cuttingPlanes, "Cutting planes");
SET_PROPERTY_FIELD_LABEL(PeriodicDomainObject, title, "Title");
SET_PROPERTY_FIELD_CHANGE_EVENT(PeriodicDomainObject, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
PeriodicDomainObject::PeriodicDomainObject(ObjectInitializationFlags flags, const QString& title) : DataObject(flags),
    _title(title)
{
}

/******************************************************************************
* Returns the display title of this object.
******************************************************************************/
QString PeriodicDomainObject::objectTitle() const
{
    if(!title().isEmpty()) return title();
    return DataObject::objectTitle();
}

}   // End of namespace
