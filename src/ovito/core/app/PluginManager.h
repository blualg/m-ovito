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
#include <ovito/core/oo/OvitoClass.h>

namespace Ovito {

/**
 * \brief Represents a plugin that is loaded at runtime.
 */
class OVITO_CORE_EXPORT Plugin : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString pluginId READ pluginId CONSTANT)

public:

    /// \brief Returns the unique identifier of the plugin.
    const QString& pluginId() const { return _pluginId; }

    /// \brief Finds the plugin class with the given name defined by the plugin.
    /// \param name The class name.
    /// \return The descriptor for the plugin class with the given name or \c nullptr
    ///         if no such class is defined by the plugin.
    /// \sa classes()
    OvitoClassPtr findClass(const QString& name) const {
        for(OvitoClassPtr type : classes()) {
            if(type->isKnownUnderName(name))
                return type;
        }
        return nullptr;
    }

    /// \brief Returns whether the plugin's dynamic library has been loaded.
    /// \sa loadPlugin()
    bool isLoaded() const { return true; }

    /// \brief Loads the plugin's dynamic link library into memory.
    /// \throw Exception if an error occurs.
    ///
    /// This method may load other plugins first if this plugin
    /// depends on them.
    /// \sa isLoaded()
    void loadPlugin() {}

    /// \brief Returns all classes defined by the plugin.
    /// \sa findClass()
    const QVector<OvitoClassPtr>& classes() const { return _classes; }

protected:

    /// \brief Constructor.
    Plugin(const QString& pluginId) : _pluginId(pluginId) {}

    /// \brief Adds a class to the list of plugin classes.
    void registerClass(OvitoClass* clazz) { _classes.push_back(clazz); }

private:

    /// The unique identifier of the plugin.
    QString _pluginId;

    /// The classes provided by the plugin.
    QVector<OvitoClassPtr> _classes;

    friend class PluginManager;
};

/**
 * \brief Loads and manages the installed plugins.
 */
class OVITO_CORE_EXPORT PluginManager : public QObject
{
    Q_OBJECT

public:

    /// Create the singleton instance of this class.
    static void initialize() {
        _instance = new PluginManager();
        _instance->registerLoadedPluginClasses();
    }

    /// Deletes the singleton instance of this class.
    static void shutdown() { delete _instance; _instance = nullptr; }

    /// Returns the one and only instance of this class.
    inline static PluginManager& instance() {
        OVITO_ASSERT_MSG(_instance != nullptr, "PluginManager::instance", "Singleton object is not initialized yet.");
        return *_instance;
    }

    /// Searches the plugin directories for installed plugins and loads them.
    void loadAllPlugins();

    /// Returns the plugin with a given identifier.
    Plugin* plugin(const QString& pluginId);

    /// Returns the list of installed plugins.
    const std::vector<Plugin*>& plugins() const { return _plugins; }

    /// Returns all installed plugin classes derived from the given type.
    /// \param superClass Specifies the base class from which all returned classes should be derived.
    /// \param onlyInstantiable If \c true, only non-abstract classes are returned.
    std::vector<OvitoClassPtr> listClasses(const OvitoClass& superClass, bool onlyInstantiable = true);

    /// Returns the metaclass with the given name defined by the given plugin.
    OvitoClassPtr findClass(const QString& pluginId, const QString& className);

    /// Returns a list with all classes that belong to a metaclass.
    template<class C>
    std::vector<const typename C::OOMetaClass*> metaclassMembers(const OvitoClass& parentClass = C::OOClass(), bool onlyInstantiable = true) {
        OVITO_ASSERT(parentClass.isDerivedFrom(C::OOClass()));
        std::vector<const typename C::OOMetaClass*> result;
        for(Plugin* plugin : plugins()) {
            for(OvitoClassPtr clazz : plugin->classes()) {
                if(!onlyInstantiable || clazz->isInstantiable()) {
                    if(clazz->isDerivedFrom(parentClass))
                        result.push_back(static_cast<const typename C::OOMetaClass*>(clazz));
                }
            }
        }
        return result;
    }

    /// Registers a new plugin with the manager.
    /// The PluginManager becomes the owner of the Plugin class instance and will
    /// delete it on application shutdown.
    void registerPlugin(Plugin* plugin);

    /// Registers all classes of all plugins already loaded so far.
    void registerLoadedPluginClasses();

    /// Returns the list of directories containing the Ovito plugins.
    std::vector<QDir> pluginDirs();

    /// Returns the path where OVITO Pro's Python files reside.
    QString pythonDir();

    /// Registers an extension class at runtime.
    /// The PluginMananger becomes the owner of the class object and will delete it on application shutdown.
    void addExtensionClass(std::unique_ptr<OvitoClass> clazz);

    /// Looks up a registered extension class.
    OvitoClass* getExtensionClass(const QString& name, OvitoClassPtr superClass) const;

    /// Destructor that unloads all plugins.
    ~PluginManager();

Q_SIGNALS:

    /// This signal is emitted by the PluginManager whenever a new extension class has been registered.
    void extensionClassAdded(OvitoClassPtr clazz);

private:

    /// Private constructor, because this is a singleton class.
    PluginManager();

    /// The list of installed plugins.
    std::vector<Plugin*> _plugins;

    /// The position in the global linked list of native object types up to which classes have already been registered.
    OvitoClass* _lastRegisteredClass = nullptr;

    /// Registered extension classes.
    std::vector<std::unique_ptr<OvitoClass>> _extensionClasses;

    /// The singleton instance of this class.
    static PluginManager* _instance;

    friend class Plugin;
};

}   // End of namespace
