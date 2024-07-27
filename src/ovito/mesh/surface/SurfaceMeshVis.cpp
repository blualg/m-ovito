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

#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/util/CapPolygonTessellator.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/Application.h>
#include "SurfaceMeshVis.h"
#include "SurfaceMesh.h"
#include "SurfaceMeshReadAccess.h"
#include "RenderableSurfaceMesh.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshVis);
OVITO_CLASSINFO(SurfaceMeshVis, "DisplayName", "Surface mesh");
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, surfaceColor);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, capColor);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, showCap);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, smoothShading);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, reverseOrientation);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, highlightEdges);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, surfaceIsClosed);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, colorMappingMode);
DEFINE_PROPERTY_FIELD(SurfaceMeshVis, clipAtDomainBoundaries);
DEFINE_REFERENCE_FIELD(SurfaceMeshVis, surfaceTransparencyController);
DEFINE_REFERENCE_FIELD(SurfaceMeshVis, capTransparencyController);
DEFINE_REFERENCE_FIELD(SurfaceMeshVis, surfaceColorMapping);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, surfaceColor);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, capColor);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, showCap);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, smoothShading);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, reverseOrientation);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, highlightEdges);
DEFINE_SHADOW_PROPERTY_FIELD(SurfaceMeshVis, clipAtDomainBoundaries);
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, surfaceColor, "Surface color");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, capColor, "Cap color");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, showCap, "Show cap polygons");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, smoothShading, "Smooth shading");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, surfaceTransparencyController, "Surface transparency");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, capTransparencyController, "Cap transparency");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, reverseOrientation, "Flip surface orientation");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, highlightEdges, "Highlight edges");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, surfaceIsClosed, "Closed surface");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, surfaceColorMapping, "Color mapping");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, colorMappingMode, "Color mapping mode");
SET_PROPERTY_FIELD_LABEL(SurfaceMeshVis, clipAtDomainBoundaries, "Clip at cell boundaries");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurfaceMeshVis, surfaceTransparencyController, PercentParameterUnit, 0, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurfaceMeshVis, capTransparencyController, PercentParameterUnit, 0, 1);

IMPLEMENT_ABSTRACT_OVITO_CLASS(SurfaceMeshPickInfo);

/******************************************************************************
* Constructor.
******************************************************************************/
void SurfaceMeshVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create animation controllers for the transparency parameters.
        setSurfaceTransparencyController(ControllerManager::createFloatController());
        setCapTransparencyController(ControllerManager::createFloatController());

        // Create a color mapping object for pseudo-color visualization of a surface property.
        setSurfaceColorMapping(OORef<PropertyColorMapping>::create(flags));
    }
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void SurfaceMeshVis::loadFromStreamComplete(ObjectLoadStream& stream)
{
    DataVis::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.5.4.
    // Create a color mapping sub-object if it wasn't loaded from the state file.
    if(!surfaceColorMapping())
        setSurfaceColorMapping(OORef<PropertyColorMapping>::create());
}

/******************************************************************************
* Computes the bounding box of the displayed data.
******************************************************************************/
Box3 SurfaceMeshVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    // Get the input surface mesh.
    const SurfaceMesh* surfaceMesh = path.lastAs<SurfaceMesh>();
    if(!surfaceMesh)
        return{};

    // Check validity of the surface mesh.
    surfaceMesh->verifyMeshIntegrity();
    const SimulationCell* cell = surfaceMesh->domain();
    if(cell && cell->is2D())
        return {};

    // Access mesh vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(surfaceMesh->vertices()->getProperty(SurfaceMeshVertices::PositionProperty));
    if(!vertexPositions)
        return {};

    // Compute mesh bounding box.
    Box3 bb;
    for(Point3 p : vertexPositions) {
        if(cell)
            p = cell->wrapPoint(p);
        if(!surfaceMesh->isPointCulled(p))
            bb.addPoint(p);
    }

    return bb;
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
PipelineStatus SurfaceMeshVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Get the surface mesh.
    const SurfaceMesh* surfaceMesh = path.lastAs<SurfaceMesh>();
    if(!surfaceMesh)
        return {};

    // The key type used for caching the renderable mesh:
    using RenderableMeshCacheKey = RendererResourceKey<struct RenderableMeshCache,
        ConstDataObjectRef,     // SurfaceMesh object
        bool,                   // reverseOrientation
        bool,                   // smoothShading
        ColorMappingMode,       // colorMappingMode
        PropertyReference,      // surfaceColorMapping()->sourceProperty()
        bool                    // clipAtDomainBoundaries
    >;

    // Look up the asynchronous task that generates the renderable TriangleMesh from the SurfaceMesh.
    const auto& renderableMeshFuture = frameGraph.visCache().lookup<SharedFuture<std::shared_ptr<const RenderableSurfaceMesh>>>(
        RenderableMeshCacheKey{
            path.back(),
            reverseOrientation(),
            smoothShading(),
            colorMappingMode(),
            surfaceColorMapping()->sourceProperty(),
            clipAtDomainBoundaries()
        },
        [&](SharedFuture<std::shared_ptr<const RenderableSurfaceMesh>>& renderableMeshFuture) {
            // Generate renderable mesh.
            renderableMeshFuture = transformSurfaceMesh(surfaceMesh);
            registerActiveFuture(renderableMeshFuture);
        });

    // Wait for the renderable mesh to be generated.
    std::shared_ptr<const RenderableSurfaceMesh> renderableMesh = renderableMeshFuture.result();

    // Get the rendering colors for the surface and cap meshes.
    FloatType surface_alpha = 1;
    FloatType cap_alpha = 1;
    TimeInterval iv;
    if(surfaceTransparencyController())
        surface_alpha = qBound(0.0, FloatType(1) - surfaceTransparencyController()->getFloatValue(frameGraph.time(), iv), 1.0);
    if(capTransparencyController())
        cap_alpha = qBound(0.0, FloatType(1) - capTransparencyController()->getFloatValue(frameGraph.time(), iv), 1.0);
    ColorA color_surface(colorMappingMode() == NoPseudoColoring ? surfaceColor() : Color(1,1,1), surface_alpha);
    ColorA color_cap(capColor(), cap_alpha);

    // The key type used for caching the rendering primitives:
    using PrimitiveCacheKey = RendererResourceKey<struct PrimitiveCache,
        std::shared_ptr<const RenderableSurfaceMesh>, // Renderable mesh
        ColorA,                     // Surface color
        bool                        // Edge highlighting
    >;

    // Lookup the rendering primitives in the vis cache.
    const auto& [surfacePrimitive, pickInfo] = frameGraph.visCache().lookup<std::tuple<MeshPrimitive, OORef<ObjectPickInfo>>>(
        PrimitiveCacheKey{
            renderableMesh,
            color_surface,
            highlightEdges()
        },
        [&](MeshPrimitive& surfacePrimitive, OORef<ObjectPickInfo>& pickInfo) {
            auto materialColors = renderableMesh->materialColors();
            for(ColorA& c : materialColors) {
                c.a() = surface_alpha;
            }
            surfacePrimitive.setMaterialColors(std::move(materialColors));
            surfacePrimitive.setUniformColor(color_surface);
            surfacePrimitive.setEmphasizeEdges(highlightEdges());
            surfacePrimitive.setCullFaces(renderableMesh->backfaceCulling());
            surfacePrimitive.setMesh(renderableMesh->surface());

            // Create the pick record that keeps a reference to the original data.
            pickInfo = createPickInfo(surfaceMesh, renderableMesh);
        });

    // Render the surface mesh.
    if(surfacePrimitive.mesh()) {
        auto coloredSurface = std::make_unique<MeshPrimitive>(surfacePrimitive);
        // Update the color mapping.
        coloredSurface->setPseudoColorMapping(surfaceColorMapping()->pseudoColorMapping());
        frameGraph.addPrimitive(std::move(coloredSurface), pipeline, frameGraph.addPickingGroup(pipeline, pickInfo));
    }

    // Render the surface mesh cap.
    if(showCap() && renderableMesh->capPolygons() && renderableMesh->capPolygons()->faceCount() != 0) {
        auto capPrimitive = std::make_unique<MeshPrimitive>();
        capPrimitive->setUniformColor(color_cap);
        capPrimitive->setMesh(renderableMesh->capPolygons(), MeshPrimitive::ConvexShapeMode);
        // If the caps are semi-transparent, we exclude them from the object picking render pass.
        frameGraph.addPrimitive(std::move(capPrimitive), pipeline, (cap_alpha >= 1) ? frameGraph.addPickingGroup(pipeline) : 0);
    }

    return renderableMesh->status();
}

