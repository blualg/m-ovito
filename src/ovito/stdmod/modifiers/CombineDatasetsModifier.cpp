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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/data/SyclFlatMap.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "CombineDatasetsModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CombineDatasetsModifier);
DEFINE_REFERENCE_FIELD(CombineDatasetsModifier, secondaryDataSource);
SET_PROPERTY_FIELD_LABEL(CombineDatasetsModifier, secondaryDataSource, "Secondary source");

IMPLEMENT_ABSTRACT_OVITO_CLASS(CombineDatasetsModifierDelegate);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
CombineDatasetsModifier::CombineDatasetsModifier(ObjectInitializationFlags flags) : MultiDelegatingModifier(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Generate the list of delegate objects.
        createModifierDelegates(CombineDatasetsModifierDelegate::OOClass());

        // Create the file source object, which will be responsible for loading
        // and caching the data to be merged.
        setSecondaryDataSource(OORef<FileSource>::create(flags));
    }
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> CombineDatasetsModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get the secondary data source.
    if(!secondaryDataSource())
        throw Exception(tr("No dataset to be merged has been provided."));

    // Get the state.
    PipelineEvaluationResult secondaryStateFuture = secondaryDataSource()->evaluate(request);

    // Wait for the data to become available.
    return secondaryStateFuture.then(*this, [this, state = std::move(state), request](const PipelineFlowState& secondaryState) mutable -> Future<PipelineFlowState> {

        // Make sure the obtained dataset is valid and ready to use.
        if(secondaryState.status().type() == PipelineStatus::Error) {
            if(FileSource* fileSource = dynamic_object_cast<FileSource>(secondaryDataSource())) {
                if(fileSource->sourceUrls().empty())
                    throw Exception(tr("Please pick an input file to be merged."));
            }
            state.setStatus(secondaryState.status());
            return std::move(state);
        }

        if(!secondaryState)
            throw Exception(tr("Secondary data source has not been specified yet or is empty. Please pick an input file to be merged."));

        // Merge validity intervals of primary and secondary datasets.
        state.intersectStateValidity(secondaryState.stateValidity());

        // Perform the merging of two pipeline states.
        return combineDatasets(request, std::move(state), secondaryState);
    });
}

/******************************************************************************
* Implementation method, which performs the merging of two pipeline states.
******************************************************************************/
Future<PipelineFlowState> CombineDatasetsModifier::combineDatasets(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& secondaryState)
{
    if(!state || !secondaryState)
        return std::move(state);

    // Merge validity intervals of primary and secondary datasets.
    state.intersectStateValidity(secondaryState.stateValidity());

    // Merge global attributes of primary and secondary datasets.
    for(const DataObject* obj : secondaryState.data()->objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            if(state.getAttributeValue(attribute->identifier()).isNull())
                state.addObject(attribute);
        }
    }

    // Combine surface meshes from primary and secondary datasets.
    for(const DataObject* obj : secondaryState.data()->objects()) {
        if(const SurfaceMesh* surfaceMesh = dynamic_object_cast<SurfaceMesh>(obj)) {
            if(!state.data()->contains(surfaceMesh))
                state.addObject(surfaceMesh);
        }
        else if(const TriangleMesh* triMesh = dynamic_object_cast<TriangleMesh>(obj)) {
            if(!state.data()->contains(surfaceMesh))
                state.addObject(triMesh);
        }
    }

    // Special handling for the simulation cell. If the secondary dataset contains a simulation cell but
    // the primary doesn't, then copy it over to the primary dataset.
    if(const SimulationCell* secondaryCell = secondaryState.getObject<SimulationCell>()) {
        const SimulationCell* primaryCell = state.getObject<SimulationCell>();
        if(!primaryCell) {
            state.addObject(secondaryCell);
        }
    }

    // Let the delegates do their job and merge the data objects of the two datasets.
    return applyDelegates(request, std::move(state), { std::reference_wrapper<const PipelineFlowState>(secondaryState) });
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool CombineDatasetsModifier::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::AnimationFramesChanged && source == secondaryDataSource()) {
        // Propagate animation interval events from the secondary source.
        return true;
    }
    return MultiDelegatingModifier::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data object of the node has been replaced.
