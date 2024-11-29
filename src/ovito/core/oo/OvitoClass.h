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


#include <ovito/core/Core.h>
#include <ovito/core/oo/OORef.h>

namespace Ovito {

/**
 * \brief Meta-class for classes derived from OvitoObject.
 */
class OVITO_CORE_EXPORT OvitoClass
{
public:

    /// Structure holding the serialized metadata for a class that was loaded from a file.
    /// It may be subclassed by metaclasses if they want to store additional information
    /// for each of their classes. This structure is used by the ObjectLoadStream class.
    struct SerializedClassInfo
    {
        /// Virtual destructor.
        virtual ~SerializedClassInfo() = default;

        /// The metaclass instance.
        OvitoClassPtr clazz;
    };

    /// Stores a single key-value pair associated with an OvitoClass instance.
    /// Use the OVITO_CLASSINFO macro to add a metadata item to a class.
    /// Use the OvitoClass::classMetadata() method to look up a metadata item.
    struct MetadataItem {
        MetadataItem(const char* k, const char* v, const OvitoClass& cls) : key(k), value(v), next(std::exchange(const_cast<OvitoClass&>(cls)._metadataHead, this)) {}
        const char* key;
        const char* value;
        MetadataItem* next;
    };

public:

    /// \brief Constructor.
    explicit OvitoClass(const QString& name, OvitoClassPtr superClass, const char* pluginId, OORef<OvitoObject> (*createInstanceFunc)(ObjectInitializationFlags), const std::type_info* typeInfo = nullptr);

    /// \brief Destructor.
    virtual ~OvitoClass() = default;

    /// \brief Returns the name of the C++ class described by this meta-class instance.
    /// \return The name of the class (without namespace qualifier).
    const QString& name() const { return _name; }

    /// \brief Returns the name of the C++ class as a C string.
    /// \return A pointer to the class name string (without namespace qualifier).
    const char* className() const { return _pureClassName.c_str(); }

    /// \brief Returns the human-readable display name of the class.
    /// \return The human-readable name of this object type that should be shown in the user interface.
    const QString& displayName() const { return _displayName; }

    /// \brief Returns a human-readable string describing this class.
    /// \return The description string for this class, or an empty string if the developer did not define a description.
    QString descriptionString() const { return classMetadata("Description"); }

    /// Checks if this class is known under the given name.
    /// This method alias names defined for this class, which are used when looking up the class for a serialized object in a state file.
    /// This allows to maintain backward compatibility when renaming classes in the C++ source code.
    bool isKnownUnderName(const QString& name) const;

    /// \brief Returns the meta-class of the base class.
    OvitoClassPtr superClass() const { return _superClass; }

    /// \brief Returns the identifier of the plugin that defined the class.
    const char* pluginId() const { return _pluginId; }

    /// \brief Returns the plugin that defined the class.
    Plugin* plugin() const { return _plugin; }

    /// Indicates whether this class can be instantiated at runtime.
    bool isInstantiable() const { return _createInstanceFunc != nullptr; }

    /// \brief Determines whether the class is directly or indirectly derived from some other class.
    /// \note This method also returns \c true if the class \a other is the class itself.
    bool isDerivedFrom(const OvitoClass& other) const {
        OvitoClassPtr c = this;
        do {
            if(c == &other)
                return true;
            c = c->superClass();
        }
        while(c);
        return false;
    }

    /// \brief Determines if an object is an instance of the class or one of its subclasses.
    bool isMember(const OvitoObject* obj) const;

    /// \brief Creates an instance of a class.
    /// \return The new instance of the class. The pointer can be safely cast to the corresponding C++ class type.
    /// \throw Exception if a required plugin failed to load, or if the instantiation failed for some other reason.
    OORef<OvitoObject> createInstance(ObjectInitializationFlags flags = ObjectInitializationFlag::NoFlags) const;

    /// Compares two classes based on memory address.
    bool operator==(const OvitoClass& other) const { return (this == &other); }

    /// Compares two classes based on memory address.
    bool operator!=(const OvitoClass& other) const { return (this != &other); }

    /// \brief Writes a type descriptor to the stream.
    /// \note This method is for internal use only.
    static void serializeRTTI(SaveStream& stream, OvitoClassPtr type);

    /// \brief Loads a type descriptor from the stream.
    /// \throw Exception if the class is not defined or the required plugin is not installed.
    /// \note This method is for internal use only.
    static OvitoClassPtr deserializeRTTI(LoadStream& stream);

    /// \brief Encodes the plugin ID and the class name as a string.
    static QString encodeAsString(OvitoClassPtr type);

    /// \brief Decodes a class descriptor from a string, which has been generated by encodeAsString().
    /// \throw Exception if the class is invalid or the plugin is no longer available.
    static OvitoClassPtr decodeFromString(const QString& str);

    /// \brief This method is called by the ObjectSaveStream class when saving one or more object instances of
    ///        a class belonging to this metaclass. May be overridden by sub-metaclasses if they want to store
    ///        additional meta information for the class in the output stream.
    virtual void saveClassInfo(SaveStream& stream) const {}

    /// \brief This method is called by the ObjectLoadStream class when loading one or more object instances
    ///        of a class belonging to this metaclass. May be overridden by sub-metaclasses if they want to restore
    ///        additional meta information for the class from the input stream.
    virtual void loadClassInfo(LoadStream& stream, SerializedClassInfo* classInfo) const {}

    /// Looks up a string value in the class' metadata table.
    QString classMetadata(const char* metadataKey) const;

