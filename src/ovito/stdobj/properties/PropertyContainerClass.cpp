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

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include "Property.h"
#include "OwnerPropertyRef.h"
#include "PropertyContainerClass.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Is called by the system after construction of the meta-class instance.
******************************************************************************/
void PropertyContainerClass::initialize()
{
    DataObject::OOMetaClass::initialize();

    if(this == &PropertyContainer::OOClass()) {
        // Enable automatic conversion of a PropertyContainerReference to a generic DataObjectReference and vice versa.
        QMetaType::registerConverter<PropertyContainerReference, DataObjectReference>();
        QMetaType::registerConverter<DataObjectReference, PropertyContainerReference>();
        // Enable automatic conversion of a PropertyReference to a QString and vice versa.
        QMetaType::registerConverter<PropertyReference, QString>([](const PropertyReference& ref) { return ref.nameWithComponent(); });
        QMetaType::registerConverter<QString, PropertyReference>();
    }
}

/******************************************************************************
* This helper method returns a standard property (if present) from the
* given pipeline state.
******************************************************************************/
void PropertyContainerClass::registerStandardProperty(int typeId, QString name, int dataType, QStringList componentNames, OvitoClassPtr typedPropertyElementClass, QString title)
{
    OVITO_ASSERT_MSG(typeId > 0, "PropertyContainerClass::registerStandardProperty", "Invalid standard property type ID");
    OVITO_ASSERT_MSG(_standardPropertyIds.find(name) == _standardPropertyIds.end(), "PropertyContainerClass::registerStandardProperty", "Duplicate standard property name");
    OVITO_ASSERT_MSG(_standardPropertyNames.find(typeId) == _standardPropertyNames.end(), "PropertyContainerClass::registerStandardProperty", "Duplicate standard property type ID");
    OVITO_ASSERT_MSG(dataType == Property::Int8 || dataType == Property::Int32 || dataType == Property::Int64 || dataType == Property::Float32 || dataType == Property::Float64, "PropertyContainerClass::registerStandardProperty", "Invalid standard property data type");
    OVITO_ASSERT_MSG(!typedPropertyElementClass || typedPropertyElementClass->isDerivedFrom(ElementType::OOClass()), "PropertyContainerClass::registerStandardProperty", "Element type class is not derived from ElementType base");

    if(!name.isEmpty()) {
#ifdef OVITO_DEBUG
        Property::throwIfInvalidPropertyName(name);
#endif
        _standardPropertyIds.emplace(name, typeId);
    }
    _standardPropertyNames.emplace(typeId, std::move(name));
    _standardPropertyTitles.emplace(typeId, std::move(title));
    _standardPropertyComponents.emplace(typeId, std::move(componentNames));
    _standardPropertyDataTypes.emplace(typeId, dataType);
    if(typedPropertyElementClass)
        _standardPropertyElementTypes.emplace(typeId, typedPropertyElementClass);
}

/******************************************************************************
* Creates a new property object for a standard property of this container class.
******************************************************************************/
PropertyPtr PropertyContainerClass::createStandardProperty(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const
{
    PropertyPtr property = createStandardPropertyInternal(init, elementCount, type, containerPath);
    if(property && property->isStandardProperty())
        property->setTitle(standardPropertyTitle(property->typeId()));
    return property;
}

/******************************************************************************
* Creates a new property object for a user-defined property.
******************************************************************************/
PropertyPtr PropertyContainerClass::createUserProperty(DataBuffer::BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, const QStringView name, int type, QStringList componentNames) const
{
    return PropertyPtr::create(init, elementCount, dataType, componentCount, name, type, std::move(componentNames));
}

/******************************************************************************
* Returns the default color for a numeric type ID.
******************************************************************************/
Color PropertyContainerClass::getElementTypeDefaultColor(const OwnerPropertyRef& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const
{
    // Palette of standard colors initially assigned to new element types:
    static const Color defaultTypeColors[] = {
        Color(0.97, 0.97, 0.97),// 0
        Color(1.0,  0.4,  0.4), // 1
        Color(0.4,  0.4,  1.0), // 2
        Color(1.0,  1.0,  0.0), // 3
        Color(1.0,  0.4,  1.0), // 4
        Color(0.4,  1.0,  0.2), // 5
        Color(0.8,  1.0,  0.7), // 6
        Color(0.7,  0.0,  1.0), // 7
        Color(0.2,  1.0,  1.0), // 8
    };
    return defaultTypeColors[std::abs(numericTypeId) % std::size(defaultTypeColors)];
}

/// Creates a new property storage for one of the registered standard properties.
PropertyPtr PropertyContainerClass::createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
                                                                   const ConstDataObjectPath& containerPath) const
{
    return {};
}

/// Determines which elements are located within the given viewport fence region (=2D polygon).
ConstPropertyPtr PropertyContainerClass::viewportFenceSelection(const QVector<Point2>& fence, const ConstDataObjectPath& objectPath,
                                                                Pipeline* pipeline, const Matrix4& projectionTM) const
{
    return {};  // Return empty set to indicate missing fence selection support.
}

}   // End of namespace
