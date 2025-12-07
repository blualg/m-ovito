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

#pragma once


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/simcell/SimulationCellVis.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/BufferAccess.h>

namespace Ovito {

/**
 * \brief Stores the geometry and boundary conditions of a simulation box.
 *
 * The geometry of a simulation cell is a parallelepiped defined by three edge vectors.
 * A fourth vector specifies the coordinates of the origin of the simulation cell.
 */
class OVITO_STDOBJ_EXPORT SimulationCell : public DataObject
{
public:

    /// Define a new data object metaclass.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public DataObject::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DataObject::OOMetaClass::OOMetaClass;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override { return this->displayName(); }
    };

    OVITO_CLASS_META(SimulationCell, OOMetaClass);

public:

    /// \brief Constructor. Creates an empty simulation cell.
    void initializeObject(ObjectInitializationFlags flags) {
        DataObject::initializeObject(flags);

        if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement)))
            setVisElement(OORef<SimulationCellVis>::create(flags));
    }

    /// \brief Constructs a cell from three vectors specifying the cell's edges.
    /// \param a1 The first edge vector.
    /// \param a2 The second edge vector.
    /// \param a3 The third edge vector.
    /// \param origin The origin position.
    void initializeObject(ObjectInitializationFlags flags, const Vector3& a1, const Vector3& a2, const Vector3& a3,
            const Point3& origin = Point3::Origin(), bool pbcX = false, bool pbcY = false, bool pbcZ = false, bool is2D = false)
    {
        initializeObject(flags, AffineTransformation(a1, a2, a3, origin - Point3::Origin()), pbcX, pbcY, pbcZ, is2D);
    }

    /// \brief Constructs a cell from a matrix that specifies its shape and position in space.
    /// \param cellMatrix The matrix
    void initializeObject(ObjectInitializationFlags flags, const AffineTransformation& cellMatrix, bool pbcX = false, bool pbcY = false, bool pbcZ = false, bool is2D = false)
    {
        DataObject::initializeObject(flags);

        setCellMatrix(cellMatrix);
        setPbcX(pbcX);
        setPbcY(pbcY);
        setPbcZ(pbcZ);
        setIs2D(is2D);

        if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement)))
            setVisElement(OORef<SimulationCellVis>::create(flags));
    }

    /// \brief Constructs a cell with an axis-aligned box shape.
    /// \param box The axis-aligned box.
    /// \param pbcX Specifies whether periodic boundary conditions are enabled in the X direction.
    /// \param pbcY Specifies whether periodic boundary conditions are enabled in the Y direction.
    /// \param pbcZ Specifies whether periodic boundary conditions are enabled in the Z direction.
    void initializeObject(ObjectInitializationFlags flags, const Box3& box, bool pbcX = false, bool pbcY = false, bool pbcZ = false, bool is2D = false)
    {
        OVITO_ASSERT_MSG(box.sizeX() >= 0 && box.sizeY() >= 0 && box.sizeZ() >= 0, "SimulationCell constructor", "The simulation box must have a non-negative volume.");
        initializeObject(flags, AffineTransformation(box.sizeX(), 0, 0, box.minc.x(), 0, box.sizeY(), 0, box.minc.y(), 0, 0, box.sizeZ(), box.minc.z()), pbcX, pbcY, pbcZ, is2D);
    }

    /// Returns inverse of the simulation cell matrix.
    /// This matrix maps the simulation cell to the unit cube ([0,1]^3).
    const AffineTransformation& reciprocalCellMatrix() const {
        if(!_isReciprocalMatrixValid.load(std::memory_order_acquire))
            computeInverseMatrix();
        return _reciprocalSimulationCell;
    }

    /// Discard the cached reciprocal cell matrix.
    /// All code that changes the cell matrix should call this method to invalidate the cached matrix and force a recomputation on next access.
    void invalidateReciprocalCellMatrix() { _isReciprocalMatrixValid.store(false, std::memory_order_relaxed); }

    /// Returns the current simulation cell matrix.
    const AffineTransformation& matrix() const { return cellMatrix(); }

    /// Returns the current reciprocal simulation cell matrix.
    const AffineTransformation& inverseMatrix() const { return reciprocalCellMatrix(); }

    /// Computes the (positive) volume of the three-dimensional cell.
    FloatType volume3D() const {
        return std::abs(cellMatrix().determinant());
    }

    /// Computes the (positive) volume of the two-dimensional cell.
    FloatType volume2D() const {
        return cellMatrix().column(0).cross(cellMatrix().column(1)).length();
    }

    /// Enables or disables periodic boundary conditions in the three spatial directions.
    void setPbcFlags(const std::array<bool,3>& flags) {
        setPbcX(flags[0]);
        setPbcY(flags[1]);
        setPbcZ(flags[2]);
    }

    /// Sets the PBC flags.
    void setPbcFlags(bool pbcX, bool pbcY, bool pbcZ) {
        setPbcX(pbcX);
        setPbcY(pbcY);
        setPbcZ(pbcZ);
    }

    /// Returns the periodic boundary flags in all three spatial directions.
    std::array<bool,3> pbcFlags() const {
        return {{ pbcX(), pbcY(), pbcZ() }};
    }

    /// Returns the periodic boundary flags in all three spatial directions.
    std::array<bool,3> pbcFlagsCorrected() const {
        return {{ pbcX(), pbcY(), pbcZ() && !is2D() }};
    }

    /// Returns whether the simulation cell has periodic boundary conditions applied in the given direction.
    bool hasPbc(size_t dim) const { OVITO_ASSERT(dim < 3); return dim == 0 ? pbcX() : (dim == 1 ? pbcY() : pbcZ()); }

    /// Returns whether the simulation cell has periodic boundary conditions applied in the given direction.
    bool hasPbcCorrected(size_t dim) const { OVITO_ASSERT(dim < 3); return dim == 0 ? pbcX() : (dim == 1 ? pbcY() : (pbcZ() && !is2D())); }

    /// Returns whether the simulation cell has periodic boundary conditions applied in at least one direction.
    bool hasPbc() const { return pbcX() || pbcY() || pbcZ(); }

    /// Returns whether the simulation cell has periodic boundary conditions applied in at least one direction.
    bool hasPbcCorrected() const { return pbcX() || pbcY() || (pbcZ() && !is2D()); }

    /// Returns the first edge vector of the cell.
    const Vector3& cellVector1() const { return cellMatrix().column(0); }

    /// Returns the second edge vector of the cell.
    const Vector3& cellVector2() const { return cellMatrix().column(1); }

    /// Returns the third edge vector of the cell.
    const Vector3& cellVector3() const { return cellMatrix().column(2); }

    /// Returns the origin point of the cell.
    const Point3& cellOrigin() const { return Point3::Origin() + cellMatrix().column(3); }

    /// Returns true if the three edges of the cell are parallel to the three coordinates axes.
    bool isAxisAligned() const {
        if(cellMatrix()(1,0) != 0 || cellMatrix()(2,0) != 0) return false;
        if(cellMatrix()(0,1) != 0 || cellMatrix()(2,1) != 0) return false;
        if(cellMatrix()(0,2) != 0 || cellMatrix()(1,2) != 0) return false;
        return true;
    }

    /// Checks whether the simulation cell has zero volume or the cell matrix contains NaN entries.
    bool isDegenerate() const {
        if((is2D() ? volume2D() : volume3D()) <= FLOATTYPE_EPSILON)
            return true;
        for(size_t i = 0; i < 3; i++)
            for(size_t j = 0; j < 4; j++)
                if(std::isnan(cellMatrix()(i,j)))
                    return true;
        return false;
    }

    /// Checks if two simulation cells are identical.
    bool equals(const SimulationCell& other) const {
        return cellMatrix() == other.cellMatrix() && pbcX() == other.pbcX() && pbcY() == other.pbcY() && pbcZ() == other.pbcZ() && is2D() == other.is2D();
    }

    /// Converts a point given in reduced cell coordinates to a point in absolute coordinates.
    Point3 reducedToAbsolute(const Point3& reducedPoint) const { return cellMatrix() * reducedPoint; }

    /// Converts a point given in absolute coordinates to a point in reduced cell coordinates.
    Point3 absoluteToReduced(const Point3& absPoint) const { return reciprocalCellMatrix() * absPoint; }

    /// Converts a vector given in reduced cell coordinates to a vector in absolute coordinates.
    Vector3 reducedToAbsolute(const Vector3& reducedVec) const { return cellMatrix() * reducedVec; }

    /// Converts a vector given in absolute coordinates to a point in vector cell coordinates.
    Vector3 absoluteToReduced(const Vector3& absVec) const { return reciprocalCellMatrix() * absVec; }

    /// Wraps a point at the periodic boundaries of the cell.
    Point3 wrapPoint(const Point3& p) const {
        Point3 pout = p;
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbcCorrected(dim)) {
                if(FloatType s = std::floor(reciprocalCellMatrix().prodrow(p, dim)))
                    pout -= s * cellMatrix().column(dim);
            }
        }
        return pout;
    }

    /// Wraps a vector at the periodic boundaries of the cell using minimum image convention.
    Vector3 wrapVector(const Vector3& v) const {
        Vector3 vout = v;
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbcCorrected(dim)) {
                if(FloatType s = std::floor(reciprocalCellMatrix().prodrow(v, dim) + FloatType(0.5)))
                    vout -= s * cellMatrix().column(dim);
            }
        }
        return vout;
    }

    /// Calculates the normal vector of the given simulation cell side.
    Vector3 cellNormalVector(size_t dim) const {
        size_t dim1 = (dim + 1) % 3;
        size_t dim2 = (dim + 2) % 3;
        Vector3 normal = cellMatrix().column(dim1).cross(cellMatrix().column(dim2));
        // Flip normal if necessary.
        if(normal.dot(cellMatrix().column(dim)) < 0)
            return normal / (-normal.length());
        else
            return normal.safelyNormalized();
    }

    /// Tests if a vector so long that it would be wrapped at a periodic boundary when using the minimum image convention.
    bool isWrappedVector(const Vector3& v) const {
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbcCorrected(dim)) {
                if(std::abs(reciprocalCellMatrix().prodrow(v, dim)) >= FloatType(0.5))
                    return true;
            }
        }
        return false;
    }

    /// Helper function that computes the modulo operation for two integer numbers k and n.
    ///
    /// This function can handle negative numbers k. This allows mapping any number k that is
    /// outside the interval [0,n) back into the interval. Use this to implement periodic boundary conditions.
    constexpr static inline int modulo(int k, int n) {
        return ((k %= n) < 0) ? k+n : k;
    }

    /// Helper function that computes the modulo operation for two floating-point numbers k and n.
    ///
    /// This function can handle negative numbers k. This allows mapping any number k that is
    /// outside the interval [0,n) back into the interval. Use this to implement periodic boundary conditions.
    static inline FloatType modulo(FloatType k, FloatType n)
    {
        k = std::fmod(k, n);
        return (k < 0) ? k+n : k;
    }

