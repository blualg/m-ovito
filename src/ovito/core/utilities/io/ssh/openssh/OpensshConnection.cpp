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
#include <ovito/core/app/Application.h>
#include "OpensshConnection.h"
#include "DownloadRequest.h"

#ifdef Q_OS_UNIX
    #include <unistd.h>
    #include <signal.h>
#endif

namespace Ovito::Ssh {

/******************************************************************************
* Returns the path to the "sftp" utility on the user's computer.
******************************************************************************/
QString OpensshConnection::getSftpPath()
{
    return QSettings().value("ssh/sftp_path", QStringLiteral("sftp")).toString();
}

/******************************************************************************
* Saves the path to the "sftp" utility in the application settings store.
******************************************************************************/
void OpensshConnection::setSftpPath(const QString& path)
{
    QSettings settings;
    if(path != QStringLiteral("sftp"))
        settings.setValue("ssh/sftp_path", path);
    else
        settings.remove("ssh/sftp_path");
}

/******************************************************************************
* Constructor.
******************************************************************************/
OpensshConnection::OpensshConnection(const SshConnectionParameters& serverInfo, QObject* parent) : SshConnection(serverInfo, parent)
{
    connect(this, &OpensshConnection::requestFinished, this, &OpensshConnection::processRequests, Qt::QueuedConnection);
}

/******************************************************************************
* Destructor.
******************************************************************************/
OpensshConnection::~OpensshConnection()
{
    disconnectFromHost();
}

/******************************************************************************
* Opens the connection to the host.
******************************************************************************/
void OpensshConnection::connectToHost()
{
    disconnectFromHost();
    if(_state == StateClosed) {
        _process = new QProcess(this);
        connect(_process, &QProcess::started, this, [this]() {
            setState(StateConnecting, true);
            //_process->write("-@progress\n"); // Turn off transfer progress meter.
            _process->write("@!echo \"<<<BEGIN_SESSION>>>\"\n");  // This will tell us when the ssh connection has been established.
        });
        connect(_process, &QProcess::finished, this, [this]() {
            _errorMessages.push_back(tr("sftp process has exited."));
            QByteArray errOutput = _process->readAllStandardError().trimmed();
            if(!errOutput.isEmpty())
                _errorMessages.push_back(QString::fromLocal8Bit(errOutput));
            setState(StateError, true);
        });
        connect(_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            switch(error) {
            case QProcess::FailedToStart:
                _errorMessages.push_back(tr("Failed to start the sftp utility. Either the invoked program is missing, or you may have insufficient permissions or resources to invoke the program."));
                break;
            case QProcess::Crashed:
                _errorMessages.push_back(tr("Failed to run the sftp utility. The process crashed some time after starting successfully."));
                break;
            default:
                _errorMessages.push_back(tr("Failed to run the external sftp utility."));
                break;
            }
            setState(StateError, true);
        });
        connect(_process, &QProcess::readyReadStandardOutput, this, &OpensshConnection::onReadyReadStandardOutput);
        setState(StateInit, true);
        QStringList arguments;
        if(connectionParameters().port > 0)
            arguments << QStringLiteral("-o") << QStringLiteral("Port=%1").arg(connectionParameters().port);
        if(!connectionParameters().userName.isEmpty())
            arguments << QStringLiteral("-o") << QStringLiteral("User=%1").arg(connectionParameters().userName);
        arguments << QStringLiteral("-C"); // Enable compression.
        arguments << QStringLiteral("-f"); // Flush files to disk immediately after transfer.
        arguments << QStringLiteral("-q"); // Quiet mode: disable the progress meter as well as warning and diagnostic messages from ssh.
        arguments << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=no");
        if(!qEnvironmentVariableIsEmpty("OVITO_SSH_LOG"))
            arguments << QStringLiteral("-vv"); // Raise logging level.
        arguments << connectionParameters().host;
        _process->setArguments(std::move(arguments));
        _process->setProgram(getSftpPath());
        if(_process->program().isEmpty()) {
            _errorMessages.push_back(tr("Please specify the executable path to the 'sftp' utility on your computer."));
            setState(StateError, true);
            return;
        }
#ifdef Q_OS_UNIX
        if(Application::instance()->guiMode()) {
            // Set SSH_ASKPASS and DISPLAY environment variables to make OpenSSH call the askpass utility.
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            QString askpassPath = QDir(QCoreApplication::applicationDirPath()).absolutePath() + QStringLiteral("/ssh_askpass");
            env.insert("SSH_ASKPASS", QDir::toNativeSeparators(askpassPath));
            env.insert("SSH_ASKPASS_REQUIRE", "force");
            if(!env.contains("DISPLAY"))
                env.insert("DISPLAY", ":0");
            _process->setProcessEnvironment(env);
            // Use setsid() to detach the sftp process from the terminal and force
            // it to call the askpass utility.
            _process->setChildProcessModifier([] { ::setsid(); });
        }
#endif
        _process->start();
    }
}

