////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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


#include <ovito/gui/qml/GUI.h>
#include <ovito/gui/qml/properties/ParameterUI.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief UI component that allows the user to select a property object in a modifier's pipeline input.
 */
class PropertyReferenceParameterUI : public ParameterUI
{
    Q_OBJECT
    QML_ELEMENT
    OVITO_CLASS(PropertyReferenceParameterUI)

    Q_PROPERTY(QVariant propertyContainer READ propertyContainer WRITE setPropertyContainer)
    Q_PROPERTY(PropertyComponentsMode componentsMode READ componentsMode WRITE setComponentsMode)
    Q_PROPERTY(AcceptablePropertyType acceptablePropertyType READ acceptablePropertyType WRITE setAcceptablePropertyType)
    Q_PROPERTY(PropertyParameterType propertyParameterType READ propertyParameterType WRITE setPropertyParameterType)
    Q_PROPERTY(QAbstractItemModel* model MEMBER _model CONSTANT)
    Q_PROPERTY(QString currentPropertyName READ currentPropertyName)

public:

    enum PropertyComponentsMode {
        ShowOnlyComponents,
        ShowNoComponents,
        ShowComponentsAndVectorProperties
    };
    Q_ENUM(PropertyComponentsMode);

    enum AcceptablePropertyType {
        AllProperties,
        OnlyTypedProperties
    };
    Q_ENUM(AcceptablePropertyType);

    enum PropertyParameterType {
        InputProperty,
        OutputProperty
    };
    Q_ENUM(PropertyParameterType);

    /// Constructor.
    PropertyReferenceParameterUI() : _model(new Model(this)) {
        connect(this, &ParameterUI::editObjectReplaced, this, &PropertyReferenceParameterUI::updatePropertyList);
    }

    /// Sets the property container from which the user can choose a property.
    void setPropertyContainer(const QVariant& dataObjectReference);

    /// Returns a reference to the property container from which the user can choose a property.
    QVariant propertyContainer() const { return QVariant::fromValue(_containerReference); }

    /// Obtains the current value of the parameter from the C++ object.
    virtual QVariant getCurrentValue() const override;

    /// Changes the current value of the C++ object parameter.
    virtual void setCurrentValue(const QVariant& val) override;

    /// Returns whether the model lists each component of a property separately.
    PropertyComponentsMode componentsMode() const { return _componentsMode; }

    /// Sets whether the model should list each component of a property separately.
    void setComponentsMode(PropertyComponentsMode mode) { _componentsMode = mode; }

    /// Returns which kinds of properties the user can choose from.
    AcceptablePropertyType acceptablePropertyType() const { return _acceptablePropertyType; }

    /// Sets which kinds of properties the user can choose from.
    void setAcceptablePropertyType(AcceptablePropertyType type) {
        if(_acceptablePropertyType != type) {
            _acceptablePropertyType = type;
            updatePropertyList();
            updateUI();
        }
    }

    /// Returns whether the list contains input or output properties.
    PropertyParameterType propertyParameterType() const { return _propertyParameterType; }

    /// Sets whether the list contains input or output properties.
    void setPropertyParameterType(PropertyParameterType paramType) { _propertyParameterType = paramType; }

    /// Returns the display name of the currently selected property.
    QString currentPropertyName() const;

    /// Updates the displayed value in the UI.
    virtual void updateUI() override;

Q_SIGNALS:

    /// This signal is emitted whenever a different property becomes the currently selected property.
    void currentPropertyNameChanged();

private:

    class Model : public QAbstractListModel {
    public:
        /// Inherit constructor.
        using QAbstractListModel::QAbstractListModel;

        /// Returns the number of rows in the model.
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override { return std::max((size_t)1, _properties.size()); }

        /// Returns the data stored under the given role for the item referred to by the index.
        virtual QVariant data(const QModelIndex &index, int role) const override;

        /// Returns the model's role names.
        virtual QHash<int, QByteArray> roleNames() const override { return {{ Qt::DisplayRole, "label" }}; }

        /// Returns the list of acceptable property objects the user can choose from.
        const std::vector<PropertyReference>& properties() const { return _properties; }

        /// Updates the entire list model.
        void resetList(std::vector<PropertyReference> properties, std::vector<QString> texts) {
            beginResetModel();
            _properties = std::move(properties);
            _texts = std::move(texts);
            OVITO_ASSERT(_properties.size() == _texts.size());
            endResetModel();
        }

    private:

        /// The list of acceptable property objects.
        std::vector<PropertyReference> _properties;

        /// The list of text strings, one for each property.
        std::vector<QString> _texts;
    };

private Q_SLOTS:

    /// Rebuilds the list of available property objects the user can choose from.
    void updatePropertyList();

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// The container from which properties that can be selected.
    PropertyContainerReference _containerReference;

    /// The list model containing all available properties the user can choose from.
    Model* _model;

    /// Controls whether the model should list each component of a property separately.
    PropertyComponentsMode _componentsMode = ShowOnlyComponents;

    /// Controls which kinds of properties the user can choose from.
    AcceptablePropertyType _acceptablePropertyType = AllProperties;

    /// Controls whether the list should contain input or output properties.
    PropertyParameterType _propertyParameterType = InputProperty;
};

}   // End of namespace
