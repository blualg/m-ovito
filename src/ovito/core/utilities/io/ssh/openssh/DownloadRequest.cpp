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
#include "DownloadRequest.h"
#include "OpensshConnection.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
DownloadRequest::DownloadRequest(OpensshConnection* connection, const QString& path) : SshRequest(connection), _path(path)
{
}

/******************************************************************************
* Starts sending commands to the SFTP server.
******************************************************************************/
void DownloadRequest::start(QIODevice* device)
{
    // Create a local temporary file to write into.
    _localFile = std::make_unique<QTemporaryFile>();
    if(!_localFile->open()) {
        Q_EMIT error(tr("Failed to create temporary local file for SFTP download: %1").arg(_localFile->errorString()));
        return;
    }
    _localFile->close();

    // Query file size from server.
    device->write("-@ls -n -1 -a -l ");
    device->write(quoteAgument(_path));
    device->write("\n");

    // Request file download from server.
    device->write("-@get -f ");
    device->write(quoteAgument(_path));
    device->write(" ");
    device->write(quoteAgument(_localFile->fileName()));
    device->write("\n");

    // Signal end of download.
    device->write("@!echo \"<<<END>>>\"\n");
}

/******************************************************************************
* Handles messages from the SFTP program.
******************************************************************************/
void DownloadRequest::handleSftpResponse(QIODevice* device, const QByteArray& line)
{
    if(_timer.isActive()) {
        _isInterruptable = false;
        _timer.stop();
        if(line.startsWith("<<<END>>>")) {
            Q_EMIT receivedFileComplete(&_localFile);
            _localFile.reset();
        }
        else {
            Q_EMIT error(tr("Remote file download failed. SFTP server response: %1").arg(QString::fromUtf8(line).trimmed()));
        }
        return;
    }

    QString response = QString::fromUtf8(line);

    static const QRegularExpression ws_re(QStringLiteral("\\s+"));
    QStringList tokens = response.split(ws_re, Qt::SkipEmptyParts);

    if(tokens.size() > 5 && tokens[0].size() >= 10) {
        bool ok;
        qint64 fileSize = tokens[4].toLongLong(&ok);
        if(ok) {
            _timer.start(400, Qt::VeryCoarseTimer, this);
            _isInterruptable = true;
            Q_EMIT receivingFile(fileSize);
            return;
        }
    }

    _isInterruptable = false;
    Q_EMIT error(tr("Could not determine remote file size. SFTP server response: %1").arg(response.trimmed()));
}

/******************************************************************************
* Handles messages from the SFTP program.
******************************************************************************/
bool DownloadRequest::handleSftpError(const QByteArray& line)
{
    if(line.startsWith("Can't ls: ")) {
        _isInterruptable = false;
        Q_EMIT error(tr("Could not download remote file. %1").arg(QString::fromUtf8(line.mid(10)).trimmed()));
        return true;
    }
    else if(line.startsWith("remote ") && line.contains("Permission denied")) {
        _isInterruptable = false;
        Q_EMIT error(tr("Could not download remote file: Permission denied."));
        return true;
    }
    return SshRequest::handleSftpError(line);
}

/******************************************************************************
* Handles timer events.
******************************************************************************/
void DownloadRequest::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _timer.timerId()) {
        if(_localFile) {
            qint64 currentFileSize = _localFile->size();
            if(currentFileSize > _bytesReceived) {
                _bytesReceived = currentFileSize;
                Q_EMIT receivedData(_bytesReceived);
            }
        }
    }
    SshRequest::timerEvent(event);
}

} // End of namespace
