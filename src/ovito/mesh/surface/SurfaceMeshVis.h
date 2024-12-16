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
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering SurfaceMesh data objects.
 */
class OVITO_MESH_EXPORT SurfaceMeshVis : public DataVis
{
    OVITO_CLASS(SurfaceMeshVis)

public:

    enum ColorMappingMode {
        NoPseudoColoring,
        VertexPseudoColoring,
        FacePseudoColoring,
        RegionPseudoColoring
    };
    Q_ENUM(ColorMappingMode);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Transforms the SurfaceMesh into a renderable triangle mesh.
    [[nodiscard]] Future<std::shared_ptr<const RenderableSurfaceMesh>> transformSurfaceMesh(const SurfaceMesh* surfaceMesh);

    /// Lets the visualization element render the data object.
    virtual std::variant<PipelineStatus, Future<PipelineStatus>> render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

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

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// Algorithm for building the renderable surface mesh.
    /// Subclasses can customize individual aspects of the algorithm by overriding the virtual methods.
    class OVITO_MESH_EXPORT RenderableSurfaceBuilder
    {
    public:

        /// Constructor.
        RenderableSurfaceBuilder(const SurfaceMesh* mesh, bool reverseOrientation, bool smoothShading, ColorMappingMode colorMappingMode, const PropertyReference& pseudoColorProperty, bool clipAtDomainBoundaries) :
            _inputMesh(mesh),
            _reverseOrientation(reverseOrientation),
            _smoothShading(smoothShading),
            _colorMappingMode(colorMappingMode),
            _pseudoColorProperty(pseudoColorProperty),
            _clipAtDomainBoundaries(clipAtDomainBoundaries) {}

        /// Destructor.
        virtual ~RenderableSurfaceBuilder() = default;

        /// Returns the input surface mesh.
        const DataOORef<const SurfaceMesh>& inputMesh() const { return _inputMesh; }

        /// Returns the generated output triangle mesh for the surface.
        const DataOORef<TriangleMesh>& outputMesh() { return _outputMesh; }

        /// Returns the generated output triangle mesh for the surface.
        const DataOORef<TriangleMesh>& capPolygonsMesh() { return _capPolygonsMesh; }

        /// Returns the final RenderableSurfaceMesh.
        std::shared_ptr<const RenderableSurfaceMesh> renderableMesh(bool backfaceCulling) {
            return std::make_shared<const RenderableSurfaceMesh>(
                std::move(_outputMesh),
                std::move(_capPolygonsMesh),
                std::move(_materialColors),
                std::move(_originalFaceMap),
                backfaceCulling,
                std::move(_status));
        }

    protected:

        /// This method can be overridden by subclasses to restrict the set of visible mesh faces,
        virtual boost::dynamic_bitset<> determineVisibleFaces() { return {}; }

        /// This method can be overridden by subclasses to assign colors to invidual mesh faces.
        virtual void determineFaceColors();

        /// This method can be overridden by subclasses to assign colors to invidual mesh vertices.
        virtual void determineVertexColors();

    private:

        /// Generates the triangle mesh from the periodic surface mesh, which will be rendered.
        bool buildSurfaceTriangleMesh(const boost::dynamic_bitset<>& faceSubset, bool renderFacesTwoSided);

        /// Generates the cap polygons where the surface mesh intersects the periodic domain boundaries.
        void buildCapTriangleMesh(const boost::dynamic_bitset<>& faceSubset);

        /// Returns the periodic domain the surface mesh is embedded in (if any).
        const SimulationCell* cell() const { return inputMesh()->domain(); }

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
        bool _clipAtDomainBoundaries;               ///< Clip surface mesh at non-periodic cell boundaries.
        ColorMappingMode _colorMappingMode;         ///< The pseudo-coloring mode.
        PropertyReference _pseudoColorProperty;    ///< The property used for pseudo-coloring (name & vector component).

        DataOORef<TriangleMesh> _outputMesh;        ///< The output mesh generated by clipping the surface mesh at the cell boundaries.
        DataOORef<TriangleMesh> _capPolygonsMesh;   ///< The output mesh containing the generated cap polygons.
        std::vector<ColorA> _materialColors;        ///< The list of material colors for the output TriMesh.
        std::vector<size_t> _originalFaceMap;       ///< Maps output mesh triangles to input mesh facets.
        PipelineStatus _status;                     ///< The outcome of the process.

