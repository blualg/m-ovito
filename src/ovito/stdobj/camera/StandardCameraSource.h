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


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/camera/AbstractCameraSource.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/animation/controller/Controller.h>

namespace Ovito {

/**
 * A pipeline source generating a StandardCameraObject.
 */
class OVITO_STDOBJ_EXPORT StandardCameraSource : public AbstractCameraSource
{
    OVITO_CLASS(StandardCameraSource)
    OVITO_CLASSINFO("DisplayName", "Camera");

public:

    /// Constructor.
    explicit StandardCameraSource(ObjectInitializationFlags flags);

    /// Returns whether this camera is a target camera directed at a target object.
    virtual bool isTargetCamera() const override;

    /// Changes the type of the camera to a target camera or a free camera.
    virtual void setIsTargetCamera(bool enable) override;

    /// For a target camera, queries the distance between the camera and its target.
    virtual FloatType targetDistance(AnimationTime time) const override;

    /// Returns the current orthogonal field of view.
    virtual FloatType zoom() const override;

    /// Sets the field of view of a parallel projection camera.
    virtual void setZoom(FloatType newFOV) override;

    /// Returns the current perspective field of view angle.
    virtual FloatType fov() const override;

    /// Sets the field of view angle of a perspective projection camera.
    virtual void setFov(FloatType newFOV) override;

    /// Returns whether this camera uses a perspective projection.
    virtual bool isPerspectiveCamera() const override { return isPerspective(); }

    /// Lets the source generate a camera object for the given animation time.
    virtual DataOORef<const AbstractCameraObject> cameraObject(AnimationTime time) const override;

protected:

    /// Asks the object for the result of the data pipeline.
    virtual PipelineEvaluationResult evaluateInternal(const PipelineEvaluationRequest& request) override;

private:

    /// Determines if this camera uses a perspective projection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, isPerspective, setIsPerspective);

    /// This controller stores the field of view of the camera if it uses a perspective projection.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, fovController, setFovController);

    /// This controller stores the field of view of the camera if it uses an orthogonal projection.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, zoomController, setZoomController);

    /// Controls whether this camera is a target camera directed at a target object.
    DECLARE_VIRTUAL_PROPERTY_FIELD(bool, isTargetCamera);
};

}   // End of namespace
