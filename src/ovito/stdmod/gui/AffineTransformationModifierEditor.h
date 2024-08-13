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


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/utilities/units/PrescribedScaleUnit.h>

namespace Ovito {

/**
 * A properties editor for the AffineTransformationModifier class.
 */
class AffineTransformationModifierEditor : public PropertiesEditor
{
    Q_OBJECT
    OVITO_CLASS(AffineTransformationModifierEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:

    /// Is called when the spinner value has changed.
    void onSpinnerValueChanged();

    /// This method updates the displayed matrix values.
    void updateUI();

    /// Is called when the user switches between Cartesian and reduced cell coordinates for the translation vector.
    void onReducedCoordinatesOptionChanged();

    /// Is called when the user presses the 'Enter rotation' button.
    void onEnterRotation();

    /// Auto-adjusts the increment steps of the numeric parameter spinner widgets.
    void updateParameterUnitScales();

private:

    SpinnerWidget* relativeCellSpinners[3][4];
    SpinnerWidget* absoluteCellSpinners[3][4];
    std::optional<PrescribedScaleUnit> _relativeTranslationUnits[3];
    std::optional<PrescribedScaleUnit> _relativeMatrixUnits;
    std::optional<PrescribedScaleUnit> _absoluteCellUnits[3][3];
    std::optional<PrescribedScaleUnit> _absoluteOriginUnits[3];
};

}   // End of namespace
