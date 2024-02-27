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
#include <ovito/core/dataset/pipeline/PipelineNode.h>

namespace Ovito {

/**
 * An abstract base class for pipeline sources that generate a camera object.
 */
class OVITO_CORE_EXPORT AbstractCameraSource : public PipelineNode
{
    OVITO_CLASS(AbstractCameraSource)

protected:

    /// Constructor.
    using PipelineNode::PipelineNode;

public:

    /// Returns whether this camera is a target camera directed at a target object.
    virtual bool isTargetCamera() const = 0;

    /// Changes the type of the camera to a target camera or a free camera.
    virtual void setIsTargetCamera(bool enable) = 0;

    /// For a target camera, queries the distance between the camera and its target.
    virtual FloatType targetDistance(AnimationTime time) const = 0;

    /// Returns the current orthogonal field of view.
    virtual FloatType zoom() const = 0;

    /// Sets the field of view of a parallel projection camera.
    virtual void setZoom(FloatType newFOV) = 0;

    /// Returns the current perspective field of view angle.
    virtual FloatType fov() const = 0;

    /// Sets the field of view angle of a perspective projection camera.
    virtual void setFov(FloatType newFOV) = 0;

    /// Returns whether this camera uses a perspective projection.
    virtual bool isPerspectiveCamera() const = 0;

    /// Lets the source generate a camera object for the given animation time.
    virtual DataOORef<const AbstractCameraObject> cameraObject(AnimationTime time) const = 0;
};

}   // End of namespace
