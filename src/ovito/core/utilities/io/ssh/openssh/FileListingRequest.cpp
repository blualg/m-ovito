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
#include "FileListingRequest.h"
#include "OpensshConnection.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FileListingRequest::FileListingRequest(OpensshConnection* connection, const QString& path) : SshRequest(connection), _path(path)
{
}

/******************************************************************************
* Starts sending commands to the SFTP server.
******************************************************************************/
void FileListingRequest::start(QIODevice* device)
{
    // Request directory listing from server.
    device->write("-@ls -1 -n -f -a -l ");
    device->write(quoteAgument(_path));
    device->write("\n");

    // Signal end of listing.
    device->write("@!echo \"<<<END>>>\"\n");

    Q_EMIT receivingDirectory();
}

/******************************************************************************
* Handles messages from the SFTP program.
******************************************************************************/
void FileListingRequest::handleSftpResponse(QIODevice* device, const QByteArray& line)
{
    if(line.startsWith("<<<END>>>")) {
        Q_EMIT receivedDirectoryComplete(_listing);
        return;
    }

    if(line.size() > 10) {
        int charCount;
        qlonglong fileSize;
        if(sscanf(line.constData(), "%*s %*s %*d %*d %lli %*s %*s %*s%n", &fileSize, &charCount) == 1 && charCount+2 < line.size()) {
            if(line[9] == 'x')
                return; // Skip directories.

            QByteArrayView path = QByteArrayView(line).sliced(charCount+1).chopped(1);
            auto index = path.lastIndexOf('/');
            if(index >= 0) {
                QString filename = QString::fromUtf8(path.sliced(index+1));
                if(filename != QStringLiteral(".") && filename != QStringLiteral("..")) {
                    _listing.push_back(std::move(filename));
                }
                return;
            }
        }
    }

    Q_EMIT error(tr("Could not list remote directory contents. SFTP server response: %1").arg(QString::fromUtf8(line).trimmed()));
}

/******************************************************************************
* Handles messages from the SFTP program.
******************************************************************************/
bool FileListingRequest::handleSftpError(const QByteArray& line)
{
    if(line.startsWith("Can't ls: ")) {
        Q_EMIT error(tr("Could not list remote directory contents. %1").arg(QString::fromUtf8(line.mid(10)).trimmed()));
        return true;
    }
    else if(line.startsWith("remote readdir") && line.contains("Permission denied")) {
        Q_EMIT error(tr("Could not list remote directory contents: Permission denied."));
        return true;
    }
    return SshRequest::handleSftpError(line);
}

} // End of namespace
