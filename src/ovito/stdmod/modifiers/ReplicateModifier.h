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


#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>

namespace Ovito {

/**
 * \brief Base class for ReplicateModifier delegates that operate on different kinds of data.
 */
class OVITO_STDMOD_EXPORT ReplicateModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(ReplicateModifierDelegate)

protected:

    /// Abstract class constructor.
    using ModifierDelegate::ModifierDelegate;
};

/**
 * \brief Delegate for the ReplicateModifier that operates on lines.
 */
class OVITO_STDMOD_EXPORT LinesReplicateModifierDelegate : public ReplicateModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class OOMetaClass : public ReplicateModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using ReplicateModifierDelegate::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesReplicateModifierDelegate, OOMetaClass)

    Q_CLASSINFO("DisplayName", "Lines");

public:
    /// Constructor.
    Q_INVOKABLE LinesReplicateModifierDelegate(ObjectInitializationFlags flags) : ReplicateModifierDelegate(flags) {}

    /// Applies the modifier operation to the data in a pipeline flow state.
    virtual PipelineStatus apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState,
                                 const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief This modifier duplicates data elements (e.g. particles) multiple times and shifts them by
 *        the simulation cell vectors to visualize periodic images.
 */
class OVITO_STDMOD_EXPORT ReplicateModifier : public MultiDelegatingModifier
{
public:

    /// Give this modifier class its own metaclass.
    class OOMetaClass : public MultiDelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using MultiDelegatingModifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return ReplicateModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(ReplicateModifier, OOMetaClass)
    Q_CLASSINFO("DisplayName", "Replicate");
    Q_CLASSINFO("Description", "Duplicate the dataset to visualize periodic images of the system.");
    Q_CLASSINFO("ModifierCategory", "Modification");

public:

    /// \brief Constructs a new instance of this class.
    Q_INVOKABLE ReplicateModifier(ObjectInitializationFlags flags);

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Helper function that returns the range of replicated boxes.
    Box3I replicaRange() const;

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return tr("%1 x %2 x %3").arg(numImagesX()).arg(numImagesY()).arg(numImagesZ()); }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Controls the number of periodic images generated in the X direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, numImagesX, setNumImagesX);
    /// Controls the number of periodic images generated in the Y direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, numImagesY, setNumImagesY);
    /// Controls the number of periodic images generated in the Z direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, numImagesZ, setNumImagesZ);

    /// Controls whether the size of the simulation box is adjusted to the extended system.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, adjustBoxSize, setAdjustBoxSize);

    /// Controls whether the modifier assigns unique identifiers to particle copies.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, uniqueIdentifiers, setUniqueIdentifiers);
};

}   // End of namespace
