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

namespace Ovito {

class OpensshConnection;

class SshRequest : public QObject
{
    Q_OBJECT

public:

    /// Destructor.
    virtual ~SshRequest() {
        Q_EMIT closed();
    }

    /// Initiate the request.
    void submit();

Q_SIGNALS:

    /// This signal is generated when the process terminated (for whatever reason).
    void closed();

    /// This signal is generated when the download process failed because of some error.
    void error(const QString& errorMessage);

protected:

    /// Constructor.
    explicit SshRequest(OpensshConnection* connection);

    /// Tells the request to start sending commands to the SFTP server.
    virtual void start(QIODevice* device) = 0;

    /// Handles responses from the SFTP program.
    virtual void handleSftpResponse(QIODevice* device, const QByteArray& line) = 0;

    /// Handles responses from the SFTP program.
    virtual bool handleSftpError(const QByteArray& line) {
        if(line.startsWith("Connection closed")) {
            _isInterruptable = false;
            Q_EMIT error(tr("SSH connection was closed."));
            return true;
        }
        return false;
    }

    /// Puts a path argument in quotes and escapes special characters.
    static QByteArray quoteAgument(const QString& arg);

    bool _isInterruptable = false;

    friend class OpensshConnection;
};

} // End of namespace
