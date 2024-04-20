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
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/simcell/PeriodicDomainObject.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "AffineTransformationModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(AffineTransformationModifier);
OVITO_CLASSINFO(AffineTransformationModifier, "DisplayName", "Affine transformation");
OVITO_CLASSINFO(AffineTransformationModifier, "Description", "Apply an affine transformation to the dataset.");
OVITO_CLASSINFO(AffineTransformationModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(AffineTransformationModifier, transformationTM);
DEFINE_PROPERTY_FIELD(AffineTransformationModifier, selectionOnly);
DEFINE_PROPERTY_FIELD(AffineTransformationModifier, targetCell);
DEFINE_PROPERTY_FIELD(AffineTransformationModifier, relativeMode);
DEFINE_PROPERTY_FIELD(AffineTransformationModifier, translationReducedCoordinates);
SET_PROPERTY_FIELD_LABEL(AffineTransformationModifier, transformationTM, "Transformation");
SET_PROPERTY_FIELD_LABEL(AffineTransformationModifier, selectionOnly, "Transform selected elements only");
SET_PROPERTY_FIELD_LABEL(AffineTransformationModifier, targetCell, "Target cell shape");
SET_PROPERTY_FIELD_LABEL(AffineTransformationModifier, relativeMode, "Relative transformation");
SET_PROPERTY_FIELD_LABEL(AffineTransformationModifier, translationReducedCoordinates, "Relative transformation");

IMPLEMENT_ABSTRACT_OVITO_CLASS(AffineTransformationModifierDelegate);
IMPLEMENT_CREATABLE_OVITO_CLASS(LinesAffineTransformationModifierDelegate);
OVITO_CLASSINFO(LinesAffineTransformationModifierDelegate, "DisplayName", "Lines");
IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCellAffineTransformationModifierDelegate);
OVITO_CLASSINFO(SimulationCellAffineTransformationModifierDelegate, "DisplayName", "Simulation cell");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
AffineTransformationModifier::AffineTransformationModifier(ObjectInitializationFlags flags) : MultiDelegatingModifier(flags),
    _selectionOnly(false),
    _transformationTM(AffineTransformation::Identity()),
    _targetCell(AffineTransformation::Zero()),
    _relativeMode(true),
    _translationReducedCoordinates(false)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Generate the list of delegate objects.
        createModifierDelegates(AffineTransformationModifierDelegate::OOClass());
    }
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void AffineTransformationModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    MultiDelegatingModifier::initializeModifier(request);

    // Take the simulation cell from the input object as the default destination cell geometry for absolute scaling.
    if(targetCell() == AffineTransformation::Zero()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).result();
        if(const SimulationCell* cell = input.getObject<SimulationCell>())
            setTargetCell(cell->cellMatrix());
    }
}

/******************************************************************************
* Returns the effective affine transformation matrix to be applied to points.
* It depends on the linear matrix, the translation vector, relative/target cell mode, and
* whether the translation is specified in terms of reduced cell coordinates.
* Thus, the affine transformation may depend on the current simulation cell shape.
******************************************************************************/
AffineTransformation AffineTransformationModifier::effectiveAffineTransformation(const PipelineFlowState& state) const
{
    AffineTransformation tm;
    if(relativeMode()) {
        tm = transformationTM();
        if(translationReducedCoordinates()) {
            tm.translation() = tm * (state.expectObject<SimulationCell>()->matrix() * tm.translation());
        }
    }
    else {
        const SimulationCell* simCell = state.getObject<SimulationCell>();
        if(!simCell || simCell->cellMatrix().determinant() == 0 || simCell->isDegenerate())
            throw Exception(tr("Input simulation cell does not exist or is degenerate. Transformation to target cell would be singular."));

        tm = targetCell() * state.expectObject<SimulationCell>()->inverseMatrix();
    }

    return tm;
}

