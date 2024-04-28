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

namespace Ovito {

/**
 * \brief A reference to a particular property, i.e., a way to refer to a property that may not exist yet.
 */
class OVITO_STDOBJ_EXPORT PropertyReference
{
public:

    /// \brief Default constructor. Creates a null reference.
    PropertyReference() = default;

    /// \brief Constructs a reference to a standard property.
    PropertyReference(PropertyContainerClassPtr pclass, int typeId, int vectorComponentIndex = -1);

    /// \brief Constructs a reference to a user-defined property.
    PropertyReference(PropertyContainerClassPtr pclass, const QString& name, int vectorComponentIndex = -1);

    /// \brief Constructs a reference to a named vector component of a user-defined property.
    PropertyReference(PropertyContainerClassPtr pclass, const QString& name, const QString& vectorComponentName);

    /// \brief Constructs a reference based on an existing Property.
    PropertyReference(PropertyContainerClassPtr pclass, const Property* property, int vectorComponentIndex = -1);

    /// \brief Returns the kind of property being referenced (standard property or user-defined property).
    int typeId() const { return _typeId; }

    /// \brief Indicates whether the referenced property is a standard property (and not a user-defined property).
    bool isStandardProperty() const { return typeId() != 0; }

    /// \brief Gets the human-readable name of the referenced property.
    const QString& name() const { return _name; }

    /// Return the class of the referenced property.
    PropertyContainerClassPtr containerClass() const { return _containerClass; }

    /// Returns the selected component index.
    int vectorComponentIndex() const { return _vectorComponentIndex; }

    /// Returns the selected vector component name (if known).
    const QString& vectorComponentName() const { return _vectorComponentName; }

    /// \brief Compares two references for equality.
    bool operator==(const PropertyReference& other) const {
        if(containerClass() != other.containerClass()) return false;
        if(typeId() != other.typeId()) return false;
        if(vectorComponentIndex() != other.vectorComponentIndex()) return false;
        if(vectorComponentName() != other.vectorComponentName()) return false;
        if(typeId() != 0) return true;
        return name() == other.name();
    }

    /// \brief Compares two references for inequality.
    bool operator!=(const PropertyReference& other) const { return !(*this == other); }

    /// \brief Strict ordering function.
    bool operator<(const PropertyReference& other) const;

    /// \brief Returns true if this reference does not point to any property.
    /// \return true if this is a default-constructed PropertyReference.
    bool isNull() const { return typeId() == 0 && name().isEmpty(); }

    /// \brief Returns whether this reference refers to any property.
    /// \return false if this is a default-constructed PropertyReference.
    explicit operator bool() const { return !isNull(); }

    /// \brief Returns the display name of the referenced property including the optional vector component.
    QString nameWithComponent() const;

    /// Finds the referenced property in the given property container object.
    const Property* findInContainer(const PropertyContainer* container) const;

    /// Finds the referenced property in the given property container object and
    /// resolves the referenced vector component (if any). If resolution fails,
    /// an error message is returned in the errorDescription parameter.
    std::pair<const Property*, int> findInContainerWithComponent(const PropertyContainer* container, QString& errorDescription) const;

    /// Returns a new property reference that uses the same name as the current one, but with a different property container class.
    PropertyReference convertToContainerClass(PropertyContainerClassPtr containerClass) const;

private:

    /// The class of property container.
    PropertyContainerClassPtr _containerClass = nullptr;

    /// The container-specific standard identifier of the property being referenced.
    int _typeId = 0;

    /// The zero-based component index if the property is a vector property.
    /// Can be negative or zero if not a vector property.
    int _vectorComponentIndex = -1;

    /// The human-readable name of the property being referenced.
    QString _name;

    /// The human-readable component name of the vector property.
    /// Used if the numeric vector component index is not known.
    QString _vectorComponentName;

    friend OVITO_STDOBJ_EXPORT SaveStream& operator<<(SaveStream& stream, const PropertyReference& r);
    friend OVITO_STDOBJ_EXPORT LoadStream& operator>>(LoadStream& stream, PropertyReference& r);
};

/// Writes a PropertyReference to an output stream.
/// \relates PropertyReference
extern OVITO_STDOBJ_EXPORT SaveStream& operator<<(SaveStream& stream, const PropertyReference& r);

/// Reads a PropertyReference from an input stream.
/// \relates PropertyReference
extern OVITO_STDOBJ_EXPORT LoadStream& operator>>(LoadStream& stream, PropertyReference& r);

/// Outputs a PropertyReference to a debug stream.
/// \relates PropertyReference
extern OVITO_STDOBJ_EXPORT QDebug operator<<(QDebug debug, const PropertyReference& r);

/**
 * Encapsulates a reference to a property from a specific container class.
 */
template<class PropertyContainerType>
class TypedPropertyReference : public PropertyReference
{
public:

    /// \brief Default constructor. Creates a null reference.
    TypedPropertyReference() = default;

    /// \brief Conversion copy constructor.
    TypedPropertyReference(const PropertyReference& other) : PropertyReference(other) {}

    /// \brief Conversion move constructor.
    TypedPropertyReference(PropertyReference&& other) : PropertyReference(std::move(other)) {}

    /// \brief Constructs a reference to a standard property.
    TypedPropertyReference(int typeId, int vectorComponentIndex = -1) : PropertyReference(&PropertyContainerType::OOClass(), typeId, vectorComponentIndex) {}

    /// \brief Constructs a reference to a user-defined property.
    TypedPropertyReference(const QString& name, int vectorComponentIndex = -1) : PropertyReference(&PropertyContainerType::OOClass(), name, vectorComponentIndex) {}

    /// \brief Constructs a reference to a named vector component of a user-defined property.
    TypedPropertyReference(const QString& name, const QString& vectorComponentName) : PropertyReference(&PropertyContainerType::OOClass(), name, vectorComponentName) {}

    /// \brief Constructs a reference based on an existing Property.
    TypedPropertyReference(const Property* property, int vectorComponentIndex = -1) : PropertyReference(&PropertyContainerType::OOClass(), property, vectorComponentIndex) {}

    /// \brief Compares two references for equality.
    bool operator==(const TypedPropertyReference& other) const { return PropertyReference::operator==(other); }

    /// \brief Compares two references for inequality.
    bool operator!=(const TypedPropertyReference& other) const { return PropertyReference::operator!=(other); }

    /// \brief Strict ordering function.
    bool operator<(const TypedPropertyReference& other) const { return PropertyReference::operator<(other); }

    friend SaveStream& operator<<(SaveStream& stream, const TypedPropertyReference& r) {
        return stream << static_cast<const PropertyReference&>(r);
    }

    friend LoadStream& operator>>(LoadStream& stream, TypedPropertyReference& r) {
        return stream >> static_cast<PropertyReference&>(r);
    }

    friend QDebug operator<<(QDebug debug, const TypedPropertyReference& r) {
        return debug << static_cast<const PropertyReference&>(r);
    }
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::PropertyReference);
