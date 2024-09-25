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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertyParameterUI.h>
#include <ovito/gui/desktop/widgets/general/StableComboBox.h>

namespace Ovito {

/**
 * \brief UI component for selecting the data object from a data collection.
 */
class OVITO_GUI_EXPORT DataObjectReferenceParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(DataObjectReferenceParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const DataObject::OOMetaClass& dataObjectClass);

    /// Destructor.
    virtual ~DataObjectReferenceParameterUI();

    /// This returns the combobox widget managed by this ParameterUI.
    StableComboBox* comboBox() const { return _comboBox; }

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// Returns the type of data object that can be selected.
    DataObjectClassPtr dataObjectClass() const { return _dataObjectClass; }

    /// Sets the tooltip text for the combo box widget.
    void setToolTip(const QString& text) const {
        if(comboBox()) comboBox()->setToolTip(text);
    }

    /// Sets the What's This helper text for the combo box.
    void setWhatsThis(const QString& text) const {
        if(comboBox()) comboBox()->setWhatsThis(text);
    }

    /// Installs an optional callback function for filtering the displayed object list.
    template<typename F>
    void setObjectFilter(F&& filter) {
        _objectFilter = std::forward<F>(filter);
        updateUI();
    }

    /// Installs an optional callback function for filtering the displayed object list.
    template<typename DataObjectClass, typename F>
    void setObjectFilter(F&& filter) {
        OVITO_ASSERT(_dataObjectClass->isDerivedFrom(DataObjectClass::OOClass()));
        setObjectFilter([filter=std::forward<F>(filter)](const DataObject* obj) {
            return std::invoke(filter, static_object_cast<DataObjectClass>(obj));
        });
    }

public:

    Q_PROPERTY(StableComboBox comboBox READ comboBox)

public Q_SLOTS:

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    void updatePropertyValue();

protected:

    /// The combo-box widget.
    QPointer<StableComboBox> _comboBox;

    /// The type of data object that can be selected.
    DataObjectClassPtr _dataObjectClass;

    /// An optional callback function that allows clients to filter the displayed object list.
    std::function<bool(const DataObject*)> _objectFilter;
};

}   // End of namespace
