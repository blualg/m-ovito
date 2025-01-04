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
#include <ovito/core/utilities/io/ssh/SshConnection.h>
#include "SshRequest.h"

namespace Ovito {

class OVITO_CORE_EXPORT OpensshConnection : public SshConnection
{
    Q_OBJECT

public:

    /// Constructor.
    explicit OpensshConnection(const SshConnectionParameters& serverInfo, QObject* parent = nullptr);

    /// Destructor.
    virtual ~OpensshConnection();

    /// Returns the host this connection is to.
    const QString& hostname() const { return connectionParameters().host; }

    /// Returns the kind of ssh connection this is.
    virtual SshImplementation implementation() const override { return Openssh; }

    /// Returns the path to the "sftp" utility on the user's computer.
    static QString getSftpPath();

    /// Saves the path to the "sftp" utility in the application settings store.
    static void setSftpPath(const QString& path);

public Q_SLOTS:

    /// Opens the connection to the host.
    virtual void connectToHost() override;

    /// Closes the connection to the host.
    virtual void disconnectFromHost() override;

private Q_SLOTS:

    /// Handles QProcess::readyReadStandardOutput() signal.
    void onReadyReadStandardOutput();

    /// Handles QProcess::readyReadStandardError() signal.
    void onReadyReadStandardError();

    /// Starts the next waiting request.
    void processRequests();

Q_SIGNALS:

    void requestFinished();

private:

    /// The sftp process.
    QProcess* _process = nullptr;

    /// The active request.
    QPointer<SshRequest> _activeRequest;

    bool _requestInFlight = false;

    friend class SshRequest;
};

} // End of namespace
