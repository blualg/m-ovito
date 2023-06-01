////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>

namespace Ovito::StdObj {

/**
 * \brief A meta-class for property containers (i.e. classes derived from the PropertyContainer base class).
 */
 class OVITO_STDOBJ_EXPORT PropertyContainerClass : public DataObject::OOMetaClass
{
public:

    /// Inherit standard constructor from base meta class.
    using DataObject::OOMetaClass::OOMetaClass;

    /// Returns a human-readable name used for the property class in the user interface, e.g. 'Particles' or 'Bonds'.
    const QString& propertyClassDisplayName() const { return _propertyClassDisplayName; }

    /// Returns a human-readable name describing the data elements of this property class in the user interface, e.g. 'particles' or 'bonds'.
    const QString& elementDescriptionName() const { return _elementDescriptionName; }

    /// Returns the name by which this property class is referred to from Python scripts.
    const QString& pythonName() const { return _pythonName; }

    /// Creates a new property storage for one of the registered standard properties.
    virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const { return {}; }

    /// Creates a new property object for a standard property of this container class.
    PropertyPtr createStandardProperty(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath = ConstDataObjectPath{}) const;

    /// Creates a new property object for a user-defined property.
    PropertyPtr createUserProperty(DataBuffer::BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, const QString& name, int type = 0, QStringList componentNames = QStringList()) const {
        return PropertyPtr::create(init, elementCount, dataType, componentCount, name, type, std::move(componentNames));
    }

    /// Indicates whether this kind of property container supports picking of individual elements in the viewports.
    virtual bool supportsViewportPicking() const { return false; }

    /// Returns the index of the data element that was picked in a viewport.
    virtual std::pair<size_t, ConstDataObjectPath> elementFromPickResult(const ViewportPickResult& pickResult) const {
        return std::pair<size_t, ConstDataObjectPath>(std::numeric_limits<size_t>::max(), ConstDataObjectPath{});
    }

    /// Tries to remap an index from one property container to another, considering the possibility that
    /// data elements may have been added or removed.
    virtual size_t remapElementIndex(const ConstDataObjectPath& source, size_t elementIndex, const ConstDataObjectPath& dest) const {
        return std::numeric_limits<size_t>::max();
    }

    /// Determines which elements are located within the given viewport fence region (=2D polygon).
    virtual boost::dynamic_bitset<> viewportFenceSelection(const QVector<Point2>& fence, const ConstDataObjectPath& objectPath, PipelineSceneNode* node, const Matrix4& projectionTM) const {
        return boost::dynamic_bitset<>{}; // Return empty set to indicate missing fence selection support.
    }

    /// This method is called by InputColumnMapping::validate() to let the container class perform custom checks
    /// on the mapping of the file data columns to internal properties.
    virtual void validateInputColumnMapping(const InputColumnMapping& mapping) const {}

    /// Returns a default color for an ElementType given its numeric type ID.
    virtual Color getElementTypeDefaultColor(const PropertyReference& property, const QString& typeName, int numericTypeId, bool loadUserDefaults) const;

    /// Determines whether a standard property ID is defined for this property class.
    bool isValidStandardPropertyId(int id) const {
        return _standardPropertyNames.find(id) != _standardPropertyNames.end();
    }

    /// Returns the standard property type ID from a property name.
    int standardPropertyTypeId(const QString& name) const {
        if(auto item = _standardPropertyIds.find(name); item != _standardPropertyIds.end())
            return item.value();
        else
            return 0;
    }

    /// Returns the name of a standard property type.
    const QString& standardPropertyName(int typeId) const {
        OVITO_ASSERT(isValidStandardPropertyId(typeId));
        return _standardPropertyNames.find(typeId)->second;
    }

    /// Returns the display title used for a standard property type.
    const QString& standardPropertyTitle(int typeId) const {
        OVITO_ASSERT(_standardPropertyTitles.find(typeId) != _standardPropertyTitles.end());
        return _standardPropertyTitles.find(typeId)->second;
    }

    /// Returns the data type used by the given standard property type.
    int standardPropertyDataType(int typeId) const {
        OVITO_ASSERT(_standardPropertyDataTypes.find(typeId) != _standardPropertyDataTypes.end());
        return _standardPropertyDataTypes.find(typeId)->second;
    }

    /// Returns the number of vector components per element used by the given standard property type.
    size_t standardPropertyComponentCount(int typeId) const {
        OVITO_ASSERT(_standardPropertyComponents.find(typeId) != _standardPropertyComponents.end());
        return std::max((size_t)_standardPropertyComponents.find(typeId)->second.size(), (size_t)1);
    }

