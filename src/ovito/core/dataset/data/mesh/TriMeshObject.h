////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {


/// \brief The maximum number of smoothing groups in a mesh.
///
/// Each face in a TriMesh can be a member of one of the 32 possible smoothing groups.
/// Adjacent faces that belong to the same smoothing group are rendered with
/// interpolated normal vectors.
#define OVITO_MAX_NUM_SMOOTHING_GROUPS      32

/**
 * \brief Represents a triangle in a TriMeshObject.
 */
class OVITO_CORE_EXPORT TriMeshFace
{
public:

    /// Bit-flags that can be assigned to a mesh face.
    enum MeshFaceFlag {
        NONE = 0,       //< No flags
        EDGE1 = (1<<0), //< First edge visible
        EDGE2 = (1<<1), //< Second edge visible
        EDGE3 = (1<<2), //< Third edge visible
        IS_SELECTED = (1<<3), //< Face selection state
        EDGES12 = EDGE1 | EDGE2,    //< First and second edge visible
        EDGES23 = EDGE2 | EDGE3,    //< Second and third edge visible
        EDGES13 = EDGE1 | EDGE3,    //< First and third edge visible
        EDGES123 = EDGE1 | EDGE2 | EDGE3    //< All edges visible
    };
    Q_DECLARE_FLAGS(MeshFaceFlags, MeshFaceFlag);

public:

    /************************************ Vertices *******************************/

    /// Sets the vertex indices of this face to new values.
    void setVertices(int a, int b, int c) {
        _vertices[0] = a; _vertices[1] = b; _vertices[2] = c;
    }

    /// Sets the vertex index of one vertex to a new value.
    ///    which - 0, 1 or 2
    ///    newIndex - The new index for the vertex.
    void setVertex(size_t which, int newIndex) {
        OVITO_ASSERT(which < 3);
        _vertices[which] = newIndex;
    }

    /// Returns the index into the Mesh vertices array of a face vertex.
    ///    which - 0, 1 or 2
    /// Returns the index of the requested vertex.
    int vertex(size_t which) const {
        OVITO_ASSERT(which < 3);
        return _vertices[which];
    }

    /************************************ Edges *******************************/

    /// Sets the visibility of the three face edges.
    void setEdgeVisibility(bool e1, bool e2, bool e3) {
        _flags.setFlag(EDGE1, e1);
        _flags.setFlag(EDGE2, e2);
        _flags.setFlag(EDGE3, e3);
    }

    /// Sets the visibility of the three face edges all at once.
    void setEdgeVisibility(MeshFaceFlags edgeVisibility) {
        _flags = edgeVisibility | (_flags & ~EDGES123);
    }

    /// Makes one of the edges of the triangle face visible.
    void setEdgeVisible(size_t which) {
        OVITO_ASSERT(which < 3);
        _flags.setFlag(MeshFaceFlag(EDGE1 << which));
    }

    /// Hides one of the edges of the triangle face.
    void setEdgeHidden(size_t which) {
        OVITO_ASSERT(which < 3);
        _flags.setFlag(MeshFaceFlag(EDGE1 << which), false);
    }

    /// Returns true if the edge is visible.
    ///    which - The index of the edge (0, 1 or 2)
    bool edgeVisible(size_t which) const {
        OVITO_ASSERT(which < 3);
        return _flags.testFlag(MeshFaceFlag(EDGE1 << which));
    }

    /************************************ Material *******************************/

    /// Returns the material index assigned to this face.
    int materialIndex() const { return _materialIndex; }

    /// Sets the material index of this face.
    void setMaterialIndex(int index) { _materialIndex = index; }

    /// Sets the smoothing groups of this face.
    void setSmoothingGroups(quint32 smGroups) { _smoothingGroups = smGroups; }

    /// Returns the smoothing groups this face belongs to as a bit array.
    quint32 smoothingGroups() const { return _smoothingGroups; }

    /************************************ Selection *******************************/

    /// Returns whether the face selection flag is set.
    bool isSelected() const { return _flags.testFlag(IS_SELECTED); }