/******************************************************************************
* Closes the connection to the host.
******************************************************************************/
void OpensshConnection::disconnectFromHost()
{
    if(_process) {
        setState(StateClosing, false);
        disconnect(_process, nullptr, this, nullptr);
        if(_process->state() == QProcess::Running) {
            connect(_process, &QProcess::finished, _process, &QObject::deleteLater);
            _process->setParent(nullptr);
            _process->write("-@quit\n");
            _process->closeWriteChannel();
        }
        else {
            _process->deleteLater();
        }
        _process = nullptr;
    }
    if(_state != StateClosed && _state != StateCanceledByUser)
        setState(StateClosed, true);
}

/******************************************************************************
* Handles QProcess::readyReadStandardOutput() signal.
******************************************************************************/
void OpensshConnection::onReadyReadStandardOutput()
{
    for(;;) {
        QByteArray line = _process->readLine();
        if(line.isEmpty())
            break;

        if(_state == StateConnecting && line.startsWith("<<<BEGIN_SESSION>>>")) {
            connect(_process, &QProcess::readyReadStandardError, this, &OpensshConnection::onReadyReadStandardError);
            setState(StateOpened, true);
            processRequests();
        }
        else if(line.startsWith("<<<END_REQUEST>>>")) {
            OVITO_ASSERT(_requestInFlight);
            _requestInFlight = false;
            if(_activeRequest)
                delete _activeRequest.data();
            OVITO_ASSERT(_activeRequest.isNull());
            Q_EMIT requestFinished();
        }
        else if(_state == StateOpened && _requestInFlight) {
            if(!_activeRequest.isNull())
                _activeRequest->handleSftpResponse(_process, line);
        }
        else {
#ifdef OVITO_DEBUG
            std::cout << "stdout: ";
#endif
            std::cout << line.trimmed().constData() << std::endl;
        }
    }
}

/******************************************************************************
* Handles QProcess::readyReadStandardError() signal.
******************************************************************************/
void OpensshConnection::onReadyReadStandardError()
{
    auto lines = _process->readAllStandardError().split('\n');
    for(const QByteArray& line : lines) {
        if(line.isEmpty())
            continue;

        if(_state == StateOpened && _requestInFlight && !_activeRequest.isNull()) {
            if(_activeRequest->handleSftpError(line))
                continue;
        }
#ifdef OVITO_DEBUG
        std::cerr << "stderr: ";
#endif
        std::cerr << line.trimmed().constData() << std::endl;
    }
}

/******************************************************************************
* Proceed with processing the next waiting request.
******************************************************************************/
void OpensshConnection::processRequests()
{
    if(_state == StateOpened && !_requestInFlight && _activeRequest.isNull()) {
        _activeRequest = findChild<SshRequest*>({}, Qt::FindDirectChildrenOnly);
        if(!_activeRequest.isNull()) {
            connect(_activeRequest, &SshRequest::closed, this, [&]() {
#ifdef Q_OS_UNIX
                if(_activeRequest.data() == QObject::sender() && _activeRequest->_isInterruptable && _requestInFlight && _process && _process->processId() > 0) {
                    ::kill(pid_t(_process->processId()), SIGINT);
                }
#endif
                _activeRequest.clear();
            });
            _activeRequest->start(_process);
            if(_process && !_activeRequest.isNull()) {
                // Signal end of request.
                _requestInFlight = true;
                _process->write("@!echo \"<<<END_REQUEST>>>\"\n");
            }
        }
    }
}

} // End of namespace