    /// Creates a new instance of the SerializedClassInfo structure.
    virtual std::unique_ptr<SerializedClassInfo> createClassInfoStructure() const {
        return std::make_unique<SerializedClassInfo>();
    }

    /// Is called by OVITO to ask the class for any information that should be included in the application's system report.
    virtual void querySystemInformation(QTextStream& stream, UserInterface& userInterface) const {}

    /// Changes the human-readable name of this plugin class.
    void setDisplayName(const QString& name) { _displayName = name; }

    /// Returns the class' C++ std::type_info object if it's a native class.
    const std::type_info* typeInfo() const { return _typeInfo; }

protected:

    /// \brief Is called by the system on program startup.
    virtual void initialize();

    /// \brief Creates an instance of the class described by this meta-class.
    /// \return The new instance of the class. The pointer can be safely cast to the corresponding C++ class type.
    /// \throw Exception if the instance could not be created for some reason.
    virtual OORef<OvitoObject> createInstanceImpl(ObjectInitializationFlags flags) const;

protected:

    /// Pointer to function which creates an instance of the class.
    OORef<OvitoObject> (*_createInstanceFunc)(ObjectInitializationFlags);

    /// The class name.
    QString _name;

    /// The human-readable name of this plugin class.
    QString _displayName;

    /// The identifier of the plugin that hosts the class.
    const char* _pluginId = nullptr;

    /// The plugin that hosts the class.
    Plugin* _plugin = nullptr;

    /// The base class descriptor (or nullptr if this is the descriptor for the root OvitoObject class).
    OvitoClassPtr _superClass;

    /// The name of the C++ class if it's a native class.
    std::string _pureClassName;

    /// Head of a linked list of metadata items attached to this class.
    MetadataItem* _metadataHead = nullptr;

    /// The std::type_info object of the C++ class if it's a native class.
    const std::type_info* _typeInfo = nullptr;

    /// All native C++ meta-classes are collected in one linked list.
    OvitoClass* _nextNativeMetaclass;

    /// The head of the linked list with all native C++ meta-classes.
    inline static OvitoClass* _firstNativeMetaClass = nullptr;

    friend class PluginManager;
    friend class RefTarget;     // Give RefTarget::clone() access to low-level method createInstanceImpl().
};

/// \brief Static cast operator for OvitoClass pointers.
///
/// Returns a OvitoClass pointer, cast to target type \c T, which must be an OvitoObject-derived type.
/// Performs a runtime check in debug builds to make sure the input class
/// is really a derived type of the target class.
///
/// \relates OvitoClass
template<class T>
inline const typename T::OOMetaClass* static_class_cast(const OvitoClass* clazz) {
    OVITO_ASSERT_MSG(!clazz || clazz->isDerivedFrom(T::OOClass()), "static_class_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source class %1 is not drived from the target class %2.").arg(clazz->name()).arg(T::OOClass().name())));
    return static_cast<const typename T::OOMetaClass*>(clazz);
}

/// This macro must be included in the class definition of any OvitoObject-derived class.
#define OVITO_CLASS_INTERNAL(classname, baseclassname) \
public: \
    using ovito_parent_class = baseclassname; \
    using ovito_class = classname; \
    static inline const OOMetaClass& OOClass() { return __OOClass_instance; } \
    virtual const Ovito::OvitoClass& getOOClass() const override { return OOClass(); } \
    const OOMetaClass& getOOMetaClass() const { return static_cast<const OOMetaClass&>(getOOClass()); } \
private: \
    static const OOMetaClass __OOClass_instance;

/// This macro must be included in the class definition of any OvitoObject-derived class.
#define OVITO_CLASS(classname) \
    OVITO_CLASS_INTERNAL(classname, ovito_class)

/// This macro is used instead of the default one above when the class should get its own metaclass type.
#define OVITO_CLASS_META(classname, metaclassname) \
    public: \
        using OOMetaClass = metaclassname; \
    OVITO_CLASS(classname)

/// This macro must be included in the .cpp file for any OvitoObject-derived class that can be instantiated at runtime.
#define IMPLEMENT_CREATABLE_OVITO_CLASS(classname) \
    const classname::OOMetaClass classname::__OOClass_instance{ \
        QStringLiteral(#classname), \
        &classname::ovito_parent_class::OOClass(), \
        OVITO_PLUGIN_NAME, \
        [](ObjectInitializationFlags flags) -> OORef<OvitoObject> { return static_object_cast<OvitoObject>(OORef<classname>::createInstanceInternal(flags)); }, \
        &typeid(classname)};

/// This macro must be included in the .cpp file for any abstract OvitoObject-derived class that cannot be instantiated at runtime.
#define IMPLEMENT_ABSTRACT_OVITO_CLASS(classname) \
    const classname::OOMetaClass classname::__OOClass_instance{ \
        QStringLiteral(#classname), \
        &classname::ovito_parent_class::OOClass(), \
        OVITO_PLUGIN_NAME, \
        nullptr, \
        &typeid(classname)};

#define OVITO_CLASSINFO_JOIN_IMPL(A, B) A ## B
#define OVITO_CLASSINFO_JOIN(A, B) OVITO_CLASSINFO_JOIN_IMPL(A, B)

/// Adds a static key-value pair to a class' metadata table, similar to Qt's Q_CLASSINFO macro.
#define OVITO_CLASSINFO(classname, key, value_str) \
    Q_DECL_UNUSED static const Ovito::OvitoClass::MetadataItem OVITO_CLASSINFO_JOIN(__metadata_, __LINE__){key, value_str, classname::OOClass()};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::OvitoClassPtr);
