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
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/LaunchTask.h>
#include <ovito/core/utilities/io/ssh/SshConnection.h>
#include <ovito/core/utilities/io/ssh/openssh/OpensshConnection.h>
#ifdef OVITO_SSH_CLIENT
    #include <ovito/core/utilities/io/ssh/libssh/LibsshConnection.h>
#endif
#ifdef OVITO_ZLIB_SUPPORT
    #include <ovito/core/utilities/io/gzdevice/GzipIODevice.h>
#endif
#include "FileManager.h"
#include "RemoteFileJob.h"

namespace Ovito {

/******************************************************************************
* Create a QIODevice that permits reading data from the file referred to by this handle.
******************************************************************************/
std::unique_ptr<QIODevice> FileHandle::createIODevice() const
{
    if(!localFilePath().isEmpty()) {
        return std::make_unique<QFile>(localFilePath());
    }
    else {
        auto buffer = std::make_unique<QBuffer>();
        buffer->setData(_fileData);
        OVITO_ASSERT(buffer->data().constData() == _fileData.constData()); // Rely on a shallow copy of the buffer being created.
        return buffer;
    }
}

/******************************************************************************
* Constructor.
******************************************************************************/
FileManager::FileManager()
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
FileManager::~FileManager()
{
    for(SshConnection* connection : _unacquiredConnections) {
        disconnect(connection, nullptr, this, nullptr);
        delete connection;
    }
    Q_ASSERT(_acquiredConnections.empty());
}

/******************************************************************************
* Makes a file available on this computer.
******************************************************************************/
SharedFuture<FileHandle> FileManager::fetchUrl(const QUrl& url)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(url.isLocalFile()) {
        // Nothing to do to fetch local files. Simply return a finished Future object.

        // But first check if the file exists.
        QString filePath = url.toLocalFile();
        if(!QFileInfo(filePath).exists())
            return Future<FileHandle>::createFailed(Exception(tr("File does not exist: %1").arg(filePath)));

        return FileHandle(url, std::move(filePath));
    }
    else if(url.scheme() == QStringLiteral("sftp") || url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
        QUrl normalizedUrl = normalizeUrl(url);
        QMutexLocker lock(&mutex());

        // Check if requested URL is already in the cache.
        if(auto cacheEntry = _downloadedFiles.object(normalizedUrl)) {
            return FileHandle(url, cacheEntry->fileName());
        }

        // Check if requested URL is already being loaded.
        if(auto inProgressEntry = _pendingFiles.find(normalizedUrl); inProgressEntry != _pendingFiles.end()) {
            if(SharedFuture<FileHandle> future = inProgressEntry->second.lock())
                return future;
            else
                _pendingFiles.erase(inProgressEntry);
        }

        // Start the background download job.
        auto future = launchTask(std::make_shared<DownloadRemoteFileJob>(url));
        _pendingFiles.emplace(normalizedUrl, future);
        return future;
    }
    else {
        return Future<FileHandle>::createFailed(Exception(tr("URL scheme '%1' not supported. The program supports only the sftp and http(s) URLs as well as local file paths.").arg(url.scheme())));
    }
}

/******************************************************************************
* Lists all files in a remote directory.
******************************************************************************/
Future<QStringList> FileManager::listDirectoryContents(const QUrl& url)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(url.scheme() == QStringLiteral("sftp")) {
        return launchTask(std::make_shared<ListRemoteDirectoryJob>(url));
    }
    else if(url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
#ifndef Q_OS_WASM
        QUrl normalizedUrl = normalizeUrl(url);
        QMutexLocker lock(&mutex());

        // The http(s) protocol doesn't support directory listings. Thus, we have no means of discovering files on the server.
        // As a workaround, we simply look in our local cache for downloaded files that are located in the requested directory.
        QStringList fileList;
        for(const auto& cacheEntry : _downloadedFiles.keys()) {
            QString path = cacheEntry.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
            if(cacheEntry.host() == url.host() && path == url.path())
                fileList.push_back(cacheEntry.fileName());
        }
        return std::move(fileList);
#else
        return Future<QStringList>::createFailed(Exception(tr("URL scheme not supported. This version of OVITO was built without support for the http:// protocol and can open local files only.")));
#endif
    }
    else {
        return Future<QStringList>::createFailed(Exception(tr("Directory listings for URL scheme '%1' not supported. The program can only look for files in sftp:// locations and in local directories.").arg(url.scheme())));
    }
}