/******************************************************************************
* Create the viewport picking record for the surface mesh object.
******************************************************************************/
OORef<ObjectPickInfo> SurfaceMeshVis::createPickInfo(const SurfaceMesh* mesh, std::shared_ptr<const RenderableSurfaceMesh> renderableMesh) const
{
    return OORef<SurfaceMeshPickInfo>::create(this, mesh, std::move(renderableMesh));
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString SurfaceMeshPickInfo::infoString(Pipeline* pipeline, quint32 subobjectId)
{
    QString str = surfaceMesh()->objectTitle();

    // Display all the properties of the face and also the properties of the mesh region to which the face belongs.
    auto facetIndex = faceIndexFromSubObjectID(subobjectId);
    if(surfaceMesh()->faces() && facetIndex >= 0 && facetIndex < surfaceMesh()->faces()->elementCount()) {
        for(const Property* property : surfaceMesh()->faces()->properties()) {
            if(facetIndex >= property->size()) continue;
            if(property->typeId() == SurfaceMeshFaces::SelectionProperty) continue;
            if(property->typeId() == SurfaceMeshFaces::ColorProperty) continue;
            if(property->typeId() == SurfaceMeshFaces::RegionProperty) continue;
            if(!str.isEmpty()) str += QStringLiteral("<sep>");
            str += QStringLiteral("<key>");
            str += property->name();
            str += QStringLiteral(":</key> ");
            if(property->dataType() == Property::Int32) {
                BufferReadAccess<int32_t*> data(property);
                for(size_t component = 0; component < data.componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(facetIndex, component));
                    if(property->elementTypes().empty() == false) {
                        if(const ElementType* ptype = property->elementType(data.get(facetIndex, component))) {
                            if(!ptype->name().isEmpty())
                                str += QStringLiteral(" (%1)").arg(ptype->name());
                        }
                    }
                }
            }
            else if(property->dataType() == Property::Int64) {
                BufferReadAccess<int64_t*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(facetIndex, component));
                }
            }
            else if(property->dataType() == Property::Int8) {
                BufferReadAccess<int8_t*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(facetIndex, component));
                }
            }
            else if(property->dataType() == Property::Float32) {
                BufferReadAccess<float*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(facetIndex, component));
                }
            }
            else if(property->dataType() == Property::Float64) {
                BufferReadAccess<double*> data(property);
                for(size_t component = 0; component < property->componentCount(); component++) {
                    if(component != 0) str += QStringLiteral(", ");
                    str += QString::number(data.get(facetIndex, component));
                }
            }
            else {
                str += QStringLiteral("<%1>").arg(property->dataTypeName());
            }
        }

        // Additionally, list all properties of the region to which the face belongs.
        if(BufferReadAccess<int32_t> regionProperty = surfaceMesh()->faces()->getProperty(SurfaceMeshFaces::RegionProperty)) {
            if(facetIndex < regionProperty.size() && surfaceMesh()->regions()) {
                int regionIndex = regionProperty[facetIndex];
                if(!str.isEmpty()) str += QStringLiteral("<sep>");
                str += QStringLiteral("<key>Region:</key> %1").arg(regionIndex);
                for(const Property* property : surfaceMesh()->regions()->properties()) {
                    if(regionIndex < 0 || regionIndex >= property->size()) continue;
                    if(property->typeId() == SurfaceMeshRegions::SelectionProperty) continue;
                    if(property->typeId() == SurfaceMeshRegions::ColorProperty) continue;
                    str += QStringLiteral("<sep><key>");
                    str += property->name();
                    str += QStringLiteral(":</key> ");
                    if(property->dataType() == Property::Int32) {
                        BufferReadAccess<int32_t*> data(property);
                        for(size_t component = 0; component < property->componentCount(); component++) {
                            if(component != 0) str += QStringLiteral(", ");
                            str += QString::number(data.get(regionIndex, component));
                            if(property->elementTypes().empty() == false) {
                                if(const ElementType* ptype = property->elementType(data.get(regionIndex, component))) {
                                    if(!ptype->name().isEmpty())
                                        str += QStringLiteral(" (%1)").arg(ptype->name());
                                }
                            }
                        }
                    }
                    else if(property->dataType() == Property::Int64) {
                        BufferReadAccess<int64_t*> data(property);
                        for(size_t component = 0; component < property->componentCount(); component++) {
                            if(component != 0) str += QStringLiteral(", ");
                            str += QString::number(data.get(regionIndex, component));
                        }
                    }
                    else if(property->dataType() == Property::Int8) {
                        BufferReadAccess<int8_t*> data(property);
                        for(size_t component = 0; component < property->componentCount(); component++) {
                            if(component != 0) str += QStringLiteral(", ");
                            str += QString::number(data.get(regionIndex, component));
                        }
                    }
                    else if(property->dataType() == Property::Float32) {
                        BufferReadAccess<float*> data(property);
                        for(size_t component = 0; component < property->componentCount(); component++) {
                            if(component != 0) str += QStringLiteral(", ");
                            str += QString::number(data.get(regionIndex, component));
                        }
                    }
                    else if(property->dataType() == Property::Float64) {
                        BufferReadAccess<double*> data(property);
                        for(size_t component = 0; component < property->componentCount(); component++) {
                            if(component != 0) str += QStringLiteral(", ");
                            str += QString::number(data.get(regionIndex, component));
                        }
                    }
                    else {
                        str += QStringLiteral("<%1>").arg(property->dataTypeName());
                    }
                }
            }
        }
    }

    return str;
}

/******************************************************************************
* Transforms the SurfaceMesh into a renderable triangle mesh.
******************************************************************************/
Future<std::shared_ptr<const RenderableSurfaceMesh>> SurfaceMeshVis::transformSurfaceMesh(const SurfaceMesh* surfaceMesh)
{
    // Make sure the surface mesh is ok.
    surfaceMesh->verifyMeshIntegrity();

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            builder = createRenderableSurfaceBuilder(surfaceMesh),
            surfaceIsClosed = surfaceIsClosed()]()
    {
        this_task::setProgressText(tr("Preparing mesh for display"));

        // Determine which parts of the surface mesh to render.
        // The following method may return optionally return a bit array indicating which mesh faces are part of the render set.
        boost::dynamic_bitset<> faceSubset = builder->determineVisibleFaces();
        this_task::throwIfCanceled();

        const SurfaceMesh* inputMesh = builder->inputMesh();
        OVITO_ASSERT(faceSubset.empty() || faceSubset.size() == inputMesh->topology()->faceCount());

        // Determine whether we can use double-sided rendering mode for the faces.
        // This is the case if there are no visible mesh faces that have an opposite face.
        // If the input mesh contains pairs of faces, we have to use back-face culling during rendering.
        bool renderFacesTwoSided;
        if(faceSubset.empty()) {
            renderFacesTwoSided = boost::algorithm::none_of(inputMesh->topology()->facesRange(),
                std::bind(&SurfaceMeshTopology::hasOppositeFace, inputMesh->topology(), std::placeholders::_1));
        }
        else {
            renderFacesTwoSided = boost::algorithm::none_of(inputMesh->topology()->facesRange(),
                [&, topology=inputMesh->topology()](SurfaceMesh::face_index face) { return faceSubset[face] && topology->hasOppositeFace(face) && faceSubset[topology->oppositeFace(face)]; });
        }
        this_task::throwIfCanceled();

        // Generate the non-periodic representation of the main surface mesh.
        if(!builder->buildSurfaceTriangleMesh(faceSubset, renderFacesTwoSided))
            throw Exception(tr("Failed to build non-periodic representation of periodic surface mesh. Periodic domain might be too small."));
        this_task::throwIfCanceled();

        // Determine the colors of the triangle mesh faces.
        builder->determineFaceColors();
        this_task::throwIfCanceled();

        // Decide whether cap polygons need to be built.
        if(surfaceIsClosed && inputMesh->domain() && !inputMesh->domain()->isDegenerate() && inputMesh->topology()->isClosed()) {
            builder->buildCapTriangleMesh(faceSubset);
        }

        return builder->renderableMesh(!renderFacesTwoSided);
    });
}

/******************************************************************************
* Creates the object that builds the non-peridic representation of the input surface mesh.
******************************************************************************/
std::unique_ptr<SurfaceMeshVis::RenderableSurfaceBuilder> SurfaceMeshVis::createRenderableSurfaceBuilder(const SurfaceMesh* mesh) const
{
    return std::make_unique<RenderableSurfaceBuilder>(
        mesh,
        reverseOrientation(),
        smoothShading(),
        colorMappingMode(),
        surfaceColorMapping()->sourceProperty(),
        clipAtDomainBoundaries());
}

