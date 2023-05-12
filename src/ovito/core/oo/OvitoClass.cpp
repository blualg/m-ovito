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

#include <ovito/core/Core.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/DataSet.h>
#include "OvitoClass.h"

namespace Ovito {

// Head of linked list of native C++ classes.
OvitoClass* OvitoClass::_firstNativeMetaClass{};

/******************************************************************************
* Constructor used for non-templated classes.
******************************************************************************/
OvitoClass::OvitoClass(const QString& name, OvitoClassPtr superClass, const char* pluginId, const QMetaObject* qtClassInfo) :
    _name(name),
    _displayName(name),
    _superClass(superClass),
    _pluginId(pluginId),
    _qtClassInfo(qtClassInfo)
{
    OVITO_ASSERT(superClass != nullptr || name == QStringLiteral("OvitoObject"));
    OVITO_ASSERT(pluginId != nullptr);

    // If it is a native C++ class, insert it into the linked list.
    _nextNativeMetaclass = _firstNativeMetaClass;
    _firstNativeMetaClass = this;
}

/******************************************************************************
* Is called by the system on program startup.
******************************************************************************/
void OvitoClass::initialize()
{
    // Class must have been initialized with a plugin id.
    OVITO_ASSERT(_pluginId != nullptr);

    // Initialize native C++ classes.
    if(qtMetaObject()) {

        // Mark class as instantiable if it has an invokable constructor.
        if(qtMetaObject()->constructorCount())
            setIsInstantiable();

        // Remove namespace qualifier from Qt's class name.
        _pureClassName = qtMetaObject()->className();
        for(const char* p = _pureClassName; *p != '\0'; p++) {
            if(p[0] == ':' && p[1] == ':') {
                p++;
                _pureClassName = p+1;
            }
        }

        // Fetch display name assigned to the Qt object class.
        if(int idx = qtMetaObject()->indexOfClassInfo("DisplayName"); idx >= 0)
            setDisplayName(QString::fromUtf8(qtMetaObject()->classInfo(idx).value()));

        // Load name alias assigned to the Qt object class.
        if(int idx = qtMetaObject()->indexOfClassInfo("ClassNameAlias"); idx >= 0)
            setNameAlias(QString::fromUtf8(qtMetaObject()->classInfo(idx).value()));
    }
}

/******************************************************************************
* Returns a human-readable string describing this class.
******************************************************************************/
QString OvitoClass::descriptionString() const
{
    if(qtMetaObject()) {
        int index = qtMetaObject()->indexOfClassInfo("Description");
        if(index >= 0)
            return QString::fromUtf8(qtMetaObject()->classInfo(index).value());
    }
    return QString();
}

/******************************************************************************
* Determines if an object is an instance of the class or one of its subclasses.
******************************************************************************/
bool OvitoClass::isMember(const OvitoObject* obj) const
{
    return obj ? obj->getOOClass().isDerivedFrom(*this) : false;
}

/******************************************************************************
* Creates an instance of a class that is NOT derived from RefTarget.
* Throws an exception if the containing plugin failed to load.
******************************************************************************/
OORef<OvitoObject> OvitoClass::createInstance() const
{
    if(plugin()) {
        OVITO_CHECK_POINTER(plugin());
        if(!plugin()->isLoaded()) {
            OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());
            // Load plugin first.
            try {
                plugin()->loadPlugin();
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(OvitoObject::tr("Could not create instance of class %1. Failed to load plugin '%2'").arg(name()).arg(plugin()->pluginId()));
                throw ex;
            }
        }
    }
    if(!isInstantiable())
        throw Exception(OvitoObject::tr("Cannot instantiate abstract class '%1'.").arg(name()));

    // Instantiate the class.
    // Special handling of RefTarget-derived classes.
    if(isDerivedFrom(RefTarget::OOClass()))
        return createInstance(ObjectInitializationFlag::NoFlags);
    else
        return createInstanceImpl({});
}

/******************************************************************************
* Creates an instance of this object class.
* Throws an exception if the containing plugin failed to load.
******************************************************************************/
OORef<RefTarget> OvitoClass::createInstance(ObjectInitializationFlags flags) const
{
    if(plugin()) {
        OVITO_CHECK_POINTER(plugin());
        if(!plugin()->isLoaded()) {
            OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());
            // Load plugin first.
            try {
                plugin()->loadPlugin();
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(OvitoObject::tr("Could not create instance of class %1. Failed to load plugin '%2'").arg(name()).arg(plugin()->pluginId()));
                throw ex;
            }
        }
    }
    if(!isInstantiable())
        throw Exception(OvitoObject::tr("Cannot instantiate abstract class '%1'.").arg(name()));

    OVITO_ASSERT_MSG(isDerivedFrom(RefTarget::OOClass()), "OvitoClass::createInstance()", "This method overload must only be used to instantiate RefTarget-derived classes.");

    // Don't record object initialization on the undo stack.
    UndoSuspender noUndo;

    // Instantiate the class.
    OORef<RefTarget> obj = static_object_cast<RefTarget>(createInstanceImpl(flags));

    // Initialize the parameters of the new object to default values.
    if(ExecutionContext::isInteractive())
        obj->initializeParametersToUserDefaults();

    return obj;
}