    /// Sets the face's selection flag.
    void setSelected(bool selected = true) { _flags.setFlag(IS_SELECTED, selected); }

private:

    /// \brief The three vertices of the triangle face.
    ///
    /// These values are indices into the vertex array of the mesh, starting at 0.
    std::array<int,3> _vertices;

    /// The bit flags.
    MeshFaceFlags _flags = EDGES123;

    /// Smoothing group bits. Specifies the smoothing groups this face belongs to.
    quint32 _smoothingGroups = 0;

    /// The material index assigned to the face.
    int _materialIndex = 0;

    // Make sure the constant OVITO_MAX_NUM_SMOOTHING_GROUPS has correct value.
    static_assert(std::numeric_limits<decltype(TriMeshFace::_smoothingGroups)>::digits == OVITO_MAX_NUM_SMOOTHING_GROUPS, "Compile-time constant OVITO_MAX_NUM_SMOOTHING_GROUPS has incorrect value.");

    friend class TriMeshObject;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TriMeshFace::MeshFaceFlags);

/**
 * \brief A data object represeting a mesh made of vertices and triangles.
 */
class OVITO_CORE_EXPORT TriMeshObject : public DataObject
{
    OVITO_CLASS(TriMeshObject)

public:

    /// Constructor that creates an object with an empty triangle mesh.
    Q_INVOKABLE TriMeshObject(ObjectCreationParams params);

    /// \brief Returns the title of this object.
    virtual QString objectTitle() const override { return tr("Triangle mesh"); }

    /// \brief Resets the mesh to the empty state.
    void clear();

    /// Swaps the contents of this mesh with another mesh.
    void swap(TriMeshObject& other) noexcept {
        _vertices.swap(other._vertices);
        _faces.swap(other._faces);
        std::swap(_boundingBox, other._boundingBox);
        std::swap(_hasVertexColors, other._hasVertexColors);
        _vertexColors.swap(other._vertexColors);
        std::swap(_hasVertexPseudoColors, other._hasVertexPseudoColors);
        _vertexPseudoColors.swap(other._vertexPseudoColors);
        std::swap(_hasFaceColors, other._hasFaceColors);
        _faceColors.swap(other._faceColors);
        std::swap(_hasFacePseudoColors, other._hasFacePseudoColors);
        _facePseudoColors.swap(other._facePseudoColors);
        std::swap(_hasNormals, other._hasNormals);
        _normals.swap(other._normals);
    }

    /// \brief Returns the bounding box of the mesh.
    /// \return The bounding box of the mesh.
    ///
    /// The bounding box is cached by the TriMesh object.
    /// Calling this method multiple times is cheap as long as the vertices of the mesh are not changed.
    const Box3& boundingBox() const {
        if(_boundingBox.isEmpty())
            _boundingBox.addPoints(vertices());
        return _boundingBox;
    }

    /// \brief Returns the number of vertices in this mesh.
    int vertexCount() const { return _vertices.size(); }

    /// \brief Sets the number of vertices in this mesh.
    /// \param n The new number of vertices.
    ///
    /// If \a n is larger than the old vertex count then new vertices are added to the mesh.
    /// These new vertices are not initialized by this method. One should use a method like setVertexPos()
    /// to assign a position to the new vertices.
    void setVertexCount(int n);

    /// \brief Allows direct access to the vertex position array of the mesh.
    /// \return A reference to the internal container that stores the vertex coordinates.
    /// \note When you change the vertex positions then you have to call invalidateVertices()
    /// to let the mesh know that it has to update its internal cache based on the new vertex coordinates.
    QVector<Point3>& vertices() { return _vertices; }

    /// \brief Allows direct read-access to the vertex position array of the mesh.
    /// \return A constant reference to the internal container that stores the vertex positions.
    const QVector<Point3>& vertices() const { return _vertices; }

    /// \brief Returns the coordinates of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose position should be returned.
    /// \return The position of the given vertex.
    const Point3& vertex(int index) const {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        return _vertices[index];
    }