/******************************************************************************
* Removes a cached remote file so that it will be downloaded again next
* time it is requested.
******************************************************************************/
void FileManager::removeFromCache(const QUrl& url)
{
    QMutexLocker lock(&_mutex);
    QTemporaryFile* file = _downloadedFiles.take(normalizeUrl(url));
    if(file) {
#ifdef OVITO_ZLIB_SUPPORT
        QString filename = getFilenameFromDevice(file);
        _gzipOpenFileCache.erase(filename);
        _gzipIndexCache.remove(filename);
#endif
        delete file;
    }
}

/******************************************************************************
* Is called when a remote file has been fetched.
******************************************************************************/
void FileManager::fileFetched(QUrl url, QTemporaryFile* localFile)
{
    QUrl normalizedUrl = normalizeUrl(std::move(url));
    QMutexLocker lock(&mutex());

    if(auto itemInProgress = _pendingFiles.find(normalizedUrl); itemInProgress != _pendingFiles.end())
        _pendingFiles.erase(itemInProgress);

    if(localFile) {
        // Store downloaded file in local cache.
        OVITO_ASSERT(localFile->thread() == this->thread());
        localFile->setParent(this);
        if(!_downloadedFiles.insert(normalizedUrl, localFile, 0))
            throw Exception(tr("Failed to insert downloaded file into file cache."));
    }
}

/******************************************************************************
* Constructs a URL from a path entered by the user.
******************************************************************************/
QUrl FileManager::urlFromUserInput(const QString& path)
{
    if(path.isEmpty())
        return QUrl();
    else if(path.startsWith(QStringLiteral("sftp://"))
            || path.startsWith(QStringLiteral("http://"))
            || path.startsWith(QStringLiteral("https://")))
        return QUrl(path);
    else
        return QUrl::fromLocalFile(path);
}

/******************************************************************************
* Create a new SSH connection or returns an existing connection having the same parameters.
******************************************************************************/
SshConnection* FileManager::acquireSshConnection(const SshConnectionParameters& sshParams)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    // Determine the kind of ssh connection method to use.
    SshConnection::SshImplementation sshImpl = SshConnection::getSshImplementation();

    // Check in-use connections:
    for(SshConnection* connection : _acquiredConnections) {
        if(connection->connectionParameters() != sshParams || connection->implementation() != sshImpl)
            continue;

        _acquiredConnections.append(connection);
        return connection;
    }

    // Check cached open connections:
    for(SshConnection* connection : _unacquiredConnections) {
        if(!connection->isConnected() || connection->connectionParameters() != sshParams || connection->implementation() != sshImpl)
            continue;

        _unacquiredConnections.removeOne(connection);
        _acquiredConnections.append(connection);
        return connection;
    }

    // Create a new connection:
#ifdef OVITO_SSH_CLIENT
    if(sshImpl == SshConnection::Libssh) {
        LibsshConnection* connection = new LibsshConnection(sshParams);
        connect(connection, &SshConnection::disconnected, this, &FileManager::cleanupSshConnection);
        connect(connection, &LibsshConnection::unknownHost, this, &FileManager::unknownSshServer);
        connect(connection, &LibsshConnection::needPassword, this, &FileManager::needSshPassword);
        connect(connection, &LibsshConnection::needKbiAnswers, this, &FileManager::needKbiAnswers);
        connect(connection, &LibsshConnection::authFailed, this, &FileManager::sshAuthenticationFailed);
        connect(connection, &LibsshConnection::needPassphrase, this, &FileManager::needSshPassphrase);
        _acquiredConnections.append(connection);
        return connection;
    }
#endif
    if(sshImpl == SshConnection::Openssh) {
        OpensshConnection* connection = new OpensshConnection(sshParams);
        connect(connection, &SshConnection::disconnected, this, &FileManager::cleanupSshConnection);
        _acquiredConnections.append(connection);
        return connection;
    }
    return nullptr;
}

/******************************************************************************
* Releases an SSH connection after it is no longer used.
******************************************************************************/
void FileManager::releaseSshConnection(SshConnection* connection)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    Q_DECL_UNUSED bool wasAcquired = _acquiredConnections.removeOne(connection);
    OVITO_ASSERT(wasAcquired);
    if(_acquiredConnections.contains(connection))
        return;

    if(!connection->isConnected()) {
        disconnect(connection, nullptr, this, nullptr);
        connection->deleteLater();
    }
    else {
        Q_ASSERT(!_unacquiredConnections.contains(connection));
        _unacquiredConnections.append(connection);
    }
}