/******************************************************************************
* Transfers face colors from the input to the output mesh.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::determineFaceColors()
{
    constexpr ColorAG defaultFaceColor(1,1,1,1);

    if(BufferReadAccess<ColorG> colorProperty = inputMesh()->faces()->getProperty(SurfaceMeshFaces::ColorProperty)) {
        // The "Color" property of mesh faces has the highest priority.
        // If it is present, use its information to color the triangle faces.
        outputMesh()->setHasFaceColors(true);
        auto meshFaceColor = outputMesh()->faceColors().begin();
        for(size_t originalFace : _originalFaceMap) {
            *meshFaceColor++ = colorProperty[originalFace];
        }
    }
    else if(BufferReadAccess<ColorG> colorProperty = inputMesh()->regions()->getProperty(SurfaceMeshRegions::ColorProperty)) {
        // If the "Color" property of mesh regions is present, use it information to color the
        // mesh faces according to the region they belong to.
        if(BufferReadAccess<int32_t> regionProperty = inputMesh()->faces()->getProperty(SurfaceMeshFaces::RegionProperty)) {
            outputMesh()->setHasFaceColors(true);
            size_t regionCount = colorProperty.size();
            auto meshFaceColor = outputMesh()->faceColors().begin();
            for(size_t originalFace : _originalFaceMap) {
                SurfaceMesh::region_index regionIndex = regionProperty[originalFace];
                if(regionIndex >= 0 && regionIndex < regionCount)
                    *meshFaceColor++ = colorProperty[regionIndex];
                else
                    *meshFaceColor++ = defaultFaceColor;
            }
        }
    }
    else if(_colorMappingMode == FacePseudoColoring && _pseudoColorProperty && inputMesh()->faces()) {
        QString errorDescr;
        auto [pseudoColorProperty, pseudoColorPropertyComponent] = _pseudoColorProperty.findInContainerWithComponent(inputMesh()->faces(), errorDescr);
        if(pseudoColorProperty) {
            outputMesh()->setHasFacePseudoColors(true);
            RawBufferReadAccess pseudoColorArray(pseudoColorProperty);
            auto meshFacePseudoColor = outputMesh()->facePseudoColors().begin();
            for(size_t originalFace : _originalFaceMap) {
                *meshFacePseudoColor++ = pseudoColorArray.get<FloatType>(originalFace, pseudoColorPropertyComponent);
            }
        }
        else {
            _status = PipelineStatus(PipelineStatus::Error, std::move(errorDescr));
        }
    }
    else if(_colorMappingMode == RegionPseudoColoring && _pseudoColorProperty && inputMesh()->regions()) {
        QString errorDescr;
        auto [pseudoColorProperty, pseudoColorPropertyComponent] = _pseudoColorProperty.findInContainerWithComponent(inputMesh()->regions(), errorDescr);
        if(pseudoColorProperty) {
            if(BufferReadAccess<int32_t> regionProperty = inputMesh()->faces()->getProperty(SurfaceMeshFaces::RegionProperty)) {
                outputMesh()->setHasFacePseudoColors(true);
                RawBufferReadAccess pseudoColorArray(pseudoColorProperty);
                size_t regionCount = pseudoColorProperty->size();
                auto meshFacePseudoColor = outputMesh()->facePseudoColors().begin();
                for(size_t originalFace : _originalFaceMap) {
                    SurfaceMesh::region_index regionIndex = regionProperty[originalFace];
                    if(regionIndex >= 0 && regionIndex < regionCount)
                        *meshFacePseudoColor++ = pseudoColorArray.get<FloatType>(regionIndex, pseudoColorPropertyComponent);
                    else
                        *meshFacePseudoColor++ = 0;
                }
            }
        }
        else {
            _status = PipelineStatus(PipelineStatus::Error, std::move(errorDescr));
        }
    }

    if(BufferReadAccess<SelectionIntType> selectionProperty = inputMesh()->faces()->getProperty(SurfaceMeshFaces::SelectionProperty)) {
        auto meshFace = outputMesh()->faces().begin();
        for(size_t originalFace : _originalFaceMap) {
            if(selectionProperty[originalFace])
                meshFace->setSelected();
            ++meshFace;
        }
    }
    else if(BufferReadAccess<SelectionIntType> selectionProperty = inputMesh()->regions()->getProperty(SurfaceMeshRegions::SelectionProperty)) {
        // If the "Selection" property of mesh regions is present, use it to highlight the
        // mesh faces that belong to selected regions.
        if(BufferReadAccess<int32_t> regionProperty = inputMesh()->faces()->getProperty(SurfaceMeshFaces::RegionProperty)) {
            size_t regionCount = selectionProperty.size();
            auto meshFace = outputMesh()->faces().begin();
            for(size_t originalFace : _originalFaceMap) {
                SurfaceMesh::region_index regionIndex = regionProperty[originalFace];
                if(regionIndex >= 0 && regionIndex < regionCount && selectionProperty[regionIndex])
                    meshFace->setSelected();
                ++meshFace;
            }
        }
    }
}

/******************************************************************************
* Transfers vertex colors from the input to the output mesh.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::determineVertexColors()
{
    if(BufferReadAccess<ColorG> colorProperty = inputMesh()->vertices()->getProperty(SurfaceMeshVertices::ColorProperty)) {
        OVITO_ASSERT(colorProperty.size() == outputMesh()->vertexCount());
        if(colorProperty.size() == outputMesh()->vertexCount()) {
            outputMesh()->setHasVertexColors(true);
            boost::copy(colorProperty, outputMesh()->vertexColors().begin());
        }
    }
    else if(_colorMappingMode == VertexPseudoColoring && _pseudoColorProperty) {
        QString errorDescr;
        auto [pseudoColorProperty, pseudoColorPropertyComponent] = _pseudoColorProperty.findInContainerWithComponent(inputMesh()->vertices(), errorDescr);
        if(pseudoColorProperty) {
            OVITO_ASSERT(pseudoColorProperty->size() == outputMesh()->vertexCount());
            outputMesh()->setHasVertexPseudoColors(true);
            pseudoColorProperty->copyComponentTo(outputMesh()->vertexPseudoColors().begin(), pseudoColorPropertyComponent);
        }
        else {
            _status = PipelineStatus(PipelineStatus::Error, std::move(errorDescr));
        }
    }
}

/******************************************************************************
* Generates the triangle mesh from the periodic surface mesh, which will be rendered.
******************************************************************************/
bool SurfaceMeshVis::RenderableSurfaceBuilder::buildSurfaceTriangleMesh(const boost::dynamic_bitset<>& faceSubset, bool renderFacesTwoSided)
{
    if(cell() && cell()->is2D())
        throw Exception(tr("Cannot generate surface triangle mesh when domain is two-dimensional."));

    // Create accessor for the input mesh data.
    const SurfaceMeshReadAccess inputMeshData(inputMesh());

    // Transfer vertices and faces from half-edge mesh structure to triangle mesh structure.
    _outputMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    inputMeshData.convertToTriMesh(*_outputMesh, _smoothShading, faceSubset, &_originalFaceMap, !renderFacesTwoSided);
    this_task::throwIfCanceled();

    // Assign mesh vertex colors if available.
    determineVertexColors();
    this_task::throwIfCanceled();

    // Flip orientation of mesh faces if requested.
    if(_reverseOrientation) {
        outputMesh()->flipFaces();
        this_task::throwIfCanceled();
    }

    // Convert vertex positions to reduced coordinates and transfer them to the output mesh.
    OVITO_ASSERT(outputMesh()->vertices().size() == inputMeshData.vertexCount());
    if(cell()) {
        BufferReadAccess<Point3> vertexPositions(inputMeshData.expectVertexProperty(SurfaceMeshVertices::PositionProperty));
        SurfaceMesh::vertex_index vidx = 0;
        for(Point3& p : outputMesh()->vertices()) {
            p = cell()->absoluteToReduced(vertexPositions[vidx++]);
            OVITO_ASSERT(std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()));
        }
    }
    this_task::throwIfCanceled();

    // Wrap mesh at periodic boundaries.
    for(size_t dim = 0; dim < 3; dim++) {
        if(!cell() || cell()->hasPbc(dim) == false)
            continue;

        // Make sure all vertices are located inside the periodic box.
        for(Point3& p : outputMesh()->vertices()) {
            OVITO_ASSERT(std::isfinite(p[dim]));
            p[dim] -= std::floor(p[dim]);
            OVITO_ASSERT(p[dim] >= FloatType(0) && p[dim] <= FloatType(1));
        }
        this_task::throwIfCanceled();

        // Split triangle faces at periodic boundaries.
        auto oldFaceCount = outputMesh()->faceCount();
        auto oldVertexCount = outputMesh()->vertexCount();
        std::vector<Point3> newVertices;
        std::vector<ColorAG> newVertexColors;
        std::vector<FloatType> newVertexPseudoColors;
        std::map<std::pair<int,int>,std::tuple<int,int,FloatType>> newVertexLookupMap;
        for(int findex = 0; findex < oldFaceCount; findex++) {
            if(!splitFace(findex, oldVertexCount, newVertices, newVertexColors, newVertexPseudoColors, newVertexLookupMap, dim)) {
                return false;
            }
        }
        this_task::throwIfCanceled();

        // Insert newly created vertices into mesh.
        outputMesh()->setVertexCount(oldVertexCount + newVertices.size());
        std::copy(newVertices.cbegin(), newVertices.cend(), outputMesh()->vertices().begin() + oldVertexCount);
        if(outputMesh()->hasVertexColors()) {
            OVITO_ASSERT(newVertexColors.size() == newVertices.size());
            std::copy(newVertexColors.cbegin(), newVertexColors.cend(), outputMesh()->vertexColors().begin() + oldVertexCount);
        }
        if(outputMesh()->hasVertexPseudoColors()) {
            OVITO_ASSERT(newVertexPseudoColors.size() == newVertices.size());
            std::copy(newVertexPseudoColors.cbegin(), newVertexPseudoColors.cend(), outputMesh()->vertexPseudoColors().begin() + oldVertexCount);
        }
        this_task::throwIfCanceled();
    }

    // Convert vertex positions back from reduced coordinates to absolute coordinates.
    if(cell()) {
        const AffineTransformation cellMatrix = cell()->matrix();
        for(Point3& p : outputMesh()->vertices())
            p = cellMatrix * p;
    }
    this_task::throwIfCanceled();

    // Clip mesh at cutting planes and non-periodic cell boundaries.
    if(!inputMesh()->cuttingPlanes().empty() || (cell() && _clipAtDomainBoundaries)) {

        // Store mapping of original faces to output faces in material index field of TriangleMesh.
        auto of = _originalFaceMap.begin();
        for(TriMeshFace& face : outputMesh()->faces())
            face.setMaterialIndex(*of++);
        this_task::throwIfCanceled();

        for(const Plane3& plane : inputMesh()->cuttingPlanes()) {
            outputMesh()->clipAtPlane(plane);
            this_task::throwIfCanceled();
        }

        if(cell() && _clipAtDomainBoundaries) {
            for(size_t dim = 0; dim < 3; dim++) {
                if(cell()->hasPbc(dim))
                    continue;

                Vector3 normal = cell()->cellNormalVector(dim);
                outputMesh()->clipAtPlane(Plane3(cell()->cellOrigin(), -normal));
                this_task::throwIfCanceled();

                outputMesh()->clipAtPlane(Plane3(cell()->cellOrigin() + cell()->cellMatrix().column(dim), normal));
                this_task::throwIfCanceled();
            }
        }

        // Restore mapping of original faces to output faces from material index field of TriangleMesh.
        _originalFaceMap.resize(outputMesh()->faces().size());
        of = _originalFaceMap.begin();
        for(TriMeshFace& face : outputMesh()->faces())
            *of++ = face.materialIndex();
    }

    outputMesh()->invalidateVertices();
    OVITO_ASSERT(_originalFaceMap.size() == outputMesh()->faces().size());

    return true;
}