    /// \brief Returns a reference to the coordinates of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose position should be returned.
    /// \return A reference to the coordinates of the given vertex. The reference can be used to alter the vertex position.
    /// \note If you change the vertex' position then you have to call invalidateVertices() to let the mesh know that it has
    /// to update its internal caches based on the new vertex position.
    Point3& vertex(int index) {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        return _vertices[index];
    }

    /// \brief Sets the coordinates of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose position should be set.
    /// \param p The new position of the vertex.
    /// \note After you have finished changing the vertex positions you have to call invalidateVertices()
    /// to let the mesh know that it has to update its internal caches based on the new vertex position.
    void setVertex(int index, const Point3& p) {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        _vertices[index] = p;
    }

    /// \brief Adds a new vertex to the mesh.
    /// \param pos The coordinates of the new vertex.
    /// \return The index of the newly created vertex.
    int addVertex(const Point3& pos) {
        int index = vertexCount();
        setVertexCount(index + 1);
        _vertices[index] = pos;
        return index;
    }

    /// \brief Returns whether this mesh has RGBA colors associated with its vertices.
    bool hasVertexColors() const {
        return _hasVertexColors;
    }

    /// \brief Controls whether this mesh has RGBA colors associated with its vertices.
    void setHasVertexColors(bool enableColors) {
        _hasVertexColors = enableColors;
        _vertexColors.resize(enableColors ? _vertices.size() : 0);
    }

    /// \brief Allows direct access to the vertex RGBA color array of the mesh.
    /// \return A reference to the vector that stores all vertex colors.
    QVector<ColorA>& vertexColors() {
        OVITO_ASSERT(_hasVertexColors);
        OVITO_ASSERT(_vertexColors.size() == _vertices.size());
        return _vertexColors;
    }

    /// \brief Allows direct read-access to the vertex RGBA color array of the mesh.
    /// \return A constant reference to the vector that stores all vertex colors.
    const QVector<ColorA>& vertexColors() const {
        OVITO_ASSERT(_hasVertexColors);
        OVITO_ASSERT(_vertexColors.size() == _vertices.size());
        return _vertexColors;
    }

    /// \brief Returns the RGBA color of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose color should be returned.
    /// \return The color of the given vertex.
    const ColorA& vertexColor(int index) const {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        return vertexColors()[index];
    }

    /// \brief Returns a reference to the RGBA color of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose color should be returned.
    /// \return A reference to the color of the given vertex. The reference can be used to alter the vertex color.
    ColorA& vertexColor(int index) {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        return vertexColors()[index];
    }

    /// \brief Sets the RGBA color of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose color should be set.
    /// \param p The new color of the vertex.
    void setVertexColor(int index, const ColorA& c) {
        vertexColor(index) = c;
    }

    /// \brief Returns whether this mesh has pseudo-colors associated with its vertices.
    bool hasVertexPseudoColors() const {
        return _hasVertexPseudoColors;
    }

    /// \brief Controls whether this mesh has pseudo-colors associated with its vertices.
    void setHasVertexPseudoColors(bool enablePseudoColors) {
        _hasVertexPseudoColors = enablePseudoColors;
        _vertexPseudoColors.resize(enablePseudoColors ? _vertices.size() : 0);
    }

    /// \brief Allows direct access to the per-vertex psudo-color array of the mesh.
    /// \return A reference to the vector that stores all vertex pseudo-color values.
    QVector<FloatType>& vertexPseudoColors() {
        OVITO_ASSERT(_hasVertexPseudoColors);
        OVITO_ASSERT(_vertexPseudoColors.size() == _vertices.size());
        return _vertexPseudoColors;
    }

    /// \brief Allows direct read-access to the vertex pseudo-color array of the mesh.
    /// \return A constant reference to the vector that stores all vertex pseudo-colors.
    const QVector<FloatType>& vertexPseudoColors() const {
        OVITO_ASSERT(_hasVertexPseudoColors);
        OVITO_ASSERT(_vertexPseudoColors.size() == _vertices.size());
        return _vertexPseudoColors;
    }

    /// \brief Returns the pseudo-color value of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose pseudo-color value should be returned.
    /// \return The pseudo-color value of the given vertex.
    FloatType vertexPseudoColor(int index) const {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        return vertexPseudoColors()[index];
    }

