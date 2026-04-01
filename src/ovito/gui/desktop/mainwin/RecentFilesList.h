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

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief Singleton that manages the list of recently opened/imported files.
 *
 * Persists to QSettings under the "file/mru" group. Emits listChanged() whenever
 * the list is modified so that all MainWindow instances can update their menus.
 */
class OVITO_GUI_EXPORT RecentFilesList : public QObject
{
    Q_OBJECT

public:

    /// Represents a single entry in the recently opened files list.
    struct Entry {
        /// One or more file URLs (multiple for topology+trajectory imports).
        std::vector<QUrl> urls;
        /// Serialized importer class name (via OvitoClass::encodeAsString()), or empty for auto-detected data files.
        /// The special sentinel value "__session__" indicates a .ovito session file.
        QString importerClassName;
        /// Sub-format identifier selected by the user, or empty.
        QString importerFormat;

        /// Returns true if this entry represents a .ovito session file.
        bool isSessionFile() const { return importerClassName == QStringLiteral("__session__"); }
    };

    /// Maximum number of entries in the recent files list.
    static constexpr int MaxEntries = 8;

    /// Returns the application-wide singleton instance.
    static RecentFilesList& instance();

    /// Returns the current list of recently opened file entries.
    const QList<Entry>& entries() const { return _entries; }

    /// Adds a .ovito session file to the front of the recent files list.
    /// The list is trimmed to MaxEntries. Saves immediately to QSettings.
    void addSessionFileEntry(const QUrl& url);

    /// Adds a data file import entry to the front of the recent files list.
    /// If an entry with the same URL list already exists, it is moved to the front.
    /// The list is trimmed to MaxEntries. Saves immediately to QSettings.
    void addEntry(std::vector<QUrl> urls, const FileImporterClass* importerClass = nullptr, const QString& importerFormat = {});

    /// Removes the entry at the given index from the list. Saves immediately to QSettings.
    void removeEntry(int index);

Q_SIGNALS:

    /// Emitted whenever the list of recent files has changed.
    void listChanged();

private:

    /// Private constructor — use instance() to access.
    RecentFilesList();

    /// Loads the list from QSettings.
    void load();

    /// Saves the current list to QSettings.
    void save() const;

    /// The current list of recent files entries.
    QList<Entry> _entries;
};

}   // End of namespace
