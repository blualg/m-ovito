////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Base class for modifier delegates used by the DelegatingModifier and MultiDelegatingModifier classes.
 */
class OVITO_CORE_EXPORT ModifierDelegate : public RefTarget
{
public:

    /// Give modifier delegates their own metaclass.
    class OVITO_CORE_EXPORT ModifierDelegateClass : public RefTarget::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using RefTarget::OOMetaClass::OOMetaClass;

        /// Indicates which data objects in the given input data collection the modifier delegate is able to operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const {
            OVITO_ASSERT_MSG(false, "ModifierDelegate::OOMetaClass::getApplicableObjects()",
                qPrintable(QStringLiteral("Metaclass of modifier delegate class %1 does not override the getApplicableObjects() method.").arg(name())));
            return {};
        }

        /// Asks the metaclass which data objects in the given input pipeline state the modifier delegate can operate on.
        QVector<DataObjectReference> getApplicableObjects(const PipelineFlowState& input) const {
            if(!input) return {};
            return getApplicableObjects(*input.data());
        }

        /// Indicates which class of data objects the modifier delegate is able to operate on.
        virtual const DataObject::OOMetaClass& getApplicableObjectClass() const {
            OVITO_ASSERT_MSG(false, "ModifierDelegate::OOMetaClass::getApplicableObjectClass()",
                qPrintable(QStringLiteral("Metaclass of modifier delegate class %1 does not override the getApplicableObjectClass() method.").arg(name())));
            return DataObject::OOClass();
        }

        /// \brief The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const {
            OVITO_ASSERT_MSG(false, "ModifierDelegate::OOMetaClass::pythonDataName()",
                qPrintable(QStringLiteral("Metaclass of modifier delegate class %1 does not override the pythonDataName() method.").arg(name())));
            return {};
        }
    };

    OVITO_CLASS_META(ModifierDelegate, ModifierDelegateClass)

public:

    /// This function is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const {}

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) = 0;

    /// Returns the modifier owning this delegate.
    Modifier* modifier() const;

    /// Returns a short string to be displayed next to the modifier in the pipeline editor GUI.
    /// Strings from multiple delegates will be concatenated into a single, comma-separated string.
    virtual QString getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const { return inputDataObject().dataTitleOrPath(); }

protected:

    /// Visits the data objects in the given pipeline state that this delegate is supposed to operate on and calls the given function for each of them.
    template<typename DataObjectClass, typename Func>
    static void visitObjectsToBeProcessed(const PipelineFlowState& state, const DataObjectReference& inputObjectRef, const OOWeakRef<const PipelineNode>& modificationNode, Func&& func) {
        if(!state)
            return;

        // If the delegate is configured to operate on a specific input data object, only visit that one. Otherwise, visit all data
        // objects in the input collection and let the delegate decide which ones to operate on.
        if(inputObjectRef) {
            // The referenced object must exist in the data collection.
            ConstDataObjectPath objectPath = state.expectObject(inputObjectRef);
            // Make sure it has the expected type.
            const DataObjectClass* object = objectPath.lastAs<DataObjectClass>();
            if(!object) {
                // The reference may point not directly to the object of the expected type.
                // It may point to a child object instead (e.g. the SurfaceMeshRegions of a parent SurfaceMesh).
                // Look up the parent object if that's what the caller expects (see SurfaceMeshRegionsDeleteSelectedModifierDelegate).
                object = objectPath.nextToLastAs<DataObjectClass>();
            }
            if(!object) {
                throw Exception(tr("Invalid or incompatible input data object reference for modifier delegate: %1. "
                    "Expected a %2 object but reference resolved to an object of type %3.")
                    .arg(inputObjectRef.dataTitleOrPath())
                    .arg(DataObjectClass::OOClass().name())
                    .arg(objectPath.last()->getOOMetaClass().name()));
            }
            OVITO_ASSERT(object->createdByNode() != modificationNode);
            func(object);
        }
        else {
            state.data()->visitObjectsOfType<DataObjectClass>([&](const DataObjectClass* object) {
                // Skip objects created by the modifier node that is currently being evaluated to avoid
                // conflicts between different delegates.
                if(object->createdByNode() == modificationNode)
                    return;
                func(object);
            });
        }
    }

private:

    /// Indicates whether this delegate is active or not.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, isEnabled, setEnabled);

    /// Optionally specifies a particular input data object this delegate should operate on.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, inputDataObject, setInputDataObject);
};

}   // End of namespace
