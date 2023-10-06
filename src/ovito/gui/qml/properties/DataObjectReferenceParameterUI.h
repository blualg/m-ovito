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


#include <ovito/gui/qml/GUI.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include "ParameterUI.h"

namespace Ovito {

/**
 * \brief UI component that allows the user to select a data object in a modifier's pipeline input.
 */
class DataObjectReferenceParameterUI : public ParameterUI
{
    Q_OBJECT
    QML_ELEMENT
    OVITO_CLASS(DataObjectReferenceParameterUI)

    Q_PROPERTY(QString dataObjectType READ dataObjectType WRITE setDataObjectType)
    Q_PROPERTY(QAbstractItemModel* model MEMBER _model CONSTANT)

public:

    /// Constructor.
    DataObjectReferenceParameterUI() : _model(new Model(this)) {
        connect(this, &ParameterUI::editObjectReplaced, this, &DataObjectReferenceParameterUI::updateDataObjectList);
    }

    /// Sets the class of data objects the user can choose from.
    void setDataObjectType(const QString& typeName);

    /// Returns the name of the data object class the user can choose from.
    QString dataObjectType() const { return _dataObjectType ? _dataObjectType->name() : QString(); }

    /// Obtains the current value of the parameter from the C++ object.
    virtual QVariant getCurrentValue() const override;

    /// Changes the current value of the C++ object parameter.
    virtual void setCurrentValue(const QVariant& val) override;

    /// Returns the i-th reference from the list of available input data objects.
    Q_INVOKABLE QVariant get(int index) const {
        if(index >= 0 && index < _model->rowCount())
            return _model->data(_model->index(index, 0), Qt::UserRole);
        else
            return {};
    }

private:

    class Model : public QAbstractListModel
    {
    public:

        /// Constructor that takes a pointer to the owning object.
        Model(DataObjectReferenceParameterUI* owner) : QAbstractListModel(owner) {}

        /// Returns the owner of the model.
        DataObjectReferenceParameterUI* owner() const { return static_cast<DataObjectReferenceParameterUI*>(QObject::parent()); }

        /// Returns the number of rows in the model.
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override { return std::max((size_t)1, _dataObjects.size()); }

        /// Returns the data stored under the given role for the item referred to by the index.
        virtual QVariant data(const QModelIndex &index, int role) const override;

        /// Returns the model's role names.
        virtual QHash<int, QByteArray> roleNames() const override {
            return {
                { Qt::DisplayRole, "label" },
                { Qt::UserRole, "reference" }
            };
        }

        /// Returns the list of acceptable data objects in the modifier's pipeline input.
        const std::vector<DataObjectReference>& dataObjects() const { return _dataObjects; }

        /// Updates the entire list model.
        void resetList(std::vector<DataObjectReference> dataObjects) {
            beginResetModel();
            _dataObjects = std::move(dataObjects);
            endResetModel();
        }

    private:

        /// The list of acceptable data objects in the modifier's pipeline input.
        std::vector<DataObjectReference> _dataObjects;
    };

private Q_SLOTS:

    /// Rebuilds the list of available input data objects the user can choose from.
    void updateDataObjectList();

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// The type of data objects the user can pick.
    DataObjectClassPtr _dataObjectType = nullptr;

    /// The list model containing all available input data objects.
    Model* _model;
};

}   // End of namespace
