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
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/vectors/Vectors.h>
#include "SliceModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SliceModifierDelegate);
IMPLEMENT_CREATABLE_OVITO_CLASS(SliceModifier);
OVITO_CLASSINFO(SliceModifier, "DisplayName", "Slice");
OVITO_CLASSINFO(SliceModifier, "Description", "Cut away some part of the dataset using a 3d plane.");
OVITO_CLASSINFO(SliceModifier, "ModifierCategory", "Modification");
DEFINE_REFERENCE_FIELD(SliceModifier, normalController);
DEFINE_REFERENCE_FIELD(SliceModifier, distanceController);
DEFINE_REFERENCE_FIELD(SliceModifier, widthController);
DEFINE_PROPERTY_FIELD(SliceModifier, createSelection);
DEFINE_PROPERTY_FIELD(SliceModifier, inverse);
DEFINE_PROPERTY_FIELD(SliceModifier, applyToSelection);
DEFINE_PROPERTY_FIELD(SliceModifier, enablePlaneVisualization);
DEFINE_PROPERTY_FIELD(SliceModifier, reducedCoordinates);
DEFINE_REFERENCE_FIELD(SliceModifier, planeVis);
SET_PROPERTY_FIELD_LABEL(SliceModifier, normalController, "Normal");
SET_PROPERTY_FIELD_LABEL(SliceModifier, distanceController, "Distance");
SET_PROPERTY_FIELD_LABEL(SliceModifier, widthController, "Slab width");
SET_PROPERTY_FIELD_LABEL(SliceModifier, createSelection, "Create selection (do not delete)");
SET_PROPERTY_FIELD_LABEL(SliceModifier, inverse, "Reverse orientation");
SET_PROPERTY_FIELD_LABEL(SliceModifier, applyToSelection, "Apply to selection only");
SET_PROPERTY_FIELD_LABEL(SliceModifier, enablePlaneVisualization, "Visualize plane");
SET_PROPERTY_FIELD_LABEL(SliceModifier, reducedCoordinates, "Reduced cell coordinates");
SET_PROPERTY_FIELD_LABEL(SliceModifier, planeVis, "Plane");
SET_PROPERTY_FIELD_UNITS(SliceModifier, normalController, WorldParameterUnit);
SET_PROPERTY_FIELD_UNITS(SliceModifier, distanceController, WorldParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SliceModifier, widthController, WorldParameterUnit, 0);

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesSliceModifierDelegate);
OVITO_CLASSINFO(LinesSliceModifierDelegate, "DisplayName", "Lines");

IMPLEMENT_CREATABLE_OVITO_CLASS(VectorsSliceModifierDelegate);
OVITO_CLASSINFO(VectorsSliceModifierDelegate, "DisplayName", "Vectors");

/******************************************************************************
 * Asks the metaclass which data objects in the given input data collection the
 * modifier delegate can operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Performs the slicing of the lines.
 ******************************************************************************/
Future<PipelineFlowState> LinesSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* mod = static_object_cast<SliceModifier>(request.modifier());
    QString statusMessage;

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = mod->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    // Loop over all lines objects in data collection
    for(const DataObject* obj : originalState.data()->objects()) {
        // Transform the Lines.
        if(const Lines* inputLines = dynamic_object_cast<Lines>(obj)) {
            // Make sure we can safely modify the lines object.
            Lines* outputLines = state.makeMutable(inputLines);

            QVector<Plane3> planes = outputLines->cuttingPlanes();
            if(sliceWidth <= 0) {
                planes.push_back(plane);
            }
            else {
                planes.push_back(Plane3(plane.normal, plane.dist + sliceWidth));
                planes.push_back(Plane3(-plane.normal, -plane.dist + sliceWidth));
            }
            outputLines->setCuttingPlanes(std::move(planes));
        }
    }

    return state;
}

/******************************************************************************
 * Asks the metaclass which data objects in the given input data collection the
 * modifier delegate can operate on.
 ******************************************************************************/
QVector<DataObjectReference> VectorsSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all vectors objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Vectors::OOClass())) {
        objects.push_back(path);
    }

    return objects;
}

/******************************************************************************
 * Performs the slicing of the vectors.
 ******************************************************************************/
