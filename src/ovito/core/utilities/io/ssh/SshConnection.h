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

namespace Ovito {

struct OVITO_CORE_EXPORT SshConnectionParameters
{
    QString host;
    QString userName;
    QString password;
    unsigned int port = 0;

    bool operator==(const SshConnectionParameters& other) const {
        return host == other.host && userName == other.userName && port == other.port;
    }
    bool operator!=(const SshConnectionParameters& other) const {
        return !(*this == other);
    }
};

class OVITO_CORE_EXPORT SshConnection : public QObject
{
    Q_OBJECT

public:

    enum SshImplementation {
        None,
        Libssh,
        Openssh
    };

    /// Returns the user's preferred SSH implementation.
    static SshImplementation getSshImplementation();

    /// Sets the user's preferred SSH implementation.
    static void setSshImplementation(SshImplementation implementation);

public:

    /// Constructor.
    explicit SshConnection(const SshConnectionParameters& serverInfo, QObject* parent = nullptr);

    /// Returns the error message string after the error() signal was emitted.
    virtual QStringList errorMessages() const { return _errorMessages; }

    /// Returns the SSH connection parameters.
    const SshConnectionParameters& connectionParameters() const { return _connectionParams; }

    /// Indicates whether this connection is successfully opened.
    bool isConnected() const { return _state == StateOpened; }

    /// Returns the kind of ssh connection this is.
    virtual SshImplementation implementation() const = 0;

public Q_SLOTS:

    /// Opens the connection to the host.
    virtual void connectToHost() = 0;

    /// Closes the connection to the host.
    virtual void disconnectFromHost() = 0;

    /// Cancels the connection.
    void cancel();

Q_SIGNALS:

    void connected();
    void disconnected();
    void error();
    void stateChanged();
    void canceled();
    void unknownHost();
    void chooseAuth();
    void needPassword();        ///< Use setPassword() to set password
    void needKbiAnswers();      ///< Use setKbiAnswers() set answers
    void authFailed(int auth);  ///< One authentication attempt has failed
    void allAuthsFailed();      ///< All authentication attempts have failed
    void needPassphrase(QString prompt);      ///< Use setPassprhase() to set passphrase

protected:

    enum State {
        StateClosed                 = 0,
        StateClosing                = 1,
        StateInit                   = 2,
        StateConnecting             = 3,
        StateServerIsKnown          = 4,
        StateUnknownHost            = 5,
        StateAuthChoose             = 6,
        StateAuthContinue           = 7,
        StateAuthNone               = 8,
        StateAuthAutoPubkey         = 9,
        StateAuthPassword           = 10,
        StateAuthNeedPassword       = 11,
        StateAuthKbi                = 12,
        StateAuthKbiQuestions       = 13,
        StateAuthAllFailed          = 14,
        StateOpened                 = 15,
        StateError                  = 16,
        StateCanceledByUser         = 17
    };

    /// Sets the internal state variable to a new value.
    virtual void setState(State state, bool emitStateChangedSignal);

    /// The SSH connection parameters.
    SshConnectionParameters _connectionParams;

    /// The current state of the connection.
    State _state = StateClosed;

    /// The error message describing the last error.
    QStringList _errorMessages;
};

} // End of namespace
