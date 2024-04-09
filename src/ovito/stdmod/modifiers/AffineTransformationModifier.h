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


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>

namespace Ovito {

/**
 * \brief Base class for AffineTransformationModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT AffineTransformationModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(AffineTransformationModifierDelegate)

protected:

    /// Abstract class constructor.
    using ModifierDelegate::ModifierDelegate;
};

/**
 * \brief Delegate for the AffineTransformationModifier that operates on simulation cells.
 */
class OVITO_STDMOD_EXPORT SimulationCellAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public AffineTransformationModifierDelegate::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AffineTransformationModifierDelegate::OOMetaClass::OOMetaClass;

        /// Asks the metaclass which data objects in the given input data collection the modifier delegate can operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("cell"); }
    };

    OVITO_CLASS_META(SimulationCellAffineTransformationModifierDelegate, OOMetaClass)

public:

    /// Constructor.
    using AffineTransformationModifierDelegate::AffineTransformationModifierDelegate;

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief Delegate for the AffineTransformationModifier that operates on lines.
 **/
class OVITO_STDMOD_EXPORT LinesAffineTransformationModifierDelegate : public AffineTransformationModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public AffineTransformationModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using AffineTransformationModifierDelegate::OOMetaClass::OOMetaClass;

        /// Asks the metaclass which data objects in the given input data collection the modifier delegate can operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesAffineTransformationModifierDelegate, OOMetaClass)

public:

    /// Constructor.
    using AffineTransformationModifierDelegate::AffineTransformationModifierDelegate;

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief This modifier applies an arbitrary affine transformation to the
 *        particles, the simulation box and other entities.
 *
 * The affine transformation is specified as a 3x4 matrix.
 */
class OVITO_STDMOD_EXPORT AffineTransformationModifier : public MultiDelegatingModifier
{
public:

    /// Give this modifier class its own metaclass.
    class OOMetaClass : public MultiDelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using MultiDelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return AffineTransformationModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(AffineTransformationModifier, OOMetaClass)

public:

    /// Constructor.
    explicit AffineTransformationModifier(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Returns the effective affine transformation matrix to be applied to points.
    /// It depends on the linear matrix, the translation vector, relative/target cell mode, and
    /// whether the translation is specified in terms of reduced cell coordinates.
    /// Thus, the affine transformation may depend on the current simulation cell shape.
    AffineTransformation effectiveAffineTransformation(const PipelineFlowState& state) const;

    /// Copies positions from one buffer to another while transforming them.
    /// If enabled, the transformation is only applied to selected elements.
    void transformCoordinates(const PipelineFlowState& inputState, const Property* input, Property* output, const Property* selection) const {
        transformCoordinates(effectiveAffineTransformation(inputState), selectionOnly(), input, output, selection);
    }

    /// Copies vectors from one buffer to another while transforming them.
    /// If enabled, the transformation is only applied to selected elements.
    void transformVectors(const PipelineFlowState& inputState, const Property* input, Property* output, const Property* selection) const {
        transformVectors(effectiveAffineTransformation(inputState), selectionOnly(), input, output, selection);
    }

    /// Copies positions from one buffer to another while transforming them.
    /// The transformation may be applied only to selected elements.
    static void transformCoordinates(const AffineTransformation tm, bool selectionOnly, const Property* input, Property* output, const Property* selection);

    /// Copies vectors from one buffer to another while transforming them.
    /// If enabled, the transformation is only applied to selected elements.
    static void transformVectors(const AffineTransformation tm, bool selectionOnly, const Property* input, Property* output, const Property* selection);

protected:

    /// This property fields stores the transformation matrix (used in 'relative' mode).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(AffineTransformation, transformationTM, setTransformationTM);

    /// This property fields stores the simulation cell geometry (used in 'absolute' mode).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(AffineTransformation, targetCell, setTargetCell);

    /// This controls whether the transformation is applied only to the selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, selectionOnly, setSelectionOnly);

    /// This controls whether a relative transformation is applied to the simulation box or
    /// the absolute cell geometry has been specified.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, relativeMode, setRelativeMode);

    /// Controls whether the translation vector is specified in reduced cell coordinated.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, translationReducedCoordinates, setTranslationReducedCoordinates);
};

}   // End of namespace
