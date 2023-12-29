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
};

}   // End of namespace
