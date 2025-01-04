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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/vectors/Vectors.h>
#include "ExpressionSelectionModifier.h"
#include "DeleteSelectedModifier.h"
#include "ComputePropertyModifier.h"

namespace Ovito {

/**
 * \brief Delegate for the ExpressionSelectionModifier that operates on vectors.
 */
class VectorsExpressionSelectionModifierDelegate : public ExpressionSelectionModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ExpressionSelectionModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ExpressionSelectionModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Vectors::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsExpressionSelectionModifierDelegate, OOMetaClass)
};

/**
 * \brief Delegate for the DeleteSelectedModifier that operates on lines.
 */
class VectorsDeleteSelectedModifierDelegate : public DeleteSelectedModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public DeleteSelectedModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DeleteSelectedModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsDeleteSelectedModifierDelegate, OOMetaClass)

public:
    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state,
                                            const PipelineFlowState& originalState,
                                            const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief Delegate plugin for the ComputePropertyModifier that operates on lines.
 */
class VectorsComputePropertyModifierDelegate : public ComputePropertyModifierDelegate
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
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const override { return Vectors::OOClass(); }

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsComputePropertyModifierDelegate, OOMetaClass)
};

}  // namespace Ovito
