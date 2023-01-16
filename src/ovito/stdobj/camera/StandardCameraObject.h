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


#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/data/camera/AbstractCameraObject.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/rendering/LinePrimitive.h>

namespace Ovito::StdObj {

/**
 * The standard camera data object.
 */
class OVITO_STDOBJ_EXPORT StandardCameraObject : public AbstractCameraObject
{
    /// Give this class its own metaclass.
    class StandardCameraObjectClass : public AbstractCameraObject::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using AbstractCameraObject::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes are of the deserialization of a serialized property field that has been removed from the class. 
        /// This is needed for backward compatibility with OVITO 3.3.
        virtual SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr overrideFieldDeserialization(const SerializedClassInfo::PropertyFieldInfo& field) const override;
    };

    OVITO_CLASS_META(StandardCameraObject, StandardCameraObjectClass)
    Q_CLASSINFO("DisplayName", "Camera");
    Q_CLASSINFO("ClassNameAlias", "CameraObject");  // For backward compatibility with OVITO 3.3.

public:

    /// Constructor.
    Q_INVOKABLE StandardCameraObject(ObjectCreationParams params);

    /// With a target camera, indicates the distance between the camera and its target.
    static FloatType getTargetDistance(AnimationTime time, const PipelineSceneNode* node);

    /// \brief Returns a structure describing the camera's projection.
    /// \param[in] time The animation time for which the camera's projection parameters should be determined.
    /// \param[in,out] projParams The structure that is to be filled with the projection parameters.
    ///     The following fields of the ViewProjectionParameters structure are already filled in when the method is called:
    ///   - ViewProjectionParameters::aspectRatio (The aspect ratio (height/width) of the viewport)
    ///   - ViewProjectionParameters::viewMatrix (The world to view space transformation)
    ///   - ViewProjectionParameters::boundingBox (The bounding box of the scene in world space coordinates)
    virtual void projectionParameters(AnimationTime time, ViewProjectionParameters& projParams) const override;

    /// \brief Returns whether this camera uses a perspective projection.
    virtual bool isPerspectiveCamera() const override { return isPerspective(); }

    /// \brief Returns the field of view of the camera.
    virtual FloatType fieldOfView(AnimationTime time, TimeInterval& validityInterval) const override {
        return isPerspective() ? fov() : zoom();
    }

private:

    /// Determines if this camera uses a perspective projection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, isPerspective, setIsPerspective);

    /// Field of view of the camera if it uses a perspective projection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, fov, setFov);

    /// Field of view of the camera if it uses an orthogonal projection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, zoom, setZoom);
};

/**
 * \brief A visual element for rendering camera objects in the interactive viewports.
 */
class OVITO_STDOBJ_EXPORT CameraVis : public DataVis
{
    OVITO_CLASS(CameraVis)
    Q_CLASSINFO("DisplayName", "Camera icon");

public:

    /// \brief Constructor.
    Q_INVOKABLE CameraVis(ObjectCreationParams params) : DataVis(params) {}

    /// \brief Lets the vis element render a camera object.
    virtual PipelineStatus render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode) override;

    /// \brief Computes the bounding box of the object.
    virtual Box3 boundingBox(AnimationTime time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval) override;

private:

    /// The cached geometry data of the 3d camera icon.
    ConstDataBufferPtr _cameraIconVertices;
};

}   // End of namespace
