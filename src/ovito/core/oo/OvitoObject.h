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
#include <ovito/core/oo/OvitoClass.h>
#include <ovito/core/oo/ObjectExecutor.h>

namespace Ovito {

#ifdef OVITO_DEBUG
    /// Checks whether a pointer to an OvitoObject is valid.
    #define OVITO_CHECK_OBJECT_POINTER(object) { OVITO_CHECK_POINTER(static_cast<const OvitoObject*>(object)); OVITO_ASSERT_MSG(static_cast<const OvitoObject*>(object)->__isObjectAlive(), "OVITO_CHECK_OBJECT_POINTER", "OvitoObject pointer is invalid. Object has been deleted."); }
#else
    /// Do nothing for release builds.
    #define OVITO_CHECK_OBJECT_POINTER(object)
#endif

/**
 * \brief Universal base class for most objects in OVITO.
 */
class OVITO_CORE_EXPORT OvitoObject : public std::enable_shared_from_this<OvitoObject>
{
    Q_GADGET

private:

    /// The meta-class descriptor for the OvitoObject C++ class.
    static const OvitoClass __OOClass_instance;
    inline static OvitoClass::MetadataItem* __OOClass_metadata_head = nullptr;

public:

    /// Flags which may be associated with an OvitoObject.
    enum ObjectFlag
    {
        NoFlags = 0,                  //< No flags set.
        BeingConstructed = (1 << 0),  //< Indicates that this object's constructor is executing.
        BeingInitialized = (1 << 1),  //< Indicates that this object is being initialized (initializeObject() hasn't finished yet).
        BeingDeleted = (1 << 2),      //< Indicates that this object is in the process of being deleted.
        BeingLoaded = (1 << 3),       //< Indicates that this object is in the process of being restored from an ObjectLoadStream.
        BeingCopied = (1 << 4),       //< Indicates that this object is in the process of being copied or cloned.
    };
    Q_DECLARE_FLAGS(ObjectFlags, ObjectFlag);

    using ovito_class = OvitoObject;
    using OOMetaClass = OvitoClass;

    /// Returns the class' meta-class descriptor.
    static const OvitoClass& OOClass() { return __OOClass_instance; }

    /// Mimic Qt's string localization function tr() for string literals.
    static inline QString tr(const char* sourceText) { return QString::fromUtf8(sourceText); }

#ifdef OVITO_DEBUG
    /// Note: No need to make the base class destructor virtual, because we use std::shared_ptr to manage an object's lifetime.
    /// std::shared_ptr will call the right destructor of a derived class automatically.
    ~OvitoObject();
#endif

    /// Indicates whether this object is currently being constructed, i.e., the constructor is still executing.
    inline bool isBeingConstructed() const { return _flags.testFlag(BeingConstructed); }

    /// Indicates whether this object is currently being constructed and initialized,
    /// which means the object is not yet in a fully initialized state (initializeObject() has not finished yet).
    inline bool isBeingInitialized() const { return _flags.testFlag(BeingInitialized); }

    /// Indicates whether this object is currently being loaded from an ObjectLoadStream,
    /// which means it is not yet in a fully initialized state.
    inline bool isBeingLoaded() const { return _flags.testFlag(BeingLoaded); }

    /// Returns true if this object is about to be deleted, i.e., if the reference count has reached zero
    /// and aboutToBeDeleted() is being invoked.
    inline bool isBeingDeleted() const { return _flags.testFlag(BeingDeleted); }

    /// Indicates whether this object is currently being copied or cloned.
    inline bool isBeingCopied() const { return _flags.testFlag(BeingCopied); }

    /// Indicates whether this object is currently being initialized or destroyed.
    inline bool isBeingInitializedOrDeleted() const { return _flags.testAnyFlags(ObjectFlags(BeingInitialized | BeingDeleted)); }

#ifdef OVITO_DEBUG
    /// \brief Returns whether this object has not been deleted yet.
    ///
    /// This hidden function is used by the OVITO_CHECK_OBJECT_POINTER macro in debug builds.
    bool __isObjectAlive() const { return _magicAliveCode == 0x87ABCDEF; }
#endif

