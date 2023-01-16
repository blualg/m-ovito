////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/oo/ReferenceEvent.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "PropertyFieldDescriptor.h"

namespace Ovito {

/******************************************************************************
* This structure describes one member field of a RefMaker object that stores
* a property of that object.
******************************************************************************/
class OVITO_CORE_EXPORT NativePropertyFieldDescriptor : public PropertyFieldDescriptor
{
public:

    /// Inherit all constructors from base class.
    using PropertyFieldDescriptor::PropertyFieldDescriptor;

public:

    // Internal helper class that is used to specify the units for a controller
    // property field. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_UNITS macro instead.
    struct PropertyFieldUnitsSetter : public NumericalParameterDescriptor {
        PropertyFieldUnitsSetter(NativePropertyFieldDescriptor* propfield, const QMetaObject* parameterUnitType, FloatType minValue = FLOATTYPE_MIN, FloatType maxValue = FLOATTYPE_MAX) {
            OVITO_ASSERT(propfield->_parameterInfo == nullptr);
            propfield->_parameterInfo = this;
            this->unitType = parameterUnitType;
            this->minValue = minValue;
            this->maxValue = maxValue;
        }
    };

    // Internal helper class that is used to specify the label text for a
    // property field. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_LABEL macro instead.
    struct PropertyFieldDisplayNameSetter {
        PropertyFieldDisplayNameSetter(NativePropertyFieldDescriptor* propfield, const QString& label) {
            OVITO_ASSERT(propfield->_displayName.isEmpty());
            propfield->_displayName = label;
        }
    };

