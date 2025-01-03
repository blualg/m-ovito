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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/camera/TargetObject.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/Application.h>
#include "StandardCameraObject.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(StandardCameraObject);
OVITO_CLASSINFO(StandardCameraObject, "DisplayName", "Camera");
OVITO_CLASSINFO(StandardCameraObject, "ClassNameAlias", "CameraObject");  // For backward compatibility with OVITO 3.3.
DEFINE_PROPERTY_FIELD(StandardCameraObject, isPerspective);
DEFINE_PROPERTY_FIELD(StandardCameraObject, fov);
DEFINE_PROPERTY_FIELD(StandardCameraObject, zoom);
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, isPerspective, "Perspective projection");
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, fov, "FOV angle");
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, zoom, "FOV size");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardCameraObject, fov, AngleParameterUnit, FloatType(1e-3), FLOATTYPE_PI - FloatType(1e-2));
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StandardCameraObject, zoom, WorldParameterUnit, 0);

IMPLEMENT_CREATABLE_OVITO_CLASS(CameraVis);
OVITO_CLASSINFO(CameraVis, "DisplayName", "Camera icon");

/******************************************************************************
* Constructor.
******************************************************************************/
void StandardCameraObject::initializeObject(ObjectInitializationFlags flags)
{
    AbstractCameraObject::initializeObject(flags);

    if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement))) {
        setVisElement(OORef<CameraVis>::create(flags));
    }
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.3.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr StandardCameraObject::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // The CameraObject used to manage animation controllers for FOV and Zoom parameters in OVITO 3.3 and earlier.
    if(field.identifier == "fovController" && field.definingClass == &StandardCameraObject::OOClass()) {
        return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
            OVITO_ASSERT(field.isReferenceField);
            stream.expectChunk(0x02);
            OORef<Controller> controller = stream.loadObject<Controller>();
            stream.closeChunk();
            // Need to wait until the animation keys of the controller have been completely loaded.
            // Only then it is safe to query the controller for its value.
            stream.registerPostLoadCallback([camera = static_cast<StandardCameraObject*>(&owner), controller = std::move(controller)]() {
                camera->setFov(controller->getFloatValue(AnimationTime(0)));
            });
        };
    }
    else if(field.identifier == "zoomController" && field.definingClass == &StandardCameraObject::OOClass()) {
        return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
            OVITO_ASSERT(field.isReferenceField);
            stream.expectChunk(0x02);
            OORef<Controller> controller = stream.loadObject<Controller>();
            stream.closeChunk();
            // Need to wait until the animation keys of the controller have been completely loaded.
            // Only then it is safe to query the controller for its value.
            stream.registerPostLoadCallback([camera = static_cast<StandardCameraObject*>(&owner), controller = std::move(controller)]() {
                camera->setZoom(controller->getFloatValue(AnimationTime(0)));
            });
        };
    }
    return AbstractCameraObject::OOMetaClass::overrideFieldDeserialization(stream, field);
}

/******************************************************************************
* Fills in the missing fields of the camera view descriptor structure.
******************************************************************************/
void StandardCameraObject::projectionParameters(AnimationTime time, ViewProjectionParameters& params) const
{
    // Transform scene bounding box to camera space.
    Box3 bb = params.boundingBox.transformed(params.viewMatrix).centerScale(FloatType(1.01));

    // Compute projection matrix.
    params.isPerspective = isPerspective();
    if(params.isPerspective) {
        if(bb.minc.z() < -FLOATTYPE_EPSILON) {
            params.zfar = -bb.minc.z();
            params.znear = std::max(-bb.maxc.z(), params.zfar * FloatType(1e-4));
        }
        else {
            params.zfar = std::max(params.boundingBox.size().length(), FloatType(1));
            params.znear = params.zfar * FloatType(1e-4);
        }
        params.zfar = std::max(params.zfar, params.znear * FloatType(1.01));

        // Get the camera angle.
        params.fieldOfView = qBound(FLOATTYPE_EPSILON, fov(), FLOATTYPE_PI - FLOATTYPE_EPSILON);

        params.projectionMatrix = Matrix4::perspective(params.fieldOfView, FloatType(1) / params.aspectRatio, params.znear, params.zfar);
    }
    else {
        if(!bb.isEmpty()) {
            params.znear = -bb.maxc.z();
            params.zfar  = std::max(-bb.minc.z(), params.znear + FloatType(1));
        }
        else {
            params.znear = 1;
            params.zfar = 100;
        }

        // Get the camera zoom.
        params.fieldOfView = qMax(FLOATTYPE_EPSILON, zoom());

        params.projectionMatrix = Matrix4::ortho(-params.fieldOfView / params.aspectRatio, params.fieldOfView / params.aspectRatio,
                            -params.fieldOfView, params.fieldOfView, params.znear, params.zfar);
    }
    params.inverseProjectionMatrix = params.projectionMatrix.inverse();
}