/******************************************************************************
* Creates an instance of this object class.
******************************************************************************/
OvitoObject* OvitoClass::createInstanceImpl(ObjectInitializationFlags flags) const
{
#ifdef OVITO_DEBUG
    // Check if class hierarchy is consistent.
    OvitoClassPtr ovitoSuperClass = superClass();
    while(ovitoSuperClass && ovitoSuperClass->qtMetaObject() == nullptr)
        ovitoSuperClass = ovitoSuperClass->superClass();
    OVITO_ASSERT(ovitoSuperClass != nullptr);
    const QMetaObject* qtSuperClass = qtMetaObject()->superClass();
    while(qtSuperClass && qtSuperClass != ovitoSuperClass->qtMetaObject())
        qtSuperClass = qtSuperClass->superClass();
    OVITO_ASSERT_MSG(qtSuperClass != nullptr, "OvitoClass::createInstanceImpl", qPrintable(QString("Class %1 is not derived from base class %2 as specified by the object type descriptor. Qt super class is %3.").arg(name()).arg(superClass()->name()).arg(qtMetaObject()->superClass()->className())));
#endif

    OvitoObject* obj;

    if(isDerivedFrom(RefTarget::OOClass())) {
        obj = qobject_cast<OvitoObject*>(qtMetaObject()->newInstance(Q_ARG(ObjectInitializationFlags, flags)));
    }
    else {
        obj = qobject_cast<OvitoObject*>(qtMetaObject()->newInstance());
    }

    if(!obj)
        throw Exception(OvitoObject::tr("Failed to instantiate class '%1'.").arg(name()));

#ifdef OVITO_DEBUG
    // Mark the object as having been allocated on the heap.
    obj->_isAllocatedOnTheHeap = true;
#endif

    return obj;
}

/******************************************************************************
* Writes a class descriptor to the stream. This is for internal use of the core only.
******************************************************************************/
void OvitoClass::serializeRTTI(SaveStream& stream, OvitoClassPtr type)
{
    stream.beginChunk(0x10000000);
    if(type) {
        stream << type->plugin()->pluginId();
        stream << type->name();
    }
    else {
        stream << QString() << QString();
    }
    stream.endChunk();
}

/******************************************************************************
* Loads a class descriptor from the stream. This is for internal use of the core only.
* Throws an exception if the class is not defined or the required plugin is not installed.
******************************************************************************/
OvitoClassPtr OvitoClass::deserializeRTTI(LoadStream& stream)
{
    QString pluginId, className;
    stream.expectChunk(0x10000000);
    stream >> pluginId;
    stream >> className;
    stream.closeChunk();

    if(pluginId.isEmpty() && className.isEmpty())
        return nullptr;

    try {
        // Look plugin.
        Plugin* plugin = PluginManager::instance().plugin(pluginId);
        if(!plugin) {

            // If plugin does not exist anymore, fall back to searching other plugins for the requested class.
            for(Plugin* otherPlugin : PluginManager::instance().plugins()) {
                if(OvitoClassPtr clazz = otherPlugin->findClass(className))
                    return clazz;
            }

            throw Exception(OvitoObject::tr("A required plugin is not installed: %1").arg(pluginId));
        }
        OVITO_CHECK_POINTER(plugin);

        // Look up class descriptor.
        OvitoClassPtr clazz = plugin->findClass(className);
        if(!clazz) {

            // If class does not exist in the plugin anymore, fall back to searching other plugins for the requested class.
            for(Plugin* otherPlugin : PluginManager::instance().plugins()) {
                if(OvitoClassPtr clazz = otherPlugin->findClass(className))
                    return clazz;
            }

            throw Exception(OvitoObject::tr("Required class '%1' not found in plugin '%2'.").arg(className, pluginId));
        }

        return clazz;
    }
    catch(Exception& ex) {
        ex.prependGeneralMessage(OvitoObject::tr("File cannot be loaded, because it contains object types that are not (or no longer) available in this program version."));
        throw ex;
    }
}

/******************************************************************************
* Encodes the plugin ID and the class name in a string.
******************************************************************************/
QString OvitoClass::encodeAsString(OvitoClassPtr type)
{
    OVITO_CHECK_POINTER(type);
    return type->plugin()->pluginId() + QStringLiteral("::") + type->name();
}

/******************************************************************************
* Decodes a class descriptor from a string, which has been generated by encodeAsString().
******************************************************************************/
OvitoClassPtr OvitoClass::decodeFromString(const QString& str)
{
    QStringList tokens = str.split(QStringLiteral("::"));
    if(tokens.size() != 2)
        throw Exception(OvitoObject::tr("Invalid type or encoding: %1").arg(str));

    // Look up plugin.
    Plugin* plugin = PluginManager::instance().plugin(tokens[0]);
    if(!plugin) {

        // If plugin does not exist anymore, fall back to searching other plugins for the requested class.
        for(Plugin* otherPlugin : PluginManager::instance().plugins()) {
            if(OvitoClassPtr clazz = otherPlugin->findClass(tokens[1]))
                return clazz;
        }

        throw Exception(OvitoObject::tr("A required plugin is not installed: %1").arg(tokens[0]));
    }
    OVITO_CHECK_POINTER(plugin);

    // Look up class descriptor.
    OvitoClassPtr clazz = plugin->findClass(tokens[1]);
    if(!clazz) {

        // If class does not exist in the plugin anymore, fall back to searching other plugins for the requested class.
        for(Plugin* otherPlugin : PluginManager::instance().plugins()) {
            if(OvitoClassPtr clazz = otherPlugin->findClass(tokens[1]))
                return clazz;
        }

        throw Exception(OvitoObject::tr("Required class '%1' not found in plugin '%2'.").arg(tokens[1], tokens[0]));
    }

    return clazz;
}

}   // End of namespace
