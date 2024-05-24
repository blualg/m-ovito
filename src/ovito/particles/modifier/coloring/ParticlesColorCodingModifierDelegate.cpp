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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/vectors/VectorVis.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "ParticlesColorCodingModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesColorCodingModifierDelegate);
OVITO_CLASSINFO(ParticlesColorCodingModifierDelegate, "DisplayName", "Particles");
IMPLEMENT_CREATABLE_OVITO_CLASS(ParticleVectorsColorCodingModifierDelegate);
OVITO_CLASSINFO(ParticleVectorsColorCodingModifierDelegate, "DisplayName", "Particle vectors");
IMPLEMENT_CREATABLE_OVITO_CLASS(BondsColorCodingModifierDelegate);
OVITO_CLASSINFO(BondsColorCodingModifierDelegate, "DisplayName", "Bonds");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticleVectorsColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        for(const Property* property : particles->properties()) {
            if(property->visElement<VectorVis>() != nullptr)
                return { DataObjectReference(&Particles::OOClass()) };
        }
    }
    return {};
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds())
            return { DataObjectReference(&Particles::OOClass()) };
    }
    return {};
}

}   // End of namespace
