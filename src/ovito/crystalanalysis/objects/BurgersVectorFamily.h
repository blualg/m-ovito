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


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/crystalanalysis/objects/Cluster.h>

namespace Ovito {

/**
 * \brief represents a dislocation type.
 */
class OVITO_CRYSTALANALYSIS_EXPORT BurgersVectorFamily : public ElementType
{
    OVITO_CLASS(BurgersVectorFamily)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, int id = 0, const QString& name = tr("Other"));

    /// Checks if the given Burgers vector is a member of this family.
    bool isMember(const Cluster::VecType& v, const MicrostructurePhase* latticeStructure) const;

private:

    /// This prototype Burgers vector of this family.
    DECLARE_MODIFIABLE_PROPERTY_FIELD((Vector3{0,0,0}), burgersVector, setBurgersVector);
    DECLARE_SHADOW_PROPERTY_FIELD(burgersVector);
};

}   // End of namespace
