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

#include <ovito/core/Core.h>
#include "SshConnection.h"

namespace Ovito {

/******************************************************************************
* Returns the user's preferred SSH implementation.
******************************************************************************/
SshConnection::SshImplementation SshConnection::getSshImplementation()
{
#if defined(OVITO_BUILD_PROFESSIONAL) || defined(OVITO_BUILD_PYPI)
    #ifdef OVITO_SSH_CLIENT
        QString selectedSshMethod = QString::fromLocal8Bit(qgetenv("OVITO_SSH_METHOD")).toLower();
        if(selectedSshMethod.isEmpty()) {
            QSettings settings;
            selectedSshMethod = settings.value("ssh/connection_method", QStringLiteral("libssh")).toString();
        }
        if(selectedSshMethod == QStringLiteral("openssh"))
            return Openssh;
        else
            return Libssh;
    #else
        return Openssh;
    #endif
#else
    #ifdef OVITO_SSH_CLIENT
        return Libssh;
    #else
        return None;
    #endif
#endif
}

/******************************************************************************
* Sets the user's preferred SSH implementation.
******************************************************************************/
void SshConnection::setSshImplementation(SshImplementation implementation)
{
#ifdef OVITO_SSH_CLIENT
    QSettings settings;
    settings.setValue("ssh/connection_method", implementation == Openssh ? QStringLiteral("openssh") : QStringLiteral("libssh"));
#endif
}

/******************************************************************************
* Constructor.
******************************************************************************/
SshConnection::SshConnection(const SshConnectionParameters& serverInfo, QObject* parent) : QObject(parent)
{
    _connectionParams = serverInfo;

    // Ensure that connections are always properly closed before the application shuts down.
    OVITO_ASSERT(QCoreApplication::instance() != nullptr);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &SshConnection::disconnectFromHost);
}

/******************************************************************************
* Cancels the connection.
******************************************************************************/
void SshConnection::cancel()
{
    disconnectFromHost();
    setState(StateCanceledByUser, false);
}

/******************************************************************************
* Sets the internal state variable to a new value.
******************************************************************************/
void SshConnection::setState(State state, bool emitStateChangedSignal)
{
    if(_state != state) {
        _state = state;

        // Emit signals:
        switch(_state) {
        case StateClosed:           Q_EMIT disconnected();            break;
        case StateClosing:                                            break;
        case StateInit:                                               break;
        case StateConnecting:                                         break;
        case StateServerIsKnown:                                      break;
        case StateUnknownHost:      Q_EMIT unknownHost();             break;
        case StateAuthChoose:       Q_EMIT chooseAuth();              break;
        case StateAuthContinue:                                       break;
        case StateAuthNone:                                           break;
        case StateAuthAutoPubkey:                                     break;
        case StateAuthPassword:                                       break;
        case StateAuthNeedPassword: Q_EMIT needPassword();            break;
        case StateAuthKbi:                                            break;
        case StateAuthKbiQuestions: Q_EMIT needKbiAnswers();          break;
        case StateAuthAllFailed:    Q_EMIT allAuthsFailed();          break;
        case StateOpened:           Q_EMIT connected();               break;
        case StateError:            Q_EMIT error();                   break;
        case StateCanceledByUser:   Q_EMIT canceled();                break;
        }
    }

    if(emitStateChangedSignal)
        Q_EMIT stateChanged();
}
} // End of namespace
