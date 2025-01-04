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


#include <ovito/core/Core.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/oo/PropertyFieldDescriptor.h>
#include <ovito/core/oo/ReferenceEvent.h>
#include <ovito/core/oo/RefMaker.h>

#include <boost/type_traits/has_equal_to.hpp>

namespace Ovito {

/**
 * \brief RefMaker derived classes use this implement properties and reference fields.
 */
class OVITO_CORE_EXPORT PropertyFieldBase
{
protected:

    /// Generates a notification event to inform the dependents of the field's owner that it has changed.
    static void generateTargetChangedEvent(RefMaker* owner, const PropertyFieldDescriptor* descriptor, int eventType = ReferenceEvent::TargetChanged);

    /// Generates a notification event to inform the dependents of the field's owner that it has changed.
    static void generatePropertyChangedEvent(RefMaker* owner, const PropertyFieldDescriptor* descriptor);

#ifdef OVITO_DEBUG
    /// Verifies that owner is of the right type, i.e., defines the given property field.
    static bool ownerTypeCheck(RefMaker* owner, const PropertyFieldDescriptor* descriptor);
#endif

protected:

    /// This abstract undo record class keeps a strong reference object whose property has been changed.
    /// This is needed to keep the owner object alive as long as this undo record is on the undo stack.
    class OVITO_CORE_EXPORT PropertyFieldOperation : public UndoableOperation
    {
    public:

        /// Constructor.
        PropertyFieldOperation(RefMaker* owner, const PropertyFieldDescriptor* descriptor);
        /// Access to the object whose property was changed.
        RefMaker* owner() const;
        /// Access to the descriptor of the reference field whose value has changed.
        const PropertyFieldDescriptor* descriptor() const { return _descriptor; }

    private:

        /// The object whose property has been changed.
        /// This is only used if the owner is not the DataSet, because that would create a circular reference.
        OORef<OvitoObject> _owner;
        /// The descriptor of the reference field whose value has changed.
        const PropertyFieldDescriptor* _descriptor;
    };
};

/**
 * \brief Stores a non-animatable property of a RefTarget derived class, which is not serializable.
 */
template<typename property_data_type, int flags>
class RuntimePropertyField : public PropertyFieldBase
{
public:
    using property_type = property_data_type;
    static constexpr int property_field_flags = flags;

    // Pick the QVariant data type used to wrap the property type.
    // If the property type is an enum, then use 'int'.
    // If the property is 'Color', then use 'QColor'.
    // Otherwise just use the property type.
    using qvariant_type = std::conditional_t<std::is_enum_v<property_data_type>, int,
        std::conditional_t<std::is_same_v<property_data_type, Color>, QColor, property_data_type>>;

    // For enum types, the QVariant data type must always be set to 'int'.
    static_assert(!std::is_enum_v<property_type> || std::is_same_v<qvariant_type, int>, "QVariant data type must be 'int' for enum property types.");
    // For color properties, the QVariant data type must always be set to 'QColor'.
    static_assert(!std::is_same_v<property_type, Color> || std::is_same_v<qvariant_type, QColor>, "QVariant data type must be 'QColor' for color property types.");

    /// Forwarding constructor.
    template<class... Args>
    explicit RuntimePropertyField(Args&&... args) : _value(std::forward<Args>(args)...) {}

    /// Changes the value of the property. Handles undo and sends a notification message.
    template<typename T>
    void set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, T&& newValue) {
        OVITO_ASSERT(ownerTypeCheck(owner, descriptor));

        // The value type supports comparison, do nothing if the new value is equal to the old one.
        if constexpr(boost::has_equal_to<property_type, std::decay_t<T>>::value) {
            if(get() == newValue)
                return;
        }

        // Handle automatic undo recording of the value change (if enabled for this property field).
        if constexpr(!(flags & PROPERTY_FIELD_NO_UNDO)) {
            OVITO_ASSERT(descriptor->automaticUndo());
            if(!owner->isBeingInitializedOrDeleted() && CompoundOperation::isUndoRecording())
                CompoundOperation::current()->addOperation(std::make_unique<PropertyChangeOperation>(owner, *this, descriptor));
        }

        // Assign the new value to the storage.
        mutableValue() = std::forward<T>(newValue);

        // Send notification signals.
        valueChangedInternal(owner, descriptor);
    }

