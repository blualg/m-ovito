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


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>

namespace Ovito {

/**
 * \brief A reference to a particular property, i.e., this class provides a way to refer
 *        to a property that may not exist yet, because it will be computed by the pipeline.
 *
 * This class is a simple wrapper for a QString holding the name of the referenced property
 * followed by an optional vector component (e.g. "Position.X"). Thus, it is possible to
 * refer to a specific component of a vector property.
 *
 * The PropertyReference class is used in various places in OVITO to refer to properties in a
 * generic way, without needing an actual Property object.
 */
class OVITO_STDOBJ_EXPORT PropertyReference
{
public:

    /// \brief Default constructor. Creates a null reference.
    PropertyReference() = default;

    /// \brief Constructs a reference to a property.
    PropertyReference(const QString& nameWithComponent) : _nameWithComponent(nameWithComponent) {}

    /// \brief Constructs a reference based on an existing Property.
    PropertyReference(const Property* property, int vectorComponent = -1) : _nameWithComponent(property->nameWithComponent(vectorComponent)) { OVITO_ASSERT(property != nullptr); }

    /// \brief Constructs a reference to a standard property of a given container type.
    PropertyReference(PropertyContainerClassPtr pclass, int typeId, int vectorComponent = -1);

    /// \brief Gets the human-readable name of the referenced property including the optional vector component name.
    const QString& nameWithComponent() const { return _nameWithComponent; }

    /// \brief If the referenced property is a standard property from the given container type, returns its numeric ID.
    int standardTypeId(PropertyContainerClassPtr pclass) const;

    /// \brief Determines whether this reference is a reference to a particular standard property of the given container type.
    bool isStandardProperty(PropertyContainerClassPtr pclass, int typeId) const;

    /// \brief Returns the vector component name (if specified).
    QStringView componentName() const;

    /// \brief Determines the numeric vector component index.
    int componentIndex(PropertyContainerClassPtr pclass) const;

    /// \brief Returns the base property name (without the vector component).
    QStringView name() const;

    /// \brief Compares two references for equality.
    bool operator==(const PropertyReference& other) const {
        return nameWithComponent() == other.nameWithComponent();
    }

    /// \brief Compares two references for inequality.
    bool operator!=(const PropertyReference& other) const { return !(*this == other); }

    /// \brief Strict ordering function.
    bool operator<(const PropertyReference& other) const { return nameWithComponent() < other.nameWithComponent(); }

    /// \brief Returns whether this reference refers to any property.
    /// \return false if this is a default-constructed PropertyReference.
    explicit operator bool() const { return !nameWithComponent().isEmpty(); }

    /// Finds the referenced property in the given property container object.
    const Property* findInContainer(const PropertyContainer* container) const;

    /// Finds the referenced property in the given property container object and
    /// resolves the referenced vector component (if any). If resolution fails,
    /// an error message is returned in the errorDescription parameter.
    std::pair<const Property*, int> findInContainerWithComponent(const PropertyContainer* container, QString& errorDescription, bool requireComponent = true) const;

private:

    /// The human-readable name of the property being referenced, including an optional vector component.
    QString _nameWithComponent;

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

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::PropertyReference);
