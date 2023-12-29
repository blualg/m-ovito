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

#pragma once


#include <ovito/core/Core.h>

namespace Ovito {

/// Bit-flags controlling the behavior of a property field.
enum PropertyFieldFlag
{
    /// Selects the default behavior.
    PROPERTY_FIELD_NO_FLAGS                     = 0,
    /// Indicates that a reference field is a vector of references.
    PROPERTY_FIELD_VECTOR                       = (1<<1),
    /// Do not create automatic undo records when the value of the property or reference field changes.
    PROPERTY_FIELD_NO_UNDO                      = (1<<2),
    /// Marks a reference to an object as a weak one that doesn't keep the target object alive.
    PROPERTY_FIELD_WEAK_REF                     = (1<<3),
    /// Controls whether or not a ReferenceField::TargetChanged event should
    /// be generated each time the property value changes.
    PROPERTY_FIELD_NO_CHANGE_MESSAGE            = (1<<4),
    /// The target of the reference field is never cloned when the owning object is cloned.
    PROPERTY_FIELD_NEVER_CLONE_TARGET           = (1<<5),
    /// The target of the reference field is shallow/deep copied depending on the mode when the owning object is cloned.
    PROPERTY_FIELD_ALWAYS_CLONE                 = (1<<6),
    /// The target of the reference field is always deep-copied completely when the owning object is cloned.
    PROPERTY_FIELD_ALWAYS_DEEP_COPY             = (1<<7),
    /// Save the last value of the property in the application's settings store and use it to initialize
    /// the property when a new object instance is created.
    PROPERTY_FIELD_MEMORIZE                     = (1<<8),
    /// Indicates that the reference field is NOT an animatable parameter owned by the RefMaker object.
    PROPERTY_FIELD_NO_SUB_ANIM                  = (1<<9),
    /// Indicates that the object(s) stored in the reference field should not save their recomputable data to a scene file.
    PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA  = (1<<10),
    /// Blocks propagating messages sent by the target.
    PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES      = (1<<11),
    /// Automatically opens a sub-editor for the given reference field.
    PROPERTY_FIELD_OPEN_SUBEDITOR               = (1<<12),
    /// Automatically create a UI to reset this property field to its default.
    PROPERTY_FIELD_RESETTABLE                   = (1<<13),
    /// Never save the target(s) of the reference field to a state file.
    PROPERTY_FIELD_DONT_SAVE_TARGET             = (1<<14)
};
Q_DECLARE_FLAGS(PropertyFieldFlags, PropertyFieldFlag);

}   // End of namespace