protected:

    /// Is called when the value of a non-animatable field of this object changes.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Computes the inverse of the cell matrix.
    void computeInverseMatrix() const;

    /// Stores the three cell vectors and the position of the cell origin.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(AffineTransformation{AffineTransformation::Zero()}, cellMatrix, setCellMatrix);

    /// The inverse of the cell matrix, which is kept in sync with the cell matrix at all times.
    mutable AffineTransformation _reciprocalSimulationCell{AffineTransformation::Zero()};
    /// Indicates whether the reciprocal matrix is in sync with the cell's matrix.
    mutable std::atomic<bool> _isReciprocalMatrixValid{false};

    /// Specifies periodic boundary condition in the X direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcX, setPbcX);
    DECLARE_SHADOW_PROPERTY_FIELD(pbcX);
    /// Specifies periodic boundary condition in the Y direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcY, setPbcY);
    DECLARE_SHADOW_PROPERTY_FIELD(pbcY);
    /// Specifies periodic boundary condition in the Z direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, pbcZ, setPbcZ);
    DECLARE_SHADOW_PROPERTY_FIELD(pbcZ);

    /// Stores the dimensionality of the system.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, is2D, setIs2D);
    DECLARE_SHADOW_PROPERTY_FIELD(is2D);
};

/**
 * \brief A read-only data structure holding the information of a simulation cell.
 *
 * This lightweight utility class can be used instead of a full SimulationCell object,
 * when a full-fledged DataObject is not needed. It offers performance advantages
 * because it can be stored on the stack and does not require dynamic memory allocation.
 */
