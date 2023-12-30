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
#include <ovito/core/app/Application.h>
#include "PluginManager.h"

#ifndef OVITO_BUILD_MONOLITHIC
    #include <QLibrary>
#endif

namespace Ovito {

/// The singleton instance of this class.
PluginManager* PluginManager::_instance = nullptr;

/******************************************************************************
* Initializes the plugin manager.
******************************************************************************/
PluginManager::PluginManager()
{
    OVITO_ASSERT_MSG(!_instance, "PluginManager constructor", "Multiple instances of this singleton class have been created.");
}

/******************************************************************************
* Unloads all plugins.
******************************************************************************/
PluginManager::~PluginManager()
{
    // Unload extension classes.
    _extensionClasses.clear();

    // Unload plugins in reverse order.
    for(int i = plugins().size() - 1; i >= 0; --i) {
        delete plugins()[i];
    }
}

/******************************************************************************
* Returns the plugin with the given identifier.
* Returns NULL when no such plugin is installed.
******************************************************************************/
Plugin* PluginManager::plugin(const QString& pluginId)
{
    for(Plugin* plugin : plugins()) {
        if(plugin->pluginId() == pluginId)
            return plugin;
    }
    return nullptr;
}

/******************************************************************************
* Registers a new plugin with the manager.
******************************************************************************/
void PluginManager::registerPlugin(Plugin* plugin)
{
    OVITO_CHECK_POINTER(plugin);

    // Make sure the plugin's ID is unique.
    if(this->plugin(plugin->pluginId())) {
        QString id = plugin->pluginId();
        delete plugin;
        throw Exception(tr("Non-unique plugin identifier detected: %1").arg(id));
    }

    _plugins.push_back(plugin);
}

/******************************************************************************
* Returns the list of directories containing the Ovito plugins.
******************************************************************************/
QList<QDir> PluginManager::pluginDirs()
{
    // Resolve path to Ovito's plugin directory, which is specified at compile time relative to the executable path.
    // See ovito/core/CMakeLists.txt for details.
    QDir prefixDir(Application::instance()->applicationDirPath());
    QString pluginsPath = prefixDir.absolutePath() + QChar('/') + QStringLiteral(OVITO_PLUGINS_RELATIVE_PATH);
    return { QDir(pluginsPath) };
}

/******************************************************************************
* Returns the path where OVITO Pro's Python files reside.
******************************************************************************/
QString PluginManager::pythonDir()
{
#ifndef OVITO_BUILD_CONDA
    // Resolve path to Ovito's Python layer files, which is specified at compile time relative to the executable path.
    // See ovito/core/CMakeLists.txt for details.
    QDir prefixDir(Application::instance()->applicationDirPath());
    return QDir(prefixDir.filePath(QStringLiteral(OVITO_PYTHON_LAYER_PATH))).absolutePath();
#else
    // Resolve path to Ovito's Python layer files, which is specified at compile time relative to the Conda prefix path.
    // See ovito/core/CMakeLists.txt for details.
    QDir prefixDir(QDir::fromNativeSeparators(qEnvironmentVariable("CONDA_PREFIX")));
    return QDir(prefixDir.filePath(QStringLiteral(OVITO_PYTHON_LAYER_PATH))).absolutePath();
#endif
}

/******************************************************************************
* Searches the plugin directories for installed plugins and loads them.
******************************************************************************/
void PluginManager::loadAllPlugins()
{
// Only load plugin dynamic libraries if they are not already linked into the executable.
#ifndef OVITO_BUILD_MONOLITHIC

#ifdef Q_OS_WIN
    // Extend enviroment variable PATH so that the plugin DLLs are automatically found, because
    // there typically are inter-dependencies between them.
    QByteArray path = qgetenv("PATH");
    for(QDir pluginDir : pluginDirs()) {
        path = QDir::toNativeSeparators(pluginDir.absolutePath()).toUtf8() + ";" + path;
    }
    qputenv("PATH", path);
#endif

    // Scan the plugin directories for installed plugins.
    // This only done in standalone mode.
    // When OVITO is being used from an external Python interpreter,
    // then plugins are loaded via explicit import statements.
    for(QDir pluginDir : pluginDirs()) {
        if(!pluginDir.exists())
            throw Exception(tr("Failed to scan the plugin directory. Path %1 does not exist.").arg(pluginDir.path()));

        // List all plugin files.
#ifndef Q_OS_WIN
        pluginDir.setNameFilters(QStringList() << "*.so");
#else
        pluginDir.setNameFilters(QStringList() << "*.ovito.dll");
#endif

        pluginDir.setFilter(QDir::Files);
        for(const QString& file : pluginDir.entryList()) {
            QString filePath = pluginDir.absoluteFilePath(file);
            QLibrary* library = new QLibrary(filePath, this);
            library->setLoadHints(QLibrary::ExportExternalSymbolsHint);
            if(!library->load()) {
                Application::instance()->reportError(QStringLiteral("Failed to load native plugin library.\nLibrary file: %1\nError: %2").arg(filePath, library->errorString()));
            }
        }
    }
#endif

    registerLoadedPluginClasses();
}

/******************************************************************************
* Registers all classes of all plugins already loaded so far.
******************************************************************************/
void PluginManager::registerLoadedPluginClasses()
{
    for(OvitoClass* clazz = OvitoClass::_firstNativeMetaClass; clazz != _lastRegisteredClass; clazz = clazz->_nextNativeMetaclass) {
        clazz->initialize();
        OVITO_ASSERT(clazz->pluginId() != nullptr);

        Plugin* classPlugin = nullptr;
        for(Plugin* plugin : plugins()) {
            if(plugin->pluginId() == clazz->pluginId()) {
                classPlugin = plugin;
                break;
            }
        }
        if(!classPlugin) {
            classPlugin = new Plugin(clazz->pluginId());
            registerPlugin(classPlugin);
        }
        OVITO_ASSERT(clazz->plugin() == nullptr);
        clazz->_plugin = classPlugin;
        classPlugin->registerClass(clazz);
    }
    _lastRegisteredClass = OvitoClass::_firstNativeMetaClass;
}

/******************************************************************************
* Registers an extension class at runtime.
* The PluginMananger becomes the owner of the class object and will delete
* it on application shutdown.
******************************************************************************/
void PluginManager::addExtensionClass(std::unique_ptr<OvitoClass> clazz)
{
    OVITO_ASSERT(clazz->pluginId() != nullptr);
    OVITO_ASSERT(clazz->plugin() == nullptr);

    registerLoadedPluginClasses();
    OVITO_ASSERT(clazz->plugin() != nullptr);

    _extensionClasses.push_back(std::move(clazz));
}

/******************************************************************************
* Returns the metaclass with the given name defined by the given plugin.
******************************************************************************/
OvitoClassPtr PluginManager::findClass(const QString& pluginId, const QString& className)
{
    if(pluginId.isEmpty()) {
        for(Plugin* p : plugins()) {
            if(OvitoClassPtr clazz = p->findClass(className))
                return clazz;
        }
    }
    else if(Plugin* p = plugin(pluginId)) {
        return p->findClass(className);
    }
    return nullptr;
}

/******************************************************************************
* Returns all installed plugin classes derived from the given type.
******************************************************************************/
QVector<OvitoClassPtr> PluginManager::listClasses(const OvitoClass& superClass, bool onlyInstantiable)
{
    QVector<OvitoClassPtr> result;

    for(Plugin* plugin : plugins()) {
        for(OvitoClassPtr clazz : plugin->classes()) {
            if(!onlyInstantiable || clazz->isInstantiable()) {
                if(clazz->isDerivedFrom(superClass))
                    result.push_back(clazz);
            }
        }
    }

    return result;
}

}   // End of namespace
