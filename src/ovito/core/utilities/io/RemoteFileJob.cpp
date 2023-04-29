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
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#ifdef OVITO_SSH_CLIENT
    #include <ovito/core/utilities/io/ssh/libssh/LibsshConnection.h>
    #include <ovito/core/utilities/io/ssh/libssh/ScpChannel.h>
    #include <ovito/core/utilities/io/ssh/libssh/LsChannel.h>
#endif
#include <ovito/core/utilities/io/ssh/openssh/OpensshConnection.h>
#include <ovito/core/utilities/io/ssh/openssh/DownloadRequest.h>
#include <ovito/core/utilities/io/ssh/openssh/FileListingRequest.h>
#include "RemoteFileJob.h"

#ifndef Q_OS_WASM
    #include <QNetworkReply>
#endif

namespace Ovito {

using namespace Ovito::Ssh;

/// List SFTP jobs that are waiting to be executed.
QQueue<RemoteFileJob*> RemoteFileJob::_queuedJobs;

/// Tracks of how many jobs are currently active.
int RemoteFileJob::_numActiveJobs = 0;

/// The maximum number of simultaneous jobs at a time.
constexpr int MaximumNumberOfSimultaneousJobs = 2;

/******************************************************************************
* Constructor.
******************************************************************************/
RemoteFileJob::RemoteFileJob(QUrl url, PromiseBase& promise) :
        _url(std::move(url)), _promise(promise)
{
    OVITO_ASSERT(QCoreApplication::instance() != nullptr);

    // Run all event handlers of this class in the main thread.
    moveToThread(QCoreApplication::instance()->thread());

    // Start download process in the main thread.
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

/******************************************************************************
* Opens the network connection.
******************************************************************************/
void RemoteFileJob::start()
{
    if(!_isActive) {
        // Keep a counter of active jobs.
        // If there are too many jobs active simultaneously, queue them to be executed later.
        if(_numActiveJobs >= MaximumNumberOfSimultaneousJobs) {
            _queuedJobs.enqueue(this);
            return;
        }
        else {
            _numActiveJobs++;
            _isActive = true;
        }
    }

    // This background task started to run.
    _promise.setStarted();

    // Check if process has already been canceled.
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }

    // When the user cancels the task, cancel the remote connection.
    _promise.finally([this](Task& task) noexcept {
        if(task.isCanceled())
            QMetaObject::invokeMethod(this, "connectionCanceled");
    });

    if(_url.scheme() == QStringLiteral("sftp")) {
        // Handle sftp URLs.

        SshConnectionParameters connectionParams;
        connectionParams.host = _url.host();
        connectionParams.userName = _url.userName();
        connectionParams.password = _url.password();
        connectionParams.port = _url.port(0);
        _promise.setProgressText(tr("Connecting to remote host %1").arg(connectionParams.host));

        // Open connection.
        _connection = Application::instance()->fileManager().acquireSshConnection(connectionParams);
        OVITO_CHECK_POINTER(_connection);
        if(!_connection) {
            shutdown(false);
            return;
        }

        // Listen for signals from the connection.
        connect(_connection, &SshConnection::error, this, &RemoteFileJob::connectionError);
        connect(_connection, &SshConnection::canceled, this, &RemoteFileJob::connectionCanceled);
        connect(_connection, &SshConnection::allAuthsFailed, this, &RemoteFileJob::authenticationFailed);
        if(_connection->isConnected()) {
            // The connection may already be established at this point if it was chached by the FileManager.
            QTimer::singleShot(0, this, &RemoteFileJob::connectionEstablished);
            return;
        }
        connect(_connection, &SshConnection::connected, this, &RemoteFileJob::connectionEstablished);

        // Start connecting to remote host.
        _connection->connectToHost();
    }
    else {
        // Handle http(s) URLs.
#ifndef Q_OS_WASM
        _promise.setProgressText(tr("Downloading file %1 from %2").arg(_url.fileName()).arg(_url.host()));
        QNetworkAccessManager* networkAccessManager = Application::instance()->networkAccessManager();
        _networkReply = networkAccessManager->get(QNetworkRequest(_url));

        connect(_networkReply, &QNetworkReply::downloadProgress, this, &RemoteFileJob::networkReplyDownloadProgress);
        connect(_networkReply, &QNetworkReply::finished, this, &RemoteFileJob::networkReplyFinished);
#endif
    }
}

/******************************************************************************
* Closes the network connection.
******************************************************************************/
void RemoteFileJob::shutdown(bool success)
{
    if(_connection) {
        disconnect(_connection, nullptr, this, nullptr);
        Application::instance()->fileManager().releaseSshConnection(_connection);
        _connection = nullptr;
    }
#ifndef Q_OS_WASM
    if(_networkReply) {
        disconnect(_networkReply, nullptr, this, nullptr);
        _networkReply->abort();
        _networkReply->deleteLater();
        _networkReply = nullptr;
    }
#endif
    _promise.setFinished();

    // Update the counter of active jobs.
    if(_isActive) {
        _numActiveJobs--;
        _isActive = false;
    }

    // Schedule this object for deletion.
    deleteLater();

    // If there jobs waiting in the queue, execute next job.
    if(!_queuedJobs.isEmpty() && _numActiveJobs < MaximumNumberOfSimultaneousJobs) {
        RemoteFileJob* waitingJob = _queuedJobs.dequeue();
        if(!waitingJob->_promise.isCanceled()) {
            waitingJob->start();
        }
        else {
            // Skip canceled jobs.
            waitingJob->_promise.setStarted();
            waitingJob->shutdown(false);
        }
    }
}

/******************************************************************************
* Handles SSH connection errors.
******************************************************************************/
void RemoteFileJob::connectionError()
{
    QStringList errorMessages = _connection->errorMessages();
    if(errorMessages.size() != 0) {
        if(Application::instance()->guiMode()) {
            errorMessages[0] = tr("<p>Cannot access URL:</p><p><i>%1</i></p><p>SSH connection error: %2</p><p>See <a href=\"https://docs.ovito.org/advanced_topics/remote_file_access.html#troubleshooting-information\">troubleshooting information</a>.</p>")
                .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded).toHtmlEscaped())
                .arg(errorMessages[0].toHtmlEscaped());
        }
        else {
            errorMessages[0] = tr("Accessing URL %1 failed due to SSH connection error: %2. "
                        "See https://docs.ovito.org/advanced_topics/remote_file_access.html#troubleshooting-information for further information.")
                        .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded))
                        .arg(errorMessages[0]);
        }
    }

    _promise.setException(std::make_exception_ptr(Exception(std::move(errorMessages))));

    shutdown(false);
}

