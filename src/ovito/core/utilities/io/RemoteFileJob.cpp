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
#include <ovito/core/utilities/concurrent/Future.h>
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

/// List SFTP jobs that are waiting to be executed.
std::queue<PromiseBase> RemoteFileJob::_queuedJobs;

/// Tracks of how many jobs are currently active.
int RemoteFileJob::_numActiveJobs = 0;

/// The maximum number of simultaneous jobs at a time.
constexpr int MaximumNumberOfSimultaneousJobs = 2;

/******************************************************************************
* Constructor.
******************************************************************************/
RemoteFileJob::RemoteFileJob(const QUrl& url, void* resultsStorage) :
    Task(Task::NoState, resultsStorage),
    _url(url)
{
    // Run all signal handlers of this class in the main thread.
    moveToThread(Application::instance()->thread());
}

/******************************************************************************
* Begins execution of the task. This function gets invoked by the launchTask() helper.
******************************************************************************/
void RemoteFileJob::operator()()
{
    // We may not be in the main thread at this point.
    // Make sure to start the download process in the main thread.
    Application::instance()->taskManager().submitWork([promise = PromiseBase(shared_from_this())]() mutable noexcept {
        static_cast<RemoteFileJob*>(promise.task().get())->start(std::move(promise));
    });
}

