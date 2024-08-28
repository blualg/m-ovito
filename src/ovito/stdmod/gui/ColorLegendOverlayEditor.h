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

namespace Ovito {

/**
 * \brief A properties editor for the ColorLegendOverlay class.
 */
class ColorLegendOverlayEditor : public PropertiesEditor
{
    OVITO_CLASS(ColorLegendOverlayEditor)
    Q_OBJECT

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Updates the combo-box showing the available color mappings.
    void updateColorMappingsList();

    /// Is called when the user selects a new source color mapping for the color legend.
    void colorMappingSelectedSelected();

    /// Updates the placeholder texts of the label input fields to reflect the current values.
    void updateLabelPlaceholderTexts();

private:

    PopupUpdateComboBox* _colorMappingsComboBox;
    StringParameterUI* _titlePUI;
    StringParameterUI* _label1PUI;
    StringParameterUI* _label2PUI;
    StringParameterUI* _valueFormatStringPUI;
    BooleanGroupBoxParameterUI* _tickEnabledPUI;
};

}   // End of namespace
