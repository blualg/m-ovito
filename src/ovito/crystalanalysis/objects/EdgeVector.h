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
#include "Cluster.h"
#include "ClusterGraph.h"

namespace Ovito {

/**
 * A cluster vector combined with a cluster transition.
 */
class OVITO_CRYSTALANALYSIS_EXPORT EdgeVector
{
public:

    /// Default constructor, which creates an invalid edge vector.
    EdgeVector() = default;

    /// Constructor.
    EdgeVector(const Vector3& vec, ClusterTransition* transition) : _vec(vec), _transition(transition) {}

    /// Returns the XYZ components of the vector expressed in the local coordinate system of the first cluster.
    const Vector3& vec() const { return _vec; }

    /// Returns the cluster transition from the cluster at the vector base to the cluster at the vector head.
    ClusterTransition* transition() const { return _transition; }

    /// Returns true if this edge vector is valid, i.e. if it is associated with a cluster transition.
    bool isValid() const { return _transition != nullptr; }

    /// Returns the inverse edge vector.
    EdgeVector operator-() const {
        OVITO_ASSERT(isValid());
        return { transition()->transform(-vec()), transition()->reverse };
    }

private:

    /// The XYZ components of the vector expressed in the local lattice coordinate system of the first cluster.
    Vector3 _vec = Vector3::Zero();

    /// The cluster transition from the cluster at the vector base to the cluster at the vector head.
    ClusterTransition* _transition = nullptr;
};

}   // End of namespace