******************************************************************************/
void CombineDatasetsModifier::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(secondaryDataSource) && !isBeingLoaded() && !isBeingDeleted()) {
        // The animation length might have changed when the secondary source has been replaced.
        notifyDependents(ReferenceEvent::AnimationFramesChanged);
    }
    MultiDelegatingModifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Helper method that merges the set of element types defined for a property.
******************************************************************************/
void CombineDatasetsModifierDelegate::mergeElementTypes(Property* property1, const Property* property2, CloneHelper& cloneHelper)
{
    // Check if input properties have the right format.
    if(!property2) return;
    if(property2->elementTypes().empty()) return;
    if(property1->componentCount() != 1 || property2->componentCount() != 1) return;
    if(property1->dataType() != Property::Int32 || property2->dataType() != Property::Int32) return;

    std::map<int,int> typeMap;
    for(const ElementType* type2 : property2->elementTypes()) {
        if(!type2->name().isEmpty()) {
            const ElementType* type1 = property1->elementType(type2->numericId());
            if(!type1 || type1->name() != type2->name())
                type1 = property1->elementType(type2->name());
            if(type1 == nullptr) {
                OORef<ElementType> type2clone = cloneHelper.cloneObject(type2, false);
                type2clone->setNumericId(property1->generateUniqueElementTypeId());
                property1->addElementType(type2clone);
                typeMap.insert(std::make_pair(type2->numericId(), type2clone->numericId()));
            }
            else if(type1->numericId() != type2->numericId()) {
                typeMap.insert(std::make_pair(type2->numericId(), type1->numericId()));
            }
        }
        else {
            const ElementType* type1 = property1->elementType(type2->numericId());
            if(!type1) {
                OORef<ElementType> type2clone = cloneHelper.cloneObject(type2, false);
                property1->addElementType(type2clone);
                OVITO_ASSERT(type2clone->numericId() == type2->numericId());
            }
            else if(!type1->name().isEmpty()) {
                OORef<ElementType> type2clone = cloneHelper.cloneObject(type2, false);
                type2clone->setNumericId(property1->generateUniqueElementTypeId());
                property1->addElementType(type2clone);
                typeMap.insert(std::make_pair(type2->numericId(), type2clone->numericId()));
            }
        }
    }

    // Remap the values stored in the type property.
    if(typeMap.empty() == false) {
#ifdef OVITO_USE_SYCL
        // Convert the mapping table into a SYCL-compatible data structure.
        const SyclFlatMap typeMapSycl = typeMap;

        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            // Access the type property array (just the sub-section of newly added entries).
            SyclBufferAccess<int32_t, access_mode::read_write> typeAcc(property1, property1->size() - property2->size(), property2->size(), cgh);
            // Access mapping table.
            auto typeMapAcc = typeMapSycl.get_access(cgh);

            OVITO_SYCL_PARALLEL_FOR(cgh, CombineDatasetsModifierDelegate_mergeElementTypes)(sycl::range(typeAcc.size()), [=](size_t i) {
                // Use the mapping table to update type IDs in the property array.
                if(auto iter = typeMapAcc.find(typeAcc[i]); iter != typeMapAcc.end())
                    typeAcc[i] = (*iter).second;
            });
        });
#else
        BufferWriteAccess<int32_t, access_mode::read_write> typeArray1(property1);
        auto p = typeArray1.begin() + (property1->size() - property2->size());
        auto p_end = typeArray1.end();
        for(; p != p_end; ++p) {
            if(auto item = typeMap.find(*p); item != typeMap.end())
                *p = item->second;
        }
#endif
    }
}

}   // End of namespace
