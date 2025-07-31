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
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "Controller.h"
#include "AnimationKeys.h"

namespace Ovito {

/**
 * \brief Base class for animation controllers that are based on animation keys.
 */
class OVITO_CORE_EXPORT KeyframeController : public Controller
{
    OVITO_CLASS(KeyframeController)

public:

    /// \brief Returns whether the value of this controller is changing over time.
    virtual bool isAnimated() const override { return keys().size() >= 2; }

    /// Maps all keys from the old animation interval to the new interval.
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

    /// Calculates the largest time interval containing the given time during which the controller's value does not change.
    virtual TimeInterval validityInterval(AnimationTime time) override;

    /// Determines whether the animation keys of this controller are sorted with respect to time.
    bool areKeysSorted() const;

    /// Moves the keys in the given set by the given time shift.
    void moveKeys(const QVector<OORef<AnimationKey>>& keysToMove, AnimationTime::value_type shift);

    /// Deletes the given set of keys from the controller.
    void deleteKeys(const QVector<OORef<AnimationKey>>& keysToDelete);

    /// Creates am animation key at the given time.
    /// Returns the index of the key, which may be an existing key.
    virtual int createKey(AnimationTime time) = 0;

protected:

    /// Inserts a new animation key into this controller's list of keys.
    int insertKey(OORef<AnimationKey> key, int insertionPos = -1);

    /// This updates the keys after their times or values have changed.
    virtual void updateKeys() {}

    /// Changes a key's value.
    /// This is a wrapper for the protected function TypedAnimationKey::setValue().
    template<typename KeyType>
    static void setKeyValueInternal(const OORef<KeyType>& key, const typename KeyType::value_type& newValue) { key->setValue(newValue); }

private:

    /// Stores the list of animation keys.
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<AnimationKey>, keys, PROPERTY_FIELD_ALWAYS_CLONE | PROPERTY_FIELD_NO_SUB_ANIM);
};

/**
 * \brief Base class for animation controllers that are based on animation keys.
 */
template<class KeyType, typename KeyInterpolator, Controller::ControllerType ctrlType>
class KeyframeControllerTemplate : public KeyframeController
{
public:

    /// The value type stored by this controller.
    typedef typename KeyType::value_type value_type;

    /// The type used to construct default key values.
    typedef typename KeyType::nullvalue_type nullvalue_type;

    /// Returns the value type of the controller.
    virtual ControllerType controllerType() const override { return ctrlType; }

    /// Returns the list of keys of this animation controller.
    const QVector<OORef<KeyType>>& typedKeys() const { return reinterpret_cast<const QVector<OORef<KeyType>>&>(keys()); }

    /// Creates am animation key at the given time.
    /// Returns the index of the key, which may be an existing key.
    virtual int createKey(AnimationTime time) override {
        OVITO_ASSERT(areKeysSorted());
        // Look for existing key.
        int index;
        for(index = 0; index < this->keys().size(); index++) {
            if(this->keys()[index]->time() == time) {
                return index;
            }
            else if(this->keys()[index]->time() > time) {
                break;
            }
        }
        // Insert a new key.
        TimeInterval iv;
        value_type currentValue;
        getInterpolatedValue(time, currentValue, iv);
        insertKey(OORef<KeyType>::create(time, currentValue), index);
        OVITO_ASSERT(areKeysSorted());
        return index;
    }

protected:

