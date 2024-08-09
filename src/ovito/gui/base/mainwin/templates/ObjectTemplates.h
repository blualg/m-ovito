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


#include <ovito/gui/base/GUIBase.h>
#include <QAbstractListModel>

namespace Ovito {

/**
 * \brief Base class for modifier and viewport overlay template management.
 */
class OVITO_GUIBASE_EXPORT ObjectTemplates : public QAbstractListModel
{
    Q_OBJECT

protected:

    /// \brief Constructor.
    ObjectTemplates(const QString& templateStoreGroup, const QString& objectName, QObject* parent = nullptr);

public:

    /// \brief Returns the names of the stored templates.
    const QStringList& templateList() const { return _templateNames; }

    /// \brief Returns the number of rows in this list model.
    virtual int rowCount(const QModelIndex& parent) const override { return _templateNames.size(); }

    /// \brief Returns the data stored in this list model under the given role.
    virtual QVariant data(const QModelIndex& index, int role) const override {
        if(role == Qt::DisplayRole)
            return _templateNames[index.row()];
        else
            return {};
    }

    /// \brief Creates a new template on the basis of the given object(s).
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param objects The list of one or more objects from which the template should be created.
    /// \return The index of the created template.
    int createTemplate(const QString& templateName, const QVector<OORef<RefTarget>>& objects);

    /// \brief Creates a new template from a serialized version of the object.
    /// \param templateName The name of the new template. If a template with the same name exists, it is overwritten.
    /// \param data The serialized object data, which was originally obtained by a call to templateData().
    /// \return The index of the created template.
    int restoreTemplate(const QString& templateName, QByteArray data);

    /// \brief Deletes the given modifier  from the store.
    void removeTemplate(const QString& templateName);

    /// \brief Renames an existing template.
    void renameTemplate(const QString& oldTemplateName, const QString& newTemplateName);

    /// \brief Instantiates the objects that are stored under the given template name.
    QVector<OORef<RefTarget>> instantiateTemplate(const QString& templateName);

    /// \brief Returns the serialized object data for the given template.
    QByteArray templateData(const QString& templateName);

#ifndef OVITO_DISABLE_QSETTINGS
    /// \brief Writes in-memory template list to the given settings store.
    void commit(QSettings& settings);

    /// \brief Loads a template list from the given settings store.
    int load(QSettings& settings);

    /// \brief Reloads the in-memory template list from the given settings store.
    void restore(QSettings& settings);
#endif

    /// \brief Writes in-memory template list to the given settings store.
    void commit() {
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings settings;
        commit(settings);
#endif
    }

    /// \brief Reloads the in-memory template list from the given settings store.
    void restore() {
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings settings;
        restore(settings);
#endif
    }

private:

    /// Place where the templates are stored in the application settings store.
    const QString _templateStoreGroup;

    /// The type of object that is managed by this template store, e.g. "Modifier". This human-readable name is used in error messages and should be capitalized.
    const QString _objectName;

    /// Holds the names of the templates.
    QStringList _templateNames;

    /// Holds the serialized templates in binary form.
    std::map<QString, QByteArray> _templateData;
};

}   // End of namespace