Future<PipelineFlowState> VectorsSliceModifierDelegate::apply(
    const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState,
    const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());
    bool applyToSelection = modifier->applyToSelection();

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    return asyncLaunch([state = std::move(state), plane, sliceWidth, invert = modifier->inverse(),
                        createSelection = modifier->createSelection(), applyToSelection]() mutable {
        // Loop over all vectors objects in the data collection
        for(qsizetype i = 0; i < state.data()->objects().size(); i++) {
            // Slice the vectors.
            if(const Vectors* inputVectors = dynamic_object_cast<Vectors>(state.data()->objects()[i])) {
                inputVectors->verifyIntegrity();

                // Create mask array to be computed.
                DataOORef<DataBuffer> maskBuffer = DataOORef<DataBuffer>::create(
                    ObjectInitializationFlag::NoFlags, DataBuffer::Uninitialized, inputVectors->elementCount(), DataBuffer::IntSelection);

                // Get the input basis points.
                ConstPropertyPtr positionProperty = inputVectors->expectProperty(Vectors::PositionProperty);

                // Number of marked/selected particles.
                size_t numMarked = SliceModifier::sliceCoordinatesToMask(plane, sliceWidth, invert, positionProperty, maskBuffer, nullptr);

                // Make sure we can safely modify the vectors object.
                Vectors* outputVectors = state.makeMutable(inputVectors);
                if(createSelection == false) {
                    outputVectors->deleteElements(std::move(maskBuffer), numMarked);
                }
                outputVectors->verifyIntegrity();
            }
        }
        return std::move(state);
    });
}

size_t SliceModifier::sliceCoordinatesToMask(Plane3 plane, FloatType sliceWidth, bool invert, const Property* positionProperty,
                                             DataBuffer* maskBuffer, const Property* selectionProperty)
{
    // Number of marked/selected particles.
    size_t numMarked = 0;

#ifdef OVITO_USE_SYCL
    if(maskProperty->size() != 0) {
        // This is a single-element counter variable that will be incremented by the kernel for each marked element.
        sycl::buffer<size_t> numMarkedBuf(&numMarked, 1);

        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            // Access the input coordinates.
            SyclBufferAccess<Point3, access_mode::read> posAcc(positionProperty, cgh);
            // Access the input selection flags.
            SyclBufferAccess<SelectionIntType, access_mode::read> selAcc(selectionProperty, cgh);
            // Access output flags array.
            SyclBufferAccess<SelectionIntType, access_mode::write> maskAcc(maskProperty, cgh, DataBuffer::Uninitialized);
#ifdef OVITO_USE_SYCL_ACPP
            auto reduction = sycl::reduction(sycl::accessor{numMarkedBuf, cgh, sycl::no_init}, size_t{0}, sycl::plus<size_t>());
#else
            auto reduction =
                sycl::reduction(numMarkedBuf, cgh, size_t{0}, sycl::plus<size_t>(), sycl::property::reduction::initialize_to_identity{});
#endif
            if(sliceWidth <= 0) {
                OVITO_SYCL_PARALLEL_FOR(cgh, SliceModifier_particles_kernel1)
                (sycl::range(maskProperty->size()), reduction, [=](size_t i, auto& red) {
                    if(!selAcc || selAcc[i]) {
                        if(plane.pointDistance(posAcc[i]) > 0) {
                            maskAcc[i] = 1;
                            red += (size_t)1;
                        }
                        else
                            maskAcc[i] = 0;
                    }
                    else
                        maskAcc[i] = 0;
                });
            }
            else {
                OVITO_SYCL_PARALLEL_FOR(cgh, SliceModifier_particles_kernel2)
                (sycl::range(mask->size()), reduction, [=](size_t i, auto& red) {
                    if(!selAcc || selAcc[i]) {
                        if(invert == (plane.classifyPoint(posAcc[i], sliceWidth) == 0)) {
                            maskAcc[i] = 1;
                            red += (size_t)1;
                        }
                        else
                            maskAcc[i] = 0;
                    }
                    else
                        maskAcc[i] = 0;
                });
            }
        });
    }
