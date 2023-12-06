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
#include <ovito/core/utilities/concurrent/Future.h>

#include <QQueue>

namespace Ovito {

// Defined elsewhere:
class SshConnection;

/**
 * \brief Base class for background jobs that access remote files and directories via SSH.
 */
class RemoteFileJob : public QObject
{
    Q_OBJECT

public:

    /// Constructor.
    RemoteFileJob(QUrl url, PromiseBase& promise);

    /// Returns the URL being accessed.
    const QUrl& url() const { return _url; }

    /// The associated asynchronous task of the job.
    const PromiseBase& promise() const { return _promise; }

protected:

    /// Opens the network connection.
    Q_INVOKABLE void start();

    /// Closes the network connection.
    virtual void shutdown(bool success);

protected Q_SLOTS:

    /// Handles network connection errors.
    void connectionError();

    /// Handles network authentication errors.
    void authenticationFailed();

    /// Is called when the network connection has been established.
    virtual void connectionEstablished() = 0;

    /// Handles network connection cancelation by user.
    void connectionCanceled();

    /// Handles QNetworkReply finished signals.
    void networkReplyFinished();

    /// Handles QNetworkReply progress signals.
    virtual void networkReplyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {}

protected:

    /// The URL of the file or directory.
    const QUrl _url;

    /// The SSH connection.
    Ovito::SshConnection* _connection = nullptr;

#ifndef Q_OS_WASM
    /// The Qt network request reply.
    QNetworkReply* _networkReply = nullptr;
#endif

    /// The associated asynchronous task of the job.
    PromiseBase& _promise;

    /// Indicates whether this job is currently active.
    bool _isActive = false;

    /// Queue of jobs that are waiting to be executed.
    static QQueue<RemoteFileJob*> _queuedJobs;

    /// Keeps track of how many jobs are currently active.
    static int _numActiveJobs;
};

/**
 * \brief A background jobs that downloads a file stored on a remote host to the local computer.
 */
class DownloadRemoteFileJob : public RemoteFileJob
{
    Q_OBJECT

public:

    /// Constructor.
    DownloadRemoteFileJob(QUrl url) :
        RemoteFileJob(std::move(url), _promise),
        _promise(Promise<FileHandle>::create<ProgressingTask>(false)) {}

    /// Returns a future yielding the file downloaded by this job.
    SharedFuture<FileHandle> sharedFuture() {
        return _promise.sharedFuture();
    }

protected:

    /// Closes the network connection.
    virtual void shutdown(bool success) override;

    /// Is called when the network connection has been established.
    virtual void connectionEstablished() override;

    /// Handles QNetworkReply progress signals.
    virtual void networkReplyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) override;

    /// Writes the data received from the server so far to the local file.
    void storeReceivedData();

protected Q_SLOTS:

    /// Is called when the remote host starts sending the file.
    void receivingFile(qint64 fileSize);

    /// Is called when the remote host sent some file data.
    void receivedData(qint64 totalReceivedBytes);

    /// Is called after the file has been downloaded.
    void receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile);

    /// Is called when an SCP error occurs in the channel.
    void channelError(const QString& errorMessage);

    /// Handles closing of the SSH channel.
    void channelClosed();

private:

    /// The local copy of the file.
    std::unique_ptr<QTemporaryFile> _localFile;

    /// The promise through which the result of this download job is returned.
    Promise<FileHandle> _promise;
};

/**
 * \brief A background jobs that lists the files in a directory on a remote host.
 */
class ListRemoteDirectoryJob : public RemoteFileJob
{
    Q_OBJECT

public:

    /// Constructor.
    ListRemoteDirectoryJob(QUrl url) :
        RemoteFileJob(std::move(url), _promise),
        _promise(Promise<QStringList>::create<ProgressingTask>(false)) {}

    /// Returns a future yielding the file list downloaded by this job.
    Future<QStringList> future() {
        return _promise.future();
    }

protected:

    /// Is called when the network connection has been established.
    virtual void connectionEstablished() override;

protected Q_SLOTS:

    /// Is called before transmission of the directory listing begins.
    void receivingDirectory();

    /// Is called after the directory listing has been fully transmitted.
    void receivedDirectoryComplete(const QStringList& listing);

    /// Is called when an error occurs in the SSH channel.
    void channelError(const QString& errorMessage);

    /// Handles closing of the SSH channel.
    void channelClosed();

private:

    /// The promise through which the result of this job is returned.
    Promise<QStringList> _promise;
};

}   // End of namespace
