////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "KeyframeController.h"

namespace Ovito {

/**
 * \brief Base template class for animation keys used by Tension-Continuity-Bias interpolation controllers.
 */
template<class BaseKeyClass>
class OVITO_CORE_EXPORT TCBAnimationKey : public BaseKeyClass
{
    OVITO_CLASS_TEMPLATE(TCBAnimationKey, BaseKeyClass)

public:

    using typename BaseKeyClass::value_type;

    /// Constructor.
    TCBAnimationKey(ObjectInitializationFlags flags, AnimationTime time, const value_type& value)
        : BaseKeyClass(flags, time, value), _easeTo(0), _easeFrom(0), _tension(0), _continuity(0), _bias(0) {}

public:

    /// Slows the velocity of the animation curve as it approaches the key.
    DECLARE_PROPERTY_FIELD(FloatType, easeTo);

    /// Slows the velocity of the animation curve as it leaves the key.
    DECLARE_PROPERTY_FIELD(FloatType, easeFrom);

    /// Controls the amount of curvature in the animation curve.
    DECLARE_PROPERTY_FIELD(FloatType, tension);

    /// Controls the tangential property of the curve at the key.
    DECLARE_PROPERTY_FIELD(FloatType, continuity);

    /// Controls where the animation curve occurs with respect to the key.
    DECLARE_PROPERTY_FIELD(FloatType, bias);
};

/**
 * \brief Animation key class for TCB interpolation of float values.
 */
class OVITO_CORE_EXPORT FloatTCBAnimationKey : public TCBAnimationKey<FloatAnimationKey>
{
    OVITO_CLASS(FloatTCBAnimationKey)

public:

    /// Constructor.
    Q_INVOKABLE FloatTCBAnimationKey(ObjectInitializationFlags flags, AnimationTime time = AnimationTime(0), FloatType value = 0)
        : TCBAnimationKey<FloatAnimationKey>(flags, time, value) {}
};

/**
 * \brief Animation key class for TCB interpolation of position values.
 */
class OVITO_CORE_EXPORT PositionTCBAnimationKey : public TCBAnimationKey<PositionAnimationKey>
{
    OVITO_CLASS(PositionTCBAnimationKey)

public:

    /// Constructor.
    Q_INVOKABLE PositionTCBAnimationKey(ObjectInitializationFlags flags, AnimationTime time = AnimationTime(0), const Vector3& value = Vector3::Zero())
        : TCBAnimationKey<PositionAnimationKey>(flags, time, value) {}
};

/**
 * \brief Implementation of the key interpolator concept that performs TCB interpolation.
 *
 * This class is used with the TCB interpolation controllers.
 */
template<typename KeyType>
struct TCBKeyInterpolator {
    typename KeyType::value_type operator()(AnimationTime time, KeyType* key0, KeyType* key1, KeyType* key2, KeyType* key3) const {
        OVITO_ASSERT(key2->time() > key1->time());
        FloatType t = (FloatType)(time - key1->time()) / (key2->time() - key1->time());
        typename KeyType::tangent_type chord01 = key0 ? (key1->value() - key0->value()) : typename KeyType::nullvalue_type();
        typename KeyType::tangent_type chord12 = key2->value() - key1->value();
        typename KeyType::tangent_type chord23 = key3 ? (key3->value() - key2->value()) : typename KeyType::nullvalue_type();
        typename KeyType::tangent_type outTangent1 = ((FloatType(1) - key1->tension()) * (FloatType(1) + key1->continuity()) * (FloatType(1) + key1->bias()) / 2) * chord01 + ((FloatType(1) - key1->tension()) * (FloatType(1) - key1->continuity()) * (FloatType(1) - key1->bias()) / 2) * chord12;
        typename KeyType::tangent_type inTangent2 = ((FloatType(1) - key2->tension()) * (FloatType(1) - key2->continuity()) * (FloatType(1) + key2->bias()) / 2) * chord12 + ((FloatType(1) - key2->tension()) * (FloatType(1) + key2->continuity()) * (FloatType(1) - key2->bias()) / 2) * chord23;
        typename KeyType::value_type outPoint1 = key1->value() + outTangent1;
        typename KeyType::value_type inPoint2 = key2->value() - inTangent2;
        SplineValueInterpolator<typename KeyType::value_type> valueInterpolator;
        return valueInterpolator(t, key1->value(), key2->value(), outPoint1, inPoint2);
    }
};

/**
 * \brief Base class for TCB interpolation controllers.
 */
template<class KeyType, Controller::ControllerType ctrlType>
class TCBControllerBase : public KeyframeControllerTemplate<KeyType, TCBKeyInterpolator<KeyType>, ctrlType>
{
public:

    /// Constructor.
    TCBControllerBase(ObjectInitializationFlags flags)
        : KeyframeControllerTemplate<KeyType, TCBKeyInterpolator<KeyType>, ctrlType>(flags) {}
};

/**
 * \brief A keyframe controller that interpolates between position values using the TCB interpolation scheme.
 */
class OVITO_CORE_EXPORT TCBPositionController
    : public TCBControllerBase<PositionTCBAnimationKey, Controller::ControllerTypePosition>
{
    OVITO_CLASS(TCBPositionController)

public:

    /// Constructor.
    Q_INVOKABLE TCBPositionController(ObjectInitializationFlags flags)
        : TCBControllerBase<PositionTCBAnimationKey, Controller::ControllerTypePosition>(flags) {}

    /// \brief Gets the controller's value at a certain animation time.
    virtual void getPositionValue(AnimationTime time, Vector3& value, TimeInterval& validityInterval) override {
        getInterpolatedValue(time, value, validityInterval);
    }

    /// \brief Sets the controller's value at the given animation time.
    virtual void setPositionValue(AnimationTime time, const Vector3& newValue, bool isAbsolute) override {
        if(isAbsolute)
            setAbsoluteValue(time, newValue);
        else
            setRelativeValue(time, newValue);
    }
};

}   // End of namespace
