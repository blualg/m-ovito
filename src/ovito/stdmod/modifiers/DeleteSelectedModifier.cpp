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

#include <ovito/stdmod/StdMod.h>
#include "DeleteSelectedModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DeleteSelectedModifierDelegate);
IMPLEMENT_CREATABLE_OVITO_CLASS(DeleteSelectedModifier);
OVITO_CLASSINFO(DeleteSelectedModifier, "DisplayName", "Delete selected");
OVITO_CLASSINFO(DeleteSelectedModifier, "Description", "Remove all currently selected elements.");
OVITO_CLASSINFO(DeleteSelectedModifier, "ModifierCategory", "Modification");

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant DeleteSelectedModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(scene);

    // If there is exactly one enabled delegate, use its name as short info.
    QVariant shortInfo;
    for(const auto& delegate : delegates()) {
        if(delegate->isEnabled()) {
            if(!shortInfo.isNull())
                return {};  // More than one enabled delegate -> no short info.
            else
                shortInfo = delegate->objectTitle();
        }
    }
    return shortInfo;
}

}   // End of namespace
