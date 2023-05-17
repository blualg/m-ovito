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
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/dataset/data/DataBufferAccess.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/stdobj/properties/ElementType.h>

namespace Ovito::StdObj {

/**
 * \brief Stores a property data array.
 */
class OVITO_STDOBJ_EXPORT PropertyObject : public DataBuffer
{
public:

    /// Define a new property metaclass for particle containers.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public DataBuffer::OOMetaClass
    {
    public:
        /// Inherit constructor from base class.
        using DataBuffer::OOMetaClass::OOMetaClass;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override;
    };

    OVITO_CLASS_META(PropertyObject, OOMetaClass);
    Q_CLASSINFO("DisplayName", "Property");

public:

    /// \brief The standard property types defined by all property classes.
    enum GenericStandardType {
        GenericUserProperty = 0,    //< This is reserved for user-defined properties.
        GenericSelectionProperty = 1,
        GenericColorProperty = 2,
        GenericTypeProperty = 3,
        GenericIdentifierProperty = 4,

        // This is value at which type IDs of specific standard properties start:
        FirstSpecificProperty = 1000
    };

public:

    /// \brief Creates an empty property array.
    Q_INVOKABLE PropertyObject(ObjectInitializationFlags flags);

    /// \brief Constructor creating a new property array.
    PropertyObject(ObjectInitializationFlags flags, BufferInitialization init, size_t elementCount, int dataType, size_t componentCount, const QString& name, int type = 0, QStringList componentNames = QStringList());

    /// \brief Constructor creating a new property array.
    PropertyObject(ObjectInitializationFlags flags, size_t elementCount, int dataType, size_t componentCount, const QString& name, int type = 0, QStringList componentNames = QStringList()) :
        PropertyObject(flags, BufferInitialization::Uninitialized, elementCount, dataType, componentCount, name, type, std::move(componentNames)) {}

    /// \brief Gets the property's name.
    /// \return The name of property.
    const QString& name() const { return _name; }

    /// \brief Sets the property's name.
    /// \param name The new name string.
    void setName(const QString& name);

    /// \brief Returns the type of this property.
    int type() const { return _type; }

    /// \brief Changes the type of this property. Note that this method is only for internal use.
    ///        Normally, you should not change the type of a property once it was created.
    void setType(int newType) { _type = newType; }

    /// \brief Returns the display name of the property including the name of the given
    ///        vector component.
    QString nameWithComponent(int vectorComponent) const {
        if(componentCount() <= 1 || vectorComponent < 0)
            return name();
        else if(vectorComponent < componentNames().size())
            return QStringLiteral("%1.%2").arg(name()).arg(componentNames()[vectorComponent]);
        else
            return QStringLiteral("%1.%2").arg(name()).arg(vectorComponent + 1);
    }

    /// Creates a copy of the array, not containing those elements for which
    /// the corresponding bits in the given bit array were set.
    OORef<PropertyObject> filterCopy(const boost::dynamic_bitset<>& mask) const {
        return static_object_cast<PropertyObject>(DataBuffer::filterCopy(mask));
    }

    /// Checks if this property storage and its contents exactly match those of another property storage.
    bool equals(const PropertyObject& other) const;


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
    const ElementType* insertElementType(int index, const ElementType* type) {
        OVITO_ASSERT(elementTypes().contains(const_cast<ElementType*>(type)) == false);
        _elementTypes.insert(this, PROPERTY_FIELD(elementTypes), index, type);
        return type;
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
    void removeElementType(int index) {
        _elementTypes.remove(this, PROPERTY_FIELD(elementTypes), index);
    }

    /// Removes all elements types from this object.
    void clearElementTypes() {
        _elementTypes.clear(this, PROPERTY_FIELD(elementTypes));
    }

    /// Builds a mapping from numeric IDs to type colors.
    std::map<int,Color> typeColorMap() const {
        std::map<int,Color> m;
        for(const ElementType* type : elementTypes())
            m.insert({type->numericId(), type->color()});
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

    ////////////////////////////// Support functions for the Python bindings //////////////////////////////

    /// Indicates to the Python binding layer that this property object has been temporarily put into a
    /// writable state. In this state, the binding layer will allow write access to the property's internal data.
    bool isWritableFromPython() const { return _isWritableFromPython != 0; }

    /// Puts the property array into a writable state.
    /// In the writable state, the Python binding layer will allow write access to the property's internal data.
    void makeWritableFromPython();

    /// Puts the property array back into the default read-only state.
    /// In the read-only state, the Python binding layer will not permit write access to the property's internal data.
    void makeReadOnlyFromPython();

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

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

private:

    /// Contains the list of defined "types" if this is a typed property.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const ElementType>, elementTypes, setElementTypes);

    /// The user-interface title of this property.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);

    /// The type of this property.
    int _type = 0;

    /// The name of the property.
    QString _name;

    /// This is a special flag used by the Python bindings to indicate that
    /// this property object has been temporarily put into a writable state.
    int _isWritableFromPython = 0;
};

/// Smart-pointer to a PropertyObject.
using PropertyPtr = DataOORef<PropertyObject>;

/// Smart-pointer to a PropertyObject providing read-only access to the property data.
using ConstPropertyPtr = DataOORef<const PropertyObject>;

/// Encapsulates a complete data object reference to a PropertyObject in a data collection.
using PropertyDataObjectReference = TypedDataObjectReference<PropertyObject>;

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::StdObj::PropertyDataObjectReference);