    /// Returns the list of component names for the given standard property type.
    const QStringList& standardPropertyComponentNames(int typeId) const {
        OVITO_ASSERT(_standardPropertyComponents.find(typeId) != _standardPropertyComponents.end());
        return _standardPropertyComponents.find(typeId)->second;
    }

    /// Returns the mapping from standard property names to standard property type IDs.
    const QMap<QString, int>& standardPropertyIds() const {
        return _standardPropertyIds;
    }

    /// Returns whether the given standard property is a typed property.
    bool isTypedProperty(int typeId) const {
        return _standardPropertyElementTypes.find(typeId) != _standardPropertyElementTypes.end();
    }

    /// Returns the ElementType class that is used by the given typed property.
    OvitoClassPtr typedPropertyElementClass(int typeId) const {
        if(auto iter = _standardPropertyElementTypes.find(typeId); iter != _standardPropertyElementTypes.end())
            return iter->second;
        else
            return {};
    }

protected:

    /// Is called by the system after construction of the meta-class instance.
    virtual void initialize() override;

    /// Registers a new standard property with this property meta class.
    void registerStandardProperty(int typeId, QString name, int dataType, QStringList componentNames, OvitoClassPtr typedPropertyElementClass = {}, QString title = QString());

    /// Sets the human-readable name used for the property class in the user interface.
    void setPropertyClassDisplayName(const QString& name) { _propertyClassDisplayName = name; }

    /// Set the human-readable name describing the data elements of this property class in the user interface, e.g. 'particles' or 'bonds'.
    void setElementDescriptionName(const QString& name) { _elementDescriptionName = name; }

    /// Sets the name by which this property class is referred to from Python scripts.
    void setPythonName(const QString& name) { _pythonName = name; }

private:

    /// The human-readable display name of this property class used in the user interface,
    /// e.g. 'Particles' or 'Bonds'.
    QString _propertyClassDisplayName;

    /// The name of the elements described by the properties of this class, e.g. 'particles' or 'bonds'.
    QString _elementDescriptionName;

    /// The name by which this property class is referred to from Python scripts.
    QString _pythonName;

    /// Mapping from standard property names to standard property type IDs.
    QMap<QString, int> _standardPropertyIds;

    /// Mapping from standard property type ID to standard property names.
    boost::container::flat_map<int, QString> _standardPropertyNames;

    /// Mapping from standard property type ID to standard property title strings.
    boost::container::flat_map<int, QString> _standardPropertyTitles;

    /// Mapping from standard property type ID to property component names.
    boost::container::flat_map<int, QStringList> _standardPropertyComponents;

    /// Mapping from standard property type ID to property data type.
    boost::container::flat_map<int, int> _standardPropertyDataTypes;

    /// Stores the IDs of all typed standard properties and the corresponding ElementType class.
    boost::container::flat_map<int, OvitoClassPtr> _standardPropertyElementTypes;
};

/**
 * Utility class that behaves like a BufferAccessAndRef but additionally allocates a new PropertyObject of the given size upon construction.
*/
template<typename T>
class PropertyFactory : public detail::BufferAccessTyped<T, PropertyObject, true, access_mode::discard_write>
{
    using base_class = detail::BufferAccessTyped<T, PropertyObject, true, access_mode::discard_write>;
    using base_class::ComponentWise;
    using typename base_class::element_type;

public:

    /// Constructor allocating a new uninitialized user property array of the given size and name.
    template<bool IsEnabled = !ComponentWise>
    PropertyFactory(const std::enable_if_t<IsEnabled, PropertyContainerClass>& containerClazz, size_t elementCount, const QString& propertyName) :
        base_class(containerClazz.createUserProperty(
            DataBuffer::Uninitialized, elementCount,
            DataBufferPrimitiveType<element_type>::value,
            DataBufferPrimitiveComponentCount<element_type>::value,
            propertyName)) {}

    /// Constructor allocating a new uninitialized vector user property array of the given size and name.
    template<bool IsEnabled = ComponentWise>
    PropertyFactory(const std::enable_if_t<IsEnabled, PropertyContainerClass>& containerClazz, size_t elementCount, const QString& propertyName, size_t componentCount, QStringList componentNames = QStringList()) :
        base_class(containerClazz.createUserProperty(
            DataBuffer::Uninitialized, elementCount,
            DataBufferPrimitiveType<element_type>::value,
            componentCount,
            propertyName,
            0, // type = user property
            std::move(componentNames))) {
        static_assert(DataBufferPrimitiveComponentCount<element_type>::value == 1);
    }
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::StdObj::PropertyContainerClassPtr);
