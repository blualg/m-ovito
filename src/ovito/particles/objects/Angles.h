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


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief This data object stores a list of molecular angles, i.e. triplets of particles.
 */
class OVITO_PARTICLES_EXPORT Angles : public PropertyContainer
{
    /// Define a new property metaclass for the property container.
    class OVITO_PARTICLES_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// \brief Create a storage object for standard properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override { return this->displayName(); }

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(Angles, OOMetaClass);
    OVITO_CLASSINFO("DisplayName", "Angles");
    OVITO_CLASSINFO("ClassNameAlias", "AnglesObject");  // For backward compatibility with OVITO 3.9.2

public:

    /// \brief The list of standard angle properties.
    enum Type {
        UserProperty = Property::GenericUserProperty,
        TypeProperty = Property::GenericTypeProperty,
        TopologyProperty,
    };

    /// Constructor.
    explicit Angles(ObjectInitializationFlags flags);

    /// Convinience method that returns the angle topology property.
    const Property* getTopology() const { return getProperty(TopologyProperty); }
};

/**
 * The data type used for the 'Topology' angle property: three indices into the particles list.
 */
using ParticleIndexTriplet = std::array<int64_t, 3>;

}   // End of namespace
