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

#include <ovito/core/Core.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "DelegatingModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ModifierDelegate);
DEFINE_PROPERTY_FIELD(ModifierDelegate, isEnabled);
DEFINE_PROPERTY_FIELD(ModifierDelegate, inputDataObject);
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, isEnabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, inputDataObject, "Data object");

IMPLEMENT_ABSTRACT_OVITO_CLASS(DelegatingModifier);
DEFINE_REFERENCE_FIELD(DelegatingModifier, delegate);

IMPLEMENT_ABSTRACT_OVITO_CLASS(MultiDelegatingModifier);
DEFINE_VECTOR_REFERENCE_FIELD(MultiDelegatingModifier, delegates);

/******************************************************************************
* Returns the modifier to which this delegate belongs.
******************************************************************************/
Modifier* ModifierDelegate::modifier() const
{
    Modifier* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        if(DelegatingModifier* modifier = dynamic_object_cast<DelegatingModifier>(dependent)) {
            if(modifier->delegate() == this) result = modifier;
        }
        else if(MultiDelegatingModifier* modifier = dynamic_object_cast<MultiDelegatingModifier>(dependent)) {
            if(modifier->delegates().contains(const_cast<ModifierDelegate*>(this))) result = modifier;
        }
    });
    return result;
}

/******************************************************************************
* Creates a default delegate for this modifier.
******************************************************************************/
void DelegatingModifier::createDefaultModifierDelegate(const OvitoClass& delegateType, const QString& defaultDelegateTypeName)
{
    OVITO_ASSERT(delegateType.isDerivedFrom(ModifierDelegate::OOClass()));

    // Find the delegate type that corresponds to the given name string.
    for(OvitoClassPtr clazz : PluginManager::instance().listClasses(delegateType)) {
        if(clazz->name() == defaultDelegateTypeName) {
            OORef<ModifierDelegate> delegate = static_object_cast<ModifierDelegate>(clazz->createInstance());
            setDelegate(delegate);
            break;
        }
    }
    OVITO_ASSERT_MSG(delegate(), "DelegatingModifier::createDefaultModifierDelegate", qPrintable(QStringLiteral("There is no delegate class named '%1' inheriting from %2.").arg(defaultDelegateTypeName).arg(delegateType.name())));
}

/******************************************************************************
* Asks the metaclass whether the modifier can be applied to the given input data.
******************************************************************************/
bool DelegatingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    if(!ModifierClass::isApplicableTo(input))
        return false;

    // Check if there is any modifier delegate that could handle the input data.
    for(const ModifierDelegate::OOMetaClass* clazz : PluginManager::instance().metaclassMembers<ModifierDelegate>(delegateMetaclass())) {
        if(clazz->getApplicableObjects(input).empty() == false)
            return true;
    }
    return false;
}

/******************************************************************************
* This function is called by the pipeline system before a new modifier evaluation begins.
******************************************************************************/
bool DelegatingModifier::preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const
{
    if(!delegate() || !delegate()->isEnabled())
        return true;

    return delegate()->preEvaluationRun(request, result);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> DelegatingModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState input)
{
    // Apply the modifier delegate to the input data.
    return applyDelegate(request, std::move(input));
}

/******************************************************************************
* Lets the modifier's delegate operate on a pipeline flow state.
******************************************************************************/
Future<PipelineFlowState> DelegatingModifier::applyDelegate(const ModifierEvaluationRequest& request, PipelineFlowState input, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    OVITO_ASSERT(!isUndoRecording());
    OVITO_ASSERT(request.modifier() == this);

    if(!delegate() || !delegate()->isEnabled())
        return std::move(input);

    // Skip function if not applicable to the given input.
    if(delegate()->getOOMetaClass().getApplicableObjects(input).empty())
        throw Exception(tr("The modifier's pipeline input does not contain the expected kind of data."));

    // Call the delegate function.
    return delegate()->apply(request, input, input, additionalInputs);
}

/******************************************************************************
* This function is called by the pipeline system before a new modifier evaluation begins.
******************************************************************************/
bool MultiDelegatingModifier::preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const
{
    bool returnValue = true;

    for(const ModifierDelegate* delegate : delegates()) {
        if(delegate && delegate->isEnabled()) {
            returnValue = delegate->preEvaluationRun(request, result) && returnValue;
        }
    }

    return returnValue;
}

/******************************************************************************
* Creates the list of delegate objects for this modifier.
******************************************************************************/
void MultiDelegatingModifier::createModifierDelegates(const OvitoClass& delegateType)
{
    OVITO_ASSERT(delegateType.isDerivedFrom(ModifierDelegate::OOClass()));

    // Generate the list of delegate objects.
    if(delegates().empty()) {
        for(OvitoClassPtr clazz : PluginManager::instance().listClasses(delegateType)) {
            _delegates.push_back(this, PROPERTY_FIELD(delegates), static_object_cast<ModifierDelegate>(clazz->createInstance()));
        }
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool MultiDelegatingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    if(!ModifierClass::isApplicableTo(input))
        return false;

    // Check if there is any modifier delegate that could handle the input data.
    for(const ModifierDelegate::OOMetaClass* clazz : PluginManager::instance().metaclassMembers<ModifierDelegate>(delegateMetaclass())) {
        if(clazz->getApplicableObjects(input).empty() == false)
            return true;
    }

    return false;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> MultiDelegatingModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState input)
{
    // Apply all enabled modifier delegates to the input data.
    return applyDelegates(request, std::move(input));
}

/******************************************************************************
* Lets the registered modifier delegates operate on a pipeline flow state.
******************************************************************************/
Future<PipelineFlowState> MultiDelegatingModifier::applyDelegates(const ModifierEvaluationRequest& request, PipelineFlowState input, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    OVITO_ASSERT(!isUndoRecording());
    OVITO_ASSERT(request.modifier() == this);

    Future<PipelineFlowState> future = input;

    for(ModifierDelegate* delegate : delegates()) {

        // Skip function if not applicable.
        if(!input.data() || !delegate || !delegate->isEnabled() || delegate->getOOMetaClass().getApplicableObjects(*input.data()).empty())
            continue;

        // Call the delegate function.
        future.postprocess(*delegate, [delegate, request, input, additionalInputs](PipelineFlowState state) {
            return delegate->apply(request, std::move(state), input, additionalInputs);
        });
    }

    return future;
}

}   // End of namespace
