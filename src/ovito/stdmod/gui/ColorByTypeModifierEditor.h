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


#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>

namespace Ovito {

/**
 * A properties editor for the ColorByTypeModifier class.
 */
class ColorByTypeModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(ColorByTypeModifierEditor)

public:

    /// Constructor
    using PropertiesEditor::PropertiesEditor;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Determines if the given property is a valid input property for the Select Type modifier.
    static bool isValidInputProperty(const Property* property) { return property->isTypedProperty(); }

private:

    class ViewModel : public QAbstractTableModel
    {
    public:

        /// Constructor that takes a pointer to the owning editor.
        ViewModel(ColorByTypeModifierEditor* owner) : QAbstractTableModel(owner) {}

        /// Returns the editor owns this table model.
        ColorByTypeModifierEditor* editor() const { return static_cast<ColorByTypeModifierEditor*>(QObject::parent()); }

        /// Returns the number of rows in the model.
        virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { return _elementTypes.size(); }

        /// Returns the data stored under the given role for the item referred to by the index.
        virtual QVariant data(const QModelIndex& index, int role) const override;

        /// Returns the data for the given role and section in the header with the specified orientation.
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        /// Returns the item flags for the given index.
        virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

        /// Returns the number of columns of the table model.
        int columnCount(const QModelIndex& parent = QModelIndex()) const override { return 2; }

        /// Updates the contents of the model.
        void refresh();

    private:

        std::vector<DataOORef<const ElementType>> _elementTypes;
    };

private:

    // Selection box for the input property.
    PropertyReferenceParameterUI* _sourcePropertyUI;

    /// The list of selectable element types.
    QTableView* _elementTypesBox;
};

}   // End of namespace