class OVITO_STDOBJ_EXPORT SimulationCellData
{
public:

    /// Default constructor. Creates an empty simulation cell.
    constexpr SimulationCellData() = default;

    /// Constructor that copies the cell information from an existing SimulationCell object.
    SimulationCellData(const SimulationCell& cell)
        : _cellMatrix(cell.cellMatrix()),
          _reciprocalCellMatrix(cell.reciprocalCellMatrix()),
          _pbcFlags(cell.pbcFlagsCorrected()),
          _is2D(cell.is2D())
    {
    }

    /// Constructor that copies the cell information from a SimulationCell object if available.
    constexpr SimulationCellData(const SimulationCell* cell) :
        _cellMatrix(cell ? cell->cellMatrix() : AffineTransformation::Zero()),
        _reciprocalCellMatrix(cell ? cell->reciprocalCellMatrix() : AffineTransformation::Zero()),
        _pbcFlags(cell ? cell->pbcFlagsCorrected() : std::array<bool,3>{false}),
        _is2D(cell ? cell->is2D() : false) {}

    /// Constructor that constructs an ad-hoc simulation cell from a 3d bounding box.
    constexpr SimulationCellData(const Box3& box, bool is2D = false) :
        _cellMatrix(Vector3(box.sizeX(), 0, 0),
                    Vector3(0, box.sizeY(), 0),
                    Vector3(0, 0, box.sizeZ()),
                    box.minc - Point3::Origin()),
        _reciprocalCellMatrix(_cellMatrix.inverse()),
        _is2D(is2D) {}