/******************************************************************************
* Generates the cap polygons where the surface mesh intersects the
* periodic domain boundaries.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::buildCapTriangleMesh(const boost::dynamic_bitset<>& faceSubset)
{
    OVITO_ASSERT(cell());

    // Create the output mesh object.
    _capPolygonsMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);

    // Create accessor for the input mesh data.
    const SurfaceMeshReadAccess inputMeshData(inputMesh());
    BufferReadAccess<Point3> vertexPositions(inputMeshData.expectVertexProperty(SurfaceMeshVertices::PositionProperty));
    BufferReadAccess<int32_t> faceRegions(inputMeshData.faceProperty(SurfaceMeshFaces::RegionProperty));

    // Access the 'Filled' property of volumetric regions if it is defined for the input surface mesh.
    BufferReadAccess<SelectionIntType> isFilledProperty(inputMeshData.regionProperty(SurfaceMeshRegions::IsFilledProperty));
    bool hasRegions = isFilledProperty && faceRegions;
    bool flipCapNormal = (cell()->matrix().determinant() < 0);

    // Convert vertex positions to reduced coordinates.
    AffineTransformation invCellMatrix = cell()->inverseMatrix();
    if(flipCapNormal)
        invCellMatrix.column(0) = -invCellMatrix.column(0);

    std::vector<Point3> reducedPos(inputMeshData.vertexCount());
    SurfaceMesh::vertex_index vidx = 0;
    for(Point3& p : reducedPos)
        p = invCellMatrix * vertexPositions[vidx++];

    // Indicates for 4 corners of the simulation cell whether they are located inside (1) or outside (0) of the filled mesh region.
    // Initial value -1 indicates that the inside/outside test has not been performed yet.
    //
    // Array index 0: Cell origin
    // Array index 1: Cell origin + cell vector 1
    // Array index 2: Cell origin + cell vector 2
    // Array index 3: Cell origin + cell vector 3
    int isBoxCornerInside3DRegion[4] = {-1, -1, -1, -1};

    // Create caps on each side of the simulation with periodic boundary conditions.
    for(size_t dim = 0; dim < 3; dim++) {

        // Are periodic boundary conditions enabled for the current simulation cell direction?
        bool periodic = cell()->hasPbc(dim);

        // Skip non-periodic boundaries unless clipping of the mesh at non-periodic boundaries has been enabled.
        if(!periodic && !_clipAtDomainBoundaries)
            continue;

        this_task::throwIfCanceled();

        // Make sure all vertices are located inside the cell along periodic directions.
        if(periodic) {
            for(Point3& p : reducedPos) {
                FloatType& c = p[dim];
                OVITO_ASSERT(std::isfinite(c));
                if(FloatType s = std::floor(c))
                    c -= s;
            }
        }

        // Perform the following just once for periodic boundaries of the simulation cell and twice for non-periodic boundaries,
        // once for either side of the cell.
        const auto periodicList = { CapPolygonTessellator::PeriodicFace };
        const auto nonperiodicList = { CapPolygonTessellator::FrontFace, CapPolygonTessellator::BackFace };
        for(CapPolygonTessellator::FaceMode faceMode : periodic ? periodicList : nonperiodicList) {

            // Used to keep track of already visited faces during the current pass.
            std::vector<bool> visitedFaces(inputMeshData.faceCount(), false);

            // The lists of 2d contours generated by clipping the 3d surface mesh.
            std::vector<std::vector<Point2>> openContours;
            std::vector<std::vector<Point2>> closedContours;

            // Find a first edge that crosses a cell boundary.
            for(SurfaceMesh::face_index face : _originalFaceMap) {
                // Skip faces that have already been visited.
                if(visitedFaces[face]) continue;
                this_task::throwIfCanceled();
                visitedFaces[face] = true;

                // Determine whether the mesh face is bordering a filled or an empty region.
                if(hasRegions) {
                    SurfaceMesh::region_index region = faceRegions[face];
                    if(region >= 0 && region < isFilledProperty.size()) {
                        if((bool)isFilledProperty[region] == _reverseOrientation) {
                            // Skip faces that are adjacent to an empty volumetric region.
                            continue;
                        }

                        // Also skip any two-sided faces that are part of an interior interface.
                        SurfaceMesh::face_index oppositeFace = inputMeshData.oppositeFace(face);
                        if(oppositeFace != SurfaceMesh::InvalidIndex) {
                            SurfaceMesh::region_index oppositeRegion = faceRegions[oppositeFace];
                            if(oppositeRegion >= 0 && oppositeRegion < isFilledProperty.size()) {
                                if((bool)isFilledProperty[oppositeRegion] != _reverseOrientation) {
                                    continue;
                                }
                            }
                        }
                    }
                }

                // Visit the halfedges of the current mesh face.
                SurfaceMesh::edge_index startEdge = inputMeshData.firstFaceEdge(face);
                SurfaceMesh::edge_index edge = startEdge;
                do {
                    const Point3& v1 = reducedPos[inputMeshData.vertex1(edge)];
                    const Point3& v2 = reducedPos[inputMeshData.vertex2(edge)];
                    bool crossesBoundary = periodic
                        ? (v2[dim] - v1[dim] >= FloatType(0.5))
                        : (faceMode == CapPolygonTessellator::FrontFace
                            ? (v2[dim] < 0 && v1[dim] >= 0)
                            : (v2[dim] <= 1 && v1[dim] > 1));
                    if(crossesBoundary) {
                        std::vector<Point2> contour = traceContour(*inputMesh()->topology(), edge, reducedPos, visitedFaces, dim, faceMode);
                        if(contour.empty())
                            throw Exception(tr("Surface mesh does not represent a proper closed manifold."));
                        if(!_clipAtDomainBoundaries) {
                            sliceContourAtPeriodicBoundaries(contour,
                                std::array<bool,2>{{ cell()->hasPbc((dim+1)%3), cell()->hasPbc((dim+2)%3) }},
                                openContours, closedContours);
                        }
                        else {
                            sliceAndClipContour(contour,
                                std::array<bool,2>{{ cell()->hasPbc((dim+1)%3), cell()->hasPbc((dim+2)%3) }},
                                openContours, closedContours);
                        }
                        break;
                    }
                    edge = inputMeshData.nextFaceEdge(edge);
                }
                while(edge != startEdge);
            }

            // Invert surface orientation if requested. (Not needed if regions are defined. Then we can just swap roles of filled and empty regions).
            if(!hasRegions && _reverseOrientation) {
                for(auto& contour : openContours)
                    std::reverse(std::begin(contour), std::end(contour));
            }

            // Feed contours into tessellator to create triangles.
            CapPolygonTessellator tessellator(*_capPolygonsMesh, dim, faceMode);
            tessellator.beginPolygon();
            for(const auto& contour : closedContours) {
                this_task::throwIfCanceled();
                tessellator.beginContour();
                for(const Point2& p : contour) {
                    tessellator.vertex(p);
                }
                tessellator.endContour();
            }

            auto yxCoord2ArcLength = [](const Point2& p) {
                if(p.x() == 0) return p.y();
                else if(p.y() == 1) return p.x() + FloatType(1);
                else if(p.x() == 1) return FloatType(3) - p.y();
                else return std::fmod(FloatType(4) - p.x(), FloatType(4));
            };

            // Build the outer contour.
            if(!openContours.empty()) {
                boost::dynamic_bitset<> visitedContours(openContours.size());
                for(auto c1 = openContours.begin(); c1 != openContours.end(); ++c1) {
                    this_task::throwIfCanceled();
                    if(!visitedContours.test(c1 - openContours.begin())) {
                        tessellator.beginContour();
                        auto currentContour = c1;
                        do {
                            for(const Point2& p : *currentContour) {
                                tessellator.vertex(p);
                            }
                            visitedContours.set(currentContour - openContours.begin());

                            FloatType t_exit = yxCoord2ArcLength(currentContour->back());

                            // Find the next contour.
                            FloatType t_entry;
                            FloatType closestDist = FLOATTYPE_MAX;
                            for(auto c = openContours.begin(); c != openContours.end(); ++c) {
                                FloatType t = yxCoord2ArcLength(c->front());
                                FloatType dist = t_exit - t;
                                if(dist < 0) dist += FloatType(4);
                                if(dist < closestDist) {
                                    closestDist = dist;
                                    currentContour = c;
                                    t_entry = t;
                                }
                            }
                            int exitCorner = (int)std::floor(t_exit);
                            int entryCorner = (int)std::floor(t_entry);
                            if(exitCorner < 0 || exitCorner >= 4) break;
                            if(entryCorner < 0 || entryCorner >= 4) break;
                            if(exitCorner != entryCorner || t_exit < t_entry) {
                                for(int corner = exitCorner;;) {
                                    switch(corner) {
                                    case 0: tessellator.vertex(Point2(0,0)); break;
                                    case 1: tessellator.vertex(Point2(0,1)); break;
                                    case 2: tessellator.vertex(Point2(1,1)); break;
                                    case 3: tessellator.vertex(Point2(1,0)); break;
                                    }
                                    corner = (corner + 3) % 4;
                                    if(corner == entryCorner) break;
                                }
                            }
                        }
                        while(!visitedContours.test(currentContour - openContours.begin()));
                        tessellator.endContour();
                    }
                }
            }
            else {
                int& isInside = (faceMode != CapPolygonTessellator::BackFace) ? isBoxCornerInside3DRegion[0] : isBoxCornerInside3DRegion[dim+1];
                if(isInside == -1) {
                    if(closedContours.empty()) {
                        Point3 corner = cell()->cellOrigin();
                        if(faceMode == CapPolygonTessellator::BackFace)
                            corner += cell()->cellMatrix().column(dim);
                        if(std::optional<std::pair<SurfaceMesh::region_index, FloatType>> region = inputMeshData.locatePoint(corner, 0, faceSubset)) {
                            if(hasRegions) {
                                if(region->first >= 0 && region->first < isFilledProperty.size()) {
                                    isInside = (bool)isFilledProperty[region->first];
                                }
                                else
                                    isInside = false;
                            }
                            else {
                                isInside = region->first != SurfaceMesh::InvalidIndex;
                            }
                        }
                        else {
                            isInside = false;
                        }
                    }
                    else {
                        isInside = isCornerInside2DRegion(closedContours);
                        if(hasRegions && _reverseOrientation)
                            isInside = !isInside;
                    }
                    if(_reverseOrientation)
                        isInside = !isInside;
                }
                if(isInside) {
                    tessellator.beginContour();
                    tessellator.vertex(Point2(0,0));
                    tessellator.vertex(Point2(1,0));
                    tessellator.vertex(Point2(1,1));
                    tessellator.vertex(Point2(0,1));
                    tessellator.endContour();
                }
            }

            tessellator.endPolygon();
        }
    }

    // Check for early abortion.
    this_task::throwIfCanceled();

    // Convert vertex positions back from reduced coordinates to absolute coordinates.
    const AffineTransformation cellMatrix = invCellMatrix.inverse();
    for(Point3& p : _capPolygonsMesh->vertices())
        p = cellMatrix * p;
    this_task::throwIfCanceled();

    // Clip mesh at cutting planes.
    for(const Plane3& plane : inputMesh()->cuttingPlanes()) {
        _capPolygonsMesh->clipAtPlane(plane);
        this_task::throwIfCanceled();
    }
}

/******************************************************************************
* Splits a triangle face at a periodic boundary.
******************************************************************************/
bool SurfaceMeshVis::RenderableSurfaceBuilder::splitFace(int faceIndex, int oldVertexCount, std::vector<Point3>& newVertices, std::vector<ColorAG>& newVertexColors,
        std::vector<FloatType>& newVertexPseudoColors, std::map<std::pair<int,int>,std::tuple<int,int,FloatType>>& newVertexLookupMap, size_t dim)
{
    TriMeshFace& face = outputMesh()->face(faceIndex);
    OVITO_ASSERT(face.vertex(0) != face.vertex(1));
    OVITO_ASSERT(face.vertex(1) != face.vertex(2));
    OVITO_ASSERT(face.vertex(2) != face.vertex(0));

    FloatType z[3];
    for(int v = 0; v < 3; v++)
        z[v] = outputMesh()->vertex(face.vertex(v))[dim];
    FloatType zd[3] = { z[1] - z[0], z[2] - z[1], z[0] - z[2] };

    OVITO_ASSERT(z[1] - z[0] == -(z[0] - z[1]));
    OVITO_ASSERT(z[2] - z[1] == -(z[1] - z[2]));
    OVITO_ASSERT(z[0] - z[2] == -(z[2] - z[0]));

    if(std::abs(zd[0]) < FloatType(0.5) && std::abs(zd[1]) < FloatType(0.5) && std::abs(zd[2]) < FloatType(0.5))
        return true;    // Face does not cross the periodic boundary.

    // Create four new vertices (or use existing ones created during splitting of adjacent faces).
    int properEdge = -1;
    int newVertexIndices[3][2];
    Vector3G interpolatedNormals[3];
    for(int i = 0; i < 3; i++) {
        if(std::abs(zd[i]) < FloatType(0.5)) {
            if(properEdge != -1)
                return false;       // The simulation box may be too small or invalid.
            properEdge = i;
            continue;
        }
        int vi1 = face.vertex(i);
        int vi2 = face.vertex((i+1)%3);
        int oi1, oi2;
        if(zd[i] <= FloatType(-0.5)) {
            std::swap(vi1, vi2);
            oi1 = 1; oi2 = 0;
        }
        else {
            oi1 = 0; oi2 = 1;
        }
        auto entry = newVertexLookupMap.find(std::make_pair(vi1, vi2));
        if(entry != newVertexLookupMap.end()) {
            newVertexIndices[i][oi1] = std::get<0>(entry->second);
            newVertexIndices[i][oi2] = std::get<1>(entry->second);
        }
        else {
            Vector3 delta = outputMesh()->vertex(vi2) - outputMesh()->vertex(vi1);
            delta[dim] -= FloatType(1);
            if(cell()) {
                for(size_t d = dim + 1; d < 3; d++) {
                    if(cell()->hasPbc(d))
                        delta[d] -= std::floor(delta[d] + FloatType(0.5));
                }
            }
            FloatType t;
            if(delta[dim] != 0)
                t = outputMesh()->vertex(vi1)[dim] / (-delta[dim]);
            else
                t = FloatType(0.5);
            OVITO_ASSERT(std::isfinite(t));
            Point3 p = delta * t + outputMesh()->vertex(vi1);
            newVertexIndices[i][oi1] = oldVertexCount + (int)newVertices.size();
            newVertexIndices[i][oi2] = oldVertexCount + (int)newVertices.size() + 1;
            entry = newVertexLookupMap.emplace(std::make_pair(vi1, vi2), std::make_tuple(newVertexIndices[i][oi1], newVertexIndices[i][oi2], t)).first;
            newVertices.push_back(p);
            p[dim] += FloatType(1);
            newVertices.push_back(p);
            // Compute the color at the intersection point by interpolating the colors of the two existing vertices.
            if(outputMesh()->hasVertexColors()) {
                const ColorAG& color1 = outputMesh()->vertexColor(vi1);
                const ColorAG& color2 = outputMesh()->vertexColor(vi2);
                ColorAG interp_color(color1.r() + (color2.r() - color1.r()) * static_cast<GraphicsFloatType>(t),
                                    color1.g() + (color2.g() - color1.g()) * static_cast<GraphicsFloatType>(t),
                                    color1.b() + (color2.b() - color1.b()) * static_cast<GraphicsFloatType>(t),
                                    color1.a() + (color2.a() - color1.a()) * static_cast<GraphicsFloatType>(t));
                newVertexColors.push_back(interp_color);
                newVertexColors.push_back(interp_color);
            }
            if(outputMesh()->hasVertexPseudoColors()) {
                FloatType pseudocolor1 = outputMesh()->vertexPseudoColor(vi1);
                FloatType pseudocolor2 = outputMesh()->vertexPseudoColor(vi2);
                FloatType interp_pseudocolor = pseudocolor1 + (pseudocolor2 - pseudocolor1) * t;
                newVertexPseudoColors.push_back(interp_pseudocolor);
                newVertexPseudoColors.push_back(interp_pseudocolor);
            }
        }
        // Compute interpolated normal vector at intersection point.
        if(_smoothShading) {
            const Vector3G& n1 = outputMesh()->faceVertexNormal(faceIndex, (i+oi1)%3);
            const Vector3G& n2 = outputMesh()->faceVertexNormal(faceIndex, (i+oi2)%3);
            GraphicsFloatType t = std::get<2>(entry->second);
            interpolatedNormals[i] = n1 * t + n2 * (GraphicsFloatType(1) - t);
            interpolatedNormals[i].normalizeSafely();
        }
    }
    OVITO_ASSERT(properEdge != -1);

    // Build output triangles.
    int originalVertices[3] = { face.vertex(0), face.vertex(1), face.vertex(2) };
    bool originalEdgeVisibility[3] = { face.edgeVisible(0), face.edgeVisible(1), face.edgeVisible(2) };
    int pe1 = (properEdge+1)%3;
    int pe2 = (properEdge+2)%3;
    face.setVertices(originalVertices[properEdge], originalVertices[pe1], newVertexIndices[pe2][1]);
    face.setEdgeVisibility(originalEdgeVisibility[properEdge], false, originalEdgeVisibility[pe2]);

    int materialIndex = face.materialIndex();
    OVITO_ASSERT(_originalFaceMap.size() == outputMesh()->faceCount());
    outputMesh()->setFaceCount(outputMesh()->faceCount() + 2);
    _originalFaceMap.resize(_originalFaceMap.size() + 2, _originalFaceMap[faceIndex]);
    TriMeshFace& newFace1 = outputMesh()->face(outputMesh()->faceCount() - 2);
    TriMeshFace& newFace2 = outputMesh()->face(outputMesh()->faceCount() - 1);
    newFace1.setVertices(originalVertices[pe1], newVertexIndices[pe1][0], newVertexIndices[pe2][1]);
    newFace2.setVertices(newVertexIndices[pe1][1], originalVertices[pe2], newVertexIndices[pe2][0]);
    newFace1.setMaterialIndex(materialIndex);
    newFace2.setMaterialIndex(materialIndex);
    newFace1.setEdgeVisibility(originalEdgeVisibility[pe1], false, false);
    newFace2.setEdgeVisibility(originalEdgeVisibility[pe1], originalEdgeVisibility[pe2], false);
    if(_smoothShading) {
        auto n = outputMesh()->normals().end() - 6;
        *n++ = outputMesh()->faceVertexNormal(faceIndex, pe1);
        *n++ = interpolatedNormals[pe1];
        *n++ = interpolatedNormals[pe2];
        *n++ = interpolatedNormals[pe1];
        *n++ = outputMesh()->faceVertexNormal(faceIndex, pe2);
        *n   = interpolatedNormals[pe2];
        n = outputMesh()->normals().begin() + faceIndex*3;
        std::rotate(n, n + properEdge, n + 3);
        outputMesh()->setFaceVertexNormal(faceIndex, 2, interpolatedNormals[pe2]);
    }

    return true;
}