/******************************************************************************
* Handles SSH authentication errors.
******************************************************************************/
void RemoteFileJob::authenticationFailed()
{
    _promise.setException(std::make_exception_ptr(
        Exception(tr("Cannot access URL\n\n%1\n\nSSH authentication failed").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)))));

    shutdown(false);
}

/******************************************************************************
* Handles cancelation by the user.
******************************************************************************/
void RemoteFileJob::connectionCanceled()
{
    // If user has canceled the connection, cancel the file retrieval operation as well.
    _promise.cancel();
    shutdown(false);
}

/******************************************************************************
* Handles QNetworkReply finished signals.
******************************************************************************/
void RemoteFileJob::networkReplyFinished()
{
#ifndef Q_OS_WASM
    if(_networkReply->error() == QNetworkReply::NoError) {
        shutdown(true);
    }
    else {
        _promise.setException(std::make_exception_ptr(
            Exception(tr("Cannot access URL\n\n%1\n\n%2").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)).
                arg(_networkReply->errorString()))));

        shutdown(false);
    }
#endif
}

/******************************************************************************
* Handles closing of the SSH channel.
******************************************************************************/
void DownloadRemoteFileJob::channelClosed()
{
    if(!_promise.isFinished()) {
        _promise.setException(std::make_exception_ptr(
            Exception(tr("Failed to download URL\n\n%1\n\nSSH channel was closed unexpectedly.")
                .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)))));
    }

    shutdown(false);
}

/******************************************************************************
* Is called when the SSH connection has been established.
******************************************************************************/
void DownloadRemoteFileJob::connectionEstablished()
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }

