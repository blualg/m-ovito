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


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/RenderableSurfaceMesh.h>
#include <ovito/mesh/util/CapPolygonTessellator.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>
#include <ovito/core/dataset/data/TransformingDataVis.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering SurfaceMesh data objects.
 */
class OVITO_MESH_EXPORT SurfaceMeshVis : public TransformingDataVis
{
    OVITO_CLASS(SurfaceMeshVis)
    OVITO_CLASSINFO("DisplayName", "Surface mesh");

public:

    enum ColorMappingMode {
        NoPseudoColoring,
        VertexPseudoColoring,
        FacePseudoColoring,
        RegionPseudoColoring
    };
    Q_ENUM(ColorMappingMode);

    /// Constructor.
    explicit SurfaceMeshVis(ObjectInitializationFlags flags);

    /// Lets the visualization element render the data object.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Returns the transparency of the surface mesh.
    FloatType surfaceTransparency() const { return surfaceTransparencyController() ? surfaceTransparencyController()->getFloatValue(AnimationTime(0)) : 0.0f; }

    /// Sets the transparency of the surface mesh.
    void setSurfaceTransparency(FloatType transparency) { if(surfaceTransparencyController()) surfaceTransparencyController()->setFloatValue(AnimationTime(0), transparency); }

    /// Returns the transparency of the surface cap mesh.
    FloatType capTransparency() const { return capTransparencyController() ? capTransparencyController()->getFloatValue(AnimationTime(0)) : 0.0f; }

    /// Sets the transparency of the surface cap mesh.
    void setCapTransparency(FloatType transparency) { if(capTransparencyController()) capTransparencyController()->setFloatValue(AnimationTime(0), transparency); }

protected:

    /// Lets the vis element transform a data object in preparation for rendering.
    virtual Future<PipelineFlowState> transformDataImpl(const PipelineEvaluationRequest& request, const DataObject* dataObject, PipelineFlowState&& flowState) override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

public:

    /// Computation engine that builds the rendering mesh.
    class OVITO_MESH_EXPORT PrepareSurfaceEngine : public AsynchronousTask<DataOORef<const TriangleMesh>, DataOORef<const TriangleMesh>, std::vector<ColorA>, std::vector<size_t>, bool, PipelineStatus>
    {
    public:

        /// Constructor.
        PrepareSurfaceEngine(const SurfaceMesh* mesh, bool reverseOrientation, bool smoothShading, ColorMappingMode colorMappingMode, const PropertyReference& pseudoColorPropertyRef, Color surfaceColor, bool generateCapPolygons, bool clipAtDomainBoundaries) :
            _inputMesh(mesh),
            _reverseOrientation(reverseOrientation),
            _smoothShading(smoothShading),
            _colorMappingMode(colorMappingMode),
            _pseudoColorPropertyRef(pseudoColorPropertyRef),
            _surfaceColor(surfaceColor),
            _generateCapPolygons(generateCapPolygons),
            _clipAtDomainBoundaries(clipAtDomainBoundaries) {}

        /// Builds the non-periodic representation of the surface mesh.
        virtual void perform() override;

        /// Returns the input surface mesh.
        const DataOORef<const SurfaceMesh>& inputMesh() const { return _inputMesh; }

        /// Returns the generated output triangle mesh for the surface.
        const DataOORef<TriangleMesh>& outputMesh() { return _outputMesh; }

        /// Returns the generated output triangle mesh for the surface.
        const DataOORef<TriangleMesh>& capPolygonsMesh() { return _capPolygonsMesh; }

    protected:

        /// This method can be overridden by subclasses to restrict the set of visible mesh faces,
        virtual void determineVisibleFaces() {}

        /// This method can be overridden by subclasses to assign colors to invidual mesh faces.
        virtual void determineFaceColors();

        /// This method can be overridden by subclasses to assign colors to invidual mesh vertices.
        virtual void determineVertexColors();

    private:

        /// Generates the triangle mesh from the periodic surface mesh, which will be rendered.
        bool buildSurfaceTriangleMesh();

        /// Generates the cap polygons where the surface mesh intersects the periodic domain boundaries.
        void buildCapTriangleMesh();

        /// Returns the periodic domain the surface mesh is embedded in (if any).
        const SimulationCell* cell() const { return inputMesh()->domain(); }

    private:

        /// Splits a triangle face at a periodic boundary.
        bool splitFace(int faceIndex, int oldVertexCount, std::vector<Point3>& newVertices, std::vector<ColorAG>& newVertexColors, std::vector<FloatType>& newVertexPseudoColors, std::map<std::pair<int,int>,std::tuple<int,int,FloatType>>& newVertexLookupMap, size_t dim);

        /// Traces the closed contour of the surface-boundary intersection.
        std::vector<Point2> traceContour(const SurfaceMeshTopology& inputMeshTopology, SurfaceMesh::edge_index firstEdge, const std::vector<Point3>& reducedPos, std::vector<bool>& visitedFaces, size_t dim, CapPolygonTessellator::FaceMode faceMode) const;

        /// Slices a 2d contour at periodic boundaries.
        static void sliceContourAtPeriodicBoundaries(std::vector<Point2>& input, std::array<bool,2> pbcFlags, std::vector<std::vector<Point2>>& openContours, std::vector<std::vector<Point2>>& closedContours);

        /// Slices a 2d contour at periodic boundaries and clips it an non-periodic boundaries.
        static void sliceAndClipContour(std::vector<Point2>& input, std::array<bool,2> pbcFlags, std::vector<std::vector<Point2>>& openContours, std::vector<std::vector<Point2>>& closedContours);

        /// Computes the intersection point of a 2d contour segment crossing a periodic boundary.
        static void computeContourIntersectionPeriodic(size_t dim, FloatType t, Point2& base, Vector2& delta, int crossDir, std::vector<std::vector<Point2>>& contours);

        /// Determines if the 2D box corner (0,0) is inside the closed region described by the 2d polygon.
        static bool isCornerInside2DRegion(const std::vector<std::vector<Point2>>& contours);

    protected:

        DataOORef<const SurfaceMesh> _inputMesh;    ///< The input surface mesh.
        bool _reverseOrientation;                   ///< Flag for inside-out display of the mesh.
        bool _smoothShading;                        ///< Flag for interpolated-normal shading
        bool _generateCapPolygons;                  ///< Controls the generation of cap polygons where the mesh intersection periodic cell boundaries.
        bool _clipAtDomainBoundaries;               ///< Clip surface mesh at non-periodic cell boundaries.
        Color _surfaceColor;                        ///< The uniform surface color set by the user.
        ColorMappingMode _colorMappingMode;         ///< The pseudo-coloring mode.
        PropertyReference _pseudoColorPropertyRef;

        DataOORef<TriangleMesh> _outputMesh;       ///< The output mesh generated by clipping the surface mesh at the cell boundaries.
        DataOORef<TriangleMesh> _capPolygonsMesh;  ///< The output mesh containing the generated cap polygons.
        boost::dynamic_bitset<> _faceSubset;        ///< Bit array indicating which surface mesh faces are part of the render set.
        std::vector<ColorA> _materialColors;        ///< The list of material colors for the output TriMesh.
        std::vector<size_t> _originalFaceMap;       ///< Maps output mesh triangles to input mesh facets.
        bool _renderFacesTwoSided = true;           ///< Indicates that output triangle faces should be rendered two-sided.
        PipelineStatus _status;                     ///< The outcome of the process.
    };

    /// Creates the asynchronous task that builds the non-peridic representation of the input surface mesh.
    /// This method may be overridden by subclasses who want to implement custom behavior.
    virtual std::shared_ptr<PrepareSurfaceEngine> createSurfaceEngine(const SurfaceMesh* mesh) const;

protected:

    /// Create the viewport picking record for the surface mesh object.
    /// The default implementation returns null, because standard surface meshes do not support picking of
    /// mesh faces or vertices. Sub-classes can override this method to implement object-specific picking
    /// strategies.
    virtual OORef<ObjectPickInfo> createPickInfo(const SurfaceMesh* mesh, const RenderableSurfaceMesh* renderableMesh) const;

private:

    /// Controls the display color of the surface mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, surfaceColor, setSurfaceColor, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(surfaceColor);

    /// Controls the display color of the cap mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, capColor, setCapColor, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(capColor);

    /// Controls whether the cap mesh is rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, showCap, setShowCap, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(showCap);

    /// Controls whether the surface mesh is rendered using smooth shading.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, smoothShading, setSmoothShading);
    DECLARE_SHADOW_PROPERTY_FIELD(smoothShading);

    /// Controls whether the mesh' orientation is flipped.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, reverseOrientation, setReverseOrientation);
    DECLARE_SHADOW_PROPERTY_FIELD(reverseOrientation);

    /// Controls whether the polygonal edges of the mesh should be highlighted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, highlightEdges, setHighlightEdges);
    DECLARE_SHADOW_PROPERTY_FIELD(highlightEdges);

    /// Controls the transparency of the surface mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, surfaceTransparencyController, setSurfaceTransparencyController);

    /// Controls the transparency of the surface cap mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, capTransparencyController, setCapTransparencyController);

    /// Internal field indicating whether the surface meshes rendered by this viz element are closed or not.
    /// Depending on this setting, the UI will show the cap polygon option to the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, surfaceIsClosed, setSurfaceIsClosed);

    /// Transfer function for pseudo-color visualization of a surface property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PropertyColorMapping>, surfaceColorMapping, setSurfaceColorMapping);

    /// Controls which part of a surface mesh is used for pseudo-color mapping.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ColorMappingMode, colorMappingMode, setColorMappingMode);

    /// Controls whether the mesh gets clipped at non-periodic cell boundaries.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, clipAtDomainBoundaries, setClipAtDomainBoundaries);
    DECLARE_SHADOW_PROPERTY_FIELD(clipAtDomainBoundaries);
};


/**
 * \brief This data structure is attached to the surface mesh by the SurfaceMeshVis when rendering
 * it in the viewports. It facilitates the picking of surface facets with the mouse.
 */
class OVITO_MESH_EXPORT SurfaceMeshPickInfo : public ObjectPickInfo
{
    OVITO_CLASS(SurfaceMeshPickInfo)

public:

    /// Constructor.
    explicit SurfaceMeshPickInfo(const SurfaceMeshVis* visElement, const SurfaceMesh* surfaceMesh, const RenderableSurfaceMesh* renderableMesh) :
        _visElement(visElement), _surfaceMesh(surfaceMesh), _renderableMesh(renderableMesh) {}

    /// The data object containing the surface mesh.
    const SurfaceMesh* surfaceMesh() const { return _surfaceMesh; }

    /// The renderable version of the surface mesh.
    const RenderableSurfaceMesh* renderableMesh() const { OVITO_ASSERT(_renderableMesh); return _renderableMesh; }

    /// Returns the vis element that rendered the surface mesh.
    const SurfaceMeshVis* visElement() const { return _visElement; }

    /// Given an sub-object ID returned by the Viewport::pick() method, looks up the corresponding surface face.
    int faceIndexFromSubObjectID(quint32 subobjID) const {
        if(subobjID < renderableMesh()->originalFaceMap().size())
            return renderableMesh()->originalFaceMap()[subobjID];
        else
            return -1;
    }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(Pipeline* pipeline, quint32 subobjectId) override;

private:

    /// The data object holding the original surface mesh.
    OORef<SurfaceMesh> _surfaceMesh;

    /// The renderable version of the surface mesh.
    OORef<RenderableSurfaceMesh> _renderableMesh;

    /// The vis element that rendered the surface mesh.
    OORef<SurfaceMeshVis> _visElement;
};

}   // End of namespace
