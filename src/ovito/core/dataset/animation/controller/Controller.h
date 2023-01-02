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

/**
 * \file Controller.h
 * \brief Contains the definition of the Ovito::Controller class and the Ovito::ControllerManager class.
 */

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * \brief Base class for all animation controllers.
 *
 * Controllers are used to describe animatable parameters of an object. A Controller
 * controls how the object parameter changes with time.
 *
 * Instances of Controller-derived classes can be created using the ControllerManager.
 */
class OVITO_CORE_EXPORT Controller : public RefTarget
{
	OVITO_CLASS(Controller)

public:

	enum ControllerType {
		ControllerTypeFloat,
		ControllerTypeInt,
		ControllerTypeVector3,
		ControllerTypePosition,
		ControllerTypeRotation,
		ControllerTypeScaling,
		ControllerTypeTransformation,
	};
	Q_ENUM(ControllerType);

protected:

	/// Constructor.
	using RefTarget::RefTarget;

	/// This method is called once for this object after it has been completely loaded from a stream.
	virtual void loadFromStreamComplete(ObjectLoadStream& stream) override {
		RefTarget::loadFromStreamComplete(stream);
		
		// Inform dependents that it is now safe to query the controller for its value.
		Q_EMIT controllerLoadingCompleted();
	}

public:

	/// \brief Returns the value type of the controller.
	virtual ControllerType controllerType() const = 0;

	/// \brief Returns whether the value of this controller is changing over time.
	virtual bool isAnimated() const = 0;

	/// \brief Calculates the largest time interval containing the given time during which the
	///        controller's value does not change.
	/// \param[in] time The animation time at which the controller's validity interval is requested.
	/// \return The interval during which the controller's value does not change.
	virtual TimeInterval validityInterval(AnimationTime time) = 0;

	/// \brief Queries a float controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual FloatType getFloatValue(AnimationTime time, TimeInterval& validityInterval) { OVITO_ASSERT_MSG(false, "Controller::getFloatValue()", "This method should be overridden."); return 0; }