    /// \brief Sets the pseudo-color value of the vertex with the given index.
    /// \param index The index starting at 0 of the vertex whose pseudo-color value should be set.
    /// \param p The new value for the vertex.
    void setVertexPseudoColor(int index, FloatType c) {
        OVITO_ASSERT(index >= 0 && index < vertexCount());
        vertexPseudoColors()[index] = c;
    }

    /// \brief Invalidates the parts of the internal mesh cache that depend on the vertex array.
    ///
    /// This method must be called each time the vertices of the mesh have been modified.
    void invalidateVertices() {
        _boundingBox.setEmpty();
    }

    /// \brief Returns the number of faces (triangles) in this mesh.
    int faceCount() const { return _faces.size(); }

    /// \brief Sets the number of faces in this mesh.
    /// \param n The new number of faces.
    ///
    /// If \a n is larger than the old face count then new faces are added to the mesh.
    /// These new faces are not initialized by this method. One has to use methods of the TriMeshFace class
    /// to assign vertices to the new faces.
    void setFaceCount(int n);

    /// \brief Allows direct access to the face array of the mesh.
    /// \return A reference to the vector that stores all mesh faces.
    QVector<TriMeshFace>& faces() { return _faces; }

    /// \brief Allows direct read-access to the face array of the mesh.
    /// \return A const reference to the vector that stores all mesh faces.
    const QVector<TriMeshFace>& faces() const { return _faces; }

    /// \brief Returns the face with the given index.
    /// \param index The index starting at 0 of the face who should be returned.
    const TriMeshFace& face(int index) const {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        return _faces[index];
    }

    /// \brief Returns a reference to the face with the given index.
    /// \param index The index starting at 0 of the face who should be returned.
    /// \return A reference to the requested face. This reference can be used to change the
    /// the face.
    TriMeshFace& face(int index) {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        return _faces[index];
    }

    /// \brief Adds a new triangle face and returns a reference to it.
    /// \return A reference to the new face. The new face has to be initialized
    /// after it has been created.
    ///
    /// Increases the number of faces by one.
    TriMeshFace& addFace();

    /// \brief Flip the orientation of all faces.
    void flipFaces();

    /// \brief Returns whether this mesh has RGBA colors associated with the individual faces.
    bool hasFaceColors() const {
        return _hasFaceColors;
    }

    /// \brief Controls whether this mesh has RGBA colors associated with the invidual faces.
    void setHasFaceColors(bool enableColors) {
        _hasFaceColors = enableColors;
        _faceColors.resize(enableColors ? _faces.size() : 0);
    }

    /// \brief Allows direct access to the per-face RGBA color array of the mesh.
    /// \return A reference to the vector storing each face's RGBA color.
    QVector<ColorA>& faceColors() {
        OVITO_ASSERT(_hasFaceColors);
        OVITO_ASSERT(_faceColors.size() == _faces.size());
        return _faceColors;
    }

    /// \brief Allows direct read-access to the per-face RGBA color array of the mesh.
    /// \return A constant reference to the vector that stores all face colors.
    const QVector<ColorA>& faceColors() const {
        OVITO_ASSERT(_hasFaceColors);
        OVITO_ASSERT(_faceColors.size() == _faces.size());
        return _faceColors;
    }

    /// \brief Returns the RGBA color of the face with the given index.
    /// \param index The index starting at 0 of the face whose color should be returned.
    /// \return The color of the given face.
    const ColorA& faceColor(int index) const {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        return faceColors()[index];
    }

    /// \brief Returns a reference to the RGBA color of the face with the given index.
    /// \param index The index starting at 0 of the face whose color should be returned.
    /// \return A reference to the color of the given face. The reference can be used to alter the face color.
    ColorA& faceColor(int index) {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        return faceColors()[index];
    }

    /// \brief Sets the RGBA color of the face with the given index.
    /// \param index The index starting at 0 of the face whose color should be set.
    /// \param p The new color of the face.
    void setFaceColor(int index, const ColorA& c) {
        faceColor(index) = c;
    }