    /// Returns the class descriptor for this object.
    /// This default implementation is overridden by subclasses to return their type descriptor instead.
    virtual const OvitoClass& getOOClass() const { return OOClass(); }

    /// Returns the class descriptor for this object.
    const OvitoClass& getOOMetaClass() const { return OOClass(); }

    ///////////////////////////////// Executor interface //////////////////////////////////////

    /// Creates some work that can be submitted for execution later and which will be executed in the context of this object (typically in the main thread).
    /// If the object gets destroyed before the work is executed, the scheduled work will be discarded.
    template<typename Function>
    [[nodiscard]] auto schedule(Function&& f) const {
        OVITO_CHECK_OBJECT_POINTER(this);
        OVITO_ASSERT(ExecutionContext::current().isValid());
        OVITO_ASSERT(!isBeingConstructed()); // Note: Cannot create a OOWeakRef<> if the object is not fully constructed yet.
        OVITO_ASSERT(!isBeingDeleted());     // Note: Cannot create a OOWeakRefr<> if the object is already being destructed.
        return [weakRef = weak_from_this(), context = ExecutionContext::current(), f = std::forward<Function>(f)]() mutable noexcept {
            if(auto self = weakRef.lock()) {
                ExecutionContext::Scope execScope(std::move(context));
                self->execute(std::move(f));
            }
        };
    }

    /// Executes some work in the context of this object (typically the main thread).
    template<typename Function>
    void execute(Function&& f) const {
        OVITO_CHECK_OBJECT_POINTER(this);
        OVITO_ASSERT(ExecutionContext::current().isValid());
        OVITO_ASSERT(!isBeingConstructed()); // Note: Cannot create a OOWeakRef<> if the object is not fully constructed yet.
        OVITO_ASSERT(!isBeingDeleted());     // Note: Cannot create a OOWeakRefr<> if the object is already being destructed.
        // If we are in the main thread already, we can immediately execute the work.
        // Otherwise, schedule its execution in the main thread.
        if(ExecutionContext::isMainThread()) {
            // Temporarily suspend undo recording, because deferred operations never get recorded by convention.
            UndoSuspender noUndo;
            std::invoke(std::forward<Function>(f));
        }
        else {
            ExecutionContext::current().runDeferred(this, std::forward<Function>(f));
        }
    }

protected:

    /// This method gets called by OORef<T>::create() right after the object has been constructed by std::make_shared<>.
    /// Subclasses can override this method to perform initialization work that may raise an exception or which needs to create
    /// OORef references to the object itself (which isn't possible in the class constructor).
    inline void initializeObject() {
        OVITO_ASSERT(isBeingConstructed());
        _flags.setFlag(BeingConstructed, false);
    }

    /// Clears the BeingInitialized flag of the object. This method gets called by OORef<T>::create() after the object has been fully initialized.
    inline void completeObjectInitialization() {
        OVITO_ASSERT(!isBeingConstructed());
        OVITO_ASSERT(isBeingInitialized());
        _flags.setFlag(BeingInitialized, false);
    }

    /// Sets the BeingCopied flag of the object indicating that the object is in the
    /// process of being copied. Set by RefTarget::clone
    inline void beginObjectCopy()
    {
        OVITO_ASSERT(!isBeingCopied());
        qDebug() << "Set is being copied";
        _flags.setFlag(BeingCopied, true);
    }

    /// Clears the BeingCopied flag of the object indicating that the copy has been completed.
    /// Called by the CloneHelper class.
    inline void completeObjectCopy()
    {
        OVITO_ASSERT(isBeingCopied());
        qDebug() << "Clear is being copied";
        _flags.setFlag(BeingCopied, false);
    }

    /// \brief Saves the internal data of this object to an output stream.
    /// \param stream The destination data stream.
    /// \param excludeRecomputableData Controls whether the object should not store data that can be recomputed at runtime.
    ///
    /// Subclasses can override this method to write their data fields
    /// to a file. The derived class must call the base implementation saveToStream() first
    /// before it writes its own data to the stream.
    ///
    /// The default implementation of this method does nothing.
    /// \sa loadFromStream()
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const {}

