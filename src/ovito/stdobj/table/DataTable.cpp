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

#include <ovito/stdobj/StdObj.h>
#include "DataTable.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTable);
OVITO_CLASSINFO(DataTable, "DisplayName", "Data table");
DEFINE_PROPERTY_FIELD(DataTable, intervalStart);
DEFINE_PROPERTY_FIELD(DataTable, intervalEnd);
DEFINE_PROPERTY_FIELD(DataTable, axisLabelX);
DEFINE_PROPERTY_FIELD(DataTable, axisLabelY);
DEFINE_PROPERTY_FIELD(DataTable, plotMode);
DEFINE_REFERENCE_FIELD(DataTable, x);
DEFINE_REFERENCE_FIELD(DataTable, y);
DEFINE_SHADOW_PROPERTY_FIELD(DataTable, plotMode);

/******************************************************************************
* Registers all standard properties with the property traits class.
******************************************************************************/
void DataTable::OOMetaClass::initialize()
{
    PropertyContainerClass::initialize();

    setPropertyClassDisplayName(tr("Data table"));
    setElementDescriptionName(QStringLiteral("points"));
    setPythonName(QStringLiteral("table"));
}

/******************************************************************************
* Creates a storage object for standard data table properties.
******************************************************************************/
PropertyPtr DataTable::OOMetaClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    OVITO_ASSERT_MSG(false, "DataTable::createStandardPropertyInternal()", "Invalid standard property type");
    throw Exception(tr("This is not a valid standard property type for DataTable: %1").arg(type));
}

/******************************************************************************
* Constructor.
******************************************************************************/
void DataTable::initializeObject(ObjectInitializationFlags flags, PlotMode plotMode, const QString& title, ConstPropertyPtr y, ConstPropertyPtr x)
{
    OVITO_ASSERT(!x || !y || x->size() == y->size());

    PropertyContainer::initializeObject(flags, title);

    setPlotMode(plotMode);
    setX(std::move(x));
    setY(std::move(y));
}

/******************************************************************************
* Assigns a property array as x-coordinates of the data points (for the purpose of plotting).
******************************************************************************/
void DataTable::setX(const Property* property)
{
    _x.set(this, PROPERTY_FIELD(x), property);
    if(property && !properties().contains(const_cast<Property*>(property))) {
        addProperty(property);
    }
}

/******************************************************************************
* Assigns a property array as y-coordinates of the data points (for the purpose of plotting).
******************************************************************************/
void DataTable::setY(const Property* property)
{
    _y.set(this, PROPERTY_FIELD(y), property);
    if(property && !properties().contains(const_cast<Property*>(property))) {
        addProperty(property);
    }
}

/******************************************************************************
* Returns the data array containing the x-coordinates of the data points.
* If no explicit x-coordinate data is available, the array is dynamically generated
* from the x-axis interval set for this data table.
******************************************************************************/
ConstPropertyPtr DataTable::getXValues() const
{
    if(const Property* xProperty = x()) {
        return xProperty;
    }
    else if(y() && elementCount() != 0 && (intervalStart() != 0 || intervalEnd() != 0)) {
        PropertyFactory<FloatType> xdata(OOClass(), elementCount(), axisLabelX());
        FloatType binSize = (intervalEnd() - intervalStart()) / elementCount();
        FloatType x = intervalStart() + binSize * FloatType(0.5);
        for(auto& v : xdata) {
            v = x;
            x += binSize;
        }
        return xdata.take();
    }
    else {
        PropertyFactory<int64_t> xdata(OOClass(), elementCount(), axisLabelX().isEmpty() ? QStringLiteral("Index") : axisLabelX());
        std::iota(xdata.begin(), xdata.end(), (int64_t)0);
        return xdata.take();
    }
}

/******************************************************************************
* From RefMaker.
******************************************************************************/
void DataTable::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(PropertyContainer::properties)) {
        if(!isBeingDeleted() && !isBeingLoaded() && !isUndoingOrRedoing()) {
            if(x() == oldTarget)
                setX({});
            if(y() == oldTarget)
                setY({});
        }
    }
    PropertyContainer::referenceRemoved(field, oldTarget, listIndex);
}

}   // End of namespace
