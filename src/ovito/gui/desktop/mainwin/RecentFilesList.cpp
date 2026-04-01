////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/oo/OvitoClass.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include "RecentFilesList.h"

namespace Ovito {

/******************************************************************************
* Returns the application-wide singleton instance, creating it on first call.
******************************************************************************/
RecentFilesList& RecentFilesList::instance()
{
    static RecentFilesList inst;
    return inst;
}

/******************************************************************************
* Constructor — loads the persisted list from QSettings.
******************************************************************************/
RecentFilesList::RecentFilesList()
{
    load();
}

/******************************************************************************
* Loads the recent files list from QSettings.
******************************************************************************/
void RecentFilesList::load()
{
    _entries.clear();
    QSettings settings;
    int count = settings.beginReadArray(QStringLiteral("file/mru"));
    for(int i = 0; i < count && _entries.size() < MaxEntries; ++i) {
        settings.setArrayIndex(i);
        QVariantList urlVariants = settings.value(QStringLiteral("urls")).toList();
        if(urlVariants.isEmpty())
            continue;
        Entry entry;
        for(const QVariant& v : urlVariants)
            entry.urls.push_back(v.toUrl());
        entry.importerClassName = settings.value(QStringLiteral("importerClass")).toString();
        entry.importerFormat = settings.value(QStringLiteral("importerFormat")).toString();
        _entries.append(std::move(entry));
    }
    settings.endArray();
}

/******************************************************************************
* Saves the current recent files list to QSettings.
******************************************************************************/
void RecentFilesList::save() const
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("file/mru"), _entries.size());
    for(int i = 0; i < _entries.size(); ++i) {
        settings.setArrayIndex(i);
        QVariantList urlVariants;
        for(const QUrl& url : _entries[i].urls)
            urlVariants.append(url);
        settings.setValue(QStringLiteral("urls"), urlVariants);
        settings.setValue(QStringLiteral("importerClass"), _entries[i].importerClassName);
        settings.setValue(QStringLiteral("importerFormat"), _entries[i].importerFormat);
    }
    settings.endArray();
}

/******************************************************************************
* Adds a .ovito session file to the front of the recent files list.
******************************************************************************/
void RecentFilesList::addSessionFileEntry(const QUrl& url)
{
    // Remove any existing session file entry with the same URL.
    for(int i = _entries.size() - 1; i >= 0; --i) {
        if(_entries[i].isSessionFile() && _entries[i].urls.size() == 1 && _entries[i].urls.front() == url) {
            _entries.removeAt(i);
            break;
        }
    }

    // Prepend the new entry with the "__session__" sentinel.
    Entry entry;
    entry.urls = {url};
    entry.importerClassName = QStringLiteral("__session__");
    _entries.prepend(std::move(entry));

    // Trim to maximum size.
    while(_entries.size() > MaxEntries)
        _entries.removeLast();

    save();
    Q_EMIT listChanged();
}

/******************************************************************************
* Adds a data file import entry to the front of the recent files list.
******************************************************************************/
void RecentFilesList::addEntry(std::vector<QUrl> urls, const FileImporterClass* importerClass, const QString& importerFormat)
{
    if(urls.empty())
        return;

    // Determine the importer class name string (empty = auto-detected).
    QString importerClassName;
    if(importerClass)
        importerClassName = OvitoClass::encodeAsString(importerClass);

    // Remove any existing entry with the same URL list.
    for(int i = _entries.size() - 1; i >= 0; --i) {
        if(_entries[i].urls.size() == urls.size()) {
            bool same = true;
            for(size_t j = 0; j < urls.size(); ++j) {
                if(_entries[i].urls[j] != urls[j]) {
                    same = false;
                    break;
                }
            }
            if(same) {
                _entries.removeAt(i);
                break;
            }
        }
    }

    // Prepend the new entry.
    Entry entry;
    entry.urls = std::move(urls);
    entry.importerClassName = std::move(importerClassName);
    entry.importerFormat = importerFormat;
    _entries.prepend(std::move(entry));

    // Trim to maximum size.
    while(_entries.size() > MaxEntries)
        _entries.removeLast();

    save();
    Q_EMIT listChanged();
}

/******************************************************************************
* Removes the entry at the given index from the list.
******************************************************************************/
void RecentFilesList::removeEntry(int index)
{
    if(index < 0 || index >= _entries.size())
        return;
    _entries.removeAt(index);
    save();
    Q_EMIT listChanged();
}

}   // End of namespace