#else
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> maskAccess(maskBuffer);
    BufferReadAccess<Point3> positionAccess(positionProperty);
    BufferReadAccess<SelectionIntType> selectionAccess(selectionProperty);

    auto maskIter = maskAccess.begin();
    if(sliceWidth <= 0) {
        if(selectionAccess) {
            const auto* selIter = selectionAccess.cbegin();
            for(const Point3& p : positionAccess) {
                this_task::throwIfCanceled();
                if(*selIter++ && plane.pointDistance(p) > 0) {
                    *maskIter = 1;
                    numMarked++;
                }
                else
                    *maskIter = 0;
                ++maskIter;
            }
        }
        else {
            for(const Point3& p : positionAccess) {
                this_task::throwIfCanceled();
                if(plane.pointDistance(p) > 0) {
                    *maskIter = 1;
                    numMarked++;
                }
                else
                    *maskIter = 0;
                ++maskIter;
            }
        }
    }
    else {
        if(selectionAccess) {
            const auto* selIter = selectionAccess.cbegin();
            for(const Point3& p : positionAccess) {
                this_task::throwIfCanceled();
                if(*selIter++ && invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                    *maskIter = 1;
                    numMarked++;
                }
                else
                    *maskIter = 0;
                ++maskIter;
            }
        }
        else {
            for(const Point3& p : positionAccess) {
                this_task::throwIfCanceled();
                if(invert == (plane.classifyPoint(p, sliceWidth) == 0)) {
                    *maskIter = 1;
                    numMarked++;
                }
                else
                    *maskIter = 0;
                ++maskIter;
            }
        }
    }
    OVITO_ASSERT(maskIter == maskAccess.end());
    positionAccess.reset();
    selectionAccess.reset();
    maskAccess.reset();
#endif

    return numMarked;
}

/******************************************************************************
* Constructor.
******************************************************************************/
void SliceModifier::initializeObject(ObjectInitializationFlags flags)
{
    MultiDelegatingModifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setNormalController(ControllerManager::createVector3Controller());
        setDistanceController(ControllerManager::createFloatController());
        setWidthController(ControllerManager::createFloatController());
        if(normalController())
            normalController()->setVector3Value(AnimationTime(0), Vector3(1,0,0));

        // Generate the list of delegate objects.
        createModifierDelegates(SliceModifierDelegate::OOClass());

        // Create the vis element for the plane.
        setPlaneVis(OORef<TriangleMeshVis>::create(flags));
        planeVis()->setTitle(tr("Plane"));
        planeVis()->setHighlightEdges(true);
        planeVis()->setTransparency(0.5);
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool SliceModifier::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && (source == distanceController() || source == normalController())) {
        // Changes of some modifier parameters affect the result of SliceModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return MultiDelegatingModifier::referenceEvent(source, event);
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void SliceModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    if(normalController())
        validityInterval.intersect(normalController()->validityInterval(request.time()));
    if(distanceController())
        validityInterval.intersect(distanceController()->validityInterval(request.time()));
    if(widthController())
        validityInterval.intersect(widthController()->validityInterval(request.time()));
}

/******************************************************************************
* Returns the slicing plane and the slab width.
******************************************************************************/
std::tuple<Plane3, FloatType> SliceModifier::slicingPlane(AnimationTime time, TimeInterval& validityInterval, const PipelineFlowState& state)
{
    Plane3 plane;

    if(normalController())
        normalController()->getVector3Value(time, plane.normal, validityInterval);

    if(plane.normal == Vector3::Zero())
        plane.normal = Vector3(0,0,1);

    if(distanceController())
        plane.dist = distanceController()->getFloatValue(time, validityInterval);

    if(inverse())
        plane = -plane;

    if(reducedCoordinates()) {
        if(const SimulationCell* cell = state.getObject<SimulationCell>()) {
            plane.normal /= plane.normal.squaredLength();
            plane = cell->cellMatrix() * plane;
        }
        else {
            throw Exception(tr("Slicing plane was specified in reduced cell coordinates but there is no simulation cell."));
        }
    }
    else {
        plane.normal.normalize();
    }

    FloatType slabWidth = 0;
    if(widthController())
        slabWidth = widthController()->getFloatValue(time, validityInterval);

    return std::make_tuple(plane, slabWidth);
}

/******************************************************************************
* Lets the modifier render itself in an interactive viewport.
******************************************************************************/
void SliceModifier::renderModifierVisual(ModificationNode* modNode, Pipeline* pipeline, FrameGraph& frameGraph)
{
    if(isBeingEdited()) {
        Box3 bb = pipeline->localBoundingBox(frameGraph.time());
        if(bb.isEmpty())
            return;

        // Obtain modifier parameter values.
        PipelineEvaluationRequest request(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive());
        const PipelineFlowState& state = modNode->evaluateInput(request).result();
        Plane3 plane;
        FloatType slabWidth;
        TimeInterval interval;
        std::tie(plane, slabWidth) = slicingPlane(frameGraph.time(), interval, state);
        if(plane.normal.isZero())
            return;

        constexpr ColorA color(0.8, 0.3, 0.3);
        if(slabWidth <= 0) {
            renderPlane(frameGraph, pipeline, plane, bb, color);
        }
        else {
            plane.dist += slabWidth / 2;
            renderPlane(frameGraph, pipeline, plane, bb, color);
            plane.dist -= slabWidth;
            renderPlane(frameGraph, pipeline, plane, bb, color);
        }
    }
}

