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

#pragma once

#include <ovito/core/Core.h>
#include "SshRequest.h"

namespace Ovito::Ssh {

class OpensshConnection;

class FileListingRequest : public SshRequest
{
    Q_OBJECT

public:

    /// Constructor.
    FileListingRequest(OpensshConnection* connection, const QString& path);

Q_SIGNALS:

    /// This signal is generated before transmission of a directory listing begins.
    void receivingDirectory();

    /// This signal is generated after a directory listing has been fully transmitted.
    void receivedDirectoryComplete(const QStringList& listing);

protected:

    /// Starts sending commands to the SFTP server.
    virtual void start(QIODevice* device) override;

    /// Handles messages from the SFTP program.
    virtual void handleSftpResponse(QIODevice* device, const QByteArray& line) override;

    /// Handles responses from the SFTP program.
    virtual bool handleSftpError(const QByteArray& line) override;

private:

    const QString _path;
    QStringList _listing;
};

} // End of namespace