    /// Changes the value of the property. Handles undo and sends a notification message.
    void setQVariant(RefMaker* owner, const PropertyFieldDescriptor* descriptor, const QVariant& newValue) {
        if constexpr(QMetaTypeId2<qvariant_type>::Defined) {
            if(newValue.canConvert<qvariant_type>()) {
                set(owner, descriptor, static_cast<property_type>(newValue.value<qvariant_type>()));
            }
            else {
                OVITO_ASSERT_MSG(false, "PropertyField::setQVariant()", qPrintable(QStringLiteral("The assigned QVariant value of type %1 cannot be converted to the data type %2 of the property field.").arg(newValue.typeName()).arg(QMetaType::fromType<qvariant_type>().name())));
            }
        }
        else {
            OVITO_ASSERT_MSG(false, "RuntimePropertyField::setQVariant()", "The data type of the property field does not support conversion to/from QVariant.");
        }
    }

    /// Returns the internal value stored in this property field as a QVariant.
    QVariant getQVariant() const {
        if constexpr(QMetaTypeId2<qvariant_type>::Defined) {
            return QVariant::fromValue<qvariant_type>(static_cast<qvariant_type>(this->get()));
        }
        else {
            OVITO_ASSERT_MSG(false, "RuntimePropertyField::getQVariant()", "The data type of the property field does not support conversion to/from QVariant.");
            return {};
        }
    }

    /// Returns the internal value stored in this property field.
    inline std::add_const_t<property_type>& get() const { return _value; }

    /// Returns a reference to the internal value stored in this property field.
    /// Warning: Do not use this function unless you know what you are doing!
    inline property_type& mutableValue() { return _value; }

    /// Cast the property field to the property value.
    inline operator std::add_const_t<property_type>&() const { return get(); }

private:

    /// Internal helper function that generates notification events.
    static void valueChangedInternal(RefMaker* owner, const PropertyFieldDescriptor* descriptor) {
        generatePropertyChangedEvent(owner, descriptor);
        generateTargetChangedEvent(owner, descriptor);
        if(descriptor->extraChangeEventType() != 0)
            generateTargetChangedEvent(owner, descriptor, descriptor->extraChangeEventType());
    }

    /// This undo class records a change to the property value.
    class PropertyChangeOperation final : public PropertyFieldOperation
    {
    public:

        /// Constructor.
        /// Makes a copy of the current property value.
        PropertyChangeOperation(RefMaker* owner, RuntimePropertyField& field, const PropertyFieldDescriptor* descriptor) :
            PropertyFieldOperation(owner, descriptor), _field(field), _oldValue(field.get()) {}

        /// Restores the old property value.
        virtual void undo() override final {
            // Swap old value and current property value.
            using std::swap; // using ADL here
            swap(_field.mutableValue(), _oldValue);
            valueChangedInternal(owner(), descriptor());
        }

    private:

        /// The property field that has been changed.
        RuntimePropertyField& _field;
        /// The old value of the property.
        property_type _oldValue;
    };

    /// The internal property value.
    property_type _value;
};

/**
 * \brief Stores a non-animatable property of a RefTarget derived class.
 */
template<typename property_data_type, int flags>
class PropertyField : public RuntimePropertyField<property_data_type, flags>
{
private:

    using base_class = RuntimePropertyField<property_data_type, flags>;

public:

    using property_type = property_data_type;

    /// Inherit constructor.
    using base_class::base_class;

    /// Saves the property's value to a stream.
    inline void saveToStream(SaveStream& stream) const {
        if constexpr(std::is_same_v<property_data_type, size_t>)
            stream.writeSizeT(this->get());
        else
            stream << this->get();
    }

    /// Loads the property's value from a stream.
    inline void loadFromStream(LoadStream& stream) {
        if constexpr(std::is_same_v<property_data_type, size_t>)
            stream.readSizeT(this->mutableValue());
        else
            stream >> this->mutableValue();
    }
};

/**
 * \brief Stores a snapshot of another parameter's value.
 */
template<typename property_data_type>
class ShadowPropertyField : public PropertyField<property_data_type, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE>
{
private:

    using base_class = PropertyField<property_data_type, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE>;

public:

    using property_type = property_data_type;

    /// Inherit constructor.
    using base_class::base_class;

    /// Saves the property's value to a stream.
    inline void saveToStream(SaveStream& stream) const {
        stream << _hasSnapshot;
        if(_hasSnapshot)
            base_class::saveToStream(stream);
    }

    /// Loads the property's value from a stream.
    inline void loadFromStream(LoadStream& stream) {
        stream >> _hasSnapshot;
        if(_hasSnapshot)
            base_class::loadFromStream(stream);
    }

    /// Returns whether this shadow field currently stores a valid value.
    bool hasSnapshot() const { return _hasSnapshot; }

