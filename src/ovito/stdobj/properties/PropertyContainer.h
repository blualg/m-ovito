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
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/core/dataset/DataSet.h>
#include "PropertyObject.h"
#include "PropertyContainerClass.h"
#include "PropertyReference.h"

namespace Ovito::StdObj {

/**
 * \brief Stores an array of properties.
 */
class OVITO_STDOBJ_EXPORT PropertyContainer : public DataObject
{
    OVITO_CLASS_META(PropertyContainer, PropertyContainerClass)

public:

    /// Constructor.
    explicit PropertyContainer(ObjectInitializationFlags flags, const QString& title = {});

    /// Returns the display title of this object.
    virtual QString objectTitle() const override;

    /// Appends a new property to the list of properties.
    void addProperty(const PropertyObject* property) {
        OVITO_ASSERT(property);
        OVITO_ASSERT(isSafeToModify());
        OVITO_ASSERT(properties().contains(const_cast<PropertyObject*>(property)) == false);
        if(properties().empty())
            _elementCount.set(this, PROPERTY_FIELD(elementCount), property->size());
        OVITO_ASSERT(property->size() == elementCount());
        _properties.push_back(this, PROPERTY_FIELD(properties), const_cast<PropertyObject*>(property));
    }

    /// Inserts a new property into the list of properties.
    void insertProperty(int index, const PropertyObject* property) {
        OVITO_ASSERT(property);
        OVITO_ASSERT(isSafeToModify());
        OVITO_ASSERT(properties().contains(const_cast<PropertyObject*>(property)) == false);
        if(properties().empty())
            _elementCount.set(this, PROPERTY_FIELD(elementCount), property->size());
        OVITO_ASSERT(property->size() == elementCount());
        _properties.insert(this, PROPERTY_FIELD(properties), index, const_cast<PropertyObject*>(property));
    }

    /// Removes a property from this container.
    void removeProperty(const PropertyObject* property) {
        OVITO_ASSERT(property);
        OVITO_ASSERT(isSafeToModify());
        int index = properties().indexOf(const_cast<PropertyObject*>(property));
        OVITO_ASSERT(index >= 0);
        _properties.remove(this, PROPERTY_FIELD(properties), index);
    }

    /// Looks up the standard property with the given ID.
    const PropertyObject* getProperty(int typeId) const {
        OVITO_ASSERT(typeId != 0);
        OVITO_ASSERT(getOOMetaClass().isValidStandardPropertyId(typeId));
        for(const PropertyObject* property : properties()) {
            if(property->type() == typeId)
                return property;
        }
        return nullptr;
    }

    /// Looks up the user-defined property with the given name.
    const PropertyObject* getProperty(const QString& name) const {
        OVITO_ASSERT(!name.isEmpty());
        for(const PropertyObject* property : properties()) {
            // Note: Prior to OVITO 3.7, we required the type id of candidate properties to be 0 here,
            // which prevented the method from finding the X and Y properties of a DataTable,
            // which have a user-defined name but a non-zero type id.
            if(property->name() == name)
                return property;
        }
        return nullptr;
    }

    /// Looks up the standard property with the given ID, removes it from this container and returns it to the caller.
    ConstPropertyPtr takeProperty(int typeId) {
        OVITO_ASSERT(typeId != 0);
        OVITO_ASSERT(getOOMetaClass().isValidStandardPropertyId(typeId));
        OVITO_ASSERT(isSafeToModify());
        for(int index = 0; index < properties().size(); index++) {
            const PropertyObject* property = properties()[index];
            if(property->type() == typeId) {
                return _properties.remove(this, PROPERTY_FIELD(properties), index);
            }
        }
        return {};
    }

    /// Looks up the standard property with the given ID and makes it mutable if necessary.
    PropertyObject* getMutableProperty(int typeId, DataBuffer::BufferInitialization cloneMode = DataBuffer::Initialized) {
        if(const PropertyObject* p = getProperty(typeId))
            return makePropertyMutable(p, cloneMode);
        else
            return nullptr;
    }

    /// Looks up a user-defined property with the given name and makes it mutable if necessary.
    PropertyObject* getMutableProperty(const QString& name, DataBuffer::BufferInitialization cloneMode = DataBuffer::Initialized) {
        if(const PropertyObject* p = getProperty(name))
            return makePropertyMutable(p, cloneMode);
        else
            return nullptr;
    }

    /// Returns the given standard property.
    /// If it does not exist, an exception is thrown.
    const PropertyObject* expectProperty(int typeId) const;

