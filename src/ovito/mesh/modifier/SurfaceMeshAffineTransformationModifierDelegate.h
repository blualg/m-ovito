////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/core/dataset/data/mesh/TriMeshObject.h>
#include <ovito/stdmod/modifiers/AffineTransformationModifier.h>

namespace Ovito::Mesh {

using namespace Ovito::StdMod;

/**
 * \brief Delegate for the AffineTransformationModifier that operates on surface meshes.
 */
class OVITO_MESHMOD_EXPORT SurfaceMeshAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class SurfaceMeshAffineTransformationModifierDelegateClass : public AffineTransformationModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AffineTransformationModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override {
            QVector<DataObjectReference> result;
            if(input.containsObject<SurfaceMesh>())
                result.push_back(DataObjectReference(&SurfaceMesh::OOClass()));
            if(input.containsObject<TriMeshObject>())
                result.push_back(DataObjectReference(&TriMeshObject::OOClass()));
            return result;
        }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("surfaces"); }
    };

    OVITO_CLASS_META(SurfaceMeshAffineTransformationModifierDelegate, SurfaceMeshAffineTransformationModifierDelegateClass)
    Q_CLASSINFO("DisplayName", "Surfaces");

public:

    /// Constructor.
    Q_INVOKABLE SurfaceMeshAffineTransformationModifierDelegate(ObjectInitializationFlags flags) : AffineTransformationModifierDelegate(flags) {}

    /// Applies the modifier operation to the data in a pipeline flow state.
    virtual PipelineStatus apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

}   // End of namespace
