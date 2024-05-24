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
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include "Property.h"
#include "PropertyReference.h"
#include "PropertyContainer.h"

namespace Ovito {

/******************************************************************************
* Constructs a reference to a standard property of a given container type.
******************************************************************************/
PropertyReference::PropertyReference(PropertyContainerClassPtr pclass, int typeId, int vectorComponent) :
    _nameWithComponent(pclass->standardPropertyName(typeId))
{
    OVITO_ASSERT(typeId != Property::GenericUserProperty);
    if(vectorComponent >= 0 && pclass->standardPropertyComponentCount(typeId) > 1) {
        OVITO_ASSERT(vectorComponent < pclass->standardPropertyComponentCount(typeId));
        _nameWithComponent += QChar('.');
        _nameWithComponent += pclass->standardPropertyComponentNames(typeId)[vectorComponent];
    }
}

/******************************************************************************
* Returns the vector component name (if specified).
******************************************************************************/
QStringView PropertyReference::componentName() const
{
    auto dotIndex = _nameWithComponent.indexOf(QChar('.'));
    if(dotIndex != -1)
        return QStringView(_nameWithComponent).mid(dotIndex + 1);
    else
        return QStringView();
}

/******************************************************************************
* Returns the base property name (without the vector component).
******************************************************************************/
QStringView PropertyReference::name() const
{
    auto dotIndex = _nameWithComponent.indexOf(QChar('.'));
    if(dotIndex != -1)
        return QStringView(_nameWithComponent).left(dotIndex);
    else
        return QStringView(_nameWithComponent);
}

/******************************************************************************
* Writes a PropertyReference to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const PropertyReference& r)
{
    stream.beginChunk(0x04);
    stream << r.nameWithComponent();
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a PropertyReference from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, PropertyReference& r)
{
    int version = stream.expectChunkRange(0x02, 2);
    if(version >= 2) {
        stream >> r._nameWithComponent;
    }
    else {
        OvitoClassPtr clazz;
        stream >> clazz;
        PropertyContainerClassPtr containerClass = static_cast<PropertyContainerClassPtr>(clazz);
        int typeId;
        stream >> typeId;
        QString name;
        stream >> name;
        int vectorComponentIndex;
        stream >> vectorComponentIndex;
        QString vectorComponentName;
        if(version >= 1)
            stream >> vectorComponentName;
        if(containerClass && vectorComponentName.isEmpty() && vectorComponentIndex >= 0 && typeId != 0) {
            if(containerClass->isValidStandardPropertyId(typeId)) {
                const QStringList& cmpntNames = containerClass->standardPropertyComponentNames(typeId);
                if(vectorComponentIndex < cmpntNames.size()) {
                    vectorComponentName = cmpntNames[vectorComponentIndex];
                    vectorComponentIndex = -1;
                }
            }
        }
        if(!vectorComponentName.isEmpty())
            r._nameWithComponent = name + QChar('.') + vectorComponentName;
        else if(vectorComponentIndex >= 0)
            r._nameWithComponent = name + QChar('.') + QString::number(vectorComponentIndex + 1);
        else
            r._nameWithComponent = name;
    }
    stream.closeChunk();
    return stream;
}

/******************************************************************************
* Outputs a PropertyReference to a debug stream.
******************************************************************************/
QDebug operator<<(QDebug debug, const PropertyReference& r)
{
    if(r)
        debug.nospace() << "PropertyReference(" << r.nameWithComponent() << ")";
    else
        debug << "PropertyReference(<null>)";
    return debug;
}

/******************************************************************************
* If the referenced property is a standard property from the given container type, returns its numeric ID.
******************************************************************************/
int PropertyReference::standardTypeId(PropertyContainerClassPtr pclass) const
{
    OVITO_ASSERT(pclass);
    return pclass->standardPropertyTypeId(name());
}

/******************************************************************************
* Determines whether this reference is a reference to a particular standard property of the given container type.
******************************************************************************/
bool PropertyReference::isStandardProperty(PropertyContainerClassPtr pclass, int typeId) const
{
    OVITO_ASSERT(pclass);
    OVITO_ASSERT(typeId != 0);
    return name() == pclass->standardPropertyName(typeId);
}

/******************************************************************************
* Determines the numeric vector component index.
******************************************************************************/
int PropertyReference::componentIndex(PropertyContainerClassPtr pclass) const
{
    OVITO_ASSERT(pclass);

    QStringView componentName = this->componentName();
    if(componentName.isEmpty())
        return -1;

    int typeId = pclass->standardPropertyTypeId(name());
    if(typeId != 0) {
        return pclass->standardPropertyComponentNames(typeId).indexOf(componentName);
    }
    else {
        return componentName.toInt() - 1;
    }
}

/******************************************************************************
* Finds the referenced property in the given property container object.
******************************************************************************/
const Property* PropertyReference::findInContainer(const PropertyContainer* container) const
{
    auto n = name();
    if(!n.isEmpty())
        return container->getProperty(n);
    else
        return nullptr;
}

/******************************************************************************
* Finds the referenced property in the given property container object and
* resolves the referenced vector component (if any). If resolution fails,
* an error message is returned in the errorDescription parameter.
******************************************************************************/
std::pair<const Property*, int> PropertyReference::findInContainerWithComponent(const PropertyContainer* container, QString& errorDescription, bool requireComponent) const
{
    return container->findPropertyWithComponent(nameWithComponent(), errorDescription, requireComponent);
}

}   // End of namespace