    /// Changes the value of the property.
    void takeSnapshot(const property_data_type& newValue) {
        this->mutableValue() = newValue;
        _hasSnapshot = true;
    }

private:

    bool _hasSnapshot = false;
};

/// This utility class template maps a specific fancy pointer type to the right general fancy pointer type.
///    OORef<T>           ->  OORef<RefTarget>
///    DataOORef<const T> ->  DataOORef<const DataObject>
template<typename T> struct SelectGenericReferenceType {};
template<typename T> struct SelectGenericReferenceType<OORef<T>> { using type = OORef<RefTarget>; };
template<typename T> struct SelectGenericReferenceType<DataOORef<const T>> { using type = DataOORef<const DataObject>; };

/**
 * \brief Stores a fancy pointer to a RefTarget object held by a RefMaker class.
 */
template<typename T>
class OVITO_CORE_EXPORT SingleReferenceFieldBase : public PropertyFieldBase
{
public:

    /// The fancy pointer type.
    using pointer = T;

#ifdef OVITO_DEBUG
    /// Destructor.
    ~SingleReferenceFieldBase();
#endif

    /// Returns a raw, untyped pointer to the currently referenced object.
    inline typename std::pointer_traits<pointer>::element_type* get() const noexcept { return _target.get(); }

protected:

    /// Replaces the current reference target with a new target. Handles undo recording.
    void set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, pointer newTarget);

    /// Replaces the target stored in the reference field.
    void swapReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, pointer& inactiveTarget);

    /// The actual fancy pointer value.
    pointer _target{};
};

// Instantiate base class template for the fancy pointer base types needed.
#ifndef OVITO_BUILD_MONOLITHIC
    #if !defined(Core_EXPORTS)
        extern template class OVITO_CORE_EXPORT SingleReferenceFieldBase<OORef<RefTarget>>;
    #elif !defined(Q_CC_MSVC) && !defined(Q_CC_CLANG)
        template class OVITO_CORE_EXPORT SingleReferenceFieldBase<OORef<RefTarget>>;
    #endif
#endif

/**
 * \brief Class template for reference fields of RefMaker derived classes.
 */
template<typename T>
class ReferenceField : public SingleReferenceFieldBase<typename SelectGenericReferenceType<T>::type>
{
public:

    /// The fancy pointer type.
    using fancy_pointer = T;
    /// The raw object pointer type.
    using raw_pointer = typename std::pointer_traits<fancy_pointer>::element_type*;
    /// The type of object referenced by this field.
    using target_object_type = std::remove_const_t<typename std::pointer_traits<fancy_pointer>::element_type>;
    /// The generic base class type.
    using base_class = SingleReferenceFieldBase<typename SelectGenericReferenceType<T>::type>;

    /// Returns true if the reference value is non-null.
    inline operator bool() const noexcept { return (bool)base_class::_target; }

    /// Write access to the RefTarget pointer. Changes the value of the reference field.
    inline void set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, fancy_pointer newPointer) {
        if constexpr(!std::is_const_v<typename std::pointer_traits<fancy_pointer>::element_type>)
            base_class::set(owner, descriptor, std::move(newPointer));
        else
            base_class::set(owner, descriptor, const_pointer_cast<target_object_type>(std::move(newPointer)));
    }

    /// Returns the target object currently being referenced by the reference field.
    inline raw_pointer get() const noexcept {
        return static_object_cast<target_object_type>(base_class::get());
    }
};

/**
 * \brief Stores a list of fancy pointers to RefTarget objects held by a RefMaker class.
 */
template<typename T>
class OVITO_CORE_EXPORT VectorReferenceFieldBase : public PropertyFieldBase
{
public:

    /// The fancy pointer type.
    using pointer = T;
    /// The vector container type.
    using container = QVector<pointer>;
    /// The size/index data type.
    using size_type = typename container::size_type;

#ifdef OVITO_DEBUG
    /// Destructor.
    ~VectorReferenceFieldBase();
#endif

    /// Returns a raw, untyped pointer to the i-th object in the vector.
    inline typename std::pointer_traits<pointer>::element_type* get(size_type i) const noexcept { return _targets[i].get(); }

    /// Clears all references and sets the vector size to zero.
    void clear(RefMaker* owner, const PropertyFieldDescriptor* descriptor);

    /// Removes the reference at index position i.
    pointer remove(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i);

    /// Returns the number of objects in the vector reference field.
    inline size_type size() const noexcept { return _targets.size(); }

    /// Returns true if the vector has size 0; otherwise returns false.
    inline bool empty() const noexcept { return _targets.empty(); }