    /// \brief Returns whether this mesh has pseudo-color values associated with the individual faces.
    bool hasFacePseudoColors() const {
        return _hasFacePseudoColors;
    }

    /// \brief Controls whether this mesh has pseudo-color values associated with the invidual faces.
    void setHasFacePseudoColors(bool enableColors) {
        _hasFacePseudoColors = enableColors;
        _facePseudoColors.resize(enableColors ? _faces.size() : 0);
    }

    /// \brief Allows direct access to the per-face pseudo-color array of the mesh.
    /// \return A reference to the vector storing each face's pseudo-color value.
    QVector<FloatType>& facePseudoColors() {
        OVITO_ASSERT(_hasFacePseudoColors);
        OVITO_ASSERT(_facePseudoColors.size() == _faces.size());
        return _facePseudoColors;
    }

    /// \brief Allows direct read-access to the per-face pseudo-color array of the mesh.
    /// \return A constant reference to the vector that stores all face pseudo-color values.
    const QVector<FloatType>& facePseudoColors() const {
        OVITO_ASSERT(_hasFacePseudoColors);
        OVITO_ASSERT(_facePseudoColors.size() == _faces.size());
        return _facePseudoColors;
    }

    /// \brief Returns the pseudo-color value of the face with the given index.
    /// \param index The index starting at 0 of the face whose pseudo-color should be returned.
    /// \return The pseudo-color value of the given face.
    FloatType facePseudoColor(int index) const {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        return facePseudoColors()[index];
    }

    /// \brief Sets the pseudo-color value of the face with the given index.
    /// \param index The index starting at 0 of the face whose color should be set.
    /// \param p The new pseudo-color value for the face.
    void setFacePseudoColor(int index, FloatType c) {
        OVITO_ASSERT(index >= 0 && index < faceCount());
        facePseudoColors()[index] = c;
    }

    /// \brief Returns whether this mesh has normal vectors stored.
    bool hasNormals() const {
        return _hasNormals;
    }

    /// \brief Controls whether this mesh has normal vectors (three per face).
    void setHasNormals(bool enableNormals) {
        _hasNormals = enableNormals;
        _normals.resize(enableNormals ? (_faces.size()*3) : 0);
    }

    /// \brief Allows direct access to the face vertex normals of the mesh.
    /// \return A reference to the vector that stores all normal vectors (three per face).
    QVector<Vector3>& normals() {
        OVITO_ASSERT(_hasNormals);
        OVITO_ASSERT(_normals.size() == _faces.size()*3);
        return _normals;
    }

    /// \brief Allows direct read-access to the normal vectors of the mesh.
    /// \return A constant reference to the vector that stores all normal vectors (three per face).
    const QVector<Vector3>& normals() const {
        OVITO_ASSERT(_hasNormals);
        OVITO_ASSERT(_normals.size() == _faces.size()*3);
        return _normals;
    }

    /// \brief Returns the normal vector stored for the given vertex of the given face.
    /// \param faceIndex The index starting at 0 of the face whose normal should be returned.
    /// \param vertexIndex The face vertex (0-2) for which the normal should be returned.
    /// \return The stored normal vector.
    const Vector3& faceVertexNormal(int faceIndex, int vertexIndex) const {
        OVITO_ASSERT(faceIndex >= 0 && faceIndex < faceCount());
        OVITO_ASSERT(vertexIndex >= 0 && vertexIndex < 3);
        return normals()[faceIndex*3 + vertexIndex];
    }

    /// \brief Returns a reference to the stored normal vector of the given vertex of the given face.
    /// \param faceIndex The index starting at 0 of the face whose normal should be returned.
    /// \param vertexIndex The face vertex (0-2) for which the normal should be returned.
    /// \return A reference to the normal vector of the given face vertex. The reference can be used to alter the vector..
    Vector3& faceVertexNormal(int faceIndex, int vertexIndex) {
        OVITO_ASSERT(faceIndex >= 0 && faceIndex < faceCount());
        OVITO_ASSERT(vertexIndex >= 0 && vertexIndex < 3);
        return normals()[faceIndex*3 + vertexIndex];
    }

