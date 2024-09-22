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
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/OwnerPropertyRef.h>
#include <ovito/stdobj/properties/PropertyContainerClass.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include "ElementType.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ElementType);
OVITO_CLASSINFO(ElementType, "DisplayName", "Element type");
DEFINE_PROPERTY_FIELD(ElementType, numericId);
DEFINE_PROPERTY_FIELD(ElementType, name);
DEFINE_PROPERTY_FIELD(ElementType, color);
DEFINE_PROPERTY_FIELD(ElementType, enabled);
DEFINE_PROPERTY_FIELD(ElementType, ownerProperty);
DEFINE_SHADOW_PROPERTY_FIELD(ElementType, name);
DEFINE_SHADOW_PROPERTY_FIELD(ElementType, color);
DEFINE_SHADOW_PROPERTY_FIELD(ElementType, enabled);
SET_PROPERTY_FIELD_LABEL(ElementType, numericId, "Id");
SET_PROPERTY_FIELD_LABEL(ElementType, name, "Name");
SET_PROPERTY_FIELD_LABEL(ElementType, color, "Color");
SET_PROPERTY_FIELD_LABEL(ElementType, enabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ElementType, ownerProperty, "Property");

/******************************************************************************
* Constructor.
******************************************************************************/
void ElementType::initializeObject(ObjectInitializationFlags flags)
{
    DataObject::initializeObject(flags);

    setIdentifier(QString::number(numericId()));
}

/******************************************************************************
* Initializes the element type to default parameter values.
******************************************************************************/
void ElementType::initializeType(const OwnerPropertyRef& property, bool loadUserDefaults)
{
    OVITO_ASSERT(property);

    // Remember the kind of typed property this type belongs to.
    _ownerProperty.set(this, PROPERTY_FIELD(ownerProperty), property);

    // Assign a standard color to this element type.
    // First load the hardcoded default color and freeze it, then load the user-defined default color.
    setColor(getDefaultColor(property, nameOrNumericId(), numericId(), false));
    freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ElementType::color)});
    if(loadUserDefaults)
        setColor(getDefaultColor(property, nameOrNumericId(), numericId(), true));
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ElementType::propertyChanged(const PropertyFieldDescriptor* field)
{
    DataObject::propertyChanged(field);

    if(field == PROPERTY_FIELD(numericId)) {
        // Always keep the data object identifier in sync with the numeric ID.
        setIdentifier(QString::number(numericId()));
    }
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void ElementType::loadFromStream(ObjectLoadStream& stream)
{
    DataObject::loadFromStream(stream);

    // Always keep the data object identifier in sync with the numeric ID.
    setIdentifier(QString::number(numericId()));
}

/******************************************************************************
* Returns the QSettings path for storing or accessing the user-defined
* default values of some ElementType parameter.
******************************************************************************/
QString ElementType::getElementSettingsKey(const OwnerPropertyRef& property, const QString& parameterName, const QString& elementTypeName)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(!parameterName.isEmpty());

    return QStringLiteral("defaults/%1/%2/%3/%4").arg(
        property.containerClass()->pythonName(),
        property.name(),
        parameterName,
        elementTypeName);
}

/******************************************************************************
* Returns the default color for a element type name.
******************************************************************************/
Color ElementType::getDefaultColor(const OwnerPropertyRef& property, const QString& typeName, int numericTypeId, bool loadUserDefaults)
{
    OVITO_ASSERT(!typeName.isEmpty());

    if(!property)
        return PropertyContainer::OOClass().getElementTypeDefaultColor(property, typeName, numericTypeId, loadUserDefaults);

    // Interactive execution context means that we are supposed to load the user-defined
    // settings from the settings store.
    if(loadUserDefaults) {

#ifndef OVITO_DISABLE_QSETTINGS
        // Use the type's name, property type and container class to look up the
        // default color saved by the user.
        QVariant v = QSettings().value(getElementSettingsKey(property, QStringLiteral("color"), typeName));
        if(v.isValid() && v.typeId() == QMetaType::QColor)
            return v.value<Color>();

        // The following is for backward compatibility with OVITO 3.3.5, which used to store the
        // default colors in a different branch of the settings registry.
        if(property.containerClass()->name() == QStringLiteral("Particles")) {
            // Load particle type colors.
            QVariant v = QSettings().value(QStringLiteral("particles/defaults/color/%1/%2").arg(property.typeId()).arg(typeName));
            if(v.isValid() && v.typeId() == QMetaType::QColor)
                return v.value<Color>();
        }
        else if(property.containerClass()->name() == QStringLiteral("Bonds")) {
            // Load bond type colors.
            QVariant v = QSettings().value(QStringLiteral("bonds/defaults/color/%1/%2").arg(property.typeId()).arg(typeName));
            if(v.isValid() && v.typeId() == QMetaType::QColor)
                return v.value<Color>();
        }
        else {
            // Load generic element type colors.
            QVariant v = QSettings().value(QStringLiteral("defaults/color/%1/%2").arg(property.typeId()).arg(typeName));
            if(v.isValid() && v.typeId() == QMetaType::QColor)
                return v.value<Color>();
        }
#endif
    }

    // Otherwise fall back to a hard-coded default colors provided by the property container class.
    return property.containerClass()->getElementTypeDefaultColor(property, typeName, numericTypeId, loadUserDefaults);
}

/******************************************************************************
* Changes the default color for an element type name.
******************************************************************************/
void ElementType::setDefaultColor(const OwnerPropertyRef& property, const QString& typeName, const Color& color)
{
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    QString settingsKey = getElementSettingsKey(property, QStringLiteral("color"), typeName);

    if(getDefaultColor(property, typeName, 0, false).equals(color, 1.0/256.0) == false) {
        settings.setValue(settingsKey, QVariant::fromValue(static_cast<QColor>(color)));
    }
    else {
        settings.remove(settingsKey);
    }
#endif
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void ElementType::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath, bool forceProxyReplacement) const
{
    // Note: 'this' may no longer exist at this point, because the sub-class implementation of the method may
    // have already replaced it with a mutable copy.
    const ElementType* self = static_object_cast<ElementType>(dataPath.back());

    if(self->editableProxy() && !forceProxyReplacement) {
        const ElementType* proxy = static_object_cast<ElementType>(self->editableProxy());

        // The numeric ID of a type and some other attributes should never change.
        OVITO_ASSERT(proxy->numericId() == self->numericId());

        if(proxy->name() != self->name() || proxy->color() != self->color() || proxy->enabled() != self->enabled()) {
            // Make this data object mutable first.
            ElementType* mutableSelf = static_object_cast<ElementType>(state.makeMutableInplace(dataPath));

            mutableSelf->setName(proxy->name());
            mutableSelf->setColor(proxy->color());
            mutableSelf->setEnabled(proxy->enabled());
        }
    }
    else {
        // Create and initialize a new proxy.
        OORef<ElementType> newProxy = CloneHelper::cloneSingleObject(self, false);
        OVITO_ASSERT(newProxy->numericId() == self->numericId());
        OVITO_ASSERT(newProxy->enabled() == self->enabled());
        newProxy->setEditableProxy(nullptr);

        // Make this element type mutable and attach the proxy object to it.
        state.makeMutableInplace(dataPath)->setEditableProxy(std::move(newProxy));
    }

    DataObject::updateEditableProxies(state, dataPath, forceProxyReplacement);
}

}   // End of namespace
