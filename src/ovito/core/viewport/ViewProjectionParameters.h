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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/******************************************************************************
* This data structure describes a projection parameters used to render
* the 3D contents in a viewport.
******************************************************************************/
struct ViewProjectionParameters
{
    /// The aspect ratio (height/width) of the viewport.
    FloatType aspectRatio = FloatType(1);

    /// Indicates whether this is a orthogonal or perspective projection.
    bool isPerspective = false;

    /// The distance of the front clipping plane in world units.
    FloatType znear = FloatType(0);

    /// The distance of the back clipping plane in world units.
    FloatType zfar = FloatType(1);

    /// For orthogonal projections this is the vertical field of view in world units.
    /// For perspective projections this is the vertical field of view angle in radians.
    FloatType fieldOfView = FloatType(1);

    /// The world to view space transformation matrix.
    AffineTransformation viewMatrix = AffineTransformation::Identity();

    /// The view space to world space transformation matrix.
    AffineTransformation inverseViewMatrix = AffineTransformation::Identity();

    /// The view space to screen space projection matrix.
    Matrix4 projectionMatrix = Matrix4::Identity();

    /// The screen space to view space transformation matrix.
    Matrix4 inverseProjectionMatrix = Matrix4::Identity();

    /// The bounding box of the scene.
    Box3 boundingBox;

    /// Specifies the time interval during which the stored parameters stay constant.
    TimeInterval validityInterval = TimeInterval::empty();

    /// \brief Computes a ray in world space going through a viewport pixel.
    /// \param viewportPoint Viewport coordinates of the point in the range [-1,+1].
    /// \return The ray that goes from the viewpoint through the specified position in the viewport.
    Ray3 viewportRay(const Point2& viewportPoint) const {
        if(isPerspective) {
            Point3 ndc1(viewportPoint.x(), viewportPoint.y(), 1);
            Point3 ndc2(viewportPoint.x(), viewportPoint.y(), 0);
            Point3 p1 = inverseViewMatrix * (inverseProjectionMatrix * ndc1);
            Point3 p2 = inverseViewMatrix * (inverseProjectionMatrix * ndc2);
            return { Point3::Origin() + inverseViewMatrix.translation(), p1 - p2 };
        }
        else {
            Point3 ndc(viewportPoint.x(), viewportPoint.y(), -1);
            return { inverseViewMatrix * (inverseProjectionMatrix * ndc), inverseViewMatrix * Vector3(0,0,-1) };
        }
    }

    /// Computes the world size of an object that should appear always in the same size on the screen.
    /// The window size must be specified in device-independent pixels.
    FloatType nonScalingSize(const Point3& worldPosition, const QSize& windowSize) const {

        OVITO_ASSERT(std::isfinite(fieldOfView));

        // Window height.
        int height = windowSize.height();
        if(height <= 0)
            return 1;

        constexpr FloatType baseSize = 60;

        if(isPerspective) {
            Point3 p = viewMatrix * worldPosition;
            if(p.isOrigin())
                return 1;
            Point3 p1 = projectionMatrix * p;
            Point3 p2 = projectionMatrix * (p + Vector3(0,1,0));
            FloatType dist = (p1 - p2).length();
            if(std::abs(dist) < FLOATTYPE_EPSILON)
                return 1;
            return FloatType(0.8) * baseSize / dist / (FloatType)height;
        }
        else {
            return fieldOfView / (FloatType)height * baseSize;
        }
    }

	/// Computes the world size of an object that should appear one pixel wide in the rendered image.
	FloatType projectedPixelSize(const Point3& worldPosition, const QSize& windowSize) const {
        // Get window size in device pixels.
        if(windowSize.height() == 0)
            return 0;

        if(isPerspective) {

            Point3 p = viewMatrix * worldPosition;
            if(p.isOrigin())
                return 1;

            Point3 p1 = projectionMatrix * p;
            Point3 p2 = projectionMatrix * (p + Vector3(1,0,0));

            return 1.0 / (p1 - p2).length() / (FloatType)windowSize.height();
        }
        else {
            return fieldOfView / (FloatType)windowSize.height();
        }
    }
};

}   // End of namespace