/******************************************************************************
* Copies positions from one buffer to another while transforming them.
* The transformation may be applied only to selected elements.
******************************************************************************/
void AffineTransformationModifier::transformCoordinates(const AffineTransformation tm, bool selectionOnly, const Property* input, Property* output, const Property* selection)
{
    OVITO_ASSERT(output != input);
    OVITO_ASSERT(output->size() == input->size());

    if(input->size() == 0)
        return;

    if(selectionOnly) {
        if(selection) {
#ifdef OVITO_USE_SYCL
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<const Point3, access_mode::read> posIn(input, cgh);
                SyclBufferAccess<Point3, access_mode::discard_write> posOut(output, cgh);
                SyclBufferAccess<const SelectionIntType, access_mode::read> selectionIn(selection, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, affine_coord_transformation_selection)(sycl::range(input->size()), [=](size_t i) {
                    posOut[i] = selectionIn[i] ? (tm * posIn[i]) : posIn[i];
                });
            });
#else
            BufferReadAccess<const Point3> posIn(input);
            const auto* pin = posIn.cbegin();
            BufferReadAccess<const SelectionIntType> selAccess(selection);
            const auto* s = selAccess.cbegin();
            for(Point3& pout : BufferWriteAccess<Point3, access_mode::discard_write>(output)) {
                pout = (*s++) ? (tm * (*pin)) : (*pin);
                ++pin;
            }
#endif
        }
        else {
            // Without any selection property present, none of the elements get transformed.
            output->copyFrom(*input);
        }
    }
    else {
        // Check if the matrix describes a pure translation, which can be applied more efficiently.
        // If so, we can simply add vectors instead of computing full matrix products.
        if(tm.isTranslationMatrix()) {
            const Vector3 translation = tm.translation();
#ifdef OVITO_USE_SYCL
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<const Point3, access_mode::read> posIn(input, cgh);
                SyclBufferAccess<Point3, access_mode::discard_write> posOut(output, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, affine_coord_transformation_simple_translation)(sycl::range(input->size()), [=](size_t i) {
                    posOut[i] = posIn[i] + translation;
                });
            });
#else
            BufferReadAccess<const Point3> posIn(input);
            const auto* pin = posIn.cbegin();
            for(Point3& pout : BufferWriteAccess<Point3, access_mode::discard_write>(output))
                pout = (*pin++) + translation;
#endif
        }
        else {
#ifdef OVITO_USE_SYCL
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<const Point3, access_mode::read> posIn(input, cgh);
                SyclBufferAccess<Point3, access_mode::discard_write> posOut(output, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, affine_coord_transformation_full_xform)(sycl::range(input->size()), [=](size_t i) {
                    posOut[i] = tm * posIn[i];
                });
            });
#else
            BufferReadAccess<const Point3> posIn(input);
            const auto* pin = posIn.cbegin();
            for(Point3& pout : BufferWriteAccess<Point3, access_mode::discard_write>(output))
                pout = tm * (*pin++);
#endif
        }
    }
}

/******************************************************************************
* Copies vectors from one buffer to another while transforming them.
* If enabled, the transformation is only applied to selected elements.
******************************************************************************/
void AffineTransformationModifier::transformVectors(const AffineTransformation tm, bool selectionOnly, const Property* input, Property* output, const Property* selection)
{
    OVITO_ASSERT(output != input);
    OVITO_ASSERT(output->size() == input->size());

    if(input->size() == 0)
        return;

    input->forTypes<DataBuffer::Float32, DataBuffer::Float64>([&](auto _) {
        using T = decltype(_);
        using Vec3 = Vector_3<T>;

        // Transformation matrix.
        const AffineTransformationT<T> tm_typed = tm.toDataType<T>();

        if(selectionOnly) {
            if(selection) {
#ifdef OVITO_USE_SYCL
                ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                    SyclBufferAccess<const Vec3, access_mode::read> vecIn(input, cgh);
                    SyclBufferAccess<Vec3, access_mode::discard_write> vecOut(output, cgh);
                    SyclBufferAccess<const SelectionIntType, access_mode::read> selectionIn(selection, cgh);
                    OVITO_SYCL_PARALLEL_FOR(cgh, affine_vec_transformation_selection)(sycl::range(input->size()), [=](size_t i) {
                        vecOut[i] = selectionIn[i] ? (tm_typed * vecIn[i]) : vecIn[i];
                    });
                });
#else
                BufferReadAccess<const Vec3> vecIn(input);
                const auto* vin = vecIn.cbegin();
                BufferReadAccess<const SelectionIntType> selAccess(selection);
                const auto* s = selAccess.cbegin();
                for(Vec3& vout : BufferWriteAccess<Vec3, access_mode::discard_write>(output)) {
                    vout = (*s++) ? (tm_typed * (*vin)) : (*vin);
                    ++vin;
                }
#endif
            }
            else {
                // Without any selection property present, none of the elements get transformed.
                output->copyFrom(*input);
            }
        }
        else {
            // Check if the matrix describes a pure translation. If so, effectively, no vector transformation takes place.
            if(tm_typed.isTranslationMatrix()) {
                output->copyFrom(*input);
            }
            else {
#ifdef OVITO_USE_SYCL
                ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                    SyclBufferAccess<const Vec3, access_mode::read> vecIn(input, cgh);
                    SyclBufferAccess<Vec3, access_mode::discard_write> vecOut(output, cgh);
                    OVITO_SYCL_PARALLEL_FOR(cgh, affine_vec_transformation_full_xform)(sycl::range(input->size()), [=](size_t i) {
                        vecOut[i] = tm_typed * vecIn[i];
                    });
                });
#else
                BufferReadAccess<const Vec3> vecIn(input);
                const auto* vin = vecIn.cbegin();
                for(Vec3& vout : BufferWriteAccess<Vec3, access_mode::discard_write>(output))
                    vout = tm_typed * (*vin++);
#endif
            }
        }
    });
}

