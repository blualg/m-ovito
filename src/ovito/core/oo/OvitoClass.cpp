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
OvitoClass::OvitoClass(const QString& name, OvitoClassPtr superClass, const char* pluginId, OORef<OvitoObject>(*createInstanceFunc)(ObjectInitializationFlags), const std::type_info* typeInfo) :
    _createInstanceFunc(createInstanceFunc),
    _name(name),
    _superClass(superClass),
    _pluginId(pluginId),
    _typeInfo(typeInfo)
{
    OVITO_ASSERT(superClass != nullptr || name == QStringLiteral("OvitoObject"));
    OVITO_ASSERT(pluginId != nullptr);

    // Insert class into the linked list.
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
    _pureClassName = name().toStdString();

    // Fetch UI name assigned to the class.
    if(displayName().isEmpty())
        setDisplayName(classMetadata("DisplayName"));

    // Fall back to using C++ class name as display name.
    if(displayName().isEmpty())
        setDisplayName(name());
}

/******************************************************************************
* Checks if this class is known under the given name.
******************************************************************************/
bool OvitoClass::isKnownUnderName(const QString& name) const
{
    if(name == this->name())
        return true;

    // Consider name aliases assigned to the Qt object class.
    for(const MetadataItem* item = _metadataHead; item != nullptr; item = item->next) {
        if(qstrcmp(item->key, "ClassNameAlias") == 0) {
            if(name == QString::fromUtf8(item->value))
                return true;
        }
    }

    return false;
}

/******************************************************************************
* Determines if an object is an instance of the class or one of its subclasses.
******************************************************************************/
bool OvitoClass::isMember(const OvitoObject* obj) const
{
    return obj ? obj->getOOClass().isDerivedFrom(*this) : false;
}

/******************************************************************************
* Looks up a string value in the class' metadata table.
******************************************************************************/
QString OvitoClass::classMetadata(const char* metadataKey) const
{
    for(const MetadataItem* item = _metadataHead; item != nullptr; item = item->next) {
        if(qstrcmp(item->key, metadataKey) == 0)
            return QString::fromUtf8(item->value);
    }
    return QString();
}

/******************************************************************************
* Creates an instance of this object class.
* Throws an exception if the containing plugin failed to load.
******************************************************************************/
OORef<OvitoObject> OvitoClass::createInstance(ObjectInitializationFlags flags) const
{
#if 0 // Note: Dynamic loading of plugins is not yet used in OVITO.
    if(plugin() && !plugin()->isLoaded()) {
        OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());
        // Load plugin first.
        try {
            plugin()->loadPlugin();
        }
        catch(Exception& ex) {
            throw ex.prependGeneralMessage(OvitoObject::tr("Could not create instance of class %1. Failed to load plugin '%2'").arg(name()).arg(plugin()->pluginId()));
        }
    }
#endif

    if(!isInstantiable())
        throw Exception(OvitoObject::tr("Cannot instantiate abstract class '%1'.").arg(name()));

    // Instantiate the class.
    OORef<OvitoObject> obj = createInstanceImpl(flags);

    // Object should have been allocated on the heap and its BeingConstructed flag cleared.
    OVITO_ASSERT(obj->isBeingInitialized() == false);

    return obj;
}

/******************************************************************************
* Creates an instance of this object class.
******************************************************************************/
OORef<OvitoObject> OvitoClass::createInstanceImpl(ObjectInitializationFlags flags) const
{
    OVITO_ASSERT(_createInstanceFunc != nullptr);
    return _createInstanceFunc(flags);
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
        throw ex.prependGeneralMessage(OvitoObject::tr("File cannot be loaded, because it contains object types that are not (or no longer) available in this program version."));
    }
}

/******************************************************************************
* Encodes the plugin ID and the class name in a string.
******************************************************************************/
QString OvitoClass::encodeAsString(OvitoClassPtr type)
{
    if(!type)
        return QString();
    else
        return type->plugin()->pluginId() + QStringLiteral("::") + type->name();
}

/******************************************************************************
* Decodes a class descriptor from a string, which has been generated by encodeAsString().
******************************************************************************/
OvitoClassPtr OvitoClass::decodeFromString(const QString& str)
{
    if(str.isEmpty())
        return nullptr;

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