    /// \brief Sets the normal vectors stored for a vertex of a face.
    /// \param faceIndex The index starting at 0 of the face whose normal should be set.
    /// \param vertexIndex The face vertex (0-2) for which the normal should be set.
    /// \param n The new normal vector.
    void setFaceVertexNormal(int faceIndex, int vertexIndex, const Vector3& n) {
        faceVertexNormal(faceIndex, vertexIndex) = n;
    }

    /// Determines the visibility of face edges depending on the angle between the normals of adjacent faces.
    void determineEdgeVisibility(FloatType thresholdAngle = qDegreesToRadians(20.0));

    /// Identifies duplicate vertices and merges them into a single vertex shared by multiple faces.
    void removeDuplicateVertices(FloatType epsilon);

    /************************************* Ray intersection *************************************/

    /// \brief Performs a ray intersection calculation.
    /// \param ray The ray to test for intersection with the object.
    /// \param t If an intersection has been found, the method stores the distance of the intersection in this output parameter.
    /// \param normal If an intersection has been found, the method stores the surface normal at the point of intersection in this output parameter.
    /// \param faceIndex If an intersection has been found, the method stores the index of the face intersected by the ray in this output parameter.
    /// \param backfaceCull Controls whether backfacing faces are tested too.
    /// \return True if the closest intersection has been found. False if no intersection has been found.
    bool intersectRay(const Ray3& ray, FloatType& t, Vector3& normal, int& faceIndex, bool backfaceCull) const;

    /*********************************** Persistence ***********************************/

    /// \brief Saves the mesh to the given stream.
    /// \param stream The destination stream.
    void saveToStream(SaveStream& stream);

    /// \brief Loads the mesh from the given stream.
    /// \param stream The source stream.
    void loadFromStream(LoadStream& stream);

    /// \brief Exports the triangle mesh to a VTK file.
    void saveToVTK(CompressedTextWriter& stream) const;

    /// \brief Exports the triangle mesh to a Wavefront .obj file.
    void saveToOBJ(CompressedTextWriter& stream) const;

    /************************************* Clipping *************************************/

    /// Clips the mesh at the given plane.
    void clipAtPlane(const Plane3& plane);

    /************************************* Mesh creation *************************************/

    /// Creates a triangulated unit sphere model by subdividing a icosahedron.
    /// The resolution parameter controls the number of subdivision iterations and determines the
    /// resulting vertices/faces of the mesh.
    void createIcosphere(int resolution);

    /************************************* Information *************************************/

    /// Determines whether the mesh forms a closed manifold, i.e. each triangle has three adjacent
    /// triangles with correct orientation.
    bool isClosed() const;

protected:

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// The cached bounding box of the mesh computed from the vertices.
    mutable Box3 _boundingBox;

    /// Array of vertex coordinates.
    QVector<Point3> _vertices;

    /// Indicates that per-vertex RGBA colors are stored in this mesh.
    bool _hasVertexColors = false;

    /// Array of per-vertex RGBA colors.
    QVector<ColorA> _vertexColors;

    /// Indicates that per-vertex pseudo-colors are stored in this mesh.
    bool _hasVertexPseudoColors = false;

    /// Array of per-vertex pseudo-colors.
    QVector<FloatType> _vertexPseudoColors;

    /// Indicates that per-face RGBA colors are stored in this mesh.
    bool _hasFaceColors = false;

    /// Array of per-face RGBA colors.
    QVector<ColorA> _faceColors;

    /// Indicates that per-face pseudo-color values are stored in this mesh.
    bool _hasFacePseudoColors = false;

    /// Array of per-face pseudo-color values.
    QVector<FloatType> _facePseudoColors;

    /// Array of mesh faces.
    QVector<TriMeshFace> _faces;

    /// Indicates that normal vectors are stored in this mesh.
    bool _hasNormals = false;

    /// Array of normals (three per face).
    QVector<Vector3> _normals;
};

}   // End of namespace

Q_DECLARE_TYPEINFO(Ovito::TriMeshFace, Q_MOVABLE_TYPE);
