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


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectionApplet.h>
#include <ovito/gui/desktop/widgets/general/ActionsItemDelegate.h>
#include <ovito/core/dataset/scene/Pipeline.h>

namespace Ovito {

/**
 * \brief Data inspector page that displays all element types.
 */
class OVITO_STDOBJGUI_EXPORT TypesInspectionApplet : public DataInspectionApplet
{
    OVITO_CLASS(TypesInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() { DataInspectionApplet::initializeObject(Property::OOClass()); }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 90; }

    /// Determines the list of data objects that are displayed by the applet.
    virtual std::vector<ConstDataObjectPath> getDataObjectPaths() override;

    /// Returns the data display widget.
    QTableView* tableView() const { return _tableView; }

    /// Returns the typed property that is currently selected.
    const Property* selectedProperty() const { return static_object_cast<Property>(selectedDataObject()); }

    /// Returns the PropertyContainer that contains the selected typed property.
    const PropertyContainer* selectedPropertyContainer() const {
        return selectedDataObjectPath().nextToLastAs<PropertyContainer>();
    }

    /// Selects a specific data object in this applet.
    virtual bool selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint) override;

protected:

    /// Initializes a list item representing the given data object path.
    virtual void configureDataObjectListItem(QListWidgetItem* item, const ConstDataObjectPath& objectPath) override;

private Q_SLOTS:

    /// Is called when the user selects a different property from the list.
    void onCurrentPropertyChanged();

    /// Is called to edit the selected element type.
    void onEditType(const QModelIndex& index);

private:

    /// A table model for displaying the element types.
    class TypeTableModel : public QAbstractTableModel
    {
    public:

        /// Additional custom roles used by the item delegate to fetch data from the model.
        enum ItemRoles {
            InfoRole = Qt::UserRole + 1,
            ActionsRole = Qt::UserRole + 2,
        };

        /// Constructor.
        TypeTableModel(TypesInspectionApplet* applet, QObject* parent) : QAbstractTableModel(parent), _applet(applet) {}

        /// Returns the typed property currently displayed by the model.
        const ConstPropertyPtr& property() const { return _property; }

        /// Returns the number of rows.
        virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { return property() ? property()->elementTypes().size() : 0; }

        /// Returns the number of columns.
        virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override { return _columnNames.size(); }

        /// Returns the data stored under the given 'role' for the item referred to by the 'index'.
        virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        /// Returns the data for the given role and section in the header with the specified orientation.
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

        /// Replaces the contents of this data model.
        void setContents(const PropertyContainer* container, const Property* property);

    private:

        /// The owner of the model.
        TypesInspectionApplet* _applet;

        /// The typed property currently displayed by the model.
        ConstPropertyPtr _property;

        /// The list of column names to be displayed.
        QStringList _columnNames;

        /// The ElementType class used by the typed property.
        ElementTypeClassPtr _elementTypeClass = nullptr;
    };

private:

    /// The table widget displaying the list of element types.
    QTableView* _tableView = nullptr;

    /// The table model listing all element types of the current typed property.
    TypeTableModel* _tableModel = nullptr;

    /// For cleaning up widgets.
    QObjectCleanupHandler _cleanupHandler;

    /// Action for editing an element type.
    ItemAction* _editTypeAction;
};

}   // End of namespace