    /// \brief Loads the data of this class from an input stream.
    /// \param stream The source data stream.
    /// \throw Exception when a parsing error has occurred.
    ///
    /// Subclasses can override this method to read their saved data
    /// from the input stream. The derived class must call the base implementation of loadFromStream() first
    /// before reading its own data from the stream.
    ///
    /// The default implementation of this method does nothing.
    ///
    /// \note The OvitoObject is not in a fully initialized state when the loadFromStream() method is called.
    ///       In particular the developer cannot assume that all other objects stored in the data stream and
    ///       referenced by this object have already been restored at the time loadFromStream() is invoked.
    ///       The loadFromStreamComplete() method will be called after all objects stored in a file have been completely
    ///       loaded and and their data has been restored. If you have to perform some post-deserialization
    ///       tasks that require other referenced objects to be in place and fully loaded, then this should
    ///       be done by overriding loadFromStreamComplete().
    ///
    /// \sa saveToStream()
    virtual void loadFromStream(ObjectLoadStream& stream) {}

    /// \brief This method is called once for this object after it has been
    ///        completely deserialized from a data stream.
    ///
    /// It is safe to access sub-objects from this method.
    /// The default implementation of this method does nothing.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) {}

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted. You should not call this method from user
    /// code and typically it is not necessary to override this method.
    virtual void aboutToBeDeleted() { OVITO_CHECK_OBJECT_POINTER(this); }

private:

    /// Internal method that calls this object's aboutToBeDeleted() routine and the deletes the object.
    /// It is automatically called when the object's reference counter reaches zero.
    void deleteObjectInternal() noexcept;

    /// Returns the name of the plugin class this object is an instance of.
    /// This method is an implementation detail required by the Q_PROPERTY macro above.
    const QString& className() const { return getOOClass().name(); }

    /// Returns the idenitifier of the plugin module this object belongs to.
    /// This method is an implementation detail required by the Q_PROPERTY macro above.
    QString pluginId() const { return QString::fromLatin1(getOOClass().pluginId()); }

    /// Bit-wise flags.
    ObjectFlags _flags = ObjectFlags(BeingConstructed | BeingInitialized);

#ifdef OVITO_DEBUG
    /// This field is initialized with a special value by the class constructor to indicate that
    /// the object is still alive and has not been deleted. When the object is deleted, the
    /// destructor sets the field to a different value to indicate that the object is no longer alive.
    quint32 _magicAliveCode = 0x87ABCDEF;

    friend class OvitoClass;
#endif

    // Give OORef smart-pointer class access to the internal reference counter.
    template<class T> friend class OORef;
    template<typename T> friend struct OOAllocator;

