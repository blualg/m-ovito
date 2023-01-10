////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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

namespace Ovito::StdObj {

IMPLEMENT_OVITO_CLASS(DataTable);
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

	// Enable automatic conversion of a DataTablePropertyReference to a generic PropertyReference and vice versa.
	QMetaType::registerConverter<DataTablePropertyReference, PropertyReference>();
	QMetaType::registerConverter<PropertyReference, DataTablePropertyReference>();

	setPropertyClassDisplayName(tr("Data table"));
	setElementDescriptionName(QStringLiteral("points"));
	setPythonName(QStringLiteral("table"));
}

/******************************************************************************
* Creates a storage object for standard data table properties.
******************************************************************************/
PropertyPtr DataTable::OOMetaClass::createStandardPropertyInternal(size_t elementCount, int type, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath) const
{
	OVITO_ASSERT_MSG(false, "DataTable::createStandardProperty()", "Invalid standard property type");
	throw Exception(tr("This is not a valid standard property type for DataTable: %1").arg(type));
}

/******************************************************************************
* Constructor.
******************************************************************************/
DataTable::DataTable(ObjectCreationParams params, PlotMode plotMode, const QString& title, ConstPropertyPtr y, ConstPropertyPtr x) : PropertyContainer(params, title),
	_intervalStart(0),
	_intervalEnd(0),
	_plotMode(plotMode)
{
	OVITO_ASSERT(!x || !y || x->size() == y->size());
	setX(std::move(x));
	setY(std::move(y));
}

/******************************************************************************
* Assigns a property array as x-coordinates of the data points (for the purpose of plotting).
******************************************************************************/
void DataTable::setX(const PropertyObject* property)
{
	_x.set(this, PROPERTY_FIELD(x), property);
	if(property && !properties().contains(const_cast<PropertyObject*>(property))) {
		addProperty(property);
	}
}

/******************************************************************************
* Assigns a property array as y-coordinates of the data points (for the purpose of plotting).
******************************************************************************/
void DataTable::setY(const PropertyObject* property)
{
	_y.set(this, PROPERTY_FIELD(y), property);
	if(property && !properties().contains(const_cast<PropertyObject*>(property))) {
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
	if(const PropertyObject* xProperty = x()) {
		return xProperty;
	}
	else if(const PropertyObject* yProperty = y()) {
		if(elementCount() != 0 && (intervalStart() != 0 || intervalEnd() != 0)) {
			PropertyAccessAndRef<FloatType> xdata = OOClass().createUserProperty(elementCount(), PropertyObject::Float, 1, axisLabelX());
			FloatType binSize = (intervalEnd() - intervalStart()) / xdata.size();
			FloatType x = intervalStart() + binSize * FloatType(0.5);
			for(FloatType& v : xdata) {
				v = x;
				x += binSize;
			}
			return xdata.take();
		}
		else {
			PropertyAccessAndRef<qlonglong> xdata = OOClass().createUserProperty(elementCount(), PropertyObject::Int64, 1, axisLabelX());
			std::iota(xdata.begin(), xdata.end(), (size_t)0);
			return xdata.take();
		}
	}
	return {};
}

}	// End of namespace