    /// Constructor that constructs an ad-hoc simulation cell from the bounding box of a set of points.
    SimulationCellData(const BufferReadAccess<Point3>& positions, bool is2D = false, FloatType minimumBoxSize = 1) : _is2D(is2D) {
        OVITO_ASSERT(minimumBoxSize > 0);
        if(positions && positions.size() != 0) {
            Box3 box = positions.buffer()->boundingBox3();
            OVITO_ASSERT(!box.isEmpty());
            if(box.sizeX() < minimumBoxSize) box.maxc.x() = box.minc.x() + minimumBoxSize;
            if(box.sizeY() < minimumBoxSize) box.maxc.y() = box.minc.y() + minimumBoxSize;
            if(box.sizeZ() < minimumBoxSize) box.maxc.z() = box.minc.z() + minimumBoxSize;
            _cellMatrix = AffineTransformation(
                    Vector3(box.sizeX(), 0, 0),
                    Vector3(0, box.sizeY(), 0),
                    Vector3(0, 0, box.sizeZ()),
                    box.minc - Point3::Origin());
            _reciprocalCellMatrix = _cellMatrix.inverse();
        }
        else {
            _cellMatrix = AffineTransformation::scaling(minimumBoxSize);
            _reciprocalCellMatrix = AffineTransformation::scaling(FloatType(1) / minimumBoxSize);
        }
    }

    /// Returns the cell matrix.
    constexpr const AffineTransformation& cellMatrix() const { return _cellMatrix; }

    /// Returns inverse of the simulation cell matrix.
    /// This matrix maps the simulation cell to the unit cube ([0,1]^3).
    constexpr const AffineTransformation& reciprocalCellMatrix() const { return _reciprocalCellMatrix; }

    /// Returns whether this simulation cell is 2D.
    constexpr bool is2D() const { return _is2D; }

    /// Change the dimensionality of the simulation cell.
    /// This method does not change the cell matrix, but only the is2D flag.
    constexpr void setIs2D(bool is2D) { _is2D = is2D; }

    /// Computes the (positive) volume of the three-dimensional cell.
    FloatType volume3D() const { return std::abs(cellMatrix().determinant()); }

    /// Computes the (positive) volume of the two-dimensional cell.
    constexpr FloatType volume2D() const { return cellMatrix().column(0).cross(cellMatrix().column(1)).length(); }

    /// Returns the periodic boundary flags in all three spatial directions.
    constexpr const std::array<bool,3>& pbcFlags() const { return _pbcFlags; }

    /// Returns whether the simulation cell has periodic boundary conditions applied in the given direction.
    constexpr bool hasPbc(size_t dim) const { OVITO_ASSERT(dim < 3); return pbcFlags()[dim]; }

    /// Returns whether the simulation cell has periodic boundary conditions applied in at least one direction.
    constexpr bool hasPbc() const { return hasPbc(0) || hasPbc(1) || hasPbc(2); }

    /// Sets the periodic boundary flags in all three spatial directions.
    constexpr void setPbcFlags(const std::array<bool,3>& flags) { _pbcFlags = flags; }

    /// Returns the first edge vector of the cell.
    constexpr const Vector3& cellVector1() const { return cellMatrix().column(0); }

    /// Returns the second edge vector of the cell.
    constexpr const Vector3& cellVector2() const { return cellMatrix().column(1); }

    /// Returns the third edge vector of the cell.
    constexpr const Vector3& cellVector3() const { return cellMatrix().column(2); }

    /// Returns the origin point of the cell.
    constexpr const Point3& cellOrigin() const { return Point3::Origin() + cellMatrix().column(3); }

    /// Returns true if the three edges of the cell are parallel to the three coordinates axes.
    constexpr bool isAxisAligned() const {
        if(cellMatrix()(1,0) != 0 || cellMatrix()(2,0) != 0) return false;
        if(cellMatrix()(0,1) != 0 || cellMatrix()(2,1) != 0) return false;
        if(cellMatrix()(0,2) != 0 || cellMatrix()(1,2) != 0) return false;
        return true;
    }

