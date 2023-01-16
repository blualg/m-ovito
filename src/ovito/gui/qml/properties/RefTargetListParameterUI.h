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

namespace Ovito {

/******************************************************************************
* A list view that shows the RefTarget items contained in a vector reference field.
******************************************************************************/
class RefTargetListParameterUI : public ParameterUI
{
    Q_OBJECT
    QML_ELEMENT
    OVITO_CLASS(RefTargetListParameterUI)
    
    Q_PROPERTY(QAbstractTableModel* model READ model CONSTANT)

public:

    /// Constructor.
    RefTargetListParameterUI() {
        connect(this, &ParameterUI::editObjectReplaced, this, &RefTargetListParameterUI::onEditObjectReplaced);
    }

    /// Destructor.
    virtual ~RefTargetListParameterUI() { clearAllReferences(); }

    /// Returns a RefTarget from the list.
    Q_INVOKABLE RefTarget* objectAtIndex(int index) const;

    /// Informs the parameter UI that the given columns of all items have changed.
    void updateColumns(int columnStartIndex, int columnEndIndex) { _model->updateColumns(columnStartIndex, columnEndIndex); }

    /// Returns the internal model used to populate the list view or table view widget.
    QAbstractTableModel* model() { 
        if(!_model) _model = createModel();
        return _model; 
    }

protected:

    class ListViewModel : public QAbstractTableModel {
    public:

        /// Constructor that takes a pointer to the owning parameter UI object.
        ListViewModel(RefTargetListParameterUI* owner) : QAbstractTableModel(owner) {}

        /// Returns the parameter UI that owns this table model.
        RefTargetListParameterUI* owner() const { return static_cast<RefTargetListParameterUI*>(QObject::parent()); }

        /// Returns the number of rows in the model.
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override { return owner()->_rowToTarget.size(); }

        /// Returns the data stored under the given role for the item referred to by the index.
        virtual QVariant data(const QModelIndex &index, int role) const override;

        /// Returns the item flags for the given index.
        virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

        /// Returns the model's role names.
        virtual QHash<int, QByteArray> roleNames() const override {
            return { 
                { Qt::DisplayRole, "reftarget" } 
            };
        }

        /// Notifies the system that the given item has changed and the display needs to be updated.
        void updateItem(int itemIndex) {
            // Update all columns of that item.
            dataChanged(index(itemIndex, 0), index(itemIndex, columnCount() - 1));
        }

        /// Notifies the system that the given columns of all items have changed and the display needs to be updated.
        void updateColumns(int columnStartIndex, int columnEndIndex) {
            // Update the columns of all items.
            dataChanged(index(0, columnStartIndex), index(rowCount() - 1, columnEndIndex));
        }

        /// Returns the number of columns of the table model. Default is 1.
        int columnCount(const QModelIndex& parent = QModelIndex()) const override { return owner()->tableColumnCount(); }

        /// Updates the entire list model.
        void resetList() { beginResetModel(); endResetModel(); }

        void beginInsert(int atIndex) { beginInsertRows(QModelIndex(), atIndex, atIndex); }
        void endInsert() { endInsertRows(); }

        void beginRemove(int atIndex) { beginRemoveRows(QModelIndex(), atIndex, atIndex); }
        void endRemove() { endRemoveRows(); }
    };

protected Q_SLOTS:

    /// This method is called when a new editable object has loaded into the editor.
    void onEditObjectReplaced();

    /// Is called when the user has selected an item in the list/table view.
    void onSelectionChanged();

protected:

    /// Creates the instance of the table model managed by this class.
    virtual ListViewModel* createModel() { return new ListViewModel(this); }

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Returns the data stored under the given role for the given RefTarget.
    /// This method is part of the data model used by the list widget and can be overridden
    /// by sub-classes. The default implementation returns the title of the RefTarget
    /// for the Qt::DisplayRole.
    virtual QVariant getItemData(RefTarget* target, const QModelIndex& index, int role);

    /// Returns the model/view item flags for the given entry.
    virtual Qt::ItemFlags getItemFlags(RefTarget* target, const QModelIndex& index) { return Qt::ItemFlags(Qt::ItemIsSelectable) | Qt::ItemIsEnabled; }

    /// Returns the number of columns for the table view. The default is 1.
    virtual int tableColumnCount() { return 1; }

    /// The internal model used for the list view widget.
    ListViewModel* _model = nullptr;

    /// The list of items in the list view.
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(RefTarget*, targets, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_WEAK_REF | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// Maps reference field indices to row indices.
    QVector<int> _targetToRow;

    /// Maps row indices to reference field indices.
    QVector<int> _rowToTarget;
};

}   // End of namespace
