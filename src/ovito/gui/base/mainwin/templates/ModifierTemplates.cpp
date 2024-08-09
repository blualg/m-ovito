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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/Application.h>
#include "ModifierTemplates.h"

namespace Ovito {

/******************************************************************************
* Returns the singleton instance of this class.
******************************************************************************/
ModifierTemplates* ModifierTemplates::get()
{
    static ModifierTemplates* instance = new ModifierTemplates(Application::instance());
    return instance;
}

/******************************************************************************
* Constructor.
******************************************************************************/
ModifierTemplates::ModifierTemplates(QObject* parent) : ObjectTemplates(QStringLiteral("core/modifier/templates/"), tr("Modifier"), parent)
{
}

}   // End of namespace
