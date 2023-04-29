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
#include <ovito/core/utilities/io/ssh/SshConnection.h>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include "LibsshWrapper.h"

#include <QSocketNotifier>

#ifdef max
    #undef max
#endif
#ifdef min
    #undef min
#endif

namespace Ovito::Ssh {

class LibsshConnection : public SshConnection
{
    Q_OBJECT

public:

    enum HostState {
#if LIBSSH_VERSION_INT >= SSH_VERSION_INT(0, 8, 0)
        HostKnown                   = SSH_KNOWN_HOSTS_OK,
        HostUnknown                 = SSH_KNOWN_HOSTS_UNKNOWN,
        HostKeyChanged              = SSH_KNOWN_HOSTS_CHANGED,
        HostKeyTypeChanged          = SSH_KNOWN_HOSTS_OTHER,
        HostKnownHostsFileMissing   = SSH_KNOWN_HOSTS_NOT_FOUND
#else
        HostKnown                   = SSH_SERVER_KNOWN_OK,
        HostUnknown                 = SSH_SERVER_NOT_KNOWN,
        HostKeyChanged              = SSH_SERVER_KNOWN_CHANGED,
        HostKeyTypeChanged          = SSH_SERVER_FOUND_OTHER,
        HostKnownHostsFileMissing   = SSH_SERVER_FILE_NOT_FOUND
#endif
    };
    Q_ENUM(HostState)

    enum AuthMehodFlag
    {
        AuthMethodUnknown           = SSH_AUTH_METHOD_UNKNOWN,
        AuthMethodNone              = SSH_AUTH_METHOD_NONE,
        AuthMethodPassword          = SSH_AUTH_METHOD_PASSWORD,
        AuthMethodPublicKey         = SSH_AUTH_METHOD_PUBLICKEY,
        AuthMethodHostBased         = SSH_AUTH_METHOD_HOSTBASED,
        AuthMethodKbi               = SSH_AUTH_METHOD_INTERACTIVE
    };
    Q_FLAGS(AuthMehodFlag)
    Q_DECLARE_FLAGS(AuthMethods, AuthMehodFlag)

    enum UseAuthFlag
    {
        UseAuthEmpty                = 0,    ///< Auth method not chosen
        UseAuthNone                 = 1<<0, ///< SSH None authentication method
        UseAuthAutoPubKey           = 1<<1, ///< Keys from ~/.ssh and ssh-agent
        UseAuthPassword             = 1<<2, ///< SSH Password auth method
        UseAuthKbi                  = 1<<3  ///< SSH KBI auth method
    };
    Q_FLAGS(UseAuthFlag)
    Q_DECLARE_FLAGS(UseAuths, UseAuthFlag)

    class KbiQuestion
    {
    public:
        QString instruction;
        QString question;
        bool showAnswer;
    };

public:

    /// Constructor.
    explicit LibsshConnection(const SshConnectionParameters& serverInfo, QObject* parent = nullptr);

    /// Destructor.
    virtual ~LibsshConnection();

    /// Returns the error message string after the error() signal was emitted.
    virtual QStringList errorMessages() const override;

    /// Returns the hashed public key of the current remote host.
    QString hostPublicKeyHash();

    /// This turns the current remote host into a known host by adding it to the knows_hosts file.
    bool markCurrentHostKnown();

    /// Returns the username used to log into the server.
    QString username() const;

    /// Returns the host this connection is to.
    QString hostname() const;

    /// Gets list of Keyboard Interactive questions sent by the server.
    QList<KbiQuestion> kbiQuestions();

    /// Sets the answers to Keyboard Interactive questions.
    void setKbiAnswers(QStringList answers);

    /// Returns the supported authentication methods.
    AuthMethods supportedAuthMethods() const { return AuthMethods(LibsshWrapper::ssh_userauth_list()(_session, 0)); }

    /// Enable or disable one or more authentications.
    void useAuth(UseAuths auths, bool enabled);

    /// Enable or disable the use of 'None' SSH authentication.
    void useNoneAuth(bool enabled) { useAuth(UseAuthNone, enabled); }

