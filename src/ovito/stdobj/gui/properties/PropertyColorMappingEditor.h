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


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/gui/desktop/utilities/DeferredMethodInvocation.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>

namespace Ovito {

/**
 * A properties editor for the PropertyColorMapping class.
 */
class OVITO_STDOBJGUI_EXPORT PropertyColorMappingEditor : public PropertiesEditor
{
    OVITO_CLASS(PropertyColorMappingEditor)

public:

    /// Default constructor.
    Q_INVOKABLE PropertyColorMappingEditor() {}

    /// Sets the property container containing the input properties the user can choose from.
    void setPropertyContainer(const PropertyContainer* container) {
        _sourcePropertyUI->setContainer(container);
    }

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// Returns an icon representing the given color map class.
    QIcon iconFromColorMapClass(OvitoClassPtr clazz);

    /// Returns an icon representing the given color map.
    QIcon iconFromColorMap(ColorCodingGradient* map);

    /// Determines the min/max range of values in the selected input property.
    std::optional<std::pair<FloatType, FloatType>> determineValueRange() const;

    /// Determine the property value corresponding to the given relative position in the range interval.
    FloatType computeRangeValue(FloatType t) const;

protected Q_SLOTS:

    /// Updates the display for the color gradient.
    void updateColorGradient();

    /// Is called when the user selects a color gradient in the list box.
    void onColorGradientSelected(int index);

    /// Is called when the user presses the "Adjust range" button.
    void onAdjustRange();

    /// Is called when the user presses the "Reverse range" button.
    void onReverseRange();

    /// Is called when the user presses the "Export color scale" button.
    void onExportColorScale();

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The list of available color gradients.
    QComboBox* _colorGradientList;

    /// Indicates the combo box already contains an item for a custom color map.
    bool _gradientListContainCustomItem;

    /// Label that displays the color gradient picture.
    QLabel* _colorLegendLabel;

    PropertyReferenceParameterUI* _sourcePropertyUI;
    FloatParameterUI* _startValueUI;
    FloatParameterUI* _endValueUI;
    QPushButton* _adjustRangeBtn;
    QPushButton* _reverseRangeBtn;
};

}   // End of namespace
