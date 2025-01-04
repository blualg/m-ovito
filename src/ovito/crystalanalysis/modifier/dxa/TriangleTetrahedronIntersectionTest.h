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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/core/utilities/DataTypes.h>

namespace Ovito::TetrahedronTriangleIntersection {

/******************************************************************************
 * Determines wheter two triangle intersect in 3d space or not
 * Triangle1 from points p1 to p3
 * Triangle 2 from points q1 to q3
 * Based of "Fast and Robust Triangle-Triangle Overlap Test Using Orientation Predicates", Philippe Guigue and Olivier Devillers
 ******************************************************************************/

// Adapted from:
// Original implementation at https://github.com/erich666/jgt-code/blob/master/Volume_08/Number_1/Guigue2003/tri_tri_intersect.c
/******************************************************************************
*
    Original license:
    The MIT License (MIT)

    Copyright (c) 2015 Eric Haines

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
 *
 ******************************************************************************/

constexpr inline bool triTriIntersectionTest(const Point3& p1, const Point3& p2, const Point3& p3, const Point3& q1, const Point3& q2,
                                             const Point3& q3);
inline bool test(const std::array<Point3, 4>& tet, const std::array<Point3, 3>& tri);

namespace Implementation {

constexpr inline FloatType orient2D(const Point2& p1, const Point2& p2, const Point2& p3)
{
    return (p1.x() - p2.x()) * (p3.y() - p2.y()) - (p1.y() - p2.y()) * (p3.x() - p2.x());
}

constexpr inline bool triVertIntersectionTest2D(const Point2& p1, const Point2& p2, const Point2& p3, const Point2& q1, const Point2& q2,
                                                const Point2& q3)
{
    if(orient2D(q3, q1, p2) >= 0.0) {
        if(orient2D(q3, q2, p2) <= 0.0) {
            if(orient2D(p1, q1, p2) > 0.0) {
                return orient2D(p1, q2, p2) <= 0.0;
            }
            else {
                if(orient2D(p1, q1, p3) >= 0.0) {
                    return orient2D(p2, p3, q1) >= 0.0;
                }
                else {
                    return false;
                }
            }
        }
        else if(orient2D(p1, q2, p2) <= 0.0) {
            if(orient2D(q3, q2, p3) <= 0.0) {
                return orient2D(p2, p3, q2) >= 0.0;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    else if(orient2D(q3, q1, p3) >= 0.0) {
        if(orient2D(p2, p3, q3) >= 0.0) {
            return orient2D(p1, q1, p3) >= 0.0;
        }
        else if(orient2D(p2, p3, q2) >= 0.0) {
            return orient2D(q3, p3, q2) >= 0.0;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
};

constexpr inline bool triEdgeIntersectionTest2D(const Point2& p1, const Point2& p2, const Point2& p3, const Point2& q1, const Point2& q2,
                                                const Point2& q3)
{
    if(orient2D(q3, q1, p2) >= 0.0) {
        if(orient2D(p1, q1, p2) >= 0.0) {
            return orient2D(p1, p2, q3) >= 0.0;
        }
        else {
            if(orient2D(p2, p3, q1) >= 0.0) {
                return orient2D(p3, p1, q1) >= 0.0;
            }
            else {
                return false;
            }
        }
    }
    else {
        if(orient2D(q3, q1, p3) >= 0.0) {
            if(orient2D(p1, q1, p3) >= 0.0) {
                if(orient2D(p1, p3, q3) >= 0.0) {
                    return true;
                }
                else {
                    return orient2D(p2, p3, q3) >= 0.0;
                }
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
}

constexpr inline bool ccwTriTriIntersectionTest2D(const Point2& p1, const Point2& p2, const Point2& p3, const Point2& q1, const Point2& q2,
                                                  const Point2& q3)
{
    if(orient2D(q1, q2, p1) >= 0.0) {
        if(orient2D(q2, q3, p1) >= 0.0) {
            if(orient2D(q3, q1, p1) >= 0.0) {
                return 1;
            }
            else {
                return triEdgeIntersectionTest2D(p1, p2, p3, q1, q2, q3);
            }
        }
        else {
            if(orient2D(q3, q1, p1) >= 0.0) {
                return triEdgeIntersectionTest2D(p1, p2, p3, q3, q1, q2);
            }
            else {
                return triVertIntersectionTest2D(p1, p2, p3, q1, q2, q3);
            }
        }
    }
    else {
        if(orient2D(q2, q3, p1) >= 0.0) {
            if(orient2D(q3, q1, p1) >= 0.0) {
                return triEdgeIntersectionTest2D(p1, p2, p3, q2, q3, q1);
            }
            else {
                return triVertIntersectionTest2D(p1, p2, p3, q2, q3, q1);
            }
        }
        else {
            return triVertIntersectionTest2D(p1, p2, p3, q3, q1, q2);
        }
    }
}

constexpr inline bool triTriOverlapTest2D(const Point2& p1, const Point2& p2, const Point2& p3, const Point2& q1, const Point2& q2,
                                          const Point2& q3)
{
    if(orient2D(p1, p2, p3) < 0.0) {
        if(orient2D(q1, q2, q3) < 0.0) {
            return ccwTriTriIntersectionTest2D(p1, p3, p2, q1, q3, q2);
        }
        else {
            return ccwTriTriIntersectionTest2D(p1, p3, p2, q1, q2, q3);
        }
    }
    else if(orient2D(q1, q2, q3) < 0.0) {
        return ccwTriTriIntersectionTest2D(p1, p2, p3, q1, q3, q2);
    }
    else {
        return ccwTriTriIntersectionTest2D(p1, p2, p3, q1, q2, q3);
    }
};

// min/max condition to determine whether or not the two intervals overlap.
// Eq.2 from paper
constexpr inline bool checkMinMaxCondition(const Point3& p1, const Point3& p2, const Point3& p3, const Point3& q1, const Point3& q2,
                                           const Point3& q3)
{
    Vector3 n = (q1 - p2).cross(p1 - p2);
    if((q2 - p2).dot(n) > 0.0) {
        return false;
    }
    n = (q1 - p1).cross(p3 - p1);
    return (q3 - p1).dot(n) <= 0.0;
}

// Projection of the triangles in 3D onto 2D such that the area of the projection is maximized
// Afterwards the coplanar triangles can be tested in 2D
constexpr inline bool checkTriTriCoplanar(const Point3& p1, const Point3& p2, const Point3& p3, const Point3& q1, const Point3& q2,
                                          const Point3& q3, Vector3 normal)
{
    normal.x() = ((normal.x() < 0) ? -normal.x() : normal.x());
    normal.y() = ((normal.y() < 0) ? -normal.y() : normal.y());
    normal.z() = ((normal.z() < 0) ? -normal.z() : normal.z());

    // Projected triangles
    Point2 pp1;
    Point2 pp2;
    Point2 pp3;
    Point2 pq1;
    Point2 pq2;
    Point2 pq3;

    if((normal.x() > normal.z()) && (normal.x() >= normal.y())) {
        // Project onto plane YZ
        pp1[0] = p2[2];
        pp1[1] = p2[1];
        pp2[0] = p1[2];
        pp2[1] = p1[1];
        pp3[0] = p3[2];
        pp3[1] = p3[1];

        pq1[0] = q2[2];
        pq1[1] = q2[1];
        pq2[0] = q1[2];
        pq2[1] = q1[1];
        pq3[0] = q3[2];
        pq3[1] = q3[1];
    }
    else if((normal.y() > normal.z()) && (normal.y() >= normal.x())) {
        // Project onto plane XZ
        pp1[0] = p2[0];
        pp1[1] = p2[2];
        pp2[0] = p1[0];
        pp2[1] = p1[2];
        pp3[0] = p3[0];
        pp3[1] = p3[2];

        pq1[0] = q2[0];
        pq1[1] = q2[2];
        pq2[0] = q1[0];
        pq2[1] = q1[2];
        pq3[0] = q3[0];
        pq3[1] = q3[2];
    }
    else {
        // Project onto plane XY
        pp1[0] = p1[0];
        pp1[1] = p1[1];
        pp2[0] = p2[0];
        pp2[1] = p2[1];
        pp3[0] = p3[0];
        pp3[1] = p3[1];

        pq1[0] = q1[0];
        pq1[1] = q1[1];
        pq2[0] = q2[0];
        pq2[1] = q2[1];
        pq3[0] = q3[0];
        pq3[1] = q3[1];
    }

    return triTriOverlapTest2D(pp1, pp2, pp3, pq1, pq2, pq3);
}

// Thus, it is only necessary to check a min/max condition to determine whether or not the two intervals overlap.
constexpr inline bool checkTriTriIntersection3d(const Point3& p1, const Point3& p2, const Point3& p3, const Point3& q1, const Point3& q2,
                                                const Point3& q3, const Vector3& normal, FloatType dq1, FloatType dq2, FloatType dq3)
{
    if(dq1 > 0.0) {
        if(dq2 > 0.0) {
            return checkMinMaxCondition(p1, p3, p2, q3, q1, q2);
        }
        else if(dq3 > 0.0) {
            return checkMinMaxCondition(p1, p3, p2, q2, q3, q1);
        }
        else {
            return checkMinMaxCondition(p1, p2, p3, q1, q2, q3);
        }
    }
    else if(dq1 < 0.0) {
        if(dq2 < 0.0) {
            return checkMinMaxCondition(p1, p2, p3, q3, q1, q2);
        }
        else if(dq3 < 0.0) {
            return checkMinMaxCondition(p1, p2, p3, q2, q3, q1);
        }
        else {
            return checkMinMaxCondition(p1, p3, p2, q1, q2, q3);
        }
    }
    else {
        if(dq2 < 0.0) {
            if(dq3 >= 0.0) {
                return checkMinMaxCondition(p1, p3, p2, q2, q3, q1);
            }
            else {
                return checkMinMaxCondition(p1, p2, p3, q1, q2, q3);
            }
        }
        else if(dq2 > 0.0) {
            if(dq3 > 0.0) {
                return checkMinMaxCondition(p1, p3, p2, q1, q2, q3);
            }
            else {
                return checkMinMaxCondition(p1, p2, p3, q2, q3, q1);
            }
        }
        else {
            if(dq3 > 0.0) {
                return checkMinMaxCondition(p1, p2, p3, q3, q1, q2);
            }
            else if(dq3 < 0.0) {
                return checkMinMaxCondition(p1, p3, p2, q3, q1, q2);
            }
            else {
                return checkTriTriCoplanar(p1, p2, p3, q1, q2, q3, normal);
            }
        }
    }
}

constexpr inline bool triTriIntersectionTest(const Point3& p1, const Point3& p2, const Point3& p3, const Point3& q1, const Point3& q2,
                                             const Point3& q3)
{
    constexpr FloatType EPSILON = 1e-6;

    // Calculate the normal vector of triangle 2
    Vector3 normal = (q1 - q3).cross((q2 - q3));

    // Compute distance signs of p1, p2, and p3 to the plane of triangle 2
    FloatType p1dist = (p1 - q3).dot(normal);
    FloatType p2dist = (p2 - q3).dot(normal);
    FloatType p3dist = (p3 - q3).dot(normal);

    // Set p_dist to 0.0 if the value is close enough (for robustness)
    p1dist = (std::abs(p1dist) < EPSILON) ? p1dist = 0.0 : p1dist;
    p2dist = (std::abs(p2dist) < EPSILON) ? p2dist = 0.0 : p2dist;
    p3dist = (std::abs(p3dist) < EPSILON) ? p3dist = 0.0 : p3dist;

    // Check whether all points of triangle 1 are on the same side of triangle 2
    // No intersection
    if(((p1dist * p2dist) > 0.0) && ((p1dist * p3dist) > 0.0)) {
        return false;
    }

    // Calculate the normal vector of triangle 2
    normal = (p2 - p1).cross((p3 - p1));

    // Compute distance signs of q1, q2, and q3 to the plane of triangle 1
    FloatType q1dist = (q1 - p3).dot(normal);
    FloatType q2dist = (q2 - p3).dot(normal);
    FloatType q3dist = (q3 - p3).dot(normal);

    // Set q_dist to 0.0 if the value is close enough (for robustness)
    q1dist = (std::abs(q1dist) < EPSILON) ? q1dist = 0.0 : q1dist;
    q2dist = (std::abs(q2dist) < EPSILON) ? q2dist = 0.0 : q2dist;
    q3dist = (std::abs(q3dist) < EPSILON) ? q3dist = 0.0 : q3dist;

    // Check whether all points of triangle 2 are on teh same side of triangle 1
    // No intersection
    if(((q1dist * q2dist) > 0.0) && ((q1dist * q3dist) > 0.0)) {
        return false;
    }

    // The algorithm then applies a circular permutation to the vertices of each triangle such that p1 (respectively, q1)
    //  is the only vertex of its triangle that lies on its side.
    //  An additional transposition operation (i.e., a swap operation) is performed at the same time on vertices
    //  q2 and q3 (respectively, p2 and p3 so that vertex p1 (respectively, q1) sees q1q2q3
    //  (respectively, p1p2p3 in counterclockwise order (see Figure 2).
    using Implementation::checkTriTriCoplanar;
    using Implementation::checkTriTriIntersection3d;
    if(p1dist > 0.0) {
        if(p2dist > 0.0)
            return checkTriTriIntersection3d(p3, p1, p2, q1, q3, q2, normal, q1dist, q3dist, q2dist);
        else if(p3dist > 0.0)
            return checkTriTriIntersection3d(p2, p3, p1, q1, q3, q2, normal, q1dist, q3dist, q2dist);
        else
            return checkTriTriIntersection3d(p1, p2, p3, q1, q2, q3, normal, q1dist, q2dist, q3dist);
    }
    else if(p1dist < 0.0) {
        if(p2dist < 0.0)
            return checkTriTriIntersection3d(p3, p1, p2, q1, q2, q3, normal, q1dist, q2dist, q3dist);
        else if(p3dist < 0.0)
            return checkTriTriIntersection3d(p2, p3, p1, q1, q2, q3, normal, q1dist, q2dist, q3dist);
        else
            return checkTriTriIntersection3d(p1, p2, p3, q1, q3, q2, normal, q1dist, q3dist, q2dist);
    }
    else {
        if(p2dist < 0.0) {
            if(p3dist >= 0.0)
                return checkTriTriIntersection3d(p2, p3, p1, q1, q3, q2, normal, q1dist, q3dist, q2dist);
            else
                return checkTriTriIntersection3d(p1, p2, p3, q1, q2, q3, normal, q1dist, q2dist, q3dist);
        }
        else if(p2dist > 0.0) {
            if(p3dist > 0.0)
                return checkTriTriIntersection3d(p1, p2, p3, q1, q3, q2, normal, q1dist, q3dist, q2dist);
            else
                return checkTriTriIntersection3d(p2, p3, p1, q1, q2, q3, normal, q1dist, q2dist, q3dist);
        }
        else {
            if(p3dist > 0.0)
                return checkTriTriIntersection3d(p3, p1, p2, q1, q2, q3, normal, q1dist, q2dist, q3dist);
            else if(p3dist < 0.0)
                return checkTriTriIntersection3d(p3, p1, p2, q1, q3, q2, normal, q1dist, q3dist, q2dist);
            else
                return checkTriTriCoplanar(p1, p2, p3, q1, q2, q3, normal);
        }
    }
};

#ifdef OVITO_DEBUG
// TODO: somehow this cannot be consteval and static_asserts
inline void test_cases()
{
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(6.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 6.0, 0.0);
        constexpr Point3 q1(0.0, 3.0, 3.0);
        constexpr Point3 q2(0.0, 3.0, -3.0);
        constexpr Point3 q3(-3.0, 3.0, 3.0);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 6.0, 0.0);
        constexpr Point3 p2(6.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 0.0, 0.0);
        constexpr Point3 q1(1.0, 3.0, 0.0);
        constexpr Point3 q2(3.0, 1.0, 0.0);
        constexpr Point3 q3(2.0, 2.0, 4.0);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(6.0, 0.0, 0.0);
        constexpr Point3 p3(3.0, 4.0, 0.0);
        constexpr Point3 q1(0.0, 2.0, 0.0);
        constexpr Point3 q2(6.0, 2.0, 0.0);
        constexpr Point3 q3(3.0, -2.0, 0.);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(1.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 1.0, 0.0);
        constexpr Point3 q1(0.5, 0.5, 0.0);
        constexpr Point3 q2(1.0, 1.0, 0.0);
        constexpr Point3 q3(0.0, 0.5, -0.5);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(2.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 2.0, 0.0);
        constexpr Point3 q1(1.0, 1.0, -1.0);
        constexpr Point3 q2(3.0, 1.0, 0.0);
        constexpr Point3 q3(1.0, 3.0, 0.0);
        OVITO_ASSERT(!triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(1.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 1.0, 0.0);
        constexpr Point3 q1(10.0, 10.0, 10.0);
        constexpr Point3 q2(11.0, 10.0, 10.0);
        constexpr Point3 q3(10.0, 11.0, 10.0);
        OVITO_ASSERT(!triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(1.0, 0.0, 0.0);
        constexpr Point3 p3(0.0, 1.0, 0.0);
        constexpr Point3 q1(0.0, 0.0, 1.0);
        constexpr Point3 q2(1.0, 0.0, 1.0);
        constexpr Point3 q3(0.0, 0.0, 1.0);
        OVITO_ASSERT(!triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(2.0, 0.0, 0.0);
        constexpr Point3 p3(1.0, 3.0, 0.0);
        constexpr Point3 q1(3.0, 0.0, 0.0);
        constexpr Point3 q2(4.0, 0.0, 0.0);
        constexpr Point3 q3(3.0, 3.0, 0.0);
        OVITO_ASSERT(!triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(0.0, 0.0, 0.0);
        constexpr Point3 p2(2.0, 0.0, 0.0);
        constexpr Point3 p3(1.0, 2.0, 0.0);
        constexpr Point3 q1(2.0, 0.0, 0.0);
        constexpr Point3 q2(3.0, 0.0, 0.0);
        constexpr Point3 q3(2.5, 1.0, 0.0);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }
    {
        constexpr Point3 p1(-21.0, -72.0, 63.0);
        constexpr Point3 p2(-78.0, 99.0, 40.0);
        constexpr Point3 p3(-19.0, -78.0, -83.0);
        constexpr Point3 q1(96.0, 77.0, -51.0);
        constexpr Point3 q2(-95.0, -1.0, -16.0);
        constexpr Point3 q3(9.0, 5.0, -21.0);
        OVITO_ASSERT(triTriIntersectionTest(p1, p2, p3, q1, q2, q3));
    }

    {
        std::vector<Point3> tri1{
            Point3{-36.7156, -13.6906, -67.2093}, Point3{-36.5648, -15.8751, -68.8058}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-37.4587, -11.8635, -65.9189}, Point3{-36.7156, -13.6906, -67.2093}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-35.7972, -12.8117, -63.8525}, Point3{-37.4587, -11.8635, -65.9189}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-38.2442, -11.8532, -63.7483}, Point3{-35.7972, -12.8117, -63.8525}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-40.8474, -13.5012, -61.905},  Point3{-38.2442, -11.8532, -63.7483}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-42.2198, -13.1119, -59.5765}, Point3{-40.8474, -13.5012, -61.905},  Point3{-39.1396, -15.7867, -64.4809},
            Point3{-42.9084, -14.7177, -61.9851}, Point3{-42.2198, -13.1119, -59.5765}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-41.5922, -17.7128, -62.0988}, Point3{-42.9084, -14.7177, -61.9851}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-38.8275, -19.0929, -62.0618}, Point3{-41.5922, -17.7128, -62.0988}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-38.6699, -20.9933, -63.542},  Point3{-38.8275, -19.0929, -62.0618}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-39.9258, -20.2716, -65.2899}, Point3{-38.6699, -20.9933, -63.542},  Point3{-39.1396, -15.7867, -64.4809},
            Point3{-39.5545, -17.6645, -67.0211}, Point3{-39.9258, -20.2716, -65.2899}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-38.6288, -17.8545, -69.7177}, Point3{-39.5545, -17.6645, -67.0211}, Point3{-39.1396, -15.7867, -64.4809},
            Point3{-36.5648, -15.8751, -68.8058}, Point3{-38.6288, -17.8545, -69.7177}, Point3{-39.1396, -15.7867, -64.4809}};

        std::vector<Point3> tri2{
            Point3{-40.0365, -15.9612, -65.1712}, Point3{-37.5603, -16.0167, -66.302},  Point3{-38.2708, -16.8255, -63.3138},
            Point3{-38.1106, -18.4331, -65.1723}, Point3{-37.5603, -16.0167, -66.302},  Point3{-40.0365, -15.9612, -65.1712},
            Point3{-38.1106, -18.4331, -65.1723}, Point3{-40.0365, -15.9612, -65.1712}, Point3{-38.2708, -16.8255, -63.3138}};

        for(size_t i = 0; i < tri2.size(); i += 3) {
            bool inter = false;
            for(size_t j = 0; j < tri1.size(); j += 3) {
                inter = triTriIntersectionTest(tri2[i], tri2[i + 1], tri2[i + 2], tri1[j], tri1[j + 1], tri1[j + 2]);
                if(inter) {
                    break;
                }
            }
            OVITO_ASSERT(inter);
        }
    }

    {
        std::vector<Point3> tri{{-87.4804, -4.86812, -85.7408},    {-87.0389, -7.27953, -85.16},      {-87.7004, -5.25003, -89.5937},
                                {-87.7039, -2.58771, -87.0065},    {-87.4804, -4.86812, -85.7408},    {-87.7004, -5.25003, -89.5937},
                                {-88.2395, -0.329221, -87.9891},   {-87.7039, -2.58771, -87.0065},    {-87.7004, -5.25003, -89.5937},
                                {-89.1769, 1.63263, -89.5438},     {-88.2395, -0.329221, -87.9891},   {-87.7004, -5.25003, -89.5937},
                                {-91.6444, 1.84494, -89.062},      {-89.1769, 1.63263, -89.5438},     {-87.7004, -5.25003, -89.5937},
                                {-93.3319, -0.00309714, -91.7785}, {-91.6444, 1.84494, -89.062},      {-87.7004, -5.25003, -89.5937},
                                {-90.9766, 0.104804, -93.0513},    {-93.3319, -0.00309714, -91.7785}, {-87.7004, -5.25003, -89.5937},
                                {-88.754, -1.54415, -94.1073},     {-90.9766, 0.104804, -93.0513},    {-87.7004, -5.25003, -89.5937},
                                {-87.3859, -4.01062, -92.8542},    {-88.754, -1.54415, -94.1073},     {-87.7004, -5.25003, -89.5937},
                                {-85.7492, -5.76544, -91.9629},    {-87.3859, -4.01062, -92.8542},    {-87.7004, -5.25003, -89.5937},
                                {-85.9438, -7.92181, -93.192},     {-85.7492, -5.76544, -91.9629},    {-87.7004, -5.25003, -89.5937},
                                {-84.9932, -9.9866, -91.8872},     {-85.9438, -7.92181, -93.192},     {-87.7004, -5.25003, -89.5937},
                                {-84.4596, -12.2811, -90.6759},    {-84.9932, -9.9866, -91.8872},     {-87.7004, -5.25003, -89.5937},
                                {-86.2803, -12.4431, -89.1573},    {-84.4596, -12.2811, -90.6759},    {-87.7004, -5.25003, -89.5937},
                                {-86.4542, -10.7712, -87.2187},    {-86.2803, -12.4431, -89.1573},    {-87.7004, -5.25003, -89.5937},
                                {-85.2421, -8.97017, -86.1293},    {-86.4542, -10.7712, -87.2187},    {-87.7004, -5.25003, -89.5937},
                                {-87.7532, -9.32109, -86.1692},    {-85.2421, -8.97017, -86.1293},    {-87.7004, -5.25003, -89.5937},
                                {-87.0389, -7.27953, -85.16},      {-87.7532, -9.32109, -86.1692},    {-87.7004, -5.25003, -89.5937}};
        std::array<Point3, 4> tet{{{-85.1796, -7.88468, -90.3071},
                                   {-86.889, -8.39243, -88.2976},
                                   {-87.3222, -6.31266, -89.5614},
                                   {-85.3906, -6.54087, -87.2267}}};

        bool inter = false;
        for(size_t i = 0; i < tri.size(); i += 3) {
            inter = test(tet, {tri[i], tri[i + 1], tri[i + 2]});
            if(inter) {
                break;
            }
        }
        OVITO_ASSERT(!inter);
    }

    qDebug("triTriIntersectionTest success!");
}
#endif

#if 0
inline bool ptsSameSideTriTest(const Point3& t1, const Point3& t2, const Point3& t3, const Point3& t4, const Point3& p)
{
    constexpr FloatType EPSILON = 1e-6;

    Vector3 normal = (t2 - t1).cross(t3 - t1);
    FloatType t4dist = (t4 - t1).dot(normal);
    FloatType pdist = (p - t1).dot(normal);

    t4dist = (std::abs(t4dist) < EPSILON) ? t4dist = 0.0 : t4dist;
    pdist = (std::abs(pdist) < EPSILON) ? pdist = 0.0 : pdist;

    return (t4dist * pdist) > 0.0;
}

inline bool ptsInTetTest(const Point3& t1, const Point3& t2, const Point3& t3, const Point3& t4, const Point3& p)
{
    return ptsSameSideTriTest(t1, t2, t3, t4, p) && ptsSameSideTriTest(t2, t3, t4, t1, p) && ptsSameSideTriTest(t3, t4, t1, t2, p) &&
           ptsSameSideTriTest(t4, t1, t2, t3, p);
}

// Adapted from:
// https://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
//      This work is licensed under the Creative Commons Attribution 4.0 International License. To view a copy
//      of this license, visit http://creativecommons.org/licenses/by/4.0/ or send a letter to Creative Commons,
//      PO Box 1866, Mountain View, CA 94042, USA.
// Original Implementation:
//      David Eberly, Geometric Tools, Redmond WA 98052
//      Copyright (c) 1998-2024
//      Distributed under the Boost Software License, Version 1.0.
//      https://www.boost.org/LICENSE_1_0.txt
//      https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
//      Version: 6.0.2023.08.08

inline FloatType triPtsSqDistance(const Point3& t1, const Point3& t2, const Point3& t3, const Point3& p)
{
    Vector3 e0 = t2 - t1;
    Vector3 e1 = t3 - t1;
    Vector3 delta = t1 - p;
    FloatType a = e0.dot(e0);
    FloatType b = e0.dot(e1);
    FloatType c = e1.dot(e1);
    FloatType d = delta.dot(e0);
    FloatType e = delta.dot(e1);

    // a = a00
    // b = a01
    // c = a11
    // d = b0
    // e = b1

    FloatType s = b * e - c * d;
    FloatType t = b * d - a * e;
    FloatType det = std::max(a * c - b * b, 0.0);

    const auto region0 = [&]() {
        s /= det;
        t /= det;
    };

    const auto region1 = [&]() {
        FloatType numer = (c + e) - (b + d);
        if(numer <= 0.0) {
            s = 0.0;
            t = 1.0;
        }
        else {
            FloatType denom = a - 2.0 * b + c;
            if(numer >= denom) {
                s = 1.0;
                t = 0.0;
            }
            else {
                s = numer / denom;
                t = 1.0 - s;
            }
        }
    };

    const auto region2 = [&]() {
        FloatType tmp0 = b + d;
        FloatType tmp1 = c + e;
        if(tmp1 > tmp0) {
            FloatType numer = tmp1 - tmp0;
            FloatType denom = a - 2.0 * b + c;
            if(numer >= denom) {
                s = 1.0;
                t = 0.0;
            }
            else {
                s = numer / denom;
                t = 1.0 - s;
            }
        }
        else {
            s = 0.0;
            if(tmp1 <= 0.0) {
                t = 1.0;
            }
            else if(e >= 0) {
                t = 0.0;
            }
            else {
                t = -e / c;
            }
        }
    };

    const auto region3 = [&]() {
        s = 0.0;
        if(e >= 0.0) {
            t = 0.0;
        }
        else if(-e >= c) {
            t = 1.0;
        }
        else {
            t = -e / c;
        }
    };

    const auto region4 = [&]() {
        if(d < 0.0) {
            t = 0.0;
            if(-d >= a) {
                s = 1.0;
            }
            else {
                s = -d / a;
            }
        }
        else {
            s = 0.0;
            if(e >= 0) {
                t = 0.0;
            }
            else if(-e >= c) {
                t = 1.0;
            }
            else {
                t = -e / c;
            }
        }
    };

    const auto region5 = [&]() {
        t = 0.0;
        if(d >= 0.0) {
            s = 0.0;
        }
        else if(-d >= a) {
            s = 1.0;
        }
        else {
            s = -d / a;
        }
    };

    const auto region6 = [&]() {
        FloatType tmp0 = b + e;
        FloatType tmp1 = a + d;
        if(tmp1 > tmp0) {
            FloatType numer = tmp1 - tmp0;
            FloatType denom = a - 2.0 * b + c;
            if(numer >= denom) {
                t = 1.0;
                s = 0.0;
            }
            else {
                t = numer / denom;
                s = 1.0 - t;
            }
        }
        else {
            t = 0.0;
            if(tmp1 <= 0.0) {
                s = 1.0;
            }
            else if(d >= 0.0) {
                s = 0.0;
            }
            else {
                s = -d / a;
            }
        }
    };

    if(s + t <= det) {
        if(s < 0.0) {
            if(t < 0.0) {
                // region4
                region4();
            }
            else {
                // region3
                region3();
            }
        }
        else if(t < 0.0) {
            // region5
            region5();
        }
        else {
            // region0
            region0();
        }
    }
    else {
        if(s < 0.0) {
            // region2
            region2();
        }
        else if(t < 0.0) {
            // region6
            region6();
        }
        else {
            // region1
            region1();
        }
    }

    Point3 closestPoint = t1 + s * e0 + t * e1;
    return (p - closestPoint).squaredLength();
}
#endif

}  // namespace Implementation

// Test the intersection of a tetrahedron and a triangle
// Currently only tet face intersections are tested for. Triangles completely inside the
// tetrahedron are not found! The code for the triangle-inside-tet test exists but is currently disabled (ptsInTetTest()).
inline bool test(const std::array<Point3, 4>& tet, const std::array<Point3, 3>& tri)
{
    static constexpr std::array<std::array<int, 3>, 4> tabVertexIndex = {{{1, 3, 2}, {0, 2, 3}, {0, 3, 1}, {0, 1, 2}}};

#if 0
    // check if one or more of the triangle vertices are inside the tetrahedron.
    using Implementation::ptsInTetTest;
    for(size_t i = 0; i < 3; ++i) {
        if(ptsInTetTest(tet[0], tet[1], tet[2], tet[3], tri[i]) && ptsInTetTest(tet[1], tet[2], tet[3], tet[0], tri[i]) &&
           ptsInTetTest(tet[2], tet[3], tet[0], tet[1], tri[i]) && ptsInTetTest(tet[3], tet[0], tet[1], tet[2], tri[i])) {
            return true;
        }
    }
#endif

#if 0
    constexpr FloatType EPSILON = 1e-6;
    // check if any of the tet points are really close to the triangle
    for(size_t i = 0; i < 4; ++i) {
        FloatType delta = Implementation::triPtsSqDistance(tri[0], tri[1], tri[2], tet[i]);
        if(delta <= EPSILON) {
            return true;
        }
    }
#endif

    // ckeck if any of the tetrahedron faces intersect with the triangle
    for(size_t i = 0; i < 4; ++i) {
        if(Implementation::triTriIntersectionTest(tet[tabVertexIndex[i][0]], tet[tabVertexIndex[i][1]], tet[tabVertexIndex[i][2]], tri[0],
                                                  tri[1], tri[2])) {
            return true;
        }
    }
    return false;
}

}  // namespace Ovito::TetrahedronTriangleIntersection
