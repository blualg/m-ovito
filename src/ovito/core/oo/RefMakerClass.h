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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/OvitoClass.h>

namespace Ovito {

/**
 * \brief Meta-class for classes derived from RefMaker.
 */
class OVITO_CORE_EXPORT RefMakerClass : public OvitoClass
{
public:

    /// Inherit standard constructor from base meta class.
    using OvitoClass::OvitoClass;

    /// Returns the list of property fields of the class, including those of all parent classes.
    const std::vector<const PropertyFieldDescriptor*>& propertyFields() const { return _propertyFields; }

    /// Returns the head of the linked list of property fields defined by this class.
    /// The linked list DOESN'T include the fields of super classes.
    const PropertyFieldDescriptor* firstPropertyField() const { return _firstPropertyField; }

    /// Looks up the reference field with the given identifier that has been defined in the RefMaker-derived
    /// class or one of its super classes. If no such field is defined, nullptr is returned.
    const PropertyFieldDescriptor* findPropertyField(const char* identifier, bool searchSuperClasses = false) const;

protected:

    /// Is called by the system after construction of the meta-class instance.
    virtual void initialize() override;

private:

    /// Lists all property fields of the class, including those of all parent classes.
    std::vector<const PropertyFieldDescriptor*> _propertyFields;

    /// The linked list of property fields.
    const PropertyFieldDescriptor* _firstPropertyField = nullptr;

    friend class PropertyFieldDescriptor; // Requires mutable access to the linked list of property fields.
};

}   // End of namespace
