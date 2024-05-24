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
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/stdobj/properties/ElementType.h>

namespace Ovito {

/**
 * \brief Stores a property data array.
 */
class OVITO_STDOBJ_EXPORT Property : public DataBuffer
{
public:

    /// Define a new data object metaclass.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public DataBuffer::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DataBuffer::OOMetaClass::OOMetaClass;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override;
    };

    OVITO_CLASS_META(Property, OOMetaClass);

public:

    /// \brief The standard property type identifiers used by all property container classes.
    enum GenericStandardType {
        GenericUserProperty = 0,    //< This is reserved for user-defined properties.
        GenericSelectionProperty = 1,
        GenericColorProperty = 2,
        GenericTypeProperty = 3,
        GenericIdentifierProperty = 4,

        // This is the value at which type identifiers of specific property container classes start:
        FirstSpecificProperty = 1000
    };

public:

    /// \brief Creates an empty property array.
    explicit Property(ObjectInitializationFlags flags);

    /// \brief Constructor creating a new property array.
    Property(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, const QStringView name, int typeId = 0, QStringList componentNames = QStringList());

    /// \brief Constructor creating a new property array.
    Property(ObjectInitializationFlags flags, size_t elementCount, int dataType, size_t componentCount, const QStringView name, int typeId = 0, QStringList componentNames = QStringList()) :
        Property(flags, BufferInitialization::Uninitialized, elementCount, dataType, componentCount, name, typeId, std::move(componentNames)) {}

#ifdef OVITO_DEBUG
    /// \brief Destructor.
    ~Property();
#endif

    /// \brief Gets the property's name, which is also the property object's unique identifier.
    const QString& name() const { return identifier(); }

    /// \brief Sets the property's name, which is also the property object's unique identifier.
    void setName(const QString& name) { setIdentifier(name); }

    /// \brief Sets the property's name, which is also the property object's unique identifier.
    void setName(const QStringView name) { setIdentifier(name.toString()); }

    /// \brief Returns the kind of this property (standard or user-defined).
    int typeId() const { return _typeId; }

    /// \brief Changes the type identifier of this property. Note that this method is only for internal use.
    ///        Normally, you should not change the type ID of a property after it has been created.
    void setTypeId(int newTypeId) { _typeId = newTypeId; }

    /// \brief Indicates whether this property is a standard property (and not a user-defined property).
    bool isStandardProperty() const { return typeId() != 0; }

    /// \brief Returns the display name of the property including the name of the given
    ///        vector component.
    QString nameWithComponent(int vectorComponent) const;

    /// Checks if this property storage and its contents exactly match those of another property storage.
    bool equals(const Property& other) const;

    /// Creates an empty copy of this property object - without copying the stored array data but cloning the metadata and list of element types.
    PropertyPtr cloneWithoutData(size_t newSize = 0) const;

    //////////////////////////////// Element types //////////////////////////////

    /// Returns true if this property has some element types attached and the data type is 'int'.
    bool isTypedProperty() const { return !elementTypes().empty() && dataType() == DataBuffer::Int32 && componentCount() == 1; }

    /// Appends an element type to the list of types.
    const ElementType* addElementType(const ElementType* type) {
        OVITO_ASSERT(elementTypes().contains(const_cast<ElementType*>(type)) == false);
        _elementTypes.push_back(this, PROPERTY_FIELD(elementTypes), type);
        return type;
    }

    /// Inserts an element type into the list of types.
    void insertElementType(qsizetype index, DataOORef<const ElementType> type) {
        OVITO_ASSERT(elementTypes().contains(type) == false);
        _elementTypes.insert(this, PROPERTY_FIELD(elementTypes), index, std::move(type));
    }

    /// Creates and returns a new numeric element type with the given numeric ID and, optionally, a human-readable name.
    /// If an element type with the given numeric ID already exists in this property's element type list, it will be returned instead.
    const ElementType* addNumericType(const PropertyContainerClass& containerClass, int id, const QString& name = {}, OvitoClassPtr elementTypeClass = {});

    /// Creates and returns a new element type with the given name and assigns a new unique ID to it.
    /// If an element type with the given name already exists in this property's element type list, it will be returned instead.
    const ElementType* addNamedType(const PropertyContainerClass& containerClass, const QString& name, OvitoClassPtr elementTypeClass = {}) {
        if(const ElementType* existingType = elementType(name))
            return existingType;
        return addNumericType(containerClass, generateUniqueElementTypeId(), name, elementTypeClass);
    }

    /// Creates and returns a new element type with the given name and assigns a new unique ID to it.
    /// If an element type with the given name already exists in this property's element type list, it will be returned instead.
    const ElementType* addNamedType(const PropertyContainerClass& containerClass, const QLatin1String& name, OvitoClassPtr elementTypeClass = {}) {
        if(const ElementType* existingType = elementType(name))
            return existingType;
        return addNumericType(containerClass, generateUniqueElementTypeId(), name, elementTypeClass);
    }