	/// \brief Queries a float controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	FloatType getFloatValue(AnimationTime time) { 
		TimeInterval iv;
		return getFloatValue(time, iv);
	}
		
	/// \brief Queries an integer controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual int getIntValue(AnimationTime time, TimeInterval& validityInterval) { OVITO_ASSERT_MSG(false, "Controller::getIntValue()", "This method should be overridden."); return 0; }

	/// \brief Queries an integer controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	int getIntValue(AnimationTime time) { 
		TimeInterval iv;
		return getIntValue(time, iv);
	}

	/// \brief Queries a Vector3 controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[out] result This output variable takes the controller's values.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void getVector3Value(AnimationTime time, Vector3& result, TimeInterval& validityInterval) { result = Vector3::Zero(); OVITO_ASSERT_MSG(false, "Controller::getVector3Value()", "This method should be overridden."); }

	/// \brief Queries a vector controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	Vector3 getVector3Value(AnimationTime time) { 
		TimeInterval iv;
		Vector3 v;
		getVector3Value(time, v, iv);
		return v;
	}

	/// \brief Queries a Vector3 controller's value at a certain animation time as a color.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[out] result This output variable takes the controller's values.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	void getColorValue(AnimationTime time, Color& result, TimeInterval& validityInterval) {
		if constexpr(sizeof(Color) == sizeof(Vector3)) {
			getVector3Value(time, reinterpret_cast<Vector3&>(result), validityInterval);
		}
		else {
			Vector3 v;
			getVector3Value(time, v, validityInterval);
			result = v;
		}
	}

	/// \brief Queries a Vector3 controller's value at a certain animation time as a color.
	/// \param[in] time The animation time at which the controller's value should be computed.
	Color getColorValue(AnimationTime time) { 
		TimeInterval iv;
		Color c;
		getColorValue(time, c, iv);
		return c;
	}

	/// \brief Queries a position controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[out] result This output variable takes the controller's values.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void getPositionValue(AnimationTime time, Vector3& result, TimeInterval& validityInterval) { result = Vector3::Zero(); OVITO_ASSERT_MSG(false, "Controller::getPositionValue()", "This method should be overridden."); }

	/// \brief Queries a rotation controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[out] result This output variable takes the controller's values.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void getRotationValue(AnimationTime time, Rotation& result, TimeInterval& validityInterval) { result = Rotation::Identity(); OVITO_ASSERT_MSG(false, "Controller::getRotationValue()", "This method should be overridden."); }

	/// \brief Queries a scaling controller's value at a certain animation time.
	/// \param[in] time The animation time at which the controller's value should be computed.
	/// \param[out] result This output variable takes the controller's values.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void getScalingValue(AnimationTime time, Scaling& result, TimeInterval& validityInterval) { result = Scaling::Identity(); OVITO_ASSERT_MSG(false, "Controller::getScalingValue()", "This method should be overridden."); }

	/// \brief Lets a position controller apply its value to an existing transformation matrix.
	/// \param[in] time The animation time.
	/// \param[in,out] result The controller will apply its transformation to this matrix.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void applyTranslation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval) {
		Vector3 t;
		getPositionValue(time, t, validityInterval);
		result = result * AffineTransformation::translation(t);
	}

	/// \brief Lets a rotation controller apply its value to an existing transformation matrix.
	/// \param[in] time The animation time.
	/// \param[in,out] result The controller will apply its transformation to this matrix.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void applyRotation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval) {
		Rotation r;
		getRotationValue(time, r, validityInterval);
		result = result * Matrix3::rotation(r);
	}

	/// \brief Lets a scaling controller apply its value to an existing transformation matrix.
	/// \param[in] time The animation time.
	/// \param[in,out] result The controller will apply its transformation to this matrix.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void applyScaling(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval) {
		Scaling s;
		getScalingValue(time, s, validityInterval);
		result = result * Matrix3::scaling(s);
	}

	/// \brief Lets a transformation controller apply its value to an existing transformation matrix.
	/// \param[in] time The animation time.
	/// \param[in,out] result The controller will apply its transformation to this matrix.
	/// \param[in,out] validityInterval This interval is reduced to the period during which the controller's value doesn't change.
	virtual void applyTransformation(AnimationTime time, AffineTransformation& result, TimeInterval& validityInterval) { OVITO_ASSERT_MSG(false, "Controller::applyTransformation()", "This method should be overridden."); }

	/// \brief Sets a float controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	virtual void setFloatValue(AnimationTime time, FloatType newValue) { OVITO_ASSERT_MSG(false, "Controller::setFloatValue()", "This method should be overridden."); }

	/// \brief Sets an integer controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	virtual void setIntValue(AnimationTime time, int newValue) { OVITO_ASSERT_MSG(false, "Controller::setIntValue()", "This method should be overridden."); }

	/// \brief Sets a Vector3 controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	virtual void setVector3Value(AnimationTime time, const Vector3& newValue) { OVITO_ASSERT_MSG(false, "Controller::setVector3Value()", "This method should be overridden."); }

	/// \brief Sets a color controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	void setColorValue(AnimationTime time, const Color& newValue) {
		if constexpr(sizeof(Color) == sizeof(Vector3)) {
			setVector3Value(time, reinterpret_cast<const Vector3&>(newValue));
		}
		else {
			setVector3Value(time, Vector3(newValue));
		}
	}

	/// \brief Sets a position controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	/// \param isAbsolute Specifies whether the value is absolute or should be applied to the existing transformation.
	virtual void setPositionValue(AnimationTime time, const Vector3& newValue, bool isAbsolute) { OVITO_ASSERT_MSG(false, "Controller::setPositionValue()", "This method should be overridden."); }

	/// \brief Sets a rotation controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	/// \param isAbsolute Specifies whether the value is absolute or should be applied to the existing transformation.
	virtual void setRotationValue(AnimationTime time, const Rotation& newValue, bool isAbsolute) { OVITO_ASSERT_MSG(false, "Controller::setRotationValue()", "This method should be overridden."); }

	/// \brief Sets a scaling controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	/// \param isAbsolute Specifies whether the value is absolute or should be applied to the existing transformation.
	virtual void setScalingValue(AnimationTime time, const Scaling& newValue, bool isAbsolute) { OVITO_ASSERT_MSG(false, "Controller::setScalingValue()", "This method should be overridden."); }

	/// \brief Sets a transformation controller's value at the given animation time.
	/// \param time The animation time at which to set the controller's value.
	/// \param newValue The new value to be assigned to the controller.
	/// \param isAbsolute Specifies whether the transformation is absolute or should be applied to the existing transformation.
	virtual void setTransformationValue(AnimationTime time, const AffineTransformation& newValue, bool isAbsolute) { OVITO_ASSERT_MSG(false, "Controller::setTransformationValue()", "This method should be overridden."); }

	/// \brief Adjusts the controller's value after a scene node has gotten a new parent node.
	/// \param time The animation at which to change the controller's parent.
	/// \param oldParentTM The transformation of the old parent node.
	/// \param newParentTM The transformation of the new parent node.
	/// \param contextNode The node to which this controller is assigned to.
	///
	/// This method is called by the SceneNode that owns the transformation controller when it
	/// is newly placed into the scene or below a different node in the node hierarchy.
	virtual void changeParent(AnimationTime time, const AffineTransformation& oldParentTM, const AffineTransformation& newParentTM, SceneNode* contextNode) {}

	/// \brief Adds a translation to the current transformation if this is a transformation controller.
	/// \param time The animation at which the translation should be applied to the transformation.
	/// \param translation The translation vector to add to the transformation. This is specified in the coordinate system given by \a axisSystem.
	/// \param axisSystem The coordinate system in which the translation should be performed.
	virtual void translate(AnimationTime time, const Vector3& translation, const AffineTransformation& axisSystem) { OVITO_ASSERT_MSG(false, "Controller::translate()", "This method should be overridden."); }

	/// \brief Adds a rotation to the current transformation if this is a transformation controller.
	/// \param time The animation at which the rotation should be applied to the transformation.
	/// \param rot The rotation to add to the transformation. This is specified in the coordinate system given by \a axisSystem.
	/// \param axisSystem The coordinate system in which the rotation should be performed.
	virtual void rotate(AnimationTime time, const Rotation& rot, const AffineTransformation& axisSystem) { OVITO_ASSERT_MSG(false, "Controller::rotate()", "This method should be overridden."); }

	/// \brief Adds a scaling to the current transformation if this is a transformation controller.
	/// \param time The animation at which the scaling should be applied to the transformation.
	/// \param scaling The scaling to add to the transformation.
	virtual void scale(AnimationTime time, const Scaling& scaling) { OVITO_ASSERT_MSG(false, "Controller::scale()", "This method should be overridden."); }