    // Internal helper class that is used to set the reference event type to generate
    // for a property field every time its value changes. Do not use this class directly but use the
    // SET_PROPERTY_FIELD_CHANGE_EVENT macro instead.
    struct PropertyFieldChangeEventSetter {
        PropertyFieldChangeEventSetter(NativePropertyFieldDescriptor* propfield, ReferenceEvent::Type eventType) {
            OVITO_ASSERT(propfield->_extraChangeEventType == 0);
            propfield->_extraChangeEventType = eventType;
        }
    };
};

/*** Macros to define reference and property fields in RefMaker-derived classes: ***/

/// This macros returns a reference to the PropertyFieldDescriptor of a
/// named reference or property field.
#define PROPERTY_FIELD(RefMakerClassPlusStorageFieldName) \
        RefMakerClassPlusStorageFieldName##__propdescr()

#define DEFINE_REFERENCE_FIELD(classname, name) \
    Ovito::NativePropertyFieldDescriptor classname::name##__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            &decltype(classname::_##name)::target_object_type::OOClass(), \
            #name, \
            static_cast<Ovito::PropertyFieldFlags>(classname::__##name##_flags), \
            [](const Ovito::RefMaker* obj) -> Ovito::RefTarget* { \
                return const_cast<classname::__##name##_target_object_type*>(static_cast<const classname*>(obj)->_##name.get()); \
            }, \
            [](Ovito::RefMaker* obj, const Ovito::RefTarget* newTarget) { \
                static_cast<classname*>(obj)->_##name.set(obj, PROPERTY_FIELD(classname::name), \
                    static_object_cast<classname::__##name##_target_object_type>(const_cast<Ovito::RefTarget*>(newTarget))); \
            }, \
            [](Ovito::RefMaker* obj, Ovito::OORef<Ovito::RefTarget> newTarget) { \
                static_cast<classname*>(obj)->_##name.set(obj, PROPERTY_FIELD(classname::name), \
                    static_object_cast<classname::__##name##_target_object_type>(std::move(newTarget))); \
            } \
        );

#define DEFINE_VECTOR_REFERENCE_FIELD(classname, name) \
    Ovito::NativePropertyFieldDescriptor classname::name##__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            &decltype(classname::_##name)::target_object_type::OOClass(), \
            #name, \
            static_cast<Ovito::PropertyFieldFlags>(classname::__##name##_flags), \
            [](const Ovito::RefMaker* obj) -> int { \
                return static_cast<const classname*>(obj)->_##name.size(); \
            }, \
            [](const Ovito::RefMaker* obj, int index) -> Ovito::RefTarget* { \
                return const_cast<classname::__##name##_target_object_type*>(static_cast<const classname*>(obj)->_##name.get(index)); \
            }, \
            [](Ovito::RefMaker* obj, int index, const Ovito::RefTarget* newTarget) { \
                static_cast<classname*>(obj)->_##name.set(obj, PROPERTY_FIELD(classname::name), index, \
                    static_object_cast<classname::__##name##_target_object_type>(const_cast<Ovito::RefTarget*>(newTarget))); \
            }, \
            [](Ovito::RefMaker* obj, int index) { \
                static_cast<classname*>(obj)->_##name.remove(obj, PROPERTY_FIELD(classname::name), index); \
            }, \
            [](Ovito::RefMaker* obj, int index, Ovito::OORef<Ovito::RefTarget> newTarget) { \
                static_cast<classname*>(obj)->_##name.insert(obj, PROPERTY_FIELD(classname::name), index, \
                    static_object_cast<classname::__##name##_target_object_type>(std::move(newTarget))); \
            } \
        );

/// Adds a reference field to a class definition.
/// The first parameter specifies the RefTarget derived class of the referenced object.
/// The second parameter determines the name of the reference field. It must be unique within the current class.
#define DECLARE_REFERENCE_FIELD_FLAGS(type, name, flags) \
    private: \
        enum { __##name##_flags = flags }; \
        using __##name##_target_object_type = Ovito::ReferenceField<type>::target_object_type; \
        static Ovito::NativePropertyFieldDescriptor name##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(name) { return &name##__propdescr_instance; } \
        Ovito::ReferenceField<type> _##name; \
        inline decltype(std::declval<Ovito::ReferenceField<type>>().get()) name() const { return _##name.get(); } \
    private:

/// Adds a reference field to a class definition.
/// The first parameter specifies the RefTarget derived class of the referenced object.
/// The second parameter determines the name of the reference field. It must be unique within the current class.
#define DECLARE_REFERENCE_FIELD(type, name) \
    DECLARE_REFERENCE_FIELD_FLAGS(type, name, PROPERTY_FIELD_NO_FLAGS)

/// Adds a settable reference field to a class definition.
/// The first parameter specifies the RefTarget derived class of the referenced object.
/// The second parameter determines the name of the reference field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this reference field.
#define DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(type, name, setterName, flags) \
    DECLARE_REFERENCE_FIELD_FLAGS(type, name, flags) \
    public: \
        template<typename U> inline void setterName(U&& newValue) { _##name.set(this, PROPERTY_FIELD(name), std::forward<U>(newValue)); } \
        inline void setterName##PYTHON(typename std::pointer_traits<type>::element_type* newValue) { _##name.set(this, PROPERTY_FIELD(name), newValue); } \
    private:

/// Adds a settable reference field to a class definition.
/// The first parameter specifies the RefTarget derived class of the referenced object.
/// The second parameter determines the name of the reference field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this reference field.
#define DECLARE_MODIFIABLE_REFERENCE_FIELD(type, name, setterName) \
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(type, name, setterName, PROPERTY_FIELD_NO_FLAGS)

/// Adds a vector reference field to a class definition.
/// The first parameter specifies the RefTarget-derived class of the objects stored in the vector reference field.
/// The second parameter determines the name of the vector reference field. It must be unique within the current class.
#define DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, flags) \
    private: \
        enum { __##name##_flags = flags | PROPERTY_FIELD_VECTOR }; \
        using __##name##_target_object_type = Ovito::VectorReferenceField<type>::target_object_type; \
        static Ovito::NativePropertyFieldDescriptor name##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(name) { return &name##__propdescr_instance; } \
        Ovito::VectorReferenceField<type> _##name; \
        inline decltype(std::declval<Ovito::VectorReferenceField<type>>().targets()) name() const { return _##name.targets(); } \
    private:

/// Adds a vector reference field to a class definition.
/// The first parameter specifies the RefTarget-derived class of the objects stored in the vector reference field.
/// The second parameter determines the name of the vector reference field. It must be unique within the current class.
#define DECLARE_VECTOR_REFERENCE_FIELD(type, name) \
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, PROPERTY_FIELD_NO_FLAGS)

/// Adds a vector reference field to a class definition that is settable.
/// The first parameter specifies the RefTarget-derived class of the objects stored in the vector reference field.
/// The second parameter determines the name of the vector reference field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this reference field.
#define DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, setterName, flags) \
    DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, flags) \
    public: \
        template<typename U> inline void setterName(U&& newList) { _##name.setTargets(this, PROPERTY_FIELD(name), std::forward<U>(newList)); } \
        inline void setterName(std::initializer_list<type> newList) { _##name.setTargets(this, PROPERTY_FIELD(name), newList); } \
    private:

/// Adds a vector reference field to a class definition that is settable.
/// The first parameter specifies the RefTarget-derived class of the objects stored in the vector reference field.
/// The second parameter determines the name of the vector reference field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this reference field.
#define DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(type, name, setterName) \
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(type, name, setterName, PROPERTY_FIELD_NO_FLAGS)

/// Assigns a unit class to an animation controller reference or numeric property field.
/// The unit class will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS(DefiningClass, name, ParameterUnitClass)                               \
    static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject);

/// Assigns a unit class and a minimum value limit to an animation controller reference or numeric property field.
/// The unit class and the value limit will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DefiningClass, name, ParameterUnitClass, minValue) \
    static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject, minValue);

/// Assigns a unit class and a minimum and maximum value limit to an animation controller reference or numeric property field.
/// The unit class and the value limits will automatically be assigned to the numeric input field for this parameter in the user interface.
#define SET_PROPERTY_FIELD_UNITS_AND_RANGE(DefiningClass, name, ParameterUnitClass, minValue, maxValue) \
    static Ovito::NativePropertyFieldDescriptor::PropertyFieldUnitsSetter __unitsSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), &ParameterUnitClass::staticMetaObject, minValue, maxValue);

/// Assigns a label string to the given reference or property field.
/// This string will be used in the user interface.
#define SET_PROPERTY_FIELD_LABEL(DefiningClass, name, labelText)                                        \
    static Ovito::NativePropertyFieldDescriptor::PropertyFieldDisplayNameSetter __displayNameSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), labelText);