    /// Returns true if the vector contains an occurrence of value; otherwise returns false.
    bool contains(const RefTarget* value) const noexcept { return indexOf(value) != -1; }

    /// Returns the index position of the first occurrence of value in the vector,
    /// searching forward from index position from. Returns -1 if no item matched.
    size_type indexOf(const RefTarget* value, size_type from = 0) const noexcept {
        for(size_type i = from, count = _targets.size(); i < count; i++)
            if(_targets[i] == value)
                return i;
        return -1;
    }

protected:

    /// Replaces one of the references with a new target object.
    void set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, pointer newTarget);

    /// Inserts or add a reference target to the internal list.
    size_type insert(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, pointer newTarget);

    /// Replaces the i-th target stored in the reference field.
    void swapReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& inactiveTarget);

    /// Removes the i-th target from the vector reference field.
    void removeReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& inactiveTarget);

    /// Adds the target to the vector reference field.
    size_type addReference(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type index, pointer& target);

    /// The actual list of fancy pointers to the RefTargets.
    container _targets;
};

// Instantiate base class template for the fancy pointer base types needed.
#ifndef OVITO_BUILD_MONOLITHIC
    #if !defined(Core_EXPORTS)
        extern template class OVITO_CORE_EXPORT VectorReferenceFieldBase<OORef<RefTarget>>;
    #elif !defined(Q_CC_MSVC) && !defined(Q_CC_CLANG)
        template class OVITO_CORE_EXPORT VectorReferenceFieldBase<OORef<RefTarget>>;
    #endif
#endif

/**
 * \brief Class template for vector reference fields of RefMaker derived classes.
 */
template<typename T>
class VectorReferenceField : public VectorReferenceFieldBase<typename SelectGenericReferenceType<T>::type>
{
public:

    /// The fancy pointer type.
    using fancy_pointer = T;
    /// The raw object pointer type.
    using raw_pointer = typename std::pointer_traits<fancy_pointer>::element_type*;
    /// The type of object referenced by this field.
    using target_object_type = std::remove_const_t<typename std::pointer_traits<fancy_pointer>::element_type>;
    /// The generic base class type.
    using base_class = VectorReferenceFieldBase<typename SelectGenericReferenceType<T>::type>;
    /// The container type for typed fancy pointers.
    using container = QVector<fancy_pointer>;
    /// The size/index data type.
    using size_type = typename container::size_type;

    /// Replaces one of the references with a new target object.
    inline void set(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, fancy_pointer newPointer) {
        base_class::set(owner, descriptor, i, std::move(newPointer));
    }

    /// Inserts a reference at the end of the vector.
    void push_back(RefMaker* owner, const PropertyFieldDescriptor* descriptor, fancy_pointer newPointer) {
        base_class::insert(owner, descriptor, -1, std::move(newPointer));
    }

    /// Inserts or add a reference target to the vector reference field.
    size_type insert(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i, fancy_pointer newPointer) {
        return base_class::insert(owner, descriptor, i, std::move(newPointer));
    }

    /// Removes the reference target at index position i.
    fancy_pointer remove(RefMaker* owner, const PropertyFieldDescriptor* descriptor, size_type i) {
        return static_object_cast<typename std::pointer_traits<fancy_pointer>::element_type>(base_class::remove(owner, descriptor, i));
    }

    /// Returns the i-th target object currently being referenced by the vector reference field.
    inline raw_pointer get(size_type i) const noexcept {
        return static_object_cast<target_object_type>(base_class::get(i));
    }

    /// Returns the stored list of object references.
    const container& targets() const { return reinterpret_cast<const container&>(base_class::_targets); }

    /// Replaces the list of object references stored in this vector reference field.
    template<typename U>
    void setTargets(RefMaker* owner, const PropertyFieldDescriptor* descriptor, U&& newTargets) {
        size_type i = 0;
        // Insert targets from the new vector. Replace existing targets up to the length
        // of the existing vector. Append additional targets if the new vector is longer than the old one.
        for(auto&& t : newTargets) {
            if(i < this->size()) set(owner, descriptor, i, t);
            else push_back(owner, descriptor, t);
            i++;
        }
        // Remove excess items from the old vector.
        for(size_type j = this->size() - 1; j >= i; j--)
            this->remove(owner, descriptor, j);
    }
};

/// Vector container type used by vector reference fields with OORef<T> fancy pointers.
template<typename T> using OORefVector = typename VectorReferenceField<OORef<T>>::container;

/// Vector container type used by vector reference fields with DataOORef<const T> fancy pointers.
template<typename T> using DataRefVector = typename VectorReferenceField<DataOORef<const T>>::container;

}   // End of namespace