/******************************************************************************
* Opens the network connection.
******************************************************************************/
void RemoteFileJob::start(PromiseBase promise) noexcept
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(!_promise);

    // Check if job has been canceled in the meantime.
    if(isCanceled()) {
        shutdown(false);
        return;
    }

    // We'll need a Qt application object to access the network and perform signal/slot handling.
    try {
        Application::instance()->createQtApplication(false);
    }
    catch(Exception& ex) {
        ex.prependGeneralMessage(tr("Network file access is not possible in this environment. A Qt application object could not be created."));
        captureExceptionAndFinish();
        return;
    }

    // Keep a counter of active jobs.
    // If there are too many jobs active simultaneously, queue them to be executed later.
    if(_numActiveJobs >= MaximumNumberOfSimultaneousJobs) {
        _queuedJobs.push(std::move(promise));
        return;
    }
    else {
        _numActiveJobs++;
        _promise = std::move(promise);
    }

    // Handle cancellation of this task.
    finally(ObjectExecutor(Application::instance()), [this](Task& task) noexcept {
        if(task.isCanceled())
            shutdown(false);
    });

    if(_url.scheme() == QStringLiteral("sftp")) {
        // Handle sftp URLs.

        SshConnectionParameters connectionParams;
        connectionParams.host = _url.host();
        connectionParams.userName = _url.userName();
        connectionParams.password = _url.password();
        connectionParams.port = _url.port(0);
        setProgressText(tr("Connecting to remote host %1").arg(connectionParams.host));

        // Open connection.
        _connection = Application::instance()->fileManager().acquireSshConnection(connectionParams);
        if(!_connection) {
            setException(std::make_exception_ptr(Exception(tr("This particular build of OVITO has no SSH connection support. Please use a different distribution of OVITO to access remote files via SSH."))));
            shutdown(false);
            return;
        }

        // Listen for signals from the connection.
        connect(_connection, &SshConnection::error, this, &RemoteFileJob::connectionError);
        connect(_connection, &SshConnection::canceled, this, &RemoteFileJob::connectionCanceled);
        connect(_connection, &SshConnection::allAuthsFailed, this, &RemoteFileJob::authenticationFailed);
        if(_connection->isConnected()) {
            // The connection may already be established at this point if it was cached by the FileManager.
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
        setProgressText(tr("Downloading file %1 from %2").arg(_url.fileName()).arg(_url.host()));
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
    OVITO_ASSERT(this_task::isMainThread());

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
    PromiseBase promise = std::move(_promise); // Extend lifetime to the end of this function. Otherwise, task may be destroyed by the setFinished() call.
    setFinished();

    // Update the counter of active jobs.
    if(promise) {
        _numActiveJobs--;

        // If there jobs waiting in the queue, execute next job.
        while(!_queuedJobs.empty() && _numActiveJobs < MaximumNumberOfSimultaneousJobs) {
            PromiseBase promise = std::move(_queuedJobs.front());
            _queuedJobs.pop();
            RemoteFileJob* job = static_cast<RemoteFileJob*>(promise.task().get());
            job->start(std::move(promise));
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
        if(Application::guiMode()) {
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

    setException(std::make_exception_ptr(Exception(std::move(errorMessages))));

    shutdown(false);
}

/******************************************************************************
* Handles SSH authentication errors.
******************************************************************************/
void RemoteFileJob::authenticationFailed()
{
    setException(std::make_exception_ptr(
        Exception(tr("Cannot access URL\n\n%1\n\nSSH authentication failed").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)))));

    shutdown(false);
}

/******************************************************************************
* Handles cancellation by the user.
******************************************************************************/
void RemoteFileJob::connectionCanceled()
{
    // If user has canceled the connection, abort the file retrieval operation as well.
    setException(std::make_exception_ptr(Exception(tr("SSH connection was canceled by the user"))));
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
        setException(std::make_exception_ptr(
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
    if(!isFinished()) {
        setException(std::make_exception_ptr(
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
    if(isCanceled()) {
        shutdown(false);
        return;
    }

    Task::Scope taskScope(this);

#ifdef OVITO_SSH_CLIENT
    if(LibsshConnection* libsshConnection = qobject_cast<LibsshConnection*>(_connection)) {
        setProgressText(tr("Opening SCP channel to remote host %1").arg(libsshConnection->hostname()));
        ScpChannel* scpChannel = new ScpChannel(libsshConnection, _url.path());
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
        setProgressText(tr("Opening download channel to remote host %1").arg(opensshConnection->hostname()));
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
    setException(std::make_exception_ptr(Exception(tr("No SSH client implementation available."))));
    shutdown(false);
}

/******************************************************************************
* Is called when the SCP channel failed.
******************************************************************************/
void DownloadRemoteFileJob::channelError(const QString& errorMessage)
{
    setException(std::make_exception_ptr(
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

    if(_localFile && success) {
        _localFile->flush();
        setResult<FileHandle>(FileHandle(url(), _localFile->fileName()));
    }
    else {
        _localFile.reset();
    }

    TaskPtr self = shared_from_this(); // Keep alive until the end of this function. Otherwise, task may be destroyed by the shutdown() call.

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
    if(isCanceled()) {
        shutdown(false);
        return;
    }
    setProgressMaximum(fileSize);
    setProgressText(tr("Fetching remote file %1").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)));
}

/******************************************************************************
* Is called after the file has been downloaded.
******************************************************************************/
void DownloadRemoteFileJob::receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile)
{
    if(isCanceled()) {
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
    if(isCanceled()) {
        shutdown(false);
        return;
    }
    setProgressValue(totalReceivedBytes);
}

/******************************************************************************
* Handles QNetworkReply progress signals.
******************************************************************************/
void DownloadRemoteFileJob::networkReplyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if(isCanceled()) {
        shutdown(false);
        return;
    }
    if(bytesTotal > 0) {
        setProgressMaximum(bytesTotal);
        setProgressValue(bytesReceived);
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
        if(!buffer.isEmpty() && _localFile->write(buffer) == -1)
            throw Exception(tr("Failed to write received data to temporary file: %1").arg(_localFile->errorString()));

        // At the end of the download, we need to flush the file buffer to disk. This is indicated by an empty receive buffer.
        if(buffer.isEmpty() && !_localFile->flush())
            throw Exception(tr("Failed to write received data to temporary local file '%1': %2").arg(_localFile->fileName()).arg(_localFile->errorString()));
    }
    catch(Exception&) {
        captureException();
        shutdown(false);
    }
#endif
}

/******************************************************************************
* Is called when the SSH connection has been established.
******************************************************************************/
void ListRemoteDirectoryJob::connectionEstablished()
{
    if(isCanceled()) {
        shutdown(false);
        return;
    }

    Task::Scope taskScope(this);

#ifdef OVITO_SSH_CLIENT
    if(LibsshConnection* libsshConnection = qobject_cast<LibsshConnection*>(_connection)) {
        // Open the LS channel.
        setProgressText(tr("Opening channel to remote host %1").arg(libsshConnection->hostname()));
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
        setProgressText(tr("Opening channel to remote host %1").arg(opensshConnection->hostname()));
        FileListingRequest* listingRequest = new FileListingRequest(opensshConnection, _url.path());
        connect(listingRequest, &FileListingRequest::error, this, &ListRemoteDirectoryJob::channelError);
        connect(listingRequest, &FileListingRequest::receivingDirectory, this, &ListRemoteDirectoryJob::receivingDirectory);
        connect(listingRequest, &FileListingRequest::receivedDirectoryComplete, this, &ListRemoteDirectoryJob::receivedDirectoryComplete);
        connect(listingRequest, &FileListingRequest::closed, this, &ListRemoteDirectoryJob::channelClosed);
        connect(this, &QObject::destroyed, listingRequest, &FileListingRequest::deleteLater);
        listingRequest->submit();
        return;
    }
    setException(std::make_exception_ptr(Exception(tr("No SSH client implementation available."))));
    shutdown(false);
}

/******************************************************************************
* Is called before transmission of the directory listing begins.
******************************************************************************/
void ListRemoteDirectoryJob::receivingDirectory()
{
    if(isCanceled()) {
        shutdown(false);
        return;
    }

    setProgressText(tr("Listing remote directory %1").arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)));
}

/******************************************************************************
* Is called when the SCP channel failed.
******************************************************************************/
void ListRemoteDirectoryJob::channelError(const QString& errorMessage)
{
    setException(std::make_exception_ptr(
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
    if(isCanceled()) {
        shutdown(false);
        return;
    }

    setResult<QStringList>(listing);
    shutdown(true);
}

/******************************************************************************
* Handles closing of the SSH channel.
******************************************************************************/
void ListRemoteDirectoryJob::channelClosed()
{
    if(!isFinished()) {
        setException(std::make_exception_ptr(
            Exception(tr("Failed to list contents of:\n\n%1\n\nSSH channel was closed unexpectedly.")
                .arg(_url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded)))));
    }

    shutdown(false);
}

}   // End of namespace
