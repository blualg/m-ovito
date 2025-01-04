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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/SyclFlatSet.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "SelectTypeModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SelectTypeModifier);
OVITO_CLASSINFO(SelectTypeModifier, "DisplayName", "Select type");
OVITO_CLASSINFO(SelectTypeModifier, "Description", "Select particles based on chemical species, or bonds based on bond type.");
OVITO_CLASSINFO(SelectTypeModifier, "ModifierCategory", "Selection");
DEFINE_PROPERTY_FIELD(SelectTypeModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(SelectTypeModifier, selectedTypeIDs);
DEFINE_PROPERTY_FIELD(SelectTypeModifier, selectedTypeNames);
SET_PROPERTY_FIELD_LABEL(SelectTypeModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(SelectTypeModifier, selectedTypeIDs, "Selected type IDs");
SET_PROPERTY_FIELD_LABEL(SelectTypeModifier, selectedTypeNames, "Selected type names");

/******************************************************************************
* Constructor.
******************************************************************************/
void SelectTypeModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void SelectTypeModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    if(!sourceProperty() && subject()) {

        // When the modifier is first inserted, automatically select the most recently added
        // typed property (in GUI mode) or the canonical type property (in script mode).
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                if(property->isTypedProperty()) {
                    if(this_task::isInteractive() || property->typeId() == Property::GenericTypeProperty) {
                        bestProperty = property;
                    }
                }
            }
            setSourceProperty(bestProperty);
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void SelectTypeModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if((field == PROPERTY_FIELD(SelectTypeModifier::sourceProperty) || field == PROPERTY_FIELD(SelectTypeModifier::selectedTypeIDs)) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of SelectTypeModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> SelectTypeModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No input element type selected."));
    if(!sourceProperty())
        throw Exception(tr("No input property selected."));

    PropertyContainer* container = state.expectMutableLeafObject(subject());
    container->verifyIntegrity();

    // Get the input property.
    const Property* typePropertyObject = sourceProperty().findInContainer(container);
    if(!typePropertyObject)
        throw Exception(tr("The selected input property '%1' is not present.").arg(sourceProperty().name()));
    if(typePropertyObject->componentCount() != 1)
        throw Exception(tr("The input property '%1' has the wrong number of components. Must be a scalar property.").arg(typePropertyObject->name()));
    if(typePropertyObject->dataType() != Property::Int32)
        throw Exception(tr("The input property '%1' has the wrong data type. Must be a 32-bit integer property.").arg(typePropertyObject->name()));

    // Generate set of numeric type IDs to select.
    QSet<int32_t> idsToSelect = selectedTypeIDs();
    // Convert type names to numeric IDs.
    for(const QString& typeName : selectedTypeNames()) {
        if(const ElementType* t = typePropertyObject->elementType(typeName))
            idsToSelect.insert(t->numericId());
        else {
            bool found = false;
            for(const ElementType* t : typePropertyObject->elementTypes()) {
                if(t->nameOrNumericId() == typeName) {
                    found = true;
                    idsToSelect.insert(t->numericId());
                    break;
                }
            }
            if(!found)
                throw Exception(tr("Type '%1' does not exist in the type list of property '%2'.").arg(typeName).arg(typePropertyObject->name()));
        }
    }

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            container,
            typePropertyObject,
            idsToSelect = std::move(idsToSelect),
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        // Counts the number of selected elements.
        size_t nSelected = 0;

        // Create the selection property.
        Property* selProperty = container->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty);

#ifdef OVITO_USE_SYCL
        if(typePropertyObject->size() != 0) {
            if(!idsToSelect.empty()) {
                // Convert set of type IDs into a SYCL-compatible data structure.
                SyclFlatSet idsToSelectSycl{std::set<int32_t>{idsToSelect.begin(), idsToSelect.end()}};
                // This is a single-element counter variable that will be incremented by the kernel for each selected element.
                sycl::buffer<size_t> numSelectedBuf(&nSelected, 1);
                this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                    // Access the input type values.
                    SyclBufferAccess<int32_t, access_mode::read> typeAcc(typePropertyObject, cgh);
                    // Access output selection array.
                    SyclBufferAccess<SelectionIntType, access_mode::write> selectionAcc(selProperty, cgh, DataBuffer::Uninitialized);
                    // Access type ID set.
                    auto idsToSelectAcc = idsToSelectSycl.get_access(cgh);
#ifdef OVITO_USE_SYCL_ACPP
                    auto reduction = sycl::reduction(sycl::accessor{numSelectedBuf, cgh, sycl::no_init}, size_t{0}, sycl::plus<size_t>());
#else
                    auto reduction = sycl::reduction(numSelectedBuf, cgh, size_t{0}, sycl::plus<size_t>(), sycl::property::reduction::initialize_to_identity{});
#endif
                    OVITO_SYCL_PARALLEL_FOR(cgh, SelectTypeModifier_kernel)(sycl::range(typePropertyObject->size()), reduction, [=](size_t i, auto& red) {
                        if(idsToSelectAcc.contains(typeAcc[i])) {
                            selectionAcc[i] = 1;
                            red += (size_t)1;
                        }
                        else {
                            selectionAcc[i] = 0;
                        }
                    });
                });
            }
            else {
                selProperty->fill<SelectionIntType>(0);
            }
        }
#else
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> selectionAcc{selProperty};
        BufferReadAccess<int32_t> typeAcc{typePropertyObject};

        boost::transform(typeAcc, selectionAcc.begin(), [&](int32_t type) {
            if(idsToSelect.contains(type)) {
                nSelected++;
                return 1;
            }
            return 0;
        });
#endif

        // To speed up future queries, store the selection count in the selection property object.
        selProperty->setNonzeroCount(nSelected);

        state.addAttribute(QStringLiteral("SelectType.num_selected"), QVariant::fromValue(nSelected), createdByNode);

        QString statusMessage = tr("%1 out of %2 %3 selected (%4%)")
            .arg(nSelected)
            .arg(typePropertyObject->size())
            .arg(container->getOOMetaClass().elementDescriptionName())
            .arg((FloatType)nSelected * 100 / std::max((size_t)1,typePropertyObject->size()), 0, 'f', 1);

        state.setStatus(std::move(statusMessage));

        return std::move(state);
    });
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant SelectTypeModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(scene);

    QString shortInfo;
    if(node && subject() && sourceProperty()) {
        const PipelineFlowState& state = node->getCachedPipelineNodeInput(scene->animationSettings()->currentTime());
        if(const PropertyContainer* container = state.getLeafObject(subject())) {
            if(const Property* inputProperty = sourceProperty().findInContainer(container)) {
                auto sortedIds = selectedTypeIDs().values();
                boost::sort(sortedIds);
                for(int id : sortedIds) {
                    if(!shortInfo.isEmpty())
                        shortInfo += QStringLiteral(", ");
                    if(const ElementType* t = inputProperty->elementType(id))
                        shortInfo += t->nameOrNumericId();
                    else
                        shortInfo += QString::number(id);
                }
            }
        }
    }
    return shortInfo;
}

}   // End of namespace
