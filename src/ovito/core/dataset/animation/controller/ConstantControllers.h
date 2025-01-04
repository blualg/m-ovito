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


#include <ovito/core/Core.h>
#include "Controller.h"

namespace Ovito {

/**
 * \brief An animation controller with a constant float value.
 */
class OVITO_CORE_EXPORT ConstFloatController : public Controller
{
    OVITO_CLASS(ConstFloatController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeFloat; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual FloatType getFloatValue(AnimationTime time, TimeInterval& validityInterval) override { return value(); }

    /// \brief Sets the controller's value at the given animation time.
    virtual void setFloatValue(AnimationTime time, FloatType newValue) override { setValue(newValue); }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, value, setValue);
};

/**
 * \brief An animation controller with a constant int value.
 */
class OVITO_CORE_EXPORT ConstIntegerController : public Controller
{
    OVITO_CLASS(ConstIntegerController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeInt; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual int getIntValue(AnimationTime time, TimeInterval& validityInterval) override { return value(); }

    /// \brief Sets the controller's value at the given animation time.
    virtual void setIntValue(AnimationTime time, int newValue) override { setValue(newValue); }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, value, setValue);
};

/**
 * \brief An animation controller with a constant Vector3 value.
 */
class OVITO_CORE_EXPORT ConstVectorController : public Controller
{
    OVITO_CLASS(ConstVectorController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeVector3; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual void getVector3Value(AnimationTime time, Vector3& result, TimeInterval& validityInterval) override { result = value(); }

    /// \brief Sets the controller's value at the given animation time.
    virtual void setVector3Value(AnimationTime time, const Vector3& newValue) override { setValue(newValue); }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(Vector3{Vector3::Zero()}, value, setValue);
};

/**
 * \brief An animation controller with a constant position value.
 */
class OVITO_CORE_EXPORT ConstPositionController : public Controller
{
    OVITO_CLASS(ConstPositionController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypePosition; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual void getPositionValue(AnimationTime time, Vector3& result, TimeInterval& validityInterval) override { result = value(); }

    /// \brief Sets a position controller's value at the given animation time.
    virtual void setPositionValue(AnimationTime time, const Vector3& newValue, bool isAbsolute) override {
        setValue(isAbsolute ? newValue : (newValue + value()));
    }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(Vector3{Vector3::Zero()}, value, setValue);
};

/**
 * \brief An animation controller with a constant rotation value.
 */
class OVITO_CORE_EXPORT ConstRotationController : public Controller
{
    OVITO_CLASS(ConstRotationController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeRotation; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual void getRotationValue(AnimationTime time, Rotation& result, TimeInterval& validityInterval) override { result = value(); }

    /// \brief Sets a rotation controller's value at the given animation time.
    virtual void setRotationValue(AnimationTime time, const Rotation& newValue, bool isAbsolute) override {
        setValue(isAbsolute ? newValue : (newValue * value()));
    }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(Rotation{Rotation::Identity()}, value, setValue);
};

/**
 * \brief An animation controller with a constant scaling value.
 */
class OVITO_CORE_EXPORT ConstScalingController : public Controller
{
    OVITO_CLASS(ConstScalingController)

public:

    /// \brief Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ControllerTypeScaling; }

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return false; }

    /// \brief Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override { return TimeInterval::infinite(); }

    /// \brief Gets the controller's value at a certain animation time.
    virtual void getScalingValue(AnimationTime time, Scaling& result, TimeInterval& validityInterval) override { result = value(); }

    /// \brief Sets a scaling controller's value at the given animation time.
    virtual void setScalingValue(AnimationTime time, const Scaling& newValue, bool isAbsolute) override {
        setValue(isAbsolute ? newValue : (newValue * value()));
    }

private:

    /// Stores the constant value of the controller.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(Scaling{Scaling::Identity()}, value, setValue);
};

}   // End of namespace