#ifdef OVITO_SSH_CLIENT
    if(LibsshConnection* libsshConnection = qobject_cast<LibsshConnection*>(_connection)) {
        _promise.setProgressText(tr("Opening SCP channel to remote host %1").arg(libsshConnection->hostname()));
        Ovito::Ssh::ScpChannel* scpChannel = new ScpChannel(libsshConnection, _url.path());
        connect(scpChannel, &ScpChannel::receivingFile, this, &DownloadRemoteFileJob::receivingFile);
        connect(scpChannel, &ScpChannel::receivedData, this, &DownloadRemoteFileJob::receivedData);
        connect(scpChannel, &ScpChannel::receivedFileComplete, this, &DownloadRemoteFileJob::receivedFileComplete);
        connect(scpChannel, &ScpChannel::error, this, &DownloadRemoteFileJob::channelError);
        connect(scpChannel, &ScpChannel::closed, this, &DownloadRemoteFileJob::channelClosed);
        connect(this, &QObject::destroyed, scpChannel, &QObject::deleteLater);
        scpChannel->openChannel();
        return;
    }
#endif
    if(OpensshConnection* opensshConnection = qobject_cast<OpensshConnection*>(_connection)) {
        _promise.setProgressText(tr("Opening download channel to remote host %1").arg(opensshConnection->hostname()));
        DownloadRequest* downloadRequest = new DownloadRequest(opensshConnection, _url.path());
        connect(downloadRequest, &DownloadRequest::receivingFile, this, &DownloadRemoteFileJob::receivingFile);
        connect(downloadRequest, &DownloadRequest::receivedData, this, &DownloadRemoteFileJob::receivedData);
        connect(downloadRequest, &DownloadRequest::receivedFileComplete, this, &DownloadRemoteFileJob::receivedFileComplete);
        connect(downloadRequest, &DownloadRequest::error, this, &DownloadRemoteFileJob::channelError);
        connect(downloadRequest, &DownloadRequest::closed, this, &DownloadRemoteFileJob::channelClosed);
        connect(this, &QObject::destroyed, downloadRequest, &DownloadRequest::deleteLater);
        downloadRequest->submit();
        return;
    }
    _promise.cancel();
    shutdown(false);
}

/******************************************************************************
* Is called when the SCP channel failed.
******************************************************************************/
void DownloadRemoteFileJob::channelError(const QString& errorMessage)
{
    _promise.setException(std::make_exception_ptr(
        Exception(tr("Cannot access remote URL\n\n%1\n\n%2")
            .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded))
            .arg(errorMessage))));

    shutdown(false);
}

/******************************************************************************
* Closes the network connection.
******************************************************************************/
void DownloadRemoteFileJob::shutdown(bool success)
{
    // Write all received data to the local file.
    if(success)
        storeReceivedData();

    if(_localFile && success)
        _promise.setResults(FileHandle(url(), _localFile->fileName()));
    else
        _localFile.reset();

    // Close network connection.
    RemoteFileJob::shutdown(success);

    // Hand downloaded file to FileManager cache.
    Application::instance()->fileManager().fileFetched(url(), _localFile.release());
}

/******************************************************************************
* Is called when the remote host starts sending the file.
******************************************************************************/
void DownloadRemoteFileJob::receivingFile(qint64 fileSize)
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }
    _promise.setProgressMaximum(fileSize);
    _promise.setProgressText(tr("Fetching remote file %1").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)));
}

/******************************************************************************
* Is called after the file has been downloaded.
******************************************************************************/
void DownloadRemoteFileJob::receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile)
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }
    _localFile = std::move(*localFile);
    shutdown(true);
}

/******************************************************************************
* Is called when the remote host sent some file data.
******************************************************************************/
void DownloadRemoteFileJob::receivedData(qint64 totalReceivedBytes)
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }
    _promise.setProgressValue(totalReceivedBytes);
}

/******************************************************************************
* Handles QNetworkReply progress signals.
******************************************************************************/
void DownloadRemoteFileJob::networkReplyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }
    if(bytesTotal > 0) {
        _promise.setProgressMaximum(bytesTotal);
        _promise.setProgressValue(bytesReceived);
    }
    storeReceivedData();
}

