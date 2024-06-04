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


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/OwnerPropertyRef.h>
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief Describes the basic properties (unique ID, name & color) of a "type" of elements stored in a Property.
 *        This serves as generic base class for particle types, bond types, structural types, etc.
 */
class OVITO_STDOBJ_EXPORT ElementType : public DataObject
{
    OVITO_CLASS(ElementType)

public:

    /// \brief Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Initializes the element type to default parameter values.
    virtual void initializeType(const OwnerPropertyRef& property, bool loadUserDefaults = ExecutionContext::isInteractive());

    /// Creates an editable proxy object for this DataObject and synchronizes its parameters.
    virtual void updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const override;

    /// Returns the name of this type, or a dynamically generated string representing the
    /// numeric ID if the type has no assigned name.
    QString nameOrNumericId() const {
        if(!name().isEmpty())
            return name();
        else
            return generateDefaultTypeName(numericId());
    }

    /// Returns an automatically generated name for a type based on its numeric ID.
    static QString generateDefaultTypeName(int id) {
        return tr("Type %1").arg(id);
    }

    /// Returns the title of this object. Same as nameOrNumericId().
    virtual QString objectTitle() const override { return nameOrNumericId(); }

    /// Returns the default color for a named element type.
    static Color getDefaultColor(const OwnerPropertyRef& property, const QString& typeName, int numericTypeId, bool loadUserDefaults = ExecutionContext::isInteractive());

    /// Changes the default color for a named element type.
    static void setDefaultColor(const OwnerPropertyRef& property, const QString& typeName, const Color& color);

    /// Returns the QSettings path for storing or accessing the user-defined
    /// default values of some ElementType parameter.
    static QString getElementSettingsKey(const OwnerPropertyRef& property, const QString& parameterName, const QString& elementTypeName);

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

    /// Stores the unique numeric identifier of the type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, numericId, setNumericId);

    /// The human-readable name assigned to this type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, name, setName);
    DECLARE_SHADOW_PROPERTY_FIELD(name);

    /// Stores the visualization color of the type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{1,1,1}), color, setColor, PROPERTY_FIELD_MEMORIZE);
    DECLARE_SHADOW_PROPERTY_FIELD(color);

    /// Stores whether this type is "enabled" or "disabled".
    /// This makes only sense in some sorts of types. For example, structure identification modifiers
    /// use this field to determine which structural types they should look for.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, enabled, setEnabled);
    DECLARE_SHADOW_PROPERTY_FIELD(enabled);

    /// Stores a reference to the property object this element type belongs to.
    DECLARE_PROPERTY_FIELD(OwnerPropertyRef{}, ownerProperty);
};

}   // End of namespace