/******************************************************************************
*  Is called whenever an SSH connection is closed.
******************************************************************************/
void FileManager::cleanupSshConnection()
{
    SshConnection* connection = qobject_cast<SshConnection*>(sender());
    if(!connection)
        return;

    if(_unacquiredConnections.removeOne(connection)) {
        disconnect(connection, nullptr, this, nullptr);
        connection->deleteLater();
    }
}

#ifdef OVITO_SSH_CLIENT
/******************************************************************************
* Is called whenever a SSH connection to an yet unknown server is being established.
******************************************************************************/
void FileManager::unknownSshServer()
{
    LibsshConnection* connection = qobject_cast<LibsshConnection*>(sender());
    if(!connection)
        return;

    if(detectedUnknownSshServer(connection->hostname(), connection->unknownHostMessage(), connection->hostPublicKeyHash())) {
        if(connection->markCurrentHostKnown())
            return;
    }
    connection->cancel();
}

/******************************************************************************
* Informs the user about an unknown SSH host.
******************************************************************************/
bool FileManager::detectedUnknownSshServer(const QString& hostname, const QString& unknownHostMessage, const QString& hostPublicKeyHash)
{
    if(_detectedUnknownSshServerImpl) {
        return _detectedUnknownSshServerImpl(hostname, unknownHostMessage, hostPublicKeyHash);
    }
    else {
        std::cout << "OVITO is connecting to remote host '" << qPrintable(hostname) << "' via SSH." << std::endl;
        std::cout << qPrintable(unknownHostMessage) << std::endl;
        std::cout << "Host key fingerprint is " << qPrintable(hostPublicKeyHash) << std::endl;
        std::cout << "Are you sure you want to continue connecting (yes/no)? " << std::flush;
        std::string reply;
        std::cin >> reply;
        return reply == "yes";
    }
}

/******************************************************************************
* Is called when an authentication attempt for a SSH connection failed.
******************************************************************************/
void FileManager::sshAuthenticationFailed(int auth)
{
    LibsshConnection* connection = qobject_cast<LibsshConnection*>(sender());
    if(!connection)
        return;

    LibsshConnection::AuthMethods supported = connection->supportedAuthMethods();
    if(auth & LibsshConnection::UseAuthPassword && supported & LibsshConnection::AuthMethodPassword) {
        connection->usePasswordAuth(true);
    }
    else if(auth & LibsshConnection::UseAuthKbi && supported & LibsshConnection::AuthMethodKbi) {
        connection->useKbiAuth(true);
    }
}

/******************************************************************************
* Is called whenever a SSH connection to a server requires password authentication.
******************************************************************************/
void FileManager::needSshPassword()
{
    LibsshConnection* connection = qobject_cast<LibsshConnection*>(sender());
    if(!connection)
        return;

    QString password = connection->password();
    if(askUserForPassword(connection->hostname(), connection->username(), password)) {
        connection->setPassword(password);
    }
    else {
        connection->cancel();
    }
}

/******************************************************************************
* Is called whenever a SSH connection to a server requires keyboard interactive authentication.
******************************************************************************/
void FileManager::needKbiAnswers()
{
    LibsshConnection* connection = qobject_cast<LibsshConnection*>(sender());
    if(!connection)
        return;

    QStringList answers;
    for(const LibsshConnection::KbiQuestion& question : connection->kbiQuestions()) {
        QString answer;
        if(askUserForKbiResponse(connection->hostname(), connection->username(), question.instruction, question.question, question.showAnswer, answer)) {
            answers << answer;
        }
        else {
            connection->cancel();
            return;
        }
    }
    connection->setKbiAnswers(std::move(answers));
}

/******************************************************************************
* Asks the user for the login password for a SSH server.
******************************************************************************/
bool FileManager::askUserForPassword(const QString& hostname, const QString& username, QString& password)
{
    if(_askUserForPasswordImpl) {
        return _askUserForPasswordImpl(hostname, username, password);
    }
    else {
        std::string pw;
        std::cout << "Please enter the password for user '" << qPrintable(username) << "' ";
        std::cout << "on SSH remote host '" << qPrintable(hostname) << "' (set echo off beforehand!): " << std::flush;
        std::cin >> pw;
        password = QString::fromStdString(pw);
        return true;
    }
}

