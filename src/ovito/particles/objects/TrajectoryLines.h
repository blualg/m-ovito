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

#pragma once


#include <ovito/particles/Particles.h>
// #include <ovito/stdobj/lines/Lines.h>
#include "Lines.h"

namespace Ovito {

/**
 * \brief Stores trajectory lines of a particles dataset.
 */
class OVITO_PARTICLES_EXPORT TrajectoryLines : public Lines
{
    /// Define a new property metaclass for this property container type.
    class OVITO_PARTICLES_EXPORT OOMetaClass : public Lines::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using Lines::OOMetaClass::OOMetaClass;

        /// Creates a storage object for standard properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(TrajectoryLines, OOMetaClass);
    Q_CLASSINFO("DisplayName", "Particle trajectories");
    Q_CLASSINFO("ClassNameAlias", "TrajectoryObject");  // For backward compatibility with OVITO 3.9.2

public:
    /// \brief Constructor.
    Q_INVOKABLE TrajectoryLines(ObjectInitializationFlags flags);
};

}   // End of namespace
