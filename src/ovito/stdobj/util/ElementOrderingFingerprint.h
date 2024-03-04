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


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Helper class used to detect changes in the storage order of particles and other property containers.
 *
 * Modifiers can use this class to detect if the storage order or the number of of input elements
 * have changed, rendering any previously computed results invalid.
 */
class ElementOrderingFingerprint
{
public:

    /// Constructor.
    ElementOrderingFingerprint(const PropertyContainer* container) :
        _count(container->elementCount()),
        _identifiers(container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty) ? container->getProperty(Property::GenericIdentifierProperty) : nullptr) {}

    /// Returns the number of elements for which this object was constructed.
    size_t elementCount() const { return _count; }

    /// Returns true if the element number and the storage order have changed
    /// with respect to the state from which this object was constructed.
    bool hasChanged(const PropertyContainer* container) const {
        if(_count != container->elementCount())
            return true;
        if(container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty)) {
            if(const Property* identifiers = container->getProperty(Property::GenericIdentifierProperty)) {
                if(!_identifiers)
                    return true;
                if(identifiers != _identifiers) {
                    if(identifiers->checksum() != _identifiers->checksum())
                        return true;
                }
            }
            else if(_identifiers) {
                return true;
            }
        }
        return false;
    }

private:

    /// The total number of elements.
    size_t _count;

    /// The list of IDs (if available).
    ConstPropertyPtr _identifiers;
};

}   // End of namespace