/******************************************************************************
* Asks the metaclass which data objects in the given input data collection the
* modifier delegate can operate on.
******************************************************************************/
QVector<DataObjectReference> SimulationCellAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<SimulationCell>())
        return { DataObjectReference(&SimulationCell::OOClass()) };
    if(input.containsObject<PeriodicDomainObject>())
        return { DataObjectReference(&PeriodicDomainObject::OOClass()) };
    return {};
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
Future<PipelineFlowState> SimulationCellAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // Transform the simulation box.
    if(const SimulationCell* inputCell = state.getObject<SimulationCell>()) {
        SimulationCell* outputCell = state.makeMutable(inputCell);
        outputCell->setCellMatrix(modifier->relativeMode() ? (modifier->effectiveAffineTransformation(originalState) * inputCell->cellMatrix()) : modifier->targetCell());
    }

    if(!modifier->selectionOnly()) {
        // Transform the domains of PeriodicDomainDataObjects.
        for(const DataObject* obj : state.data()->objects()) {
            if(const PeriodicDomainObject* existingObject = dynamic_object_cast<PeriodicDomainObject>(obj)) {
                if(existingObject->domain()) {
                    PeriodicDomainObject* newObject = state.makeMutable(existingObject);
                    newObject->mutableDomain()->setCellMatrix(modifier->effectiveAffineTransformation(originalState) * existingObject->domain()->cellMatrix());
                }
            }
        }
    }

    return std::move(state);
}

/******************************************************************************
 * Asks the metaclass which data objects in the given input data collection the
 * modifier delegate can operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> LinesAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState),
            selectionOnly = modifier->selectionOnly()]() mutable {

        // Loop over all lines objects in the data collection
        for(const DataObject* obj : state.data()->objects()) {
            // Transform the line vertices.
            if(const Lines* inputLines = dynamic_object_cast<Lines>(obj)) {
                // Get the input line coordinates (as strong reference to force creation of a mutable clone below).
                ConstPropertyPtr inputPositionProperty = inputLines->expectProperty(Lines::PositionProperty);

                // Make sure we can safely modify the lines object.
                Lines* outputLines = state.makeMutable(inputLines);

                // Create an uninitialized copy of the position property.
                Property* outputPositionProperty = outputLines->makePropertyMutable(inputPositionProperty, DataBuffer::Uninitialized);

                // Let the modifier class do the actual coordinate transformation work.
                // Note: "Selection" property is currently not supported by Lines objects.
                OVITO_ASSERT(Lines::OOClass().isValidStandardPropertyId(Property::GenericSelectionProperty) == false);
                AffineTransformationModifier::transformCoordinates(tm, selectionOnly, inputPositionProperty, outputPositionProperty, nullptr);
            }
        }

        return std::move(state);
    });
}

}   // End of namespace