    /// Checks whether the simulation cell has zero volume or the cell matrix contains NaN entries.
    constexpr bool isDegenerate() const {
        if((is2D() ? volume2D() : volume3D()) <= FLOATTYPE_EPSILON)
            return true;
        for(size_t i = 0; i < 3; i++)
            for(size_t j = 0; j < 4; j++)
                if(std::isnan(cellMatrix()(i,j)))
                    return true;
        return false;
    }

    /// Checks if two simulation cells are identical.
    constexpr bool equals(const SimulationCellData& other) const {
        return cellMatrix() == other.cellMatrix() && pbcFlags() == other.pbcFlags() && is2D() == other.is2D();
    }

    /// Converts a point given in reduced cell coordinates to a point in absolute coordinates.
    constexpr Point3 reducedToAbsolute(const Point3& reducedPoint) const { return cellMatrix() * reducedPoint; }

    /// Converts a point given in absolute coordinates to a point in reduced cell coordinates.
    constexpr Point3 absoluteToReduced(const Point3& absPoint) const { return reciprocalCellMatrix() * absPoint; }

    /// Converts a point given in absolute coordinates to a point in reduced cell coordinates and maps it to the [0,1) range.
    constexpr Point3 absoluteToReducedAndWrap(const Point3& absPoint) const {
        Point3 rp = absoluteToReduced(absPoint);
        for(size_t dim = 0; dim < 3; dim++)
            rp[dim] -= hasPbc(dim) * std::floor(rp[dim]);
        return rp;
    }

    /// Converts a vector given in reduced cell coordinates to a vector in absolute coordinates.
    constexpr Vector3 reducedToAbsolute(const Vector3& reducedVec) const { return cellMatrix() * reducedVec; }

    /// Converts a vector given in absolute coordinates to a point in vector cell coordinates.
    constexpr Vector3 absoluteToReduced(const Vector3& absVec) const { return reciprocalCellMatrix() * absVec; }

    /// Wraps a point at the periodic boundaries of the cell.
    constexpr Point3 wrapPoint(const Point3& p) const {
        Point3 pout = p;
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbc(dim)) {
                if(FloatType s = std::floor(reciprocalCellMatrix().prodrow(p, dim)))
                    pout -= s * cellMatrix().column(dim);
            }
        }
        return pout;
    }

    /// Wraps a vector at the periodic boundaries of the cell using minimum image convention.
    constexpr Vector3 wrapVector(const Vector3& v) const {
        Vector3 vout = v;
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbc(dim)) {
                if(FloatType s = std::floor(reciprocalCellMatrix().prodrow(v, dim) + FloatType(0.5)))
                    vout -= s * cellMatrix().column(dim);
            }
        }
        return vout;
    }

    /// Calculates the normal vector of the given simulation cell side.
    constexpr Vector3 cellNormalVector(size_t dim) const {
        size_t dim1 = (dim + 1) % 3;
        size_t dim2 = (dim + 2) % 3;
        Vector3 normal = cellMatrix().column(dim1).cross(cellMatrix().column(dim2));
        // Flip normal if necessary.
        if(normal.dot(cellMatrix().column(dim)) < 0)
            return normal / (-normal.length());
        else
            return normal.safelyNormalized();
    }

    /// Tests if a vector so long that it would be wrapped at a periodic boundary when using the minimum image convention.
    constexpr bool isWrappedVector(const Vector3& v) const {
        for(size_t dim = 0; dim < 3; dim++) {
            if(hasPbc(dim)) {
                if(std::abs(reciprocalCellMatrix().prodrow(v, dim)) >= FloatType(0.5))
                    return true;
            }
        }
        return false;
    }

    /// Wraps the input coordinates at the periodic boundaries of the simulation cell.
    /// The wrapped coordinates are returned as a new property array that has the same
    /// memory layout and visual elements as the input property.
    ConstPropertyPtr wrapPoints(const Property* inputPositions) const;

private:

    /// Stores the three cell vectors and the position of the cell origin.
    AffineTransformation _cellMatrix{AffineTransformation::Zero()};

    /// The inverse of the cell matrix.
    AffineTransformation _reciprocalCellMatrix{AffineTransformation::Zero()};

    /// Periodic boundary condition flags.
    std::array<bool,3> _pbcFlags{false, false, false};

    /// Stores the dimensionality of the system.
    bool _is2D{false};
};


}   // End of namespace
