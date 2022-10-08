////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/data/DataBufferAccess.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/Application.h>
#include "StandardCameraObject.h"

namespace Ovito::StdObj {

IMPLEMENT_OVITO_CLASS(StandardCameraObject);
DEFINE_PROPERTY_FIELD(StandardCameraObject, isPerspective);
DEFINE_PROPERTY_FIELD(StandardCameraObject, fov);
DEFINE_PROPERTY_FIELD(StandardCameraObject, zoom);
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, isPerspective, "Perspective projection");
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, fov, "FOV angle");
SET_PROPERTY_FIELD_LABEL(StandardCameraObject, zoom, "FOV size");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardCameraObject, fov, AngleParameterUnit, FloatType(1e-3), FLOATTYPE_PI - FloatType(1e-2));
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StandardCameraObject, zoom, WorldParameterUnit, 0);

IMPLEMENT_OVITO_CLASS(CameraVis);

/******************************************************************************
* Constructs a camera object.
******************************************************************************/
StandardCameraObject::StandardCameraObject(ObjectCreationParams params) : AbstractCameraObject(params), 
	_isPerspective(true),
	_fov(FLOATTYPE_PI/4),
	_zoom(200.0)
{
	if(params.createVisElement()) {
		setVisElement(OORef<CameraVis>::create(params));
	}
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a 
* serialized property field that has been removed from the class. 
* This is needed for file backward compatibility with OVITO 3.3.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr StandardCameraObject::OOMetaClass::overrideFieldDeserialization(const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // The CameraObject used to manage animation controllers for FOV and Zoom parameters in OVITO 3.3. and earlier.
    if(field.identifier == "fovController" && field.definingClass == &StandardCameraObject::OOClass()) {
        return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
            OVITO_ASSERT(field.isReferenceField);
			stream.expectChunk(0x02);
            OORef<Controller> controller = stream.loadObject<Controller>();
            stream.closeChunk();
			// Need to wait until the animation keys of the controller have been completely loaded.
			// Only then it is safe to query the controller for its value.
			QObject::connect(controller.get(), &Controller::controllerLoadingCompleted, &owner, [camera = static_cast<StandardCameraObject*>(&owner), controller]() {
	            camera->setFov(controller->currentFloatValue());
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
			QObject::connect(controller.get(), &Controller::controllerLoadingCompleted, &owner, [camera = static_cast<StandardCameraObject*>(&owner), controller]() {
	            camera->setZoom(controller->currentFloatValue());
			});
        };
    }
    return nullptr;
}

/******************************************************************************
* Fills in the missing fields of the camera view descriptor structure.
******************************************************************************/
void StandardCameraObject::projectionParameters(TimePoint time, ViewProjectionParameters& params) const
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
FloatType StandardCameraObject::getTargetDistance(TimePoint time, const PipelineSceneNode* node)
{
	if(node && node->lookatTargetNode() != nullptr) {
		TimeInterval iv;
		Vector3 cameraPos = node->getWorldTransform(time, iv).translation();
		Vector3 targetPos = node->lookatTargetNode()->getWorldTransform(time, iv).translation();
		return (cameraPos - targetPos).length();
	}

	// That's the fixed target distance of a free camera:
	return 50.0;
}

/******************************************************************************
* Lets the vis element render a camera object.
******************************************************************************/
PipelineStatus CameraVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	// Camera objects are only visible in the interactive viewports.
	if(renderer->isInteractive() == false || renderer->viewport() == nullptr)
		return {};

	TimeInterval iv;

	// Determine the camera and target positions when rendering a target camera.
	FloatType targetDistance = 0;
	bool showTargetLine = false;
	if(contextNode->lookatTargetNode()) {
		Vector3 cameraPos = contextNode->getWorldTransform(time, iv).translation();
		Vector3 targetPos = contextNode->lookatTargetNode()->getWorldTransform(time, iv).translation();
		targetDistance = (cameraPos - targetPos).length();
		showTargetLine = true;
	}

	// Determine the aspect ratio and angle of the camera cone.
	FloatType aspectRatio = 0;
	FloatType coneAngle = 0;
	if(contextNode->isSelected()) {
		if(RenderSettings* renderSettings = dataset()->renderSettings())
			aspectRatio = renderSettings->outputImageAspectRatio();
		if(const StandardCameraObject* camera = path.lastAs<StandardCameraObject>()) {
			if(camera->isPerspective()) {
				coneAngle = camera->fieldOfView(time, iv);
				if(targetDistance == 0)
					targetDistance = StandardCameraObject::getTargetDistance(time, contextNode);
			}
		}
	}

	if(!renderer->isBoundingBoxPass()) {
		if(!renderer->isPicking()) {
			// The key type used for caching the geometry primitive:
			using CacheKey = RendererResourceKey<struct CameraCone,
				FloatType,					// Camera target distance
				bool,						// Target line visible
				FloatType,					// Cone aspect ratio
				FloatType					// Cone angle
			>;

			// Lookup the rendering primitive in the vis cache.
			auto& conePrimitive = dataset()->visCache().get<LinePrimitive>(CacheKey(
					targetDistance,
					showTargetLine,
					aspectRatio,
					coneAngle));

			// Check if we already have a valid rendering primitive that is up to date.
			if(!conePrimitive.positions()) {
				DataBufferAccessAndRef<Point3> targetLineVertices = DataBufferPtr::create(renderer->dataset(), 0, DataBuffer::Float, 3);
				if(targetDistance != 0) {
					if(showTargetLine) {
						targetLineVertices.push_back(Point3::Origin());
						targetLineVertices.push_back(Point3(0,0,-targetDistance));
					}
					if(aspectRatio != 0 && coneAngle != 0) {
						FloatType sizeY = tan(FloatType(0.5) * coneAngle) * targetDistance;
						FloatType sizeX = sizeY / aspectRatio;
						targetLineVertices.push_back(Point3::Origin());
						targetLineVertices.push_back(Point3(sizeX, sizeY, -targetDistance));
						targetLineVertices.push_back(Point3::Origin());
						targetLineVertices.push_back(Point3(-sizeX, sizeY, -targetDistance));
						targetLineVertices.push_back(Point3::Origin());
						targetLineVertices.push_back(Point3(-sizeX, -sizeY, -targetDistance));
						targetLineVertices.push_back(Point3::Origin());
						targetLineVertices.push_back(Point3(sizeX, -sizeY, -targetDistance));

						targetLineVertices.push_back(Point3(sizeX, sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(-sizeX, sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(-sizeX, sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(-sizeX, -sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(-sizeX, -sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(sizeX, -sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(sizeX, -sizeY, -targetDistance));
						targetLineVertices.push_back(Point3(sizeX, sizeY, -targetDistance));
					}
				}
				conePrimitive.setPositions(targetLineVertices.take());
			}
			conePrimitive.setUniformColor(ViewportSettings::getSettings().viewportColor(ViewportSettings::COLOR_CAMERAS));
			renderer->renderLines(conePrimitive);
		}
	}
	else {
		// Add camera view cone to bounding box.
		if(showTargetLine) {
			renderer->addToLocalBoundingBox(Point3::Origin());
			renderer->addToLocalBoundingBox(Point3(0,0,-targetDistance));
		}
		if(aspectRatio != 0 && coneAngle != 0) {
			FloatType sizeY = tan(FloatType(0.5) * coneAngle) * targetDistance;
			FloatType sizeX = sizeY / aspectRatio;
			renderer->addToLocalBoundingBox(Point3(sizeX, sizeY, -targetDistance));
			renderer->addToLocalBoundingBox(Point3(-sizeX, sizeY, -targetDistance));
			renderer->addToLocalBoundingBox(Point3(-sizeX, -sizeY, -targetDistance));
			renderer->addToLocalBoundingBox(Point3(sizeX, -sizeY, -targetDistance));
		}
	}

	// Setup transformation matrix to always show the camera icon at the same size.
	Point3 cameraPos = Point3::Origin() + renderer->worldTransform().translation();
	FloatType scaling = FloatType(0.3) * renderer->viewport()->nonScalingSize(cameraPos);
	renderer->setWorldTransform(renderer->worldTransform() * AffineTransformation::scaling(scaling));

	if(!renderer->isBoundingBoxPass()) {

		// Load 3d camera icon.
		if(!_cameraIconVertices) {
			DataBufferAccessAndRef<Point3> lines = DataBufferPtr::create(renderer->dataset(), 0, DataBuffer::Float, 3);
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
			std::vector<Point3> vertices(numVertices);
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

		LinePrimitive cameraPrimitives;
		cameraPrimitives.setPositions(_cameraIconVertices);
		cameraPrimitives.setUniformColor(ViewportSettings::getSettings().viewportColor(contextNode->isSelected() ? ViewportSettings::COLOR_SELECTION : ViewportSettings::COLOR_CAMERAS));
		if(renderer->isPicking())
			cameraPrimitives.setLineWidth(renderer->defaultLinePickingWidth());

		renderer->beginPickObject(contextNode);
		renderer->renderLines(cameraPrimitives);
		renderer->endPickObject();
	}
	else {
		// Add camera symbol to bounding box.
		renderer->addToLocalBoundingBox(Box3(Point3::Origin(), scaling * 2));
	}

	return {};
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 CameraVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
	// This is not a physical object. It doesn't have a size.
	return Box3(Point3::Origin(), Point3::Origin());
}

}	// End of namespace