/******************************************************************************
* With a target camera, indicates the distance between the camera and its target.
******************************************************************************/
FloatType StandardCameraObject::getTargetDistance(AnimationTime time, const SceneNode* sceneNode)
{
    if(sceneNode && sceneNode->lookatTargetNode() != nullptr) {
        Vector3 cameraPos = sceneNode->getWorldTransform(time).translation();
        Vector3 targetPos = sceneNode->lookatTargetNode()->getWorldTransform(time).translation();
        return (cameraPos - targetPos).length();
    }

    // That's the fixed target distance of a free camera:
    return 50.0;
}

/******************************************************************************
* Lets the vis element render a camera object.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> CameraVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode)
{
    // Camera objects are only visible in the interactive viewports.
    if(frameGraph.isInteractive() == false)
        return {};

    // Determine the camera and target positions when rendering a target camera.
    const AffineTransformation cameraTM = sceneNode->getWorldTransform(frameGraph.time());
    Point3 cameraPos = Point3::Origin() + cameraTM.translation();
    FloatType targetDistance = 0;
    bool showTargetLine = false;
    if(sceneNode->lookatTargetNode()) {
        Point3 targetPos = Point3::Origin() + sceneNode->lookatTargetNode()->getWorldTransform(frameGraph.time()).translation();
        targetDistance = (cameraPos - targetPos).length();
        showTargetLine = true;
    }

    // Determine the aspect ratio and angle of the camera cone.
    FloatType aspectRatio = 0;
    FloatType coneAngle = 0;
    if(sceneNode->isSelected()) {
        DataSet* dataset = this_task::ui()->datasetContainer().currentSet();
        if(dataset && dataset->renderSettings()) {
            aspectRatio = dataset->renderSettings()->outputImageAspectRatio();
            if(const StandardCameraObject* camera = path.lastAs<StandardCameraObject>()) {
                if(camera->isPerspective()) {
                    TimeInterval iv;
                    coneAngle = camera->fieldOfView(frameGraph.time(), iv);
                    if(targetDistance == 0)
                        targetDistance = StandardCameraObject::getTargetDistance(frameGraph.time(), sceneNode);
                }
            }
        }
    }

    // Render camera code.

    // The key type used for caching the geometry primitive:
    using CacheKey = RendererResourceKey<struct CameraCone,
        FloatType,                  // Camera target distance
        bool,                       // Target line visible
        FloatType,                  // Cone aspect ratio
        FloatType                   // Cone angle
    >;

    // Lookup the rendering primitive in the vis cache.
    const LinePrimitive& conePrimitive = frameGraph.visCache().lookup<LinePrimitive>(CacheKey(
            targetDistance,
            showTargetLine,
            aspectRatio,
            coneAngle),
        [&](LinePrimitive& conePrimitive) {
            BufferFactory<Point3G> targetLineVertices(0);
            if(targetDistance != 0) {
                if(showTargetLine) {
                    targetLineVertices.push_back(Point3G::Origin());
                    targetLineVertices.push_back(Point3G(0,0,-targetDistance));
                }
                if(aspectRatio != 0 && coneAngle != 0) {
                    GraphicsFloatType sizeY = std::tan(GraphicsFloatType(0.5) * coneAngle) * targetDistance;
                    GraphicsFloatType sizeX = sizeY / aspectRatio;
                    targetLineVertices.push_back(Point3G::Origin());
                    targetLineVertices.push_back(Point3G(sizeX, sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G::Origin());
                    targetLineVertices.push_back(Point3G(-sizeX, sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G::Origin());
                    targetLineVertices.push_back(Point3G(-sizeX, -sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G::Origin());
                    targetLineVertices.push_back(Point3G(sizeX, -sizeY, -targetDistance));

                    targetLineVertices.push_back(Point3G(sizeX, sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(-sizeX, sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(-sizeX, sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(-sizeX, -sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(-sizeX, -sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(sizeX, -sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(sizeX, -sizeY, -targetDistance));
                    targetLineVertices.push_back(Point3G(sizeX, sizeY, -targetDistance));
                }
            }
            conePrimitive.setPositions(targetLineVertices.take());
        });

    auto coloredConePrimitive = std::make_unique<LinePrimitive>(conePrimitive);
    coloredConePrimitive->setUniformColor(ViewportSettings::getSettings().viewportColor(ViewportSettings::COLOR_CAMERAS));

    FrameGraph::RenderingCommandGroup& commandGroup = frameGraph.addCommandGroup(FrameGraph::SceneLayer);
    frameGraph.addPrimitive(commandGroup, std::move(coloredConePrimitive), sceneNode);

    // Load 3d camera icon.
    if(!_cameraIconVertices) {
        BufferFactory<Point3G> lines(0);
        // Load and parse PLY file that contains the camera icon.
        QFile meshFile(QStringLiteral(":/core/3dicons/camera.ply"));
        meshFile.open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream stream(&meshFile);
        for(int i = 0; i < 3; i++) stream.readLine();
        int numVertices = stream.readLine().section(' ', 2, 2).toInt();
        OVITO_ASSERT(numVertices > 0);
        for(int i = 0; i < 3; i++) stream.readLine();
        int numFaces = stream.readLine().section(' ', 2, 2).toInt();
        for(int i = 0; i < 2; i++) stream.readLine();
        std::vector<Point3G> vertices(numVertices);
        for(int i = 0; i < numVertices; i++)
            stream >> vertices[i].x() >> vertices[i].y() >> vertices[i].z();
        for(int i = 0; i < numFaces; i++) {
            int numEdges, vindex, lastvindex, firstvindex;
            stream >> numEdges;
            for(int j = 0; j < numEdges; j++) {
                stream >> vindex;
                if(j != 0) {
                    lines.push_back(vertices[lastvindex]);
                    lines.push_back(vertices[vindex]);
                }
                else firstvindex = vindex;
                lastvindex = vindex;
            }
            lines.push_back(vertices[lastvindex]);
            lines.push_back(vertices[firstvindex]);
        }
        _cameraIconVertices = lines.take();
    }

    // Always show the camera icon at the same size.
    FloatType scaling = FloatType(0.3) * frameGraph.nonScalingSize(cameraPos);

    std::unique_ptr<LinePrimitive> cameraPrimitive = std::make_unique<LinePrimitive>();
    cameraPrimitive->setPositions(_cameraIconVertices);
    cameraPrimitive->setUniformColor(ViewportSettings::getSettings().viewportColor(sceneNode->isSelected() ? ViewportSettings::COLOR_SELECTION : ViewportSettings::COLOR_CAMERAS));
    commandGroup.addPrimitive(std::move(cameraPrimitive), cameraTM * AffineTransformation::scaling(scaling), Box3(Point3::Origin(), 2), sceneNode);

    return {};
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 CameraVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    // This is not a physical object. It doesn't have a size.
    return Box3(Point3::Origin(), Point3::Origin());
}

}   // End of namespace
