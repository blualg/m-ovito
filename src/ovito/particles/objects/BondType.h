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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/stdobj/properties/Property.h>

namespace Ovito {

/**
 * \brief Stores the properties of a bond type, e.g. name, color, and radius.
 */
class OVITO_PARTICLES_EXPORT BondType : public ElementType
{
    /// Define a new metaclass.
    class BondTypeClass : public ElementTypeClass
    {
    public:
        /// Inherit constructor from base class.
        using ElementTypeClass::ElementTypeClass;

        /// Returns a list of column names to be displayed in the data inspector for element types of this class.
        virtual QStringList dataInspectorColumns() const override;

        /// Returns the Qt table model data for the given element type to be displayed in the data inspector.
        virtual QVariant dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const override;
    };
    OVITO_CLASS_META(BondType, BondTypeClass);

public:

    //////////////////////////////////// Utility methods ////////////////////////////////

    /// Builds a map from type identifiers to bond radii.
    static std::map<int,FloatType> typeRadiusMap(const Property* typeProperty) {
        std::map<int,FloatType> m;
        for(const ElementType* type : typeProperty->elementTypes())
            if(const BondType* bondType = dynamic_object_cast<BondType>(type))
                m.insert({ type->numericId(), bondType->radius() });
        return m;
    }

private:

    /// Stores the radius of the bond type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, radius, setRadius);
};

}   // End of namespace
