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
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/gui/desktop/widgets/general/StableComboBox.h>

namespace Ovito {

/**
 * \brief Widget that allows the user to select a property from a list (or enter a custom property name).
 */
class OVITO_STDOBJGUI_EXPORT PropertySelectionComboBox : public StableComboBox
{
    Q_OBJECT

public:

    /// Constructor.
    explicit PropertySelectionComboBox(PropertyContainerClassPtr containerClass = nullptr, QWidget* parent = nullptr) : StableComboBox(parent), _containerClass(containerClass) {}

    /// Starts a new list update. This allows adding multiple items to the list without updating the widget yet.
    void beginListUpdate() {
        _newItems.clear();
    }

    /// Completes a list update that was started with beginListUpdate().
    void endListUpdate() {
        StableComboBox::setItems(std::move(_newItems));
    }

    /// \brief Adds a property to the end of the list.
    void addItem(const PropertyReference& property, const QString& label = QString(), bool isChildItem = false, bool warningIcon = false) {
        OVITO_ASSERT(!isChildItem || !isEditable());
        _newItems.push_back(std::make_unique<QStandardItem>((isChildItem ? QStringLiteral("  ") : QString()) + (label.isEmpty() ? property.name().toString() : label)));
        _newItems.back()->setData(QVariant::fromValue(property), Qt::UserRole);
        if(warningIcon)
            _newItems.back()->setIcon(StableComboBox::warningIcon());
    }

    /// \brief Adds a property to the end of the list.
    void addItem(const Property* property, int vectorComponent = -1, bool isChildItem = false) {
        OVITO_ASSERT(property != nullptr);
        OVITO_ASSERT(!isChildItem || !isEditable());
        PropertyReference ref(property, vectorComponent);
        QString label = (isChildItem ? QStringLiteral("  ") : QString()) + ref.nameWithComponent();

        // Check if there is already an item with the same text label.
        if(boost::algorithm::any_of(_newItems, [&](const std::unique_ptr<QStandardItem>& item) { return item->text() == label; }))
            return;

        if(isChildItem) {
            // Check if there is already an item with the same base label.
            QString baseName = ref.nameWithComponent();
            if(boost::algorithm::any_of(_newItems, [&](const std::unique_ptr<QStandardItem>& item) { return item->text() == baseName; }))
                return;
        }

        _newItems.push_back(std::make_unique<QStandardItem>(label));
        _newItems.back()->setData(QVariant::fromValue(ref), Qt::UserRole);
    }

    /// \brief Adds multiple properties to the combo box.
    /// \param list The list of properties to add.
    void addItems(const QVector<Property*>& list) {
        for(const Property* p : list)
            addItem(p);
    }

    /// \brief Returns the particle property that is currently selected in the
    ///        combo box.
    /// \return The selected item. The returned reference can be null if
    ///         no item is currently selected.
    PropertyReference currentProperty() const;

    /// \brief Sets the selection of the combo box to the given particle property.
    /// \param property The particle property to be selected.
    void setCurrentProperty(const PropertyReference& property);

    /// \brief Returns the list index of the given property, or -1 if not found.
    int propertyIndexDuringUpdate(const PropertyReference& property) const {
        for(int i = 0; i < (int)_newItems.size(); ++i) {
            if(_newItems[i]->data(Qt::UserRole).value<PropertyReference>() == property)
                return i;
        }
        return -1;
    }

    int itemCountDuringUpdate() const {
        return _newItems.size();
    }

    /// \brief Returns the property at the given index.
    PropertyReference property(int index) const {
        OVITO_ASSERT(_newItems.empty());
        return itemData(index).value<PropertyReference>();
    }

    /// Returns the class of properties that can be selected with this combo box.
    PropertyContainerClassPtr containerClass() const { return _containerClass; }

    /// Sets the class of properties that can be selected with this combo box.
    void setContainerClass(PropertyContainerClassPtr containerClass) {
        OVITO_ASSERT(_newItems.empty());
        if(_containerClass != containerClass) {
            _containerClass = containerClass;
            clear();
        }
    }

protected:

    /// Is called when the widget loses the input focus.
    virtual void focusOutEvent(QFocusEvent* event) override;

private:

    /// Specifies the class of properties that can be selected in this combo box.
    PropertyContainerClassPtr _containerClass;

    /// The new list of combobox items (only used during preparation of an update).
    std::vector<std::unique_ptr<QStandardItem>> _newItems;
};

}   // End of namespace
