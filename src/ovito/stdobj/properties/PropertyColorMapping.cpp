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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/rendering/ColorMapHelper.h>
#include "PropertyColorMapping.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PropertyColorMapping);
OVITO_CLASSINFO(PropertyColorMapping, "DisplayName", "Color mapping");
DEFINE_REFERENCE_FIELD(PropertyColorMapping, colorGradient);
DEFINE_PROPERTY_FIELD(PropertyColorMapping, startValue);
DEFINE_PROPERTY_FIELD(PropertyColorMapping, endValue);
DEFINE_PROPERTY_FIELD(PropertyColorMapping, symmetricRange);
DEFINE_PROPERTY_FIELD(PropertyColorMapping, sourceProperty);
DEFINE_PROPERTY_FIELD(PropertyColorMapping, useDiscreteColorMap);
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, startValue, "Start value");
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, endValue, "End value");
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, symmetricRange, "Symmetric range");
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, colorGradient, "Color gradient");
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, sourceProperty, "Source property");
SET_PROPERTY_FIELD_LABEL(PropertyColorMapping, useDiscreteColorMap, "Discretize");

/******************************************************************************
* Constructor.
******************************************************************************/
void PropertyColorMapping::initializeObject(ObjectInitializationFlags flags)
{
    RefTarget::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(this_task::isInteractive()) {
#ifndef OVITO_DISABLE_QSETTINGS
            // Load the default gradient type set by the user.
            QSettings settings;
            settings.beginGroup(PropertyColorMapping::OOClass().plugin()->pluginId());
            settings.beginGroup(PropertyColorMapping::OOClass().name());
            QString typeString = settings.value(PROPERTY_FIELD(colorGradient)->identifier()).toString();
            if(!typeString.isEmpty()) {
                try {
                    OvitoClassPtr gradientType = OvitoClass::decodeFromString(typeString);
                    if(!colorGradient() || colorGradient()->getOOClass() != *gradientType) {
                        OORef<ColorCodingGradient> gradient = dynamic_object_cast<ColorCodingGradient>(gradientType->createInstance(flags));
                        if(gradient) setColorGradient(std::move(gradient));
                    }
                }
                catch(...) {}
            }
#endif
        }

        // Select the rainbow color gradient by default.
        if(!colorGradient())
            setColorGradient(OORef<ColorCodingHSVGradient>::create(flags));
    }
}

/******************************************************************************
* Creates a PseudoColorMapping that can be used for rendering of graphics primitives.
******************************************************************************/
PseudoColorMapping PropertyColorMapping::pseudoColorMapping() const
{
    if(useDiscreteColorMap() && std::isfinite(startValue()) && std::isfinite(endValue()))
        return {startValue(), endValue(), colorGradient(), DiscreteColorMap::binCount(startValue(), endValue())};
    else
        return {startValue(), endValue(), colorGradient()};
}

/******************************************************************************
 * Is called when the value of a property of this object has changed.
 ******************************************************************************/
void PropertyColorMapping::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(endValue) && symmetricRange() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        // Keep start value in sync when symmetricRange is active and end value changes
        setStartValue(-endValue());
    }
    else if(field == PROPERTY_FIELD(symmetricRange) && symmetricRange() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        // Symmetrize existing range when symmetricRange is turned on
        FloatType range = std::max(std::abs(startValue()), std::abs(endValue()));
        bool inverted = startValue() > endValue();
        setStartValue(inverted ? range : -range);
        setEndValue(inverted ? -range : range);
    }

    RefTarget::propertyChanged(field);
}

/******************************************************************************
* Determines the min/max range of values stored in the given property array.
******************************************************************************/
std::optional<std::pair<FloatType, FloatType>> PropertyColorMapping::determineValueRange(const Property* pseudoColorProperty, int pseudoColorPropertyComponent) const
{
    OVITO_ASSERT(pseudoColorProperty);
    OVITO_ASSERT(pseudoColorPropertyComponent >= 0 && pseudoColorPropertyComponent < pseudoColorProperty->componentCount());

    FloatType minValue = std::numeric_limits<FloatType>::max();
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();

    // Iterate over the property array to find the lowest/highest value.
    // NaNs and INFs are ignored.
    pseudoColorProperty->forEach(pseudoColorPropertyComponent, [&](size_t i, auto v) {
        if(std::isfinite(static_cast<FloatType>(v)) && (v > maxValue)) maxValue = v;
        if(std::isfinite(static_cast<FloatType>(v)) && (v < minValue)) minValue = v;
    });

    // Range may be degenerate if input property contains (valid) zero elements.
    if(minValue == std::numeric_limits<FloatType>::max())
        return {};

    return std::make_pair(minValue, maxValue);
}

/******************************************************************************
* Swaps the minimum and maximum values to reverse the color scale.
******************************************************************************/
void PropertyColorMapping::reverseRange()
{
    FloatType oldStartValue = startValue();
    setStartValue(endValue());
    setEndValue(oldStartValue);
}

}   // End of namespace
