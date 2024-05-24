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


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/gui/desktop/properties/PropertyParameterUI.h>

namespace Ovito {

/**
 * \brief This parameter UI lets the user specify a property in a property container.
 */
class OVITO_STDOBJGUI_EXPORT ContainerAndPropertyReferenceParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(ContainerAndPropertyReferenceParameterUI)
    Q_OBJECT

public:

    enum PropertyComponentsMode {
        ShowOnlyComponents,
        ShowNoComponents,
        ShowComponentsAndVectorProperties
    };
    Q_ENUM(PropertyComponentsMode);

    /// Constructor.
    explicit ContainerAndPropertyReferenceParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, PropertyComponentsMode componentsMode = ShowOnlyComponents);

    /// Destructor.
    virtual ~ContainerAndPropertyReferenceParameterUI();

    /// Returns the combo-box widget listing the available property containers.
    QComboBox* containerComboBox() const { return _containerComboBox; }

    /// Returns the combo-box widget listing the available properties.
    QComboBox* propertyComboBox() const { return _propertyComboBox; }

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// This method updates the displayed value of the property UI.
    virtual void updateUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// Sets the tooltip text for the combo box widget.
    void setToolTip(const QString& text) const {
        if(containerComboBox()) containerComboBox()->setToolTip(text);
        if(propertyComboBox()) propertyComboBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the combo box.
    void setWhatsThis(const QString& text) const {
        if(containerComboBox()) containerComboBox()->setWhatsThis(text);
        if(propertyComboBox()) propertyComboBox()->setWhatsThis(text);
    }

    /// Installs optional callback function that allows clients to filter the displayed list of properties.
    template<typename F>
    void setPropertyFilter(F&& filter) {
        _propertyFilter = std::forward<F>(filter);
        updateUI();
    }

public Q_SLOTS:

    /// Is called when the user selects a different property container from the list.
    void containerSelected();

    /// Is called when the user selects a different property from the list.
    void propertySelected();

protected:

    /// The combo-box widget listing the available property containers.
    QPointer<QComboBox> _containerComboBox;

    /// The combo-box widget listing the available properties.
    QPointer<QComboBox> _propertyComboBox;

    /// Controls whether the combo box should display a separate entry for each component of a property.
    PropertyComponentsMode _componentsMode;

    /// An optional callback function that allows clients to filter the displayed list of properties.
    std::function<bool(const PropertyContainer* container, const Property*)> _propertyFilter;
};

}   // End of namespace