/******************************************************************************
* Traces the closed contour of the surface-boundary intersection.
******************************************************************************/
std::vector<Point2> SurfaceMeshVis::RenderableSurfaceBuilder::traceContour(const SurfaceMeshTopology& inputMeshTopology, SurfaceMesh::edge_index firstEdge, const std::vector<Point3>& reducedPos, std::vector<bool>& visitedFaces, size_t dim, CapPolygonTessellator::FaceMode faceMode) const
{
    OVITO_ASSERT(cell());
    size_t dim1 = (dim + 1) % 3;
    size_t dim2 = (dim + 2) % 3;
    std::vector<Point2> contour;
    SurfaceMesh::edge_index edge = firstEdge;
    do {
        OVITO_ASSERT(inputMeshTopology.adjacentFace(edge) != SurfaceMesh::InvalidIndex);

        // Mark face as visited.
        visitedFaces[inputMeshTopology.adjacentFace(edge)] = true;

        // Compute intersection point.
        Point3 v1 = reducedPos[inputMeshTopology.vertex1(edge)];
        Point3 v2 = reducedPos[inputMeshTopology.vertex2(edge)];
        Vector3 delta = v2 - v1;

        if(faceMode == CapPolygonTessellator::PeriodicFace) {
            OVITO_ASSERT(delta[dim] >= FloatType(0.5));
            delta[dim] -= FloatType(1);
        }

        if(cell()->hasPbc(dim1)) {
            FloatType& c = delta[dim1];
            c -= std::floor(c + FloatType(0.5));
        }
        if(cell()->hasPbc(dim2)) {
            FloatType& c = delta[dim2];
            c -= std::floor(c + FloatType(0.5));
        }

        if(std::abs(delta[dim]) > FloatType(1e-9f)) {
            FloatType t = (faceMode != CapPolygonTessellator::BackFace ? v1[dim] : v1[dim]-FloatType(1)) / delta[dim];
            FloatType x = v1[dim1] - delta[dim1] * t;
            FloatType y = v1[dim2] - delta[dim2] * t;
            OVITO_ASSERT(std::isfinite(x) && std::isfinite(y));
            if(contour.empty() || std::abs(x - contour.back().x()) > FLOATTYPE_EPSILON || std::abs(y - contour.back().y()) > FLOATTYPE_EPSILON) {
                contour.push_back({x,y});
            }
        }
        else {
            FloatType x1 = v1[dim1];
            FloatType y1 = v1[dim2];
            FloatType x2 = v1[dim1] + delta[dim1];
            FloatType y2 = v1[dim2] + delta[dim2];
            if(contour.empty() || std::abs(x1 - contour.back().x()) > FLOATTYPE_EPSILON || std::abs(y1 - contour.back().y()) > FLOATTYPE_EPSILON) {
                contour.push_back({x1,y1});
            }
            else if(contour.empty() || std::abs(x2 - contour.back().x()) > FLOATTYPE_EPSILON || std::abs(y2 - contour.back().y()) > FLOATTYPE_EPSILON) {
                contour.push_back({x2,y2});
            }
        }

        // Find the face edge that crosses the boundary in the reverse direction.
        FloatType v1d = v2[dim];
        for(;;) {
            edge = inputMeshTopology.nextFaceEdge(edge);
            FloatType v2d = reducedPos[inputMeshTopology.vertex2(edge)][dim];
            if(faceMode == CapPolygonTessellator::PeriodicFace) {
                if(v2d - v1d <= FloatType(-0.5))
                    break;
            }
            else if(faceMode == CapPolygonTessellator::FrontFace) {
                if(v2d >= 0 && v1d < 0)
                    break;
            }
            else {
                if(v2d > 1 && v1d <= 1)
                    break;
            }
            v1d = v2d;
        }

        edge = inputMeshTopology.oppositeEdge(edge);
        if(edge == SurfaceMesh::InvalidIndex) {
            // Mesh is not closed (not a proper manifold).
            contour.clear();
            break;
        }
    }
    while(edge != firstEdge);
    return contour;
}