    /// Returns the property with the given name and data layout.
    /// If the container does not contain a property with the given name and data type, then an exception is thrown.
    const PropertyObject* expectProperty(const QString& propertyName, int dataType, size_t componentCount = 1) const;

    /// Returns the given standard property after making sure it can be safely modified.
    /// If it does not exist, an exception is thrown.
    PropertyObject* expectMutableProperty(int typeId, DataBuffer::BufferInitialization cloneMode = DataBuffer::Initialized) {
        return makePropertyMutable(expectProperty(typeId), cloneMode);
    }

    /// Duplicates and replaces the given property with its copy if it not exclusively owned by this container or is being accessed from Python.
    /// This method is similar to the DataObject::makeMutable() method, but it offers the cloneMode option.
    /// If cloneMode==Uninitialized and a copy of the PropertyObject needs to be made, the payload data of the original are NOT copied over to the clone.
    /// This option offers a performance benefit in situations where the calling code is going to
    /// completely overwrite the data in the mutable property with new values.
    PropertyObject* makePropertyMutable(const PropertyObject* property, DataBuffer::BufferInitialization cloneMode, bool ignorePythonAccess = false);

    /// Duplicates any property objects that are shared with other containers.
    /// After this method returns, all property objects are exclusively owned by the container and
    /// can be safely modified without unwanted side effects.
    auto makePropertiesMutable() {
        makePropertiesMutableInternal();
        auto const_cast_op = [](const DataOORef<const PropertyObject>& p) noexcept { return const_cast<PropertyObject*>(p.get()); };
        using const_cast_iter_type = boost::transform_iterator<decltype(const_cast_op), typename std::decay_t<decltype(std::declval<PropertyContainer>().properties())>::const_iterator>;
        return boost::make_iterator_range(
            const_cast_iter_type(properties().begin(), const_cast_op),
            const_cast_iter_type(properties().end(), const_cast_op)
        );
    }

    /// Creates a standard property and adds it to the container.
    /// In case the property already exists, it is made sure that it's safe to modify it.
    PropertyObject* createProperty(DataBuffer::BufferInitialization init, int typeId, const ConstDataObjectPath& containerPath = {});

    /// Creates a standard property and adds it to the contaPropertyContaineriner.
    /// In case the property already exists, it is made sure that it's safe to modify it.
    PropertyObject* createProperty(int typeId, const ConstDataObjectPath& containerPath = {}) {
        return createProperty(DataBuffer::BufferInitialization::Uninitialized, typeId, containerPath);
    }

    /// Creates a user-defined property and adds it to the container.
    /// In case the property already exists, it is made sure that it's safe to modify it.
    PropertyObject* createProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = {});

    /// Creates a user-defined property and adds it to the container.
    /// In case the property already exists, it is made sure that it's safe to modify it.
    PropertyObject* createProperty(const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = {}) {
        return createProperty(DataBuffer::BufferInitialization::Uninitialized, name, dataType, componentCount, std::move(componentNames));
    }

    /// Adds a property object to the container, replacing any preexisting property in the container with the same type.
    const PropertyObject* createProperty(const PropertyObject* property);

    /// Sets the current number of data elements stored in the container.
    /// The lengths of the property arrays will be adjusted accordingly.
    void setElementCount(size_t count);

    /// Deletes those data elements having a non-zero value in the given selection array.
    /// Returns the number of deleted elements. The original order of the remaining elements is preserved.
    virtual size_t deleteElements(ConstDataBufferPtr selection, size_t selectionCount = std::numeric_limits<size_t>::max());

    /// Replaces the property arrays in this property container with a new set of properties.
    /// Existing element types of typed properties will be preserved by the method.
    void setContent(size_t newElementCount, const DataRefVector<PropertyObject>& newProperties);

    /// Clones all properties in the container and newly allocates memory for all property arrays, possibly with a
    /// different element count than before. It's the callers responsibility to initialize the new property arrays.
    std::vector<std::pair<ConstPropertyPtr, PropertyObject*>> reallocateProperties(size_t numElements);

    /// Duplicates all data elements by extending the property arrays and replicating the existing data N times.
    void replicate(size_t n);

    /// Sorts the data elements in the container with respect to their unique IDs.
    /// Does nothing if data elements do not have the ID property.
    virtual std::vector<size_t> sortById();

    /// Makes sure that all property arrays in this container have a consistent length.
    /// If this is not the case, the method throws an exception.
    void verifyIntegrity() const;

    /// Returns the base point and vector information for visualizing a vector property from this container using a VectorVis element.
    virtual std::tuple<ConstDataBufferPtr, ConstDataBufferPtr> getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state, MixedKeyCache& visCache) const { return {}; }

    /// Generates the info string to be displayed in the OVITO status bar for an element from this container.
    virtual QString elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path = {}) const;