/// Use this macro to let the system automatically generate an event of the
/// given type every time the given property field changes its value.
#define SET_PROPERTY_FIELD_CHANGE_EVENT(DefiningClass, name, eventType)                                     \
    static Ovito::NativePropertyFieldDescriptor::PropertyFieldChangeEventSetter __changeEventSetter##DefiningClass##name(PROPERTY_FIELD(DefiningClass::name), eventType);

/// Adds a property field to a class definition.
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
#define DECLARE_PROPERTY_FIELD_FLAGS(type, fieldname, flags) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(fieldname) { \
            return &fieldname##__propdescr_instance; \
        } \
        Ovito::PropertyField<type, flags> _##fieldname; \
        inline const type& fieldname() const { return _##fieldname; } \
    private:

#define DEFINE_PROPERTY_FIELD(classname, fieldname) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname, \
            static_cast<Ovito::PropertyFieldFlags>((Ovito::PropertyFieldFlag)decltype(classname::_##fieldname)::property_field_flags), \
            [](Ovito::RefMaker* obj, const Ovito::RefMaker* other) { \
                static_cast<classname*>(obj)->_##fieldname.set(obj, PROPERTY_FIELD(classname::fieldname), static_cast<const classname*>(other)->_##fieldname.get()); \
            }, \
            [](const Ovito::RefMaker* obj) -> QVariant { \
                return static_cast<const classname*>(obj)->_##fieldname.getQVariant(); \
            }, \
            [](Ovito::RefMaker* obj, const QVariant& newValue) { \
                static_cast<classname*>(obj)->_##fieldname.setQVariant(obj, PROPERTY_FIELD(classname::fieldname), newValue); \
            }, \
            [](const Ovito::RefMaker* obj, Ovito::SaveStream& stream) { \
                static_cast<const classname*>(obj)->_##fieldname.saveToStream(stream); \
            }, \
            [](Ovito::RefMaker* obj, Ovito::LoadStream& stream) { \
                static_cast<classname*>(obj)->_##fieldname.loadFromStream(stream); \
            } \
        );

/// Adds a property field to a class definition.
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
#define DECLARE_PROPERTY_FIELD(type, fieldname) \
    DECLARE_PROPERTY_FIELD_FLAGS(type, fieldname, Ovito::PROPERTY_FIELD_NO_FLAGS)

/// Adds a settable property field to a class definition.
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this property field.
#define DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(type, fieldname, setterName, flags) \
    public: \
        void setterName(const type& value) { _##fieldname.set(this, PROPERTY_FIELD(fieldname), value); } \
        DECLARE_PROPERTY_FIELD_FLAGS(type, fieldname, flags)

/// Adds a settable property field to a class definition.
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this property field.
#define DECLARE_MODIFIABLE_PROPERTY_FIELD(type, fieldname, setterName) \
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(type, fieldname, setterName, Ovito::PROPERTY_FIELD_NO_FLAGS)

/***************** Runtime property fields *******************/

/// Adds a property field to a class definition which is not serializable.
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this property field.
#define DECLARE_RUNTIME_PROPERTY_FIELD_FLAGS(type, fieldname, setterName, flags) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* PROPERTY_FIELD(fieldname) { \
            return &fieldname##__propdescr_instance; \
        } \
        Ovito::RuntimePropertyField<type, flags> _##fieldname; \
        const type& fieldname() const { return _##fieldname; } \
        void setterName(const type& value) { _##fieldname.set(this, PROPERTY_FIELD(fieldname), value); } \
        void setterName(type&& value) { _##fieldname.set(this, PROPERTY_FIELD(fieldname), std::move(value)); } \
    private:

#define DEFINE_RUNTIME_PROPERTY_FIELD(classname, fieldname) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname, \
            static_cast<Ovito::PropertyFieldFlags>((Ovito::PropertyFieldFlag)decltype(classname::_##fieldname)::property_field_flags), \
            [](Ovito::RefMaker* obj, const Ovito::RefMaker* other) { \
                static_cast<classname*>(obj)->_##fieldname.set(obj, PROPERTY_FIELD(classname::fieldname), static_cast<const classname*>(other)->_##fieldname.get()); \
            }, \
            [](const Ovito::RefMaker* obj) -> QVariant { \
                return static_cast<const classname*>(obj)->_##fieldname.getQVariant(); \
            }, \
            [](Ovito::RefMaker* obj, const QVariant& newValue) { \
                static_cast<classname*>(obj)->_##fieldname.setQVariant(obj, PROPERTY_FIELD(classname::fieldname), newValue); \
            }, \
            [](const Ovito::RefMaker* obj, Ovito::SaveStream& stream) {}, \
            [](Ovito::RefMaker* obj, Ovito::LoadStream& stream) {} \
        );

/// Adds a property field to a class definition which is not serializble .
/// The first parameter specifies the data type of the property field.
/// The second parameter determines the name of the property field. It must be unique within the current class.
/// The third parameter is the name of the setter method to be created for this property field.
#define DECLARE_RUNTIME_PROPERTY_FIELD(type, fieldname, setterName) \
    DECLARE_RUNTIME_PROPERTY_FIELD_FLAGS(type, fieldname, setterName, Ovito::PROPERTY_FIELD_NO_FLAGS)

/***************** Shadow property fields *******************/

/// This macros returns a reference to the PropertyFieldDescriptor of a shadow property field.
#define SHADOW_PROPERTY_FIELD(RefMakerClassPlusStorageFieldName) \
        RefMakerClassPlusStorageFieldName##__shadow__propdescr()
        
/// Adds the capability to take a snapshot to an existing property field of a class.
/// A shadow property field is created by this macro, which holds a copy of the original property field value.
#define DECLARE_SHADOW_PROPERTY_FIELD(fieldname) \
    private: \
        static Ovito::NativePropertyFieldDescriptor fieldname##__shadow__propdescr_instance; \
    public: \
        static inline Ovito::NativePropertyFieldDescriptor* SHADOW_PROPERTY_FIELD(fieldname) { \
            return &fieldname##__shadow__propdescr_instance; \
        } \
        Ovito::ShadowPropertyField<decltype(_##fieldname)::property_type> _##fieldname##__shadow; \
    private:

#define DEFINE_SHADOW_PROPERTY_FIELD(classname, fieldname) \
    Ovito::NativePropertyFieldDescriptor classname::fieldname##__shadow__propdescr_instance( \
            const_cast<classname::OOMetaClass*>(&classname::OOClass()), \
            #fieldname "__shadow", \
            static_cast<Ovito::PropertyFieldFlags>(decltype(classname::_##fieldname##__shadow)::property_field_flags), \
            [](Ovito::RefMaker* obj, const Ovito::RefMaker* other) { \
                if(static_cast<const classname*>(other)->_##fieldname##__shadow.hasSnapshot()) \
                    static_cast<classname*>(obj)->_##fieldname##__shadow.takeSnapshot(static_cast<const classname*>(other)->_##fieldname##__shadow.get()); \
            }, \
            nullptr, \
            nullptr, \
            [](const Ovito::RefMaker* obj, Ovito::SaveStream& stream) { \
                static_cast<const classname*>(obj)->_##fieldname##__shadow.saveToStream(stream); \
            }, \
            [](Ovito::RefMaker* obj, Ovito::LoadStream& stream) { \
                static_cast<classname*>(obj)->_##fieldname##__shadow.loadFromStream(stream); \
            }, \
            [](Ovito::RefMaker* obj) { \
                static_cast<classname*>(obj)->_##fieldname##__shadow.takeSnapshot(static_cast<const classname*>(obj)->_##fieldname.get()); \
            }, \
            [](const Ovito::RefMaker* source, Ovito::RefMaker* target) { \
                if(static_cast<const classname*>(source)->_##fieldname##__shadow.hasSnapshot()) \
                    static_cast<classname*>(target)->_##fieldname.set(target, PROPERTY_FIELD(classname::fieldname), static_cast<const classname*>(source)->_##fieldname##__shadow.get()); \
            } \
        );

}   // End of namespace
