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

#include <ovito/core/Core.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TCBInterpolationControllers.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FloatTCBAnimationKey);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, easeTo);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, easeFrom);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, tension);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, continuity);
DEFINE_PROPERTY_FIELD(FloatTCBAnimationKey, bias);
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, easeTo, "Ease to");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, easeFrom, "Ease from");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, tension, "Tension");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, continuity, "Continuity");
SET_PROPERTY_FIELD_LABEL(FloatTCBAnimationKey, bias, "Bias");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FloatTCBAnimationKey, easeTo, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(FloatTCBAnimationKey, easeFrom, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, tension, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, continuity, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(FloatTCBAnimationKey, bias, FloatParameterUnit, -1, 1);

IMPLEMENT_CREATABLE_OVITO_CLASS(PositionTCBAnimationKey);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, easeTo);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, easeFrom);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, tension);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, continuity);
DEFINE_PROPERTY_FIELD(PositionTCBAnimationKey, bias);
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, easeTo, "Ease to");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, easeFrom, "Ease from");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, tension, "Tension");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, continuity, "Continuity");
SET_PROPERTY_FIELD_LABEL(PositionTCBAnimationKey, bias, "Bias");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PositionTCBAnimationKey, easeTo, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PositionTCBAnimationKey, easeFrom, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, tension, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, continuity, FloatParameterUnit, -1, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(PositionTCBAnimationKey, bias, FloatParameterUnit, -1, 1);

IMPLEMENT_CREATABLE_OVITO_CLASS(TCBPositionController);

}   // End of namespace
