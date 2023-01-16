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

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/controller/ConstantControllers.h>
#include <ovito/core/dataset/animation/controller/LinearInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/SplineInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/TCBInterpolationControllers.h>
#include <ovito/core/dataset/animation/controller/PRSTransformationController.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(Controller);

/******************************************************************************
* Creates a new float controller.
******************************************************************************/
OORef<Controller> ControllerManager::createFloatController()
{
    return OORef<LinearFloatController>::create();
}

/******************************************************************************
* Creates a new integer controller.
******************************************************************************/
OORef<Controller> ControllerManager::createIntController()
{
    return OORef<LinearIntegerController>::create();
}

/******************************************************************************
* Creates a new Vector3 controller.
******************************************************************************/
OORef<Controller> ControllerManager::createVector3Controller()
{
    return OORef<LinearVectorController>::create();
}

/******************************************************************************
* Creates a new position controller.
******************************************************************************/
OORef<Controller> ControllerManager::createPositionController()
{
    return OORef<SplinePositionController>::create();
}

/******************************************************************************
* Creates a new rotation controller.
******************************************************************************/
OORef<Controller> ControllerManager::createRotationController()
{
    return OORef<LinearRotationController>::create();
}

/******************************************************************************
* Creates a new scaling controller.
******************************************************************************/
OORef<Controller> ControllerManager::createScalingController()
{
    return OORef<LinearScalingController>::create();
}

/******************************************************************************
* Creates a new transformation controller.
******************************************************************************/
OORef<Controller> ControllerManager::createTransformationController()
{
    return OORef<PRSTransformationController>::create();
}

/******************************************************************************
* Queries whether the user has activated auto-key mode and controllers should automatically
* generate new animation keys whenever their current value is changed by the user.
******************************************************************************/
bool ControllerManager::isAutoGenerateAnimationKeysEnabled()
{
    const ExecutionContext& context = ExecutionContext::current();
    if(context.isValid()) {
        return context.ui().isAutoGenerateAnimationKeysEnabled();
    }
    return false;   
}

}   // End of namespace