    /// Enable or disable the use of automatic public key authentication.
    void useAutoKeyAuth(bool enabled) { useAuth(UseAuthAutoPubKey, enabled); }

    /// Enable or disable the use of password based SSH authentication.
    void usePasswordAuth(bool enabled) { useAuth(UseAuthPassword, enabled); }

    /// Enable or disable the use of Keyboard Interactive SSH authentication.
    void useKbiAuth(bool enabled) { useAuth(UseAuthKbi, enabled); }

    /// Get all enabled authentication methods.
    UseAuths enabledAuths() const { return _useAuths; }

    /// Get all failed authentication methods.
    UseAuths failedAuths() const { return _failedAuths; }

    /// Returns the know/unknown status of the current remote host.
    HostState unknownHostType() const { return _unknownHostType; }

    /// Generates a message string telling the user why the current host is unknown.
    QString unknownHostMessage();

   /// Sets the password for use in password authentication.
    void setPassword(QString password);

    /// Returns the password used to authenticate this connection to the server.
    const QString& password() const { return _password; }

    /// Sets the private key passphrase entered by the user.
    void setPassphrase(const QString& keyPassphrase) { _keyPassphrase = keyPassphrase; }

    /// Returns the kind of ssh connection this is.
    virtual SshImplementation implementation() const override { return Libssh; }

public Q_SLOTS:

    /// Opens the connection to the host.
    virtual void connectToHost() override;

    /// Closes the connection to the host.
    virtual void disconnectFromHost() override;

Q_SIGNALS:

    void doProcessState();
    void doCleanup();

private Q_SLOTS:

    /// Is called after the state has changed.
    void processStateGuard();

    /// Handles the signal from the QSocketNotifier.
    void handleSocketReadable();

    /// Handles the signal from the QSocketNotifier.
    void handleSocketWritable();

private:

    /// Sets the internal state variable to a new value.
    virtual void setState(State state, bool emitStateChangedSignal) override;

    /// The main state machine function.
    void processState();

    /// Sets an option of the libssh session object.
    bool setLibsshOption(enum ssh_options_e type, const void* value);

    /// Creates the notifier objects for the sockets.
    void createSocketNotifiers();

    /// Destroys the notifier objects for the sockets.
    void destroySocketNotifiers();

    /// Re-enables the writable socket notifier.
    void enableWritableSocketNotifier();

    /// Chooses next authentication method to try.
    void tryNextAuth();

    /// Handles the server's reponse to an authentication attempt.
    void handleAuthResponse(int rc, UseAuthFlag auth);

    /// This is a callback that gets called by libssh whenever a passphrase is required.
    static int authenticationCallback(const char* prompt, char* buf, size_t len, int echo, int verify, void* userdata);

    /// The libssh sesssion handle.
    ssh_session _session = nullptr;

    /// Indicates that the password for the connection has been set.
    bool _passwordSet = false;

    /// The passwort that has been set.
    QString _password;

    /// The private key passphrase entered by the user.
    QString _keyPassphrase;

    /// The last log line generated by libssh.
    /// This is used to filter out duplicate messages.
    std::string _lastLogMessage;

    /// Indicates that a call to processState() is in progress.
    bool _processingState = false;

    /// The host known/unknown status.
    HostState _unknownHostType = HostUnknown;

    UseAuths _useAuths = UseAuths(UseAuthNone | UseAuthAutoPubKey | UseAuthPassword | UseAuthKbi);
    UseAuths _failedAuths = UseAuthEmpty;
    UseAuthFlag _succeededAuth = UseAuthEmpty;

    QSocketNotifier* _readNotifier = nullptr;
    QSocketNotifier* _writeNotifier = nullptr;
    bool _enableWritableNofifier = false;

    /// The structure with the callback functions registered with libssh.
    struct ssh_callbacks_struct _sessionCallbacks;

    QElapsedTimer _timeSinceLastChannelClosed;

    friend class SshChannel;
    friend class ProcessChannel;
};

} // End of namespace