/******************************************************************************
* Writes the data received from the server so far to the local file.
******************************************************************************/
void DownloadRemoteFileJob::storeReceivedData()
{
#ifndef Q_OS_WASM
    if(!_networkReply)
        return;

    try {
        // Create the destination file and open it for writing.
        if(!_localFile) {
            _localFile = std::make_unique<QTemporaryFile>();
            if(!_localFile->open())
                throw Exception(tr("Failed to create temporary file: %1").arg(_localFile->errorString()));
        }

        // Read data from the network stream.
        QByteArray buffer = _networkReply->read(_networkReply->bytesAvailable());

        // Write data into local file.
        if(_localFile->write(buffer) == -1)
            throw Exception(tr("Failed to write downloaded data to temporary file: %1").arg(_localFile->errorString()));
    }
    catch(Exception&) {
        _promise.captureException();
        shutdown(false);
    }
#endif
}

/******************************************************************************
* Is called when the SSH connection has been established.
******************************************************************************/
void ListRemoteDirectoryJob::connectionEstablished()
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }

#ifdef OVITO_SSH_CLIENT
    if(LibsshConnection* libsshConnection = qobject_cast<LibsshConnection*>(_connection)) {
        // Open the LS channel.
        _promise.setProgressText(tr("Opening channel to remote host %1").arg(libsshConnection->hostname()));
        LsChannel* lsChannel = new LsChannel(libsshConnection, _url.path());
        connect(lsChannel, &LsChannel::error, this, &ListRemoteDirectoryJob::channelError);
        connect(lsChannel, &LsChannel::receivingDirectory, this, &ListRemoteDirectoryJob::receivingDirectory);
        connect(lsChannel, &LsChannel::receivedDirectoryComplete, this, &ListRemoteDirectoryJob::receivedDirectoryComplete);
        connect(lsChannel, &LsChannel::closed, this, &ListRemoteDirectoryJob::channelClosed);
        connect(this, &QObject::destroyed, lsChannel, &QObject::deleteLater);
        lsChannel->openChannel();
        return;
    }
#endif
    if(OpensshConnection* opensshConnection = qobject_cast<OpensshConnection*>(_connection)) {
        _promise.setProgressText(tr("Opening channel to remote host %1").arg(opensshConnection->hostname()));
        FileListingRequest* listingRequest = new FileListingRequest(opensshConnection, _url.path());
        connect(listingRequest, &FileListingRequest::error, this, &ListRemoteDirectoryJob::channelError);
        connect(listingRequest, &FileListingRequest::receivingDirectory, this, &ListRemoteDirectoryJob::receivingDirectory);
        connect(listingRequest, &FileListingRequest::receivedDirectoryComplete, this, &ListRemoteDirectoryJob::receivedDirectoryComplete);
        connect(listingRequest, &FileListingRequest::closed, this, &ListRemoteDirectoryJob::channelClosed);
        connect(this, &QObject::destroyed, listingRequest, &FileListingRequest::deleteLater);
        listingRequest->submit();
        return;
    }
    _promise.cancel();
    shutdown(false);
}

/******************************************************************************
* Is called before transmission of the directory listing begins.
******************************************************************************/
void ListRemoteDirectoryJob::receivingDirectory()
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }

    // Set progress text.
    _promise.setProgressText(tr("Listing remote directory %1").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)));
}

/******************************************************************************
* Is called when the SCP channel failed.
******************************************************************************/
void ListRemoteDirectoryJob::channelError(const QString& errorMessage)
{
    _promise.setException(std::make_exception_ptr(
        Exception(tr("Cannot access remote location:\n\n%1\n\n%2")
            .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded))
            .arg(errorMessage))));

    shutdown(false);
}

/******************************************************************************
* Is called after the directory listing has been fully transmitted.
******************************************************************************/
void ListRemoteDirectoryJob::receivedDirectoryComplete(const QStringList& listing)
{
    if(_promise.isCanceled()) {
        shutdown(false);
        return;
    }

    _promise.setResults(listing);
    shutdown(true);
}

/******************************************************************************
* Handles closing of the SSH channel.
******************************************************************************/
void ListRemoteDirectoryJob::channelClosed()
{
    if(!_promise.isFinished()) {
        _promise.setException(std::make_exception_ptr(
            Exception(tr("Failed to list contents of:\n\n%1\n\nSSH channel was closed unexpectedly.")
                .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)))));
    }

    shutdown(false);
}

}   // End of namespace