/******************************************************************************
* Asks the user for the answer to a keyboard-interactive question sent by the SSH server.
******************************************************************************/
bool FileManager::askUserForKbiResponse(const QString& hostname, const QString& username, const QString& instruction, const QString& question, bool showAnswer, QString& answer)
{
    if(_askUserForKbiResponseImpl) {
        return _askUserForKbiResponseImpl(hostname, username, instruction, question, showAnswer, answer);
    }
    else {
        std::cout << "SSH keyboard interactive authentication";
        if(!showAnswer)
            std::cout << " (set echo off beforehand!)";
        std::cout << " - " << qPrintable(question) << std::flush;
        std::string pw;
        std::cin >> pw;
        answer = QString::fromStdString(pw);
        return true;
    }
}

/******************************************************************************
* Is called whenever a private SSH key requires a passphrase.
******************************************************************************/
void FileManager::needSshPassphrase(const QString& prompt)
{
    LibsshConnection* connection = qobject_cast<LibsshConnection*>(sender());
    if(!connection)
        return;

    QString passphrase;
    if(askUserForKeyPassphrase(connection->hostname(), prompt, passphrase)) {
        connection->setPassphrase(passphrase);
    }
}

/******************************************************************************
* Asks the user for the passphrase for a private SSH key.
******************************************************************************/
bool FileManager::askUserForKeyPassphrase(const QString& hostname, const QString& prompt, QString& passphrase)
{
    if(_askUserForKeyPassphraseImpl) {
        return _askUserForKeyPassphraseImpl(hostname, prompt, passphrase);
    }
    else {
        std::string pp;
        std::cout << qPrintable(prompt) << std::flush;
        std::cin >> pp;
        passphrase = QString::fromStdString(pp);
        return true;
    }
}
#endif

/******************************************************************************
* Returns the filename (if it's a QFileDevice) or identifier (otherwise) for the
* given QIODevice, which can be used for cache lookups.
******************************************************************************/
QString FileManager::getFilenameFromDevice(QIODevice* device)
{
    OVITO_ASSERT(device);

    if(QFileDevice* fileDevice = qobject_cast<QFileDevice*>(device)) {
        return fileDevice->fileName();
    }

#if 0
    else if(QBuffer* bufferDevice = qobject_cast<QBuffer*>(device)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(bufferDevice->data());
        return QString::fromLatin1(hash.result().toHex());
    }
#endif

    return {};
}

#ifdef OVITO_ZLIB_SUPPORT
/******************************************************************************
* Returns index data for a gzipped file if it exists in the cache.
******************************************************************************/
std::shared_ptr<GzipIndex> FileManager::lookupGzipIndex(QIODevice* device, bool createIfNeeded)
{
    QString filename = getFilenameFromDevice(device);
    if(!filename.isEmpty()) {
        QMutexLocker lock(&_mutex);
        if(std::shared_ptr<GzipIndex>* index = _gzipIndexCache.object(filename)) {
            return *index;
        }
        if(createIfNeeded && !qEnvironmentVariableIsSet("OVITO_DISABLE_GZIP_INDEXING")) {
            std::shared_ptr<GzipIndex> index = std::make_shared<GzipIndex>();
            _gzipIndexCache.insert(filename, new std::shared_ptr<GzipIndex>(index));
            return index;
        }
    }
    return {};
}

/******************************************************************************
* Looks up a cached file object for the given filename if this gzipped file has
* been kept open after a previous read operation.
******************************************************************************/
std::pair<std::unique_ptr<GzipIODevice>, std::unique_ptr<QIODevice>> FileManager::lookupGzipOpenFile(QIODevice* device)
{
    QString filename = getFilenameFromDevice(device);
    if(!filename.isEmpty()) {
        QMutexLocker lock(&_mutex);
        if(auto node = _gzipOpenFileCache.extract(filename)) {
            OVITO_ASSERT(_gzipOpenFileCache.empty());
            return std::move(node.mapped());
        }
    }
    return {};
}

/******************************************************************************
* Returns an open gzipped file back to the global cache.
******************************************************************************/
void FileManager::returnGzipOpenFile(std::unique_ptr<GzipIODevice> uncompressor, std::unique_ptr<QIODevice> underlyingDevice)
{
    if(!qEnvironmentVariableIsSet("OVITO_DISABLE_GZIP_INDEXING")) {
        QString filename = getFilenameFromDevice(underlyingDevice.get());
        if(!filename.isEmpty()) {
            QMutexLocker lock(&_mutex);
            _gzipOpenFileCache.clear();
            _gzipOpenFileCache.try_emplace(filename, std::move(uncompressor), std::move(underlyingDevice));
        }
    }
}
#endif

}   // End of namespace