    /// Queries the controller for its value at a certain time.
    void getInterpolatedValue(AnimationTime time, value_type& result, TimeInterval& validityInterval) const {
        const auto& keys = typedKeys();
        if(keys.empty()) {
            result = nullvalue_type();
            return;
        }
        OVITO_ASSERT(areKeysSorted());

        // Handle out of range cases.
        if(time <= keys.front()->time()) {
            result = keys.front()->value();
            if(keys.size() != 1)
                validityInterval.intersect(TimeInterval(AnimationTime::negativeInfinity(), keys.front()->time()));
        }
        else if(time >= keys.back()->time()) {
            result = keys.back()->value();
            if(keys.size() != 1)
                validityInterval.intersect(TimeInterval(keys.back()->time(), AnimationTime::positiveInfinity()));
        }
        else {
            // Intersect validity interval.
            validityInterval.intersect(TimeInterval(time));

            for(auto key = keys.begin() + 1; key != keys.end(); ++key) {
                if((*key)->time() == time) {
                    // No interpolation necessary.
                    result = (*key)->value();
                    return;
                }
                else if((*key)->time() > time) {
                    // Interpolate between two keys.
                    result = KeyInterpolator()(time,
                            (key != (keys.begin() + 1)) ? *(key - 2) : nullptr,
                            *(key - 1),
                            *key,
                            (key != (keys.end() - 1)) ? *(key + 1) : nullptr);
                    return;
                }
            }

            // This should never happen.
            OVITO_ASSERT_MSG(false, "KeyframeControllerTemplate::getInterpolatedValue", "Invalid controller keys.");
            result = nullvalue_type();
        }
    }

    /// Creates a new animation key at the specified time or replaces the value of an existing key.
    void setKeyValue(AnimationTime time, const value_type& newValue) {
        const auto& keys = typedKeys();
        int index;
        for(index = 0; index < keys.size(); index++) {
            if(keys[index]->time() == time) {
                setKeyValueInternal(keys[index], newValue);
                return;
            }
            else if(keys[index]->time() > time) {
                break;
            }
        }
        insertKey(OORef<KeyType>::create(time, newValue), index);
    }

    /// Sets the controller's value at the specified time.
    void setAbsoluteValue(AnimationTime time, const value_type& newValue) {
        if(keys().empty()) {
            // Create an additional key at time 0 if the controller doesn't have any keys yet.
            if(time != AnimationTime(0) && ControllerManager::isAutoGenerateAnimationKeysEnabled() && newValue != nullvalue_type()) {
                insertKey(OORef<KeyType>::create(), 0);
                insertKey(OORef<KeyType>::create(time, newValue), time > AnimationTime(0) ? 1 : 0);
            }
            else {
                insertKey(OORef<KeyType>::create(AnimationTime(0), newValue), 0);
            }
        }
        else if(!ControllerManager::isAutoGenerateAnimationKeysEnabled()) {
            if(keys().size() == 1) {
                setKeyValueInternal(typedKeys().front(), newValue);
            }
            else {
                value_type deltaValue(newValue);
                value_type oldValue;
                // Get delta from new absolute value.
                TimeInterval iv;
                getInterpolatedValue(time, oldValue, iv);
                if(newValue == oldValue) return;
                deltaValue -= oldValue;
                // Apply delta value to all keys.
                for(const auto& key : typedKeys()) {
                    value_type v = key->value();
                    v += deltaValue;
                    setKeyValueInternal(key, v);
                }
            }
        }
        else {
            setKeyValue(time, newValue);
        }
        updateKeys();
    }

    /// Changes the controller's value at the specified time.
    void setRelativeValue(AnimationTime time, const value_type& deltaValue) {
        if(deltaValue == nullvalue_type())
            return;
        if(keys().empty()) {
            // Create an additional key at time 0 if the controller doesn't have any keys yet.
            if(time != AnimationTime(0) && ControllerManager::isAutoGenerateAnimationKeysEnabled()) {
                insertKey(OORef<KeyType>::create(), 0);
                insertKey(OORef<KeyType>::create(time, deltaValue), time > AnimationTime(0) ? 1 : 0);
            }
            else {
                insertKey(OORef<KeyType>::create(AnimationTime(0), deltaValue), 0);
            }
        }
        else if(!ControllerManager::isAutoGenerateAnimationKeysEnabled()) {
            // Apply delta value to all keys.
            for(const auto& key : typedKeys()) {
                value_type v = key->value();
                v += deltaValue;
                setKeyValueInternal(key, v);
            }
        }
        else {
            TimeInterval iv;
            value_type oldValue;
            getInterpolatedValue(time, oldValue, iv);
            oldValue += deltaValue;
            setKeyValue(time, oldValue);
        }
        updateKeys();
    }
};

}   // End of namespace
