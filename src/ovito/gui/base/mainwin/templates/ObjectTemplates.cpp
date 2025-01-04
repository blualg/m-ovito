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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/app/UserInterface.h>
#include "ObjectTemplates.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ObjectTemplates::ObjectTemplates(const QString& templateStoreGroup, const QString& objectName, QObject* parent) :
    QAbstractListModel(parent),
    _templateStoreGroup(templateStoreGroup),
    _objectName(objectName)
{
    restore();
}

/******************************************************************************
* Creates a new template on the basis of the given object(s).
******************************************************************************/
int ObjectTemplates::createTemplate(const QString& templateName, const QVector<OORef<RefTarget>>& objects)
{
    if(objects.empty())
        throw Exception(tr("Expected non-empty %1 list for creating a new %1 template.").arg(_objectName.toLower()));

    QByteArray buffer;
    QDataStream dstream(&buffer, QIODevice::WriteOnly);
    ObjectSaveStream stream(dstream);

    // Serialize objects.
    for(RefTarget* obj : objects) {
        stream.beginChunk(0x01);
        stream.saveObject(obj);
        stream.endChunk();
    }

    // Append EOF marker.
    stream.beginChunk(0x00);
    stream.endChunk();
    stream.close();

    return restoreTemplate(templateName, std::move(buffer));
}

/******************************************************************************
* Creates a template from a serialized version of the object(s).
******************************************************************************/
int ObjectTemplates::restoreTemplate(const QString& templateName, QByteArray data)
{
    if(templateName.trimmed().isEmpty())
        throw Exception(tr("Invalid %1 template name.").arg(_objectName.toLower()));

    _templateData[templateName] = std::move(data);
    int idx = _templateNames.indexOf(templateName);
    if(idx >= 0) {
        Q_EMIT dataChanged(index(idx, 0), index(idx, 0));
        return idx;
    }
    else {
        beginInsertRows(QModelIndex(), _templateNames.size(), _templateNames.size());
        _templateNames.push_back(templateName);
        endInsertRows();
        return _templateNames.size() - 1;
    }
}

/******************************************************************************
* Deletes the given template from the store.
******************************************************************************/
void ObjectTemplates::removeTemplate(const QString& templateName)
{
    int idx = _templateNames.indexOf(templateName);
    if(idx < 0)
        throw Exception(tr("%1 template with the name '%2' does not exist.").arg(_objectName).arg(templateName));

    _templateData.erase(templateName);
    beginRemoveRows(QModelIndex(), idx, idx);
    _templateNames.removeAt(idx);
    endRemoveRows();
}

/******************************************************************************
* Renames an existing template.
******************************************************************************/
void ObjectTemplates::renameTemplate(const QString& oldTemplateName, const QString& newTemplateName)
{
    int idx = _templateNames.indexOf(oldTemplateName);
    if(idx < 0)
        throw Exception(tr("%1 template with the name '%2' does not exist.").arg(_objectName).arg(oldTemplateName));
    if(_templateNames.contains(newTemplateName))
        throw Exception(tr("%1 template with the name '%2' does already exist.").arg(_objectName).arg(newTemplateName));
    if(newTemplateName.trimmed().isEmpty())
        throw Exception(tr("Invalid new %1 template name.").arg(_objectName.toLower()));

    _templateData[newTemplateName] = templateData(oldTemplateName);
    _templateData.erase(oldTemplateName);
    _templateNames[idx] = newTemplateName;
    Q_EMIT dataChanged(index(idx, 0), index(idx, 0));
}

/******************************************************************************
* Returns the serialized object data for the given template.
******************************************************************************/
QByteArray ObjectTemplates::templateData(const QString& templateName)
{
    int idx = _templateNames.indexOf(templateName);
    if(idx < 0)
        throw Exception(tr("%1 template with the name '%2' does not exist.").arg(_objectName).arg(templateName));
    auto iter = _templateData.find(templateName);
    if(iter != _templateData.end())
        return iter->second;
#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    settings.beginGroup(_templateStoreGroup);
    QByteArray buffer = settings.value(templateName).toByteArray();
#else
    QByteArray buffer;
#endif
    if(buffer.isEmpty())
        throw Exception(tr("Modifier template with the name '%1' does not exist in the settings store.").arg(templateName));
    _templateData.insert(std::make_pair(templateName, buffer));
    return buffer;
}

/******************************************************************************
* Instantiates the objects that are stored under the given template name.
******************************************************************************/
QVector<OORef<RefTarget>> ObjectTemplates::instantiateTemplate(const QString& templateName)
{
    QVector<OORef<RefTarget>> objectSet;
    try {
        UndoSuspender noUndo;
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings settings;
        settings.beginGroup(_templateStoreGroup);
        QByteArray buffer = settings.value(templateName).toByteArray();
#else
        QByteArray buffer;
#endif
        if(buffer.isEmpty())
            throw Exception(tr("%1 template with the name '%2' does not exist.").arg(_objectName).arg(templateName));
        QDataStream dstream(buffer);
        ObjectLoadStream stream(dstream);
        for(int chunkId = stream.expectChunkRange(0,1); chunkId == 1; chunkId = stream.expectChunkRange(0,1)) {
            objectSet.push_back(stream.loadObject<RefTarget>());
            stream.closeChunk();
        }
        stream.closeChunk();
        stream.close();
    }
    catch(Exception& ex) {
        throw ex.prependToMessage(tr("Failed to load %1 template: ").arg(_objectName.toLower()));
    }
    return objectSet;
}

#ifndef OVITO_DISABLE_QSETTINGS
/******************************************************************************
* Writes in-memory template list to the given settings store.
******************************************************************************/
void ObjectTemplates::commit(QSettings& settings)
{
    for(const QString& templateName : _templateNames)
        templateData(templateName);

    settings.beginGroup(_templateStoreGroup);
    settings.remove(QString());
    for(const auto& item : _templateData) {
        settings.setValue(item.first, item.second);
    }
    settings.endGroup();
}

/******************************************************************************
* Loads a template list from the given settings store.
******************************************************************************/
int ObjectTemplates::load(QSettings& settings)
{
    settings.beginGroup(_templateStoreGroup);
    int count = 0;
    for(const QString& templateName : settings.childKeys()) {
        QByteArray buffer = settings.value(templateName).toByteArray();
        if(buffer.isEmpty())
            throw Exception(tr("The stored %1 template with the name '%2' is invalid.").arg(_objectName.toLower()).arg(templateName));
        restoreTemplate(templateName, std::move(buffer));
        count++;
    }
    settings.endGroup();
    return count;
}

/******************************************************************************
* Reloads the in-memory template list from the given settings store.
******************************************************************************/
void ObjectTemplates::restore(QSettings& settings)
{
    _templateData.clear();
    settings.beginGroup(_templateStoreGroup);
    beginResetModel();
    _templateNames = settings.childKeys();
    endResetModel();
}
#endif

}   // End of namespace