/******************************************************************************
* Slices a 2d contour at periodic boundaries.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::sliceContourAtPeriodicBoundaries(std::vector<Point2>& input, std::array<bool,2> pbcFlags, std::vector<std::vector<Point2>>& openContours, std::vector<std::vector<Point2>>& closedContours)
{
    if(!pbcFlags[0] && !pbcFlags[1]) {
        closedContours.push_back(std::move(input));
        return;
    }

    // Ensure all coordinates are mapped into the primary image of the periodic domain.
    if(pbcFlags[0]) {
        for(auto& v : input) {
            OVITO_ASSERT(std::isfinite(v.x()));
             v.x() -= std::floor(v.x());
        }
    }
    if(pbcFlags[1]) {
        for(auto& v : input) {
            OVITO_ASSERT(std::isfinite(v.y()));
            v.y() -= std::floor(v.y());
        }
    }

    std::vector<std::vector<Point2>> contours;
    contours.emplace_back();

    // Starting point of the contour.
    auto v1 = std::prev(input.cend());

    // Walk around the input contour.
    for(auto v2 = input.cbegin(); v2 != input.cend(); v1 = v2, ++v2) {

        // Append current vertex to output contour.
        contours.back().push_back(*v1);

        Vector2 delta = (*v2) - (*v1);
        if((!pbcFlags[0] || std::abs(delta.x()) < FloatType(0.5)) && (!pbcFlags[1] || std::abs(delta.y()) < FloatType(0.5)))
            continue;

        FloatType t[2] = { 2, 2 };
        Vector2I crossDir(0, 0);
        for(size_t dim = 0; dim < 2; dim++) {
            if(pbcFlags[dim]) {
                if(delta[dim] >= FloatType(0.5)) {
                    delta[dim] -= FloatType(1);
                    if(std::abs(delta[dim]) > FLOATTYPE_EPSILON)
                        t[dim] = std::min((*v1)[dim] / -delta[dim], FloatType(1));
                    else
                        t[dim] = FloatType(0.5);
                    crossDir[dim] = -1;
                    OVITO_ASSERT(t[dim] >= 0 && t[dim] <= 1);
                }
                else if(delta[dim] <= FloatType(-0.5)) {
                    delta[dim] += FloatType(1);
                    if(std::abs(delta[dim]) > FLOATTYPE_EPSILON)
                        t[dim] = std::max((FloatType(1) - (*v1)[dim]) / delta[dim], FloatType(0));
                    else
                        t[dim] = FloatType(0.5);
                    crossDir[dim] = +1;
                    OVITO_ASSERT(t[dim] >= 0 && t[dim] <= 1);
                }
            }
        }

        Point2 base = *v1;
        if(t[0] < t[1]) {
            OVITO_ASSERT(t[0] <= 1);
            computeContourIntersectionPeriodic(0, t[0], base, delta, crossDir[0], contours);
            if(crossDir[1] != 0) {
                OVITO_ASSERT(t[1] <= 1);
                computeContourIntersectionPeriodic(1, t[1], base, delta, crossDir[1], contours);
            }
        }
        else if(t[1] < t[0]) {
            OVITO_ASSERT(t[1] <= 1);
            computeContourIntersectionPeriodic(1, t[1], base, delta, crossDir[1], contours);
            if(crossDir[0] != 0) {
                OVITO_ASSERT(t[0] <= 1);
                computeContourIntersectionPeriodic(0, t[0], base, delta, crossDir[0], contours);
            }
        }
    }

    if(contours.size() == 1) {
        closedContours.push_back(std::move(contours.back()));
    }
    else {
        auto& firstSegment = contours.front();
        auto& lastSegment = contours.back();
        firstSegment.insert(firstSegment.begin(), lastSegment.begin(), lastSegment.end());
        contours.pop_back();
        for(auto& contour : contours) {
            bool isDegenerate = std::all_of(contour.begin(), contour.end(), [&contour](const Point2& p) {
                return p.equals(contour.front());
            });
            if(!isDegenerate)
                openContours.push_back(std::move(contour));
        }
    }
}

/******************************************************************************
* Slices a 2d contour at periodic boundaries and clips it an non-periodic boundaries.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::sliceAndClipContour(std::vector<Point2>& input, std::array<bool,2> pbcFlags, std::vector<std::vector<Point2>>& openContours, std::vector<std::vector<Point2>>& closedContours)
{
    // Ensure all coordinates are mapped into the primary image of the periodic domain.
    if(pbcFlags[0]) {
        for(auto& v : input) {
            OVITO_ASSERT(std::isfinite(v.x()));
             v.x() -= std::floor(v.x());
        }
    }
    if(pbcFlags[1]) {
        for(auto& v : input) {
            OVITO_ASSERT(std::isfinite(v.y()));
            v.y() -= std::floor(v.y());
        }
    }

    std::vector<std::vector<Point2>> contours;
    contours.emplace_back();

    // Starting point of the contour.
    auto v1 = std::prev(input.cend());

    // Clipping out codes:
    enum {
        OUT_NEG_X = (1<<0),
        OUT_POS_X = (1<<1),
        OUT_NEG_Y = (1<<2),
        OUT_POS_Y = (1<<3)
    };

    // Does the contour start outside of the clipping boundary?
    int clipCode = 0;
    if(!pbcFlags[0]) {
        if(v1->x() < 0) clipCode |= OUT_NEG_X;
        else if(v1->x() > 1) clipCode |= OUT_POS_X;
    }
    if(!pbcFlags[1]) {
        if(v1->y() < 0) clipCode |= OUT_NEG_Y;
        else if(v1->y() > 1) clipCode |= OUT_POS_Y;
    }

    // Walk around the input contour.
    for(auto v2 = input.cbegin(); v2 != input.cend(); v1 = v2, ++v2) {

        // Append current vertex to output contour (unless we are outside the clipping boundaries).
        Point2 base = *v1;
        if(clipCode == 0)
            contours.back().push_back(base);

        do {
            Vector2 delta = (*v2) - base;

            // Determine closest boundary intersection.
            FloatType t_closest = 2;
            Point2 intersect_closest;
            int periodic_cross_dir = 0;
            int new_clip_code = -1;
            for(size_t dim = 0; dim < 2; dim++) {
                if(pbcFlags[dim]) { // Periodic boundary?
                    if(delta[dim] >= FloatType(0.5)) { // Crossing through periodic 0-edge?
                        delta[dim] -= FloatType(1);
                        if(std::abs(delta[dim]) > FLOATTYPE_EPSILON) {
                            FloatType t = std::min(base[dim] / -delta[dim], FloatType(1));
                            if(t < t_closest) {
                                t_closest = t;
                                intersect_closest = base + t * delta;
                                intersect_closest[dim] = FloatType(0);
                                periodic_cross_dir = (dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                            }
                        }
                        else {
                            t_closest = 0;
                            intersect_closest = base;
                            intersect_closest[dim] = FloatType(0);
                            periodic_cross_dir = (dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                        }
                    }
                    else if(delta[dim] <= FloatType(-0.5)) { // Crossing through periodic 1-edge?
                        delta[dim] += FloatType(1);
                        if(std::abs(delta[dim]) > FLOATTYPE_EPSILON) {
                            FloatType t = std::max((FloatType(1) - base[dim]) / delta[dim], FloatType(0));
                            if(t < t_closest) {
                                t_closest = t;
                                intersect_closest = base + t * delta;
                                intersect_closest[dim] = FloatType(1);
                                periodic_cross_dir = (dim == 0 ? OUT_POS_X : OUT_POS_Y);
                            }
                        }
                        else {
                            t_closest = 0;
                            intersect_closest = base;
                            intersect_closest[dim] = FloatType(1);
                            periodic_cross_dir = (dim == 0 ? OUT_POS_X : OUT_POS_Y);
                        }
                    }
                }
                else { // Non-periodic (clipping) boundary?
                    if(clipCode & (dim == 0 ? OUT_NEG_X : OUT_NEG_Y)) {
                        OVITO_ASSERT(base[dim] <= 0);
                        if((*v2)[dim] >= 0) { // Entering through 0-edge?
                            if(delta[dim] > FLOATTYPE_EPSILON) {
                                FloatType t = std::max(base[dim] / delta[dim], FloatType(0));
                                if(t < t_closest) {
                                    t_closest = t;
                                    intersect_closest = base + t * delta;
                                    intersect_closest[dim] = FloatType(0);
                                    periodic_cross_dir = 0;
                                    new_clip_code = clipCode & ~(dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                                }
                            }
                            else {
                                t_closest = 0;
                                intersect_closest = base;
                                intersect_closest[dim] = FloatType(0);
                                periodic_cross_dir = 0;
                                new_clip_code = clipCode & ~(dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                            }
                        }
                    }
                    else {
                        OVITO_ASSERT(base[dim] >= 0);
                        if((*v2)[dim] < 0) { // Leaving through 0-edge?
                            if(delta[dim] < -FLOATTYPE_EPSILON) {
                                FloatType t = std::min(base[dim] / -delta[dim], FloatType(1));
                                if(t < t_closest) {
                                    t_closest = t;
                                    intersect_closest = base + t * delta;
                                    intersect_closest[dim] = FloatType(0);
                                    periodic_cross_dir = 0;
                                    new_clip_code = clipCode | (dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                                }
                            }
                            else {
                                t_closest = 0;
                                intersect_closest = base;
                                intersect_closest[dim] = FloatType(0);
                                periodic_cross_dir = 0;
                                new_clip_code = clipCode | (dim == 0 ? OUT_NEG_X : OUT_NEG_Y);
                            }
                        }
                    }
                    if(clipCode & (dim == 0 ? OUT_POS_X : OUT_POS_Y)) {
                        OVITO_ASSERT(base[dim] >= 1);
                        if((*v2)[dim] <= 1) { // Entering through 1-edge?
                            if(delta[dim] < -FLOATTYPE_EPSILON) {
                                FloatType t = std::min((base[dim] - FloatType(1)) / -delta[dim], FloatType(1));
                                if(t < t_closest) {
                                    t_closest = t;
                                    intersect_closest = base + t * delta;
                                    intersect_closest[dim] = FloatType(1);
                                    periodic_cross_dir = 0;
                                    new_clip_code = clipCode & ~(dim == 0 ? OUT_POS_X : OUT_POS_Y);
                                }
                            }
                            else {
                                t_closest = 0;
                                intersect_closest = base;
                                intersect_closest[dim] = FloatType(1);
                                periodic_cross_dir = 0;
                                new_clip_code = clipCode & ~(dim == 0 ? OUT_POS_X : OUT_POS_Y);
                            }
                        }
                    }
                    else {
                        OVITO_ASSERT(base[dim] <= 1);
                        if((*v2)[dim] > 1) { // Leaving through 1-edge?
                            if(delta[dim] > FLOATTYPE_EPSILON) {
                                FloatType t = std::max((FloatType(1) - base[dim]) / delta[dim], FloatType(0));
                                if(t < t_closest) {
                                    t_closest = t;
                                    intersect_closest = base + t * delta;
                                    intersect_closest[dim] = FloatType(1);
                                    periodic_cross_dir = 0;
                                    new_clip_code = clipCode | (dim == 0 ? OUT_POS_X : OUT_POS_Y);
                                }
                            }
                            else {
                                t_closest = 0;
                                intersect_closest = base;
                                intersect_closest[dim] = FloatType(1);
                                periodic_cross_dir = 0;
                                new_clip_code = clipCode | (dim == 0 ? OUT_POS_X : OUT_POS_Y);
                            }
                        }
                    }
                }
            }
            if(t_closest > FloatType(1)) // No intersection?
                break;
            if(periodic_cross_dir != 0) { // Intersection with periodic boundary?
                if(clipCode == 0) {
                    contours.back().push_back(intersect_closest);
                }
                // Wrap point to the opposite side of the domain.
                if(periodic_cross_dir == OUT_NEG_X)
                    intersect_closest[0] = FloatType(1);
                else if(periodic_cross_dir == OUT_POS_X)
                    intersect_closest[0] = FloatType(0);
                else if(periodic_cross_dir == OUT_NEG_Y)
                    intersect_closest[1] = FloatType(1);
                else
                    intersect_closest[1] = FloatType(0);
                if(clipCode == 0) {
                    contours.push_back({intersect_closest});
                }
            }
            else { // Intersection with non-periodic (clipping) boundary?
                OVITO_ASSERT(new_clip_code != clipCode);
                if(clipCode == 0) {
                    contours.back().push_back(intersect_closest);
                }
                else if(new_clip_code == 0) {
                    contours.push_back({intersect_closest});
                }
                clipCode = new_clip_code;
            }
            base = intersect_closest;
        }
        while(true);
    }

    if(contours.size() == 1) {
        if(!contours.back().empty())
            closedContours.push_back(std::move(contours.back()));
    }
    else {
        auto& firstSegment = contours.front();
        auto& lastSegment = contours.back();
        firstSegment.insert(firstSegment.begin(), lastSegment.begin(), lastSegment.end());
        contours.pop_back();
        for(auto& contour : contours) {
            OVITO_ASSERT(contour.size() > 1);
            bool isDegenerate = std::all_of(contour.begin(), contour.end(), [&contour](const Point2& p) {
                return p.equals(contour.front());
            });
            if(!isDegenerate)
                openContours.push_back(std::move(contour));
        }
    }
}

/******************************************************************************
* Computes the intersection point of a 2d contour segment crossing a periodic boundary.
******************************************************************************/
void SurfaceMeshVis::RenderableSurfaceBuilder::computeContourIntersectionPeriodic(size_t dim, FloatType t, Point2& base, Vector2& delta, int crossDir, std::vector<std::vector<Point2>>& contours)
{
    OVITO_ASSERT(std::isfinite(t));
    Point2 intersection = base + t * delta;
    intersection[dim] = (crossDir == -1) ? 0 : 1;
    contours.back().push_back(intersection);
    intersection[dim] = (crossDir == +1) ? 0 : 1;
    contours.push_back({intersection});
    base = intersection;
    delta *= (FloatType(1) - t);
}