public:

    class OVITO_STDOBJ_EXPORT Grower
    {
    public:

        Grower(PropertyContainer* container) : _container(container), _elementCount(container->elementCount()) {
            OVITO_ASSERT(container->isSafeToModify());
            // Make all property arrays mutable to begin with.
            container->makePropertiesMutableInternal();
        }

        ~Grower() { commit(); }

        void commit() {
            // Write new element count back to container.
            _container->_elementCount.set(_container, PROPERTY_FIELD(PropertyContainer::elementCount), _elementCount);
        }

        size_t grow(size_t numAdditionalElements) {
            // Grow property arrays.
            for(const PropertyObject* prop : _container->properties()) {
                OVITO_ASSERT(prop->size() == _elementCount);
                const_cast<PropertyObject*>(prop)->grow(numAdditionalElements);
            }
            // Update only our internal element count. Container will be updated by destructor.
            size_t oldCount = _elementCount;
            _elementCount += numAdditionalElements;
            return oldCount;
        }

        bool grow(size_t numAdditionalElements, int alreadyLockedPropertyType) {
            bool wasReallocated = false;
            // Grow property arrays.
            for(const PropertyObject* prop : _container->properties()) {
                OVITO_ASSERT(prop->size() == _elementCount);
                bool b = const_cast<PropertyObject*>(prop)->grow(numAdditionalElements, prop->type() == alreadyLockedPropertyType);
                if(b && prop->type() == alreadyLockedPropertyType)
                    wasReallocated = true;
            }
            // Update only our internal element count. Container will be updated by destructor.
            _elementCount += numAdditionalElements;
            return wasReallocated;
        }

        /// Deletes a number of elements from the end of each property array (without reallocation).
        void truncate(size_t numElementsToTruncate, int alreadyLockedPropertyType = -1) {
            OVITO_ASSERT(numElementsToTruncate <= _elementCount);

            // Truncate each property array.
            for(const PropertyObject* prop : _container->properties()) {
                OVITO_ASSERT(prop->size() == _elementCount);
                const_cast<PropertyObject*>(prop)->truncate(numElementsToTruncate, prop->type() == alreadyLockedPropertyType);
            }

            // Update only our internal element count. Container will be updated by destructor.
            _elementCount -= numElementsToTruncate;
        }

        /// Moves the values from one index of property container to another in all property arrays.
        void moveElement(size_t fromIndex, size_t toIndex, int alreadyLockedPropertyType = -1) {
            OVITO_ASSERT(fromIndex < _elementCount);
            OVITO_ASSERT(toIndex < _elementCount);
            for(const PropertyObject* prop : _container->properties()) {
                OVITO_ASSERT(prop->size() == _elementCount);
                const_cast<PropertyObject*>(prop)->moveElement(fromIndex, toIndex, prop->type() == alreadyLockedPropertyType);
            }
        }

        PropertyObject* mutableProperty(int type) const {
            OVITO_ASSERT(_container->isSafeToModify());
            for(const PropertyObject* prop : _container->properties()) {
                OVITO_ASSERT(_container->isSafeToModifySubObject(prop));
                if(prop->type() == type) {
                    return const_cast<PropertyObject*>(prop);
                }
            }
            return nullptr;
        }

    private:
        PropertyContainer* _container;
        size_t _elementCount;
    };

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// Duplicates and replaces the given property with its copy if it not exclusively owned by this container or is being accessed from Python.
    /// This method is similar to the DataObject::makeMutable() method, but won't copy the contents of the PropertyObject nor
    /// allocate memory for the new array.
    PropertyObject* makePropertyMutableUnallocated(const PropertyObject* property);

    /// Duplicates any property objects that are shared with other containers or being accessed from Python.
    /// After this method returns, all property objects are exclusively owned by the container and
    /// can be safely modified without unwanted side effects.
    void makePropertiesMutableInternal();

private:

    /// Holds the list of properties.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const PropertyObject>, properties, setProperties);

    /// Keeps track of the number of data elements this property container contains.
    DECLARE_PROPERTY_FIELD(size_t, elementCount);

    /// The assigned title of the data object, which is displayed in the user interface.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);
    DECLARE_SHADOW_PROPERTY_FIELD(title);
};

/// Encapsulates a reference to a PropertyContainer in a PipelineFlowState.
using PropertyContainerReference = TypedDataObjectReference<PropertyContainer>;

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::StdObj::PropertyContainerReference);
