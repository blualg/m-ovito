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


#include <ovito/core/Core.h>
#include "UnitsManager.h"

namespace Ovito {

/**
 * \brief A wrapper unit type that allows to prescribe a "scale" or "magnitude" for a numeric parameter, which
 *        determines the step size. This is used, for example, by the SliceModifierEditor to prescribe a step
 *        size for the "Distance" parameter that depends on the diameter of the simulation box.
 */
class OVITO_CORE_EXPORT PrescribedScaleUnit : public ParameterUnit
{
    Q_OBJECT

public:

    /// \brief Constructor.
    explicit PrescribedScaleUnit(ParameterUnit* wrappedUnit, QObject* parent = nullptr) : ParameterUnit(parent), _wrappedUnit(wrappedUnit) { OVITO_ASSERT(wrappedUnit); }

    /// Returns the reference scale value used to determine the step size.
    FloatType scaleReference() const { return _scaleReference; }

    /// Sets the reference scale value used to determine the step size.
    void setScaleReference(FloatType scaleReference) { _scaleReference = scaleReference; }

public:

    /// \brief Converts a value from native units to the units presented to the user.
    virtual FloatType nativeToUser(FloatType nativeValue) override { return _wrappedUnit->nativeToUser(nativeValue); }

    /// \brief Converts a value from user units to the native units used internally.
    virtual FloatType userToNative(FloatType userValue) override { return _wrappedUnit->userToNative(userValue); }

    /// \brief Converts the given string to a value.
    virtual FloatType parseString(const QString& valueString) override { return _wrappedUnit->parseString(valueString); }

    /// \brief Converts a numeric value to a string.
    virtual QString formatValue(FloatType value) override { return _wrappedUnit->formatValue(value); }

    /// \brief Given an arbitrary value, which is potentially invalid, rounds it to the closest valid value.
    virtual FloatType roundValue(FloatType value) override { return _wrappedUnit->roundValue(value); }

    /// \brief Returns positive the step size used by spinner widgets for this parameter unit type.
    virtual FloatType stepSize(FloatType currentValue, bool upDirection) override {
        // Instead of current parameter value, use explicit scale reference value to determine an appropriate increment.
        return _wrappedUnit->stepSize(scaleReference() ? scaleReference() : currentValue, upDirection);
    }

private:

    /// The wrapped unit object.
    QPointer<ParameterUnit> _wrappedUnit;

    /// A reference scale value used to determine the step size.
    FloatType _scaleReference = 0;
};

}   // End of namespace