/******************************************************************************
* Renders the plane in the viewports.
******************************************************************************/
void SliceModifier::renderPlane(FrameGraph& frameGraph, const Pipeline* pipeline, const Plane3& plane, const Box3& bb, const ColorA& color) const
{
    auto& vertexBuffer = frameGraph.visCache().lookup<ConstDataBufferPtr>(RendererResourceKey<struct SlicePlaneCache, Plane3, Box3>{plane, bb});
    if(!vertexBuffer) {
        // Compute intersection lines of slicing plane and bounding box.
        Point3 corners[8];
        for(size_t i = 0; i < 8; i++)
            corners[i] = bb[i];

        std::vector<Point3> vertices;
        vertices.reserve(8);
        planeQuadIntersection(corners, {{0, 1, 5, 4}}, plane, vertices);
        planeQuadIntersection(corners, {{1, 3, 7, 5}}, plane, vertices);
        planeQuadIntersection(corners, {{3, 2, 6, 7}}, plane, vertices);
        planeQuadIntersection(corners, {{2, 0, 4, 6}}, plane, vertices);
        planeQuadIntersection(corners, {{4, 5, 7, 6}}, plane, vertices);
        planeQuadIntersection(corners, {{0, 2, 3, 1}}, plane, vertices);

        // If there is not intersection with the simulation box then
        // project the simulation box onto the plane.
        if(vertices.empty()) {
            const static int edges[12][2] = {
                    {0,1},{1,3},{3,2},{2,0},
                    {4,5},{5,7},{7,6},{6,4},
                    {0,4},{1,5},{3,7},{2,6}
            };
            vertices.reserve(24);
            for(int edge = 0; edge < 12; edge++) {
                vertices.push_back(plane.projectPoint(corners[edges[edge][0]]));
                vertices.push_back(plane.projectPoint(corners[edges[edge][1]]));
            }
        }

        // Render plane-box intersection lines.
        BufferFactory<Point3> positions(vertices.size());
        boost::range::copy(vertices, positions.begin());
        vertexBuffer = positions.take();
    }

    // Render the wireframe lines to visualize the cutting plane.
    std::unique_ptr<LinePrimitive> lines = std::make_unique<LinePrimitive>();
    lines->setPositions(vertexBuffer);
    lines->setUniformColor(color);
    frameGraph.addPrimitive(std::move(lines), pipeline);
}

/******************************************************************************
* Computes the intersection lines of a plane and a quad.
******************************************************************************/
void SliceModifier::planeQuadIntersection(const Point3 corners[8], const std::array<int,4>& quadVerts, const Plane3& plane, std::vector<Point3>& vertices) const
{
    Point3 p1;
    bool hasP1 = false;
    for(int i = 0; i < 4; i++) {
        Ray3 edge(corners[quadVerts[i]], corners[quadVerts[(i+1)%4]]);
        FloatType t = plane.intersectionT(edge, FLOATTYPE_EPSILON);
        if(t < 0 || t > 1) continue;
        if(!hasP1) {
            p1 = edge.point(t);
            hasP1 = true;
        }
        else {
            Point3 p2 = edge.point(t);
            if(!p2.equals(p1)) {
                vertices.push_back(p1);
                vertices.push_back(p2);
                return;
            }
        }
    }
}

