////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
 * \file
 * \brief This header file defines the default data types and numeric constants used throughout the program.
 */

#pragma once

#include <cstdint>
#include <concepts>
#include <numbers>
#include <limits>

namespace Ovito {

#ifdef FLOATTYPE_FLOAT

    /// The default floating-point type used by OVITO.
    using FloatType = float;

    /// The format specifier to be passed to the sscanf() function to parse floating-point numbers of type Ovito::FloatType.
    #define FLOATTYPE_SCANF_STRING      "%g"

#else

    /// The default floating-point type used by OVITO.
    using FloatType = double;

    /// The format specifier to be passed to the sscanf() function to parse floating-point numbers of type Ovito::FloatType.
    #define FLOATTYPE_SCANF_STRING      "%lg"

#endif

    /// Low-precision floating-point type used for graphics data.
    using GraphicsFloatType = float;

    /// The constant PI.
    template<std::floating_point T>
    inline constexpr T pi_v = std::numbers::pi_v<T>;
    inline constexpr FloatType pi = pi_v<FloatType>;
    // Legacy macros for backward compatibility:
    inline constexpr FloatType FLOATTYPE_PI = Ovito::pi_v<FloatType>;

    /// A concept for numeric types.
    template<class T>
    concept Numeric = std::integral<T> || std::floating_point<T>;
    /// A small epsilon, which is used in OVITO to test if a number is (almost) zero.
    /// This function template returns a type dependent epsilon value (for single and double precision types).
    template<Numeric T>
    inline constexpr T epsilon_v = (T)0;
    template<>
    inline constexpr double epsilon_v<double> = 1e-12;
    template<>
    inline constexpr float epsilon_v<float> = 1e-6F;
    inline constexpr FloatType epsilon = epsilon_v<FloatType>;
    // Legacy macros for backward compatibility:
    inline constexpr FloatType FLOATTYPE_EPSILON = Ovito::epsilon_v<FloatType>;

    /// A small epsilon, which is used in OVITO to test if a number is (almost) zero.
    // Legacy macros for backward compatibility:
    inline constexpr GraphicsFloatType GRAPHICS_FLOATTYPE_EPSILON = (GraphicsFloatType)1e-12;

    /// The maximum value for floating-point variables of type Ovito::FloatType.
    inline constexpr FloatType FLOATTYPE_MAX = std::numeric_limits<FloatType>::max();

    /// The lowest value for floating-point variables of type Ovito::FloatType.
    inline constexpr FloatType FLOATTYPE_MIN = std::numeric_limits<FloatType>::lowest();

    /// The format specifier to be passed to the sscanf() function to parse low-precision floating-point numbers of type
    /// Ovito::GraphicsFloatType.
#define GRAPHICS_FLOATTYPE_SCANF_STRING "%g"

    /// Data type used for storing unique identifiers.
    using IdentifierIntType = int64_t;

    /// Data type used for storing element selections.
    using SelectionIntType = int8_t;

}   // End of namespace
