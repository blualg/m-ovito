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
#include <ovito/core/dataset/data/mesh/TriangleMeshVis.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>

namespace Ovito {

/**
 * \brief Base class for delegates of the SliceModifier, which perform the slice operation on different kinds of data.
 */
class OVITO_STDMOD_EXPORT SliceModifierDelegate : public ModifierDelegate
{
    OVITO_CLASS(SliceModifierDelegate)

protected:

    /// Abstract class constructor.
    using ModifierDelegate::ModifierDelegate;
};

/**
 * \brief Slice function that operates on lines.
 */
class OVITO_STDMOD_EXPORT LinesSliceModifierDelegate : public SliceModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class LinesSliceModifierDelegateClass : public SliceModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using SliceModifierDelegate::OOMetaClass::OOMetaClass;

        /// Asks the metaclass which data objects in the given input data collection the modifier delegate can operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("lines"); }
    };

    OVITO_CLASS_META(LinesSliceModifierDelegate, LinesSliceModifierDelegateClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief Slice function that operates on vectors.
 */
class OVITO_STDMOD_EXPORT VectorsSliceModifierDelegate : public SliceModifierDelegate
{
    /// Give the modifier delegate its own metaclass.
    class VectorsSliceModifierDelegateClass : public SliceModifierDelegate::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using SliceModifierDelegate::OOMetaClass::OOMetaClass;

        /// Asks the metaclass which data objects in the given input data collection the modifier delegate can operate on.
        virtual QVector<DataObjectReference> getApplicableObjects(const DataCollection& input) const override;

        /// The name by which Python scripts can refer to this modifier delegate.
        virtual QString pythonDataName() const override { return QStringLiteral("vectors"); }
    };

    OVITO_CLASS_META(VectorsSliceModifierDelegate, VectorsSliceModifierDelegateClass)

public:

    /// Applies this modifier delegate to the data.
    virtual Future<PipelineFlowState> apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state,
                                            const PipelineFlowState& originalState,
                                            const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs) override;
};

/**
 * \brief The slice modifier performs a cut through a dataset.
 */
class OVITO_STDMOD_EXPORT SliceModifier : public MultiDelegatingModifier
{
    /// Give this modifier class its own metaclass.
    class SliceModifierClass : public MultiDelegatingModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using MultiDelegatingModifier::OOMetaClass::OOMetaClass;

        /// Return the metaclass of delegates for this modifier type.
        virtual const ModifierDelegate::OOMetaClass& delegateMetaclass() const override { return SliceModifierDelegate::OOClass(); }
    };

    OVITO_CLASS_META(SliceModifier, SliceModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

    /// Lets the modifier render itself in an interactive viewport.
    virtual void renderModifierVisual(ModificationNode* modNode, SceneNode* sceneNode, FrameGraph& frameGraph) override;

    // Property access functions:

    /// Returns the signed distance of the cutting plane from the origin.
    FloatType distance() const { return distanceController() ? distanceController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the plane's distance from the origin.
    void setDistance(FloatType newDistance) { if(distanceController()) distanceController()->setFloatValue(AnimationTime(0), newDistance); }

    /// Returns the plane's normal vector.
    Vector3 normal() const { return normalController() ? normalController()->getVector3Value(AnimationTime(0)) : Vector3(0,0,1); }

    /// Sets the plane's normal vector.
    void setNormal(const Vector3& newNormal) { if(normalController()) normalController()->setVector3Value(AnimationTime(0), newNormal); }

    /// Returns the width of the slab produced by the modifier.
    FloatType slabWidth() const { return widthController() ? widthController()->getFloatValue(AnimationTime(0)) : 0; }

    /// Sets the width of the slab produced by the modifier.
    void setSlabWidth(FloatType newWidth) { if(widthController()) widthController()->setFloatValue(AnimationTime(0), newWidth); }

    /// Returns the slicing plane and the slab width.
    std::tuple<Plane3, FloatType> slicingPlane(AnimationTime time, TimeInterval& validityInterval, const PipelineFlowState& state);

    /// Moves the plane along its current normal vector to position in the center of the simulation cell.
    void centerPlaneInSimulationCell(ModificationNode* node, AnimationTime time);

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

    /// Called when the object is opened in a new parameter editor in the UI.
    virtual void editingStarted(UserInterface& ui) override {
        // Refresh viewports because this modifier renders an overlay while being edited.
        ui.updateViewports();
    }

    /// Called when the object is now longer opened in a parameter editor in the UI.
    virtual void editingStopped(UserInterface& ui) override {
        // Refresh viewports because this modifier renders an overlay while being edited.
        ui.updateViewports();
    }

    // Slice the coordinates / particle positions to the [output] maskProperty
    // Can be reused to slice particles or vectors
    static size_t sliceCoordinatesToMask(Plane3 plane, FloatType sliceWidth, bool invert, const Property* positionProperty,
                                         DataBuffer* maskBuffer, const Property* selectionProperty);

protected:
    /// Gets called by the system when the modifier is being inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Renders the plane in the viewport.
    void renderPlane(FrameGraph& frameGraph, const SceneNode* sceneNode, const Plane3& plane, const Box3& box, const ColorA& color) const;

    /// Computes the intersection lines of a plane and a quad.
    void planeQuadIntersection(const Point3 corners[8], const std::array<int,4>& quadVerts, const Plane3& plane, std::vector<Point3>& vertices) const;

private:

    /// This controller stores the normal of the slicing plane.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, normalController, setNormalController);

    /// This controller stores the distance of the slicing plane from the origin.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, distanceController, setDistanceController);

    /// Controls the slab width.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, widthController, setWidthController);

    /// Controls whether the data elements should only be selected instead of being deleted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, createSelection, setCreateSelection);

    /// Controls whether the plane's orientation should be reversed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, inverse, setInverse);

    /// Controls whether the modifier should only be applied to the currently selected data elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, applyToSelection, setApplyToSelection);

    /// Enables the visualization of the cutting plane.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, enablePlaneVisualization, setEnablePlaneVisualization);

    /// Controls whether the plane is specified in reduced cell coordinates (Miller indices).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, reducedCoordinates, setReducedCoordinates);

    /// The vis element for plane.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<TriangleMeshVis>, planeVis, setPlaneVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