/******************************************************************************
* This method is called by the system at the time the modifier is being inserted
* into a pipeline for the first time.
******************************************************************************/
void SliceModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    MultiDelegatingModifier::initializeModifier(request);

    // Initially place the cutting plane in the center of the simulation cell.
    if(ExecutionContext::isInteractive() && distanceController() && distanceController()->getFloatValue(AnimationTime(0)) == 0) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).result();
        if(const SimulationCell* cell = input.getObject<SimulationCell>()) {
            Point3 centerPoint = cell->cellMatrix() * Point3(0.5, 0.5, 0.5);
            FloatType centerDistance = normal().dot(centerPoint - Point3::Origin());
            if(std::abs(centerDistance) > FLOATTYPE_EPSILON && distanceController())
                distanceController()->setFloatValue(AnimationTime(0), centerDistance);
        }
    }
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> SliceModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    Future<PipelineFlowState> future = MultiDelegatingModifier::evaluateModifier(request, std::move(state));

    if(enablePlaneVisualization()) {
        future.postprocess(*this, [this, request](PipelineFlowState&& state) {
            Plane3 plane;
            FloatType slabWidth;
            TimeInterval interval;
            std::tie(plane, slabWidth) = slicingPlane(request.time(), interval, state);
            if(plane.normal.isZero())
                return state;

            // Compute intersection polygon of slicing plane with simulation cell.
            const SimulationCell* cellObj = state.expectObject<SimulationCell>();
            const AffineTransformation& cellMatrix = cellObj->cellMatrix();

            // Create an output mesh for visualizing the cutting plane.
            TriangleMesh* mesh = state.createObjectWithVis<TriangleMesh>(QStringLiteral("plane"), request.modificationNode(), planeVis());

            // Compute intersection lines of slicing plane and simulation cell.
            auto createIntersectionPolygon = [&](const Plane3& plane) {
                std::vector<Point3> vertices;
                auto planeEdgeIntersection = [&](const Vector3& b, const Vector3& d) {
                    Ray3 edge(Point3::Origin() + b, d);
                    FloatType t = plane.intersectionT(edge, FLOATTYPE_EPSILON);
                    if(t >= -FLOATTYPE_EPSILON && t <= 1 + FLOATTYPE_EPSILON)
                        vertices.push_back(edge.point(t));
                };
                planeEdgeIntersection(cellMatrix.translation(), cellMatrix.column(0));
                planeEdgeIntersection(cellMatrix.translation(), cellMatrix.column(1));
                planeEdgeIntersection(cellMatrix.translation(), cellMatrix.column(2));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(0), cellMatrix.column(1));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(0), cellMatrix.column(2));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(1), cellMatrix.column(0));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(1), cellMatrix.column(2));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(2), cellMatrix.column(0));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(2), cellMatrix.column(1));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(0) + cellMatrix.column(1), cellMatrix.column(2));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(1) + cellMatrix.column(2), cellMatrix.column(0));
                planeEdgeIntersection(cellMatrix.translation() + cellMatrix.column(2) + cellMatrix.column(0), cellMatrix.column(1));
                if(vertices.size() < 3) return;
                vertices.erase(std::remove_if(vertices.begin() + 1, vertices.end(),
                    [p = vertices.front()](const Point3& p2) { return p2.equals(p); }), vertices.end());
                if(vertices.size() < 3) return;
                std::sort(vertices.begin() + 1, vertices.end(), [&](const Point3& a, const Point3& b) {
                    return (a - vertices.front()).cross(b - vertices.front()).dot(plane.normal) < 0;
                });
                int baseVertexCount = mesh->vertexCount();
                mesh->setVertexCount(baseVertexCount + vertices.size());
                std::copy(vertices.begin(), vertices.end(), mesh->vertices().begin() + baseVertexCount);
                for(int f = 2; f < vertices.size(); f++) {
                    TriMeshFace& face = mesh->addFace();
                    face.setVertices(baseVertexCount, baseVertexCount+f-1, baseVertexCount+f);
                    face.setEdgeVisibility(f == 2, true, f == vertices.size()-1);
                }
            };
            if(slabWidth <= 0) {
                createIntersectionPolygon(plane);
            }
            else {
                plane.dist += slabWidth / 2;
                createIntersectionPolygon(plane);
                plane.dist -= slabWidth;
                createIntersectionPolygon(plane);
            }
            return std::move(state);
        });
    }
    return future;
}

/******************************************************************************
* Moves the plane along its current normal vector to position in the center of the simulation cell.
******************************************************************************/
void SliceModifier::centerPlaneInSimulationCell(ModificationNode* node, AnimationTime time)
{
    if(!node)
        return;

    // Get the simulation cell from the input object to center the slicing plane in
    // the center of the simulation cell.
    const PipelineFlowState& input = node->evaluateInput(PipelineEvaluationRequest(time, false, true)).result();
    if(const SimulationCell* cell = input.getObject<SimulationCell>()) {

        FloatType centerDistance;
        if(!reducedCoordinates()) {
            Point3 centerPoint = cell->cellMatrix() * Point3(0.5, 0.5, 0.5);
            centerDistance = normal().safelyNormalized().dot(centerPoint - Point3::Origin());
        }
        else {
            if(!normal().isZero())
                centerDistance = normal().dot(Vector3(0.5, 0.5, 0.5));
            else
                centerDistance = distance();
        }

        setDistance(centerDistance);
    }
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the modifier's title in the pipeline editor list.
******************************************************************************/
QVariant SliceModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    Vector3 normal = this->normal();
    return tr("(%1 %2 %3), %4").arg(normal.x(), 0, 'g', 1).arg(normal.y(), 0, 'g', 1).arg(normal.z(), 0, 'g', 1).arg(distance(), 0, 'g', 6);
}

}   // End of namespace