Q_SIGNALS:

	/// This signal is emitted by the Controller after its data has been completely loaded from an ObjectLoadStream.
	/// After this signal was sent, it is safe to query the controller for its value.
	void controllerLoadingCompleted();
};


///////////////////////////////// Controller instantiation //////////////////////////////

/**
 * \brief Provides access to default controller implementations.
 */
class OVITO_CORE_EXPORT ControllerManager
{
public:

	/// \brief Creates a new float controller.
	static OORef<Controller> createFloatController();

	/// \brief Creates a new integer controller.
	static OORef<Controller> createIntController();

	/// \brief Creates a new Vector3 controller.
	static OORef<Controller> createVector3Controller();

	/// \brief Creates a new Color controller.
	static OORef<Controller> createColorController() { return createVector3Controller(); }

	/// \brief Creates a new position controller.
	static OORef<Controller> createPositionController();

	/// \brief Creates a new rotation controller.
	static OORef<Controller> createRotationController();

	/// \brief Creates a new scaling controller.
	static OORef<Controller> createScalingController();

	/// \brief Creates a new transformation controller.
	static OORef<Controller> createTransformationController();

	/// Queries whether the user has activated auto-key mode and controllers should automatically
	/// generate new animation keys whenever their current value is changed by the user.
	static bool isAutoGenerateAnimationKeysEnabled();
};

}	// End of namespace