    // These classes need to access the protected serialization functions.
    friend class ObjectSaveStream;
    friend class ObjectLoadStream;
};

/// Prints an object to Qt debug stream.
OVITO_CORE_EXPORT QDebug operator<<(QDebug dbg, const OvitoObject* o);

/// \brief Dynamic cast operator for subclasses of OvitoObject.
///
/// Returns a pointer to the input object, cast to type \c T if the object is of type \c T
/// (or a subclass); otherwise returns \c nullptr.
///
/// \relates OvitoObject
template<class T, class U>
inline T* dynamic_object_cast(U* obj) noexcept {
    return dynamic_cast<T*>(obj);
}

/// \brief Dynamic cast operator for subclasses of OvitoObject derived.
///
/// Returns a constant pointer to the input object, cast to type \c T if the object is of type \c T
/// (or subclass); otherwise returns \c nullptr.
///
/// \relates OvitoObject
template<class T, class U>
inline const T* dynamic_object_cast(const U* obj) noexcept {
    return dynamic_cast<const T*>(obj);
}

/// \brief Static cast operator for OvitoObject derived classes.
///
/// Returns a pointer to the object, cast to target type \c T.
/// Performs a runtime check in debug builds to make sure the input object
/// is really an instance of the target class.
///
/// \relates OvitoObject
template<class T, class U>
inline T* static_object_cast(U* obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_cast<T*>(obj);
}

/// \brief Static cast operator for OvitoObject derived object.
///
/// Returns a const pointer to the object, cast to target type \c T.
/// Performs a runtime check in debug builds to make sure the input object
/// is really an instance of the target class.
///
/// \relates OvitoObject
template<class T, class U>
inline const T* static_object_cast(const U* obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_cast<const T*>(obj);
}

/// \brief Turns a pointer to a const object into a pointer to a non-const object.
template<class T>
T* const_pointer_cast(const T* p) noexcept {
    return const_cast<T*>(p);
}

/// \brief Dynamic cast operator for fancy pointers to OVITO objects.
///
/// Returns a fancy pointer to the input object, cast to type \c T if the object is of type \c T
/// (or a subclass); otherwise returns \c nullptr.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<T> dynamic_object_cast(const Pointer<U>& obj) noexcept {
    return dynamic_pointer_cast<T, U>(obj);
}

/// \brief Dynamic cast operator for fancy pointers to OVITO objects.
///
/// Returns a fancy pointer to the input object, cast to type \c T if the object is of type \c T
/// (or a subclass); otherwise returns \c nullptr.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<const T> dynamic_object_cast(const Pointer<const U>& obj) noexcept {
    return dynamic_pointer_cast<const T, const U>(obj);
}

/// \brief Dynamic cast operator for fancy pointers to OVITO objects.
///
/// Returns a fancy pointer to the input object, cast to type \c T if the object is of type \c T
/// (or a subclass); otherwise returns \c nullptr.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<T> dynamic_object_cast(Pointer<U>&& obj) noexcept {
    return dynamic_pointer_cast<T, U>(std::move(obj));
}

/// \brief Dynamic cast operator for fancy pointers to OVITO objects.
///
/// Returns a fancy pointer to the input object, cast to type \c T if the object is of type \c T
/// (or a subclass); otherwise returns \c nullptr.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<const T> dynamic_object_cast(Pointer<const U>&& obj) noexcept {
    return dynamic_pointer_cast<const T, const U>(std::move(obj));
}

/// \brief Static cast operator for fancy pointers to OVITO objects.
///
/// Returns the given object cast to type \c T.
/// Performs a runtime check of the object type in debug build.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<T> static_object_cast(const Pointer<U>& obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_pointer_cast<T, U>(obj);
}

/// \brief Static cast operator for fancy pointers to OVITO objects.
///
/// Returns the given object cast to type \c T.
/// Performs a runtime check of the object type in debug build.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<const T> static_object_cast(const Pointer<const U>& obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_pointer_cast<const T, const U>(obj);
}

/// \brief Static cast operator for fancy pointers to OVITO objects.
///
/// Returns the given object cast to type \c T.
/// Performs a runtime check of the object type in debug build.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<T> static_object_cast(Pointer<U>&& obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_pointer_cast<T, U>(std::move(obj));
}

/// \brief Static cast operator for fancy pointers to OVITO objects.
///
/// Returns the given object cast to type \c T.
/// Performs a runtime check of the object type in debug build.
///
/// \relates OORef, DataOORef
template<class T, class U, template<typename> class Pointer>
inline Pointer<const T> static_object_cast(Pointer<const U>&& obj) noexcept {
    OVITO_ASSERT_MSG(!obj || obj->getOOClass().isDerivedFrom(T::OOClass()), "static_object_cast",
        qPrintable(QStringLiteral("Runtime type check failed. The source object %1 is not an instance of the target class %2.").arg(obj->getOOClass().name()).arg(T::OOClass().name())));
    return static_pointer_cast<const T, const U>(std::move(obj));
}

}   // End of namespace

Q_DECLARE_SMART_POINTER_METATYPE(Ovito::OORef);

#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/oo/ObjectExecutor.h>
