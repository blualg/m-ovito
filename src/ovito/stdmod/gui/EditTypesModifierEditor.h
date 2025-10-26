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

#pragma once


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/EditTypesModifier.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

/**
 * A properties editor for the EditTypesModifier class.
 */
class EditTypesModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(EditTypesModifierEditor)
    Q_OBJECT

public:

    /// Returns the EditTypesModifier object being edited.
    EditTypesModifier* modifier() const { return static_object_cast<EditTypesModifier>(editObject()); }

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:

    /// Refreshes the displayed list of element types.
    void refreshElementTypeList();

    /// Is called whenever the selection of element types in the table has changed.
    void onElementTypeSelectionChanged();

private:

    class ViewModel : public QAbstractTableModel
    {
    public:

        /// Constructor that takes a pointer to the owning editor.
        explicit ViewModel(EditTypesModifierEditor* owner) : QAbstractTableModel(owner) {}

        /// Returns the editor owning this table model.
        EditTypesModifierEditor* editor() const { return static_cast<EditTypesModifierEditor*>(QObject::parent()); }

        /// Returns the modifier being edited.
        EditTypesModifier* modifier() const { return editor()->modifier(); }

        /// Returns the list of element types displayed in the table.
        const auto& elementTypes() const { return editor()->elementTypes(); }

        /// Returns the number of rows in the model.
        virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { return elementTypes().size(); }

        /// Returns the data stored under the given role for the item referred to by the index.
        virtual QVariant data(const QModelIndex& index, int role) const override;

        /// Returns the data for the given role and section in the header with the specified orientation.
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        /// Returns the item flags for the given index.
        virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

        /// Returns the number of columns of the table model.
        int columnCount(const QModelIndex& parent = QModelIndex()) const override { return 2; }

        /// Notifies the Qt widget that the given item has changed and the display needs to be updated.
        void updateItem(int itemIndex) {
            // Update all columns of that item.
            Q_EMIT dataChanged(index(itemIndex, 0), index(itemIndex, columnCount() - 1));
        }

        /// Updates the contents of the model.
        void refresh();

        /// Returns the current title of the element type list. This is derived from the selected input property.
        const QString& listTitle() const { return _listTitle; }

    private:

        /// The current title of the element type list. This is derived from the selected input property.
        QString _listTitle;
    };

private:

    /// The widget displaying the list of element types.
    QTableView* _elementTypesTable;

    /// The Qt table model.
    ViewModel* _tableModel;

    /// The editor for the selected element type.
    OORef<PropertiesEditor> _subEditor;

    /// Container widget for the element type sub-editor.
    QWidget* _subEditorContainer;

    /// Caption label for the element type list.
    QLabel* _typeListCaption;

    /// The current list of element types displayed in UI.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const ElementType>, elementTypes, setElementTypes);
};

}   // End of namespace