    /// Returns the element type with the given ID, or NULL if no such type exists.
    const ElementType* elementType(int id) const {
        for(const ElementType* type : elementTypes())
            if(type->numericId() == id)
                return type;
        return nullptr;
    }

    /// Returns the element type with the given human-readable name, or NULL if no such type exists.
    const ElementType* elementType(const QString& name) const {
        OVITO_ASSERT(!name.isEmpty());
        for(const ElementType* type : elementTypes())
            if(type->name() == name)
                return type;
        return nullptr;
    }

    /// Returns the element type with the given human-readable name, or NULL if no such type exists.
    const ElementType* elementType(const QLatin1String& name) const {
        OVITO_ASSERT(name.size() != 0);
        for(const ElementType* type : elementTypes())
            if(type->name() == name)
                return type;
        return nullptr;
    }

    /// Removes a single element type from this object.
    void removeElementType(qsizetype index) {
        _elementTypes.remove(this, PROPERTY_FIELD(elementTypes), index);
    }

    /// Removes all elements types from this object.
    void clearElementTypes() {
        _elementTypes.clear(this, PROPERTY_FIELD(elementTypes));
    }

    /// Builds a mapping from numeric IDs to type colors.
    std::map<int, ColorG> typeColorMap(bool onlyEnabledTypes = true) const {
        std::map<int, ColorG> m;
        for(const ElementType* type : elementTypes()) {
            if(type->enabled() || !onlyEnabledTypes)
                m.insert({type->numericId(), type->color().toDataType<GraphicsFloatType>()});
        }
        return m;
    }

    /// Returns an numeric type ID that is not yet used by any of the existing element types.
    int generateUniqueElementTypeId(int startAt = 1) const {
        int maxId = startAt;
        for(const ElementType* type : elementTypes())
            maxId = std::max(maxId, type->numericId() + 1);
        return maxId;
    }

    /// Sorts the element types with respect to the numeric identifier.
    void sortElementTypesById();

    /// Sorts the types w.r.t. their name.
    /// This method is used by file parsers that create element types on the
    /// go while the read the data. In such a case, the type ordering
    /// depends on the storage order of data elements in the loaded file, which is not desirable.
    void sortElementTypesByName();

    /// Helper method that remaps the existing type IDs to a contiguous range starting at the given
    /// base ID. This method is mainly used for file output, because some file formats
    /// work with numeric particle types only, which must form a contiguous range.
    /// The method returns the mapping of output type IDs to original type IDs
    /// and a copy of the property array in which the original type ID values have
    /// been remapped to the output IDs.
    std::tuple<std::map<int,int>, ConstPropertyPtr> generateContiguousTypeIdMapping(int baseId = 1) const;

    /// Returns whether this data object wants to be shown in the pipeline editor
    /// under the data source section.
    /// This implementation returns true only it this is a typed property, i.e. if the 'elementTypes' list contains
    /// some elements. In this case we want the property to appear in the pipeline editor so that the user can
    /// edit the individual types.
    virtual bool showInPipelineEditor() const override {
        return !elementTypes().empty();
    }

    /// Creates an editable proxy object for this DataObject and synchronizes its parameters.
    virtual void updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const override;

    /// Returns the display title of this property object in the user interface.
    virtual QString objectTitle() const override;

    /// Throws an exception with an informative text if the given name is not a valid name for an OVITO property.
    /// For example, to be valid, the name must not contains any dots.
    static void throwIfInvalidPropertyName(const QStringView name);

    /// Performs name mangeling if necessary to turn the given name into a valid property name.
    static QString makePropertyNameValid(const QString& name);

public:

    /// Indicates that there current exists a Numpy view referencing this property's memory buffer.
    bool isBeingAccessedFromPython() const { return !_pythonAccessGuard.expired(); }

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Contains the list of defined "types" if this is a typed property.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const ElementType>, elementTypes, setElementTypes);

    /// The user-interface title of this property.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);

    /// The kind of this property (non-zero = predefined standard property; zero = a user-defined property).
    int _typeId = 0;

    /// Pointer to the access guard object while the Python side accesses this property's memory buffer.
    std::weak_ptr<BufferPythonAccessGuard> _pythonAccessGuard;

    friend class BufferPythonAccessGuard;
};

/// Smart-pointer to a mutable Property.
using PropertyPtr = DataOORef<Property>;

/// Smart-pointer to a read-only Property.
using ConstPropertyPtr = DataOORef<const Property>;

/// A data object reference to a Property in a data collection.
using PropertyDataObjectReference = TypedDataObjectReference<Property>;

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::PropertyDataObjectReference);