        friend class SurfaceMeshVis;
    };

    /// Creates the object that builds the non-periodic representation of the input surface mesh.
    /// This method may be overridden by subclasses that want to implement custom behavior.
    virtual std::unique_ptr<RenderableSurfaceBuilder> createRenderableSurfaceBuilder(const SurfaceMesh* mesh) const;

protected:

    /// Create the viewport picking record for the surface mesh object.
    /// The default implementation returns null, because standard surface meshes do not support picking of
    /// mesh faces or vertices. Sub-classes can override this method to implement object-specific picking
    /// strategies.
    virtual OORef<ObjectPickInfo> createPickInfo(const SurfaceMesh* mesh, std::shared_ptr<const RenderableSurfaceMesh> renderableMesh) const;

private:

    /// Controls the display color of the surface mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,1,1}), surfaceColor, setSurfaceColor, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(surfaceColor);

    /// Controls the display color of the cap mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.8, 0.8, 1.0}), capColor, setCapColor, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(capColor);

    /// Controls whether the cap mesh is rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, showCap, setShowCap, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(showCap);

    /// Controls whether the surface mesh is rendered using smooth shading.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, smoothShading, setSmoothShading);
    DECLARE_SHADOW_PROPERTY_FIELD(smoothShading);

    /// Controls whether the mesh' orientation is flipped.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, reverseOrientation, setReverseOrientation);
    DECLARE_SHADOW_PROPERTY_FIELD(reverseOrientation);

    /// Controls whether the polygonal edges of the mesh should be highlighted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, highlightEdges, setHighlightEdges);
    DECLARE_SHADOW_PROPERTY_FIELD(highlightEdges);

    /// Controls the transparency of the surface mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, surfaceTransparencyController, setSurfaceTransparencyController);

    /// Controls the transparency of the surface cap mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, capTransparencyController, setCapTransparencyController);

    /// Internal field indicating whether the surface meshes rendered by this viz element are closed or not.
    /// Depending on this setting, the UI will show the cap polygon option to the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, surfaceIsClosed, setSurfaceIsClosed);

    /// Transfer function for pseudo-color visualization of a surface property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PropertyColorMapping>, surfaceColorMapping, setSurfaceColorMapping);

    /// Controls which part of a surface mesh is used for pseudo-color mapping.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ColorMappingMode{NoPseudoColoring}, colorMappingMode, setColorMappingMode);

    /// Controls whether the mesh gets clipped at non-periodic cell boundaries.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, clipAtDomainBoundaries, setClipAtDomainBoundaries);
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
    void initializeObject(const SurfaceMeshVis* visElement, const SurfaceMesh* surfaceMesh, std::shared_ptr<const RenderableSurfaceMesh> renderableMesh) {
        ObjectPickInfo::initializeObject();
        _visElement = visElement;
        _surfaceMesh = surfaceMesh;
        _renderableMesh = std::move(renderableMesh);
    }

    /// The data object containing the surface mesh.
    const DataOORef<const SurfaceMesh>& surfaceMesh() const { return _surfaceMesh; }

    /// The renderable version of the surface mesh.
    const std::shared_ptr<const RenderableSurfaceMesh>& renderableMesh() const { return _renderableMesh; }

    /// Returns the vis element that rendered the surface mesh.
    const OORef<SurfaceMeshVis>& visElement() const { return _visElement; }

    /// Given an sub-object ID returned by the ViewportWindow::pick() method, looks up the corresponding surface face.
    int faceIndexFromSubObjectID(quint32 subobjID) const {
        if(subobjID < renderableMesh()->originalFaceMap().size())
            return renderableMesh()->originalFaceMap()[subobjID];
        else
            return -1;
    }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) override;

private:

    /// The original surface mesh.
    DataOORef<const SurfaceMesh> _surfaceMesh;

    /// The renderable version of the surface mesh.
    std::shared_ptr<const RenderableSurfaceMesh> _renderableMesh;

    /// The vis element that rendered the surface mesh.
    OORef<SurfaceMeshVis> _visElement;
};

}   // End of namespace
