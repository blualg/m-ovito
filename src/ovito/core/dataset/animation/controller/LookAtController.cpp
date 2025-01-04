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

#include <ovito/core/Core.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/animation/controller/LookAtController.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/DataSet.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LookAtController);
DEFINE_REFERENCE_FIELD(LookAtController, rollController);
DEFINE_REFERENCE_FIELD(LookAtController, targetNode);
SET_PROPERTY_FIELD_LABEL(LookAtController, rollController, "Roll");
SET_PROPERTY_FIELD_LABEL(LookAtController, targetNode, "Target");
SET_PROPERTY_FIELD_UNITS(LookAtController, rollController, AngleParameterUnit);

/******************************************************************************
* Constructor.
******************************************************************************/
void LookAtController::initializeObject(ObjectInitializationFlags flags)
{
    Controller::initializeObject(flags);

    if(!flags.testFlag(DontInitializeObject)) {
        // Create sub-controller.
        setRollController(ControllerManager::createFloatController());
    }
}

/******************************************************************************
* Queries the controller for its absolute value at a certain time.
******************************************************************************/
void LookAtController::getRotationValue(AnimationTime time, Rotation& result, TimeInterval& validityInterval)
{
    // Get position of target node.
    Vector3 targetPos = Vector3::Zero();
    if(targetNode()) {
        const AffineTransformation& targetTM = targetNode()->getWorldTransform(time, validityInterval);
        targetPos = targetTM.translation();
    }

    if(!_sourcePosValidity.isEmpty())
        validityInterval.intersect(_sourcePosValidity);
    else
        validityInterval.intersect(TimeInterval(time));

    // Get rolling angle.
    FloatType rollAngle = 0.0;
    if(rollController())
        rollAngle = rollController()->getFloatValue(time, validityInterval);

    if(targetPos == _sourcePos) {
        result.setIdentity();
        return;
    }

    AffineTransformation tm = AffineTransformation::lookAt(Point3::Origin() + _sourcePos, Point3::Origin() + targetPos, Vector3(0,0,1));
    tm.translation() = Vector3::Zero();
    result = Rotation(tm).inverse();

    if(rollAngle != 0.0)
        result = result * Rotation(Vector3(0,0,1), rollAngle);

    // Reset source validity.
    _sourcePosValidity.setEmpty();
}

/******************************************************************************
* Sets the controller's value at the specified time.
******************************************************************************/
void LookAtController::setRotationValue(AnimationTime time, const Rotation& newValue, bool isAbsoluteValue)
{
    // Cannot set value for this controller type.
}

/******************************************************************************
* Lets a rotation controller apply its value to an existing transformation matrix.
******************************************************************************/
void LookAtController::applyRotation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval)
{
    // Save source position for later use.
    _sourcePos = result.translation();
    _sourcePosValidity = validityInterval;

    Controller::applyRotation(time, result, validityInterval);
}

/******************************************************************************
* Computes the largest time interval containing the given time during which the
* controller's value is constant.
******************************************************************************/
TimeInterval LookAtController::validityInterval(AnimationTime time)
{
    TimeInterval iv = TimeInterval::infinite();
    if(rollController())
        iv.intersect(rollController()->validityInterval(time));
    if(targetNode())
        targetNode()->getWorldTransform(time, iv);
    return iv;
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.11.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr LookAtController::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // For backward compatibility with OVITO 3.11:
    // The Pipeline class has been split from the SceneNode base class in OVITO 3.12. This means we have to handle
    // the deserialization of the targetNode field here, which used to be a Pipeline object, now a pure SceneNode.
    if(field.definingClass == &LookAtController::OOClass() && stream.formatVersion() < 30013) {
        if(field.identifier == "targetNode") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                OORef<RefTarget> node = stream.loadObject<RefTarget>();
                if(OORef<Pipeline> pipeline = dynamic_object_cast<Pipeline>(node))
                    node = pipeline->deserializationSceneNode();
                static_object_cast<LookAtController>(&owner)->_targetNode.set(&owner, PROPERTY_FIELD(targetNode), static_object_cast<SceneNode>(std::move(node)));
                stream.closeChunk();
            };
        }
    }
    return Controller::OOMetaClass::overrideFieldDeserialization(stream, field);
}

}   // End of namespace