/******************************************************************************
* Determines if the 2D box corner (0,0) is inside the closed region described
* by the 2d polygon.
*
* 2D version of the algorithm:
*
* J. Andreas Baerentzen and Henrik Aanaes
* Signed Distance Computation Using the Angle Weighted Pseudonormal
* IEEE Transactions on Visualization and Computer Graphics 11 (2005), Page 243
******************************************************************************/
bool SurfaceMeshVis::RenderableSurfaceBuilder::isCornerInside2DRegion(const std::vector<std::vector<Point2>>& contours)
{
    OVITO_ASSERT(!contours.empty());
    bool isInside = true;

    // Determine which vertex is closest to the test point.
    std::vector<Point2>::const_iterator closestVertex = contours.front().end();
    FloatType closestDistanceSq = FLOATTYPE_MAX;
    for(const auto& contour : contours) {
        auto v1 = contour.end() - 1;
        for(auto v2 = contour.begin(); v2 != contour.end(); v1 = v2++) {
            Vector2 r = (*v1) - Point2::Origin();
            FloatType distanceSq = r.squaredLength();
            if(distanceSq < closestDistanceSq) {
                closestDistanceSq = distanceSq;
                closestVertex = v1;

                // Compute pseuso-normal at vertex.
                auto v0 = (v1 == contour.begin()) ? std::prev(contour.end()) : std::prev(v1);
                Vector2 edge1 = (*v2) - (*v1);
                Vector2 edge2 = (*v1) - (*v0);
                Vector2 normal1(edge1.y(), -edge1.x());
                Vector2 normal2(edge2.y(), -edge2.x());
                normal1.normalizeSafely();
                normal2.normalizeSafely();
                Vector2 normal = normal1 + normal2;
                isInside = (normal.dot(r) > 0);
            }

            // Check if any edge is closer to the test point.
            Vector2 edgeDir = (*v2) - (*v1);
            FloatType edgeLength = edgeDir.length();
            if(edgeLength <= FLOATTYPE_EPSILON) continue;
            edgeDir /= edgeLength;
            FloatType d = -edgeDir.dot(r);
            if(d <= 0 || d >= edgeLength) continue;
            Vector2 c = r + edgeDir * d;
            distanceSq = c.squaredLength();
            if(distanceSq < closestDistanceSq) {
                closestDistanceSq = distanceSq;

                // Compute normal at edge.
                Vector2 normal(edgeDir.y(), -edgeDir.x());
                isInside = (normal.dot(c) > 0);
            }
        }
    }

    return isInside;
}

}   // End of namespace
