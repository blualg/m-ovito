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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/stdobj/util/ElementOrderingFingerprint.h>
#include <ovito/stdmod/modifiers/ComputePropertyModifier.h>

namespace Ovito {

/**
 * \brief Delegate plugin for the ComputePropertyModifier that operates on particles.
 */
class OVITO_PARTICLES_EXPORT ParticlesComputePropertyModifierDelegate : public ComputePropertyModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ComputePropertyModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using ComputePropertyModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Particles::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("particles"); }
    };

    OVITO_CLASS_META(ParticlesComputePropertyModifierDelegate, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "Particles");

public:

    /// Constructor.
    explicit ParticlesComputePropertyModifierDelegate(ObjectInitializationFlags flags);

    /// \brief Sets the math expression that is used to compute the neighbor-terms of the property function.
    /// \param index The property component for which the expression should be set.
    /// \param expression The math formula.
    void setNeighborExpression(const QString& expression, int index = 0) {
        if(index < 0 || index >= neighborExpressions().size())
            throw Exception("Property component index is out of range.");
        QStringList copy = _neighborExpressions;
        copy[index] = expression;
        setNeighborExpressions(std::move(copy));
    }

    /// \brief Returns the math expression that is used to compute the neighbor-terms of the property function.
    /// \param index The property component for which the expression should be returned.
    /// \return The math formula.
    const QString& neighborExpression(int index = 0) const {
        if(index < 0 || index >= neighborExpressions().size())
            throw Exception("Property component index is out of range.");
        return neighborExpressions()[index];
    }

    /// Sets the number of vector components of the property to compute.
    virtual void setComponentCount(int componentCount) override;

    /// Checks if math expressions are time-dependent, i.e. whether they involve the animation frame number.
    virtual bool isExpressionTimeDependent(ComputePropertyModifier* modifier) const override;

protected:

    /// Launches the actual computations.
    virtual Future<PipelineFlowState> performComputation(const ComputePropertyModifier* modifier, ComputePropertyModificationNode* modNode, PipelineFlowState state, const PipelineFlowState& originalState, PropertyPtr outputProperty, ConstPropertyPtr selectionProperty, int frame) const override;

private:

    /// The math expressions for calculating the neighbor-terms of the property function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QStringList, neighborExpressions, setNeighborExpressions);

    /// Controls the cutoff radius for the neighbor lists.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether multi-line input fields are shown in the UI for the expressions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, useMultilineFields, setUseMultilineFields);
};

}   // End of namespace
