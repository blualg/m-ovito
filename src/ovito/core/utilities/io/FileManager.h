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
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>

namespace Ovito {

// Classes are defined elsewhere:
class SshConnection;
struct SshConnectionParameters;

#ifdef OVITO_ZLIB_SUPPORT
class GzipIndex; // defined in GzipIODevice.h
class GzipIODevice; // defined in GzipIODevice.h
#endif

/**
 * \brief A handle to a file manages by the FileManager.
 */
class OVITO_CORE_EXPORT FileHandle
{
public:

    /// Default constructor creating an invalid file handle.
    FileHandle() = default;

    /// Constructor for files located in the local file system.
    explicit FileHandle(const QUrl& sourceUrl, const QString& localFilePath) : _sourceUrl(sourceUrl), _localFilePath(localFilePath) {}

    /// Constructor for files stored in memory.
    explicit FileHandle(const QUrl& sourceUrl, const QByteArray& fileData) : _sourceUrl(sourceUrl), _fileData(fileData) {}

    /// Returns the URL denoting the source location of the data file.
    const QUrl& sourceUrl() const { return _sourceUrl; }

    /// Returns the path to the file in the local file system (may be empty).
    const QString& localFilePath() const { return _localFilePath; }

    /// Create a QIODevice that permits reading data from the file referred to by this handle.
    std::unique_ptr<QIODevice> createIODevice() const;

    /// Returns a human-readable representation of the source location referred to by this file handle.
    QString toString() const {
        return _sourceUrl.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded);
    }

private:

    /// The URL denoting the data source.
    QUrl _sourceUrl;

    /// A path to the file in the local file system.
    QString _localFilePath;

    /// A in-memory buffer with the file's contents.
    QByteArray _fileData;
};

/**
 * \brief The file manager provides transparent access to remote files.
 */
class OVITO_CORE_EXPORT FileManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FileManager)

public:

    /// Constructor.
    explicit FileManager();

    /// Destructor.
    ~FileManager();

    /// \brief Makes a file available locally.
    /// \return A Future that will provide access to the file contents after it has been fetched from the remote location.
    SharedFuture<FileHandle> fetchUrl(const QUrl& url);

    /// \brief Removes a cached remote file so that it will be downloaded again next time it is requested.
    void removeFromCache(const QUrl& url);

    /// \brief Lists all files in a remote directory.
    /// \return A Future that will provide the list of file names.
    [[nodiscard]] Future<QStringList> listDirectoryContents(const QUrl& url);

    /// \brief Constructs a URL from a path entered by the user.
    static QUrl urlFromUserInput(const QString& path);

    /// Create a new SSH connection or returns an existing connection having the same parameters.
    SshConnection* acquireSshConnection(const SshConnectionParameters& sshParams);

    /// Releases an SSH connection after it is no longer used.
    void releaseSshConnection(SshConnection* connection);

#ifdef OVITO_ZLIB_SUPPORT
    /// Returns index data for a gzipped file if it exists in the cache.
    std::shared_ptr<GzipIndex> lookupGzipIndex(QIODevice* device, bool createIfNeeded = false);

    /// Looks up a cached file object for the given filename if this gzipped file has been kept open after a previous read operation.
    std::pair<std::unique_ptr<GzipIODevice>, std::unique_ptr<QIODevice>> lookupGzipOpenFile(QIODevice* device);

    /// Returns an open gzipped file back to the global cache.
    void returnGzipOpenFile(std::unique_ptr<GzipIODevice> uncompressor, std::unique_ptr<QIODevice> underlyingDevice);
#endif

#ifdef OVITO_SSH_CLIENT
    /// Registers an application-function that is used by the FileManager to ask the user for the login password for a SSH server.
    template<typename Function> void registerAskUserForPasswordImpl(Function&& f) { OVITO_ASSERT(!_askUserForPasswordImpl); _askUserForPasswordImpl = std::forward<Function>(f); }

    /// Registers an application-function that is used by the FileManager to ask the user for the passphrase for a private SSH key.
    template<typename Function> void registerAskUserForKeyPassphraseImpl(Function&& f) { OVITO_ASSERT(!_askUserForKeyPassphraseImpl); _askUserForKeyPassphraseImpl = std::forward<Function>(f); }

    /// Registers an application-function that is used by the FileManager to ask the user for the login password for a SSH server.
    template<typename Function> void registerAskUserForKbiResponseImpl(Function&& f) { OVITO_ASSERT(!_askUserForKbiResponseImpl); _askUserForKbiResponseImpl = std::forward<Function>(f); }

    /// Registers an application-function that is used by the FileManager to ask the user for PKCS#11 smartcard credentials.
    template<typename Function> void registerAskUserForPKCS11CredentialsImpl(Function&& f) { OVITO_ASSERT(!_askUserForPKCS11CredentialsImpl); _askUserForPKCS11CredentialsImpl = std::forward<Function>(f); }

    /// Registers an application-function that is used by the FileManager to inform the user about an unknown SSH host.
    template<typename Function> void registerDetectedUnknownSshServerImpl(Function&& f) { OVITO_ASSERT(!_detectedUnknownSshServerImpl); _detectedUnknownSshServerImpl = std::forward<Function>(f); }
#endif

protected:

    /// Returns the mutex used internally to synchronize concurrent access to the data structures of this FileManager.
    QRecursiveMutex& mutex() { return _mutex; }

    /// Strips a URL from username and password information.
    static QUrl normalizeUrl(QUrl url) {
        url.setUserName({});
        url.setPassword({});
        return url;
    }

#ifdef OVITO_SSH_CLIENT
    /// \brief Asks the user for the login password for a SSH server.
    /// \return True on success, false if user has canceled the operation.
    bool askUserForPassword(const QString& hostname, const QString& username, QString& password);

    /// \brief Asks the user for the passphrase for a private SSH key.
    /// \return True on success, false if user has canceled the operation.
    bool askUserForKeyPassphrase(const QString& hostname, const QString& prompt, QString& passphrase);

    /// \brief Asks the user for the answer to a keyboard-interactive question sent by the SSH server.
    /// \return True on success, false if user has canceled the operation.
    bool askUserForKbiResponse(const QString& hostname, const QString& username, const QString& instruction, const QString& question, bool showAnswer, QString& answer);

    /// \brief Asks the user for PKCS#11 smartcard credentials.
    /// \return True on success, false if user has canceled the operation.
    bool askUserForPKCS11Credentials(const QString& hostname, const QString& username, QString& pkcs11Uri, QString& pin);

    /// \brief Informs the user about an unknown SSH host.
    bool detectedUnknownSshServer(const QString& hostname, const QString& unknownHostMessage, const QString& hostPublicKeyHash);
#endif

private Q_SLOTS:

    /// Is called whenever an SSH connection is closed.
    void cleanupSshConnection();

#ifdef OVITO_SSH_CLIENT
    /// Is called whenever a SSH connection to an yet unknown server is being established.
    void unknownSshServer();

    /// Is called whenever a SSH connection to a server requires password authentication.
    void needSshPassword();

    /// Is called whenever a SSH connection to a server requires keyboard interactive authentication.
    void needKbiAnswers();

    /// Is called whenever a SSH connection requires PKCS#11 smartcard credentials.
    void needPKCS11Credentials();

    /// Is called when an authentication attempt for a SSH connection failed.
    void sshAuthenticationFailed(int auth);

    /// Is called whenever a private SSH key requires a passphrase.
    void needSshPassphrase(const QString& prompt);
#endif

private:

    /// Is called when a remote file has been fetched.
    void fileFetched(const QUrl& url, QTemporaryFile* localFile);

    /// Returns the filename (if it's a QFileDevice) or identifier (otherwise) for the given QIODevice,
    /// which can be used for cache lookups.
    static QString getFilenameFromDevice(QIODevice* device);

private:

    /// The remote files that are currently being fetched.
    std::map<QUrl, WeakSharedFuture<FileHandle>> _pendingFiles;

    /// Cache holding the remote files that have already been downloaded.
    QCache<QUrl, QTemporaryFile> _downloadedFiles{std::numeric_limits<int>::max()};

#ifdef OVITO_ZLIB_SUPPORT
    /// Cached index data for file seeking/random data access in gzipped files.
    QCache<QString, std::shared_ptr<GzipIndex>> _gzipIndexCache{4};

    /// Cached open gzipped files to speed up read access to subsequent frames in trajectory files.
    std::map<QString, std::pair<std::unique_ptr<GzipIODevice>, std::unique_ptr<QIODevice>>> _gzipOpenFileCache;
#endif

    /// The mutex to synchronize access to above data structures.
    QRecursiveMutex _mutex;

    /// Holds open SSH connections, which are currently active.
    QList<SshConnection*> _acquiredConnections;

    /// Holds SSH connections, which are still open but not in use.
    QList<SshConnection*> _unacquiredConnections;

#ifdef OVITO_SSH_CLIENT
    /// Function that asks the user for the login password for a SSH server.
    fu2::unique_function<bool(const QString&, const QString&, QString&)> _askUserForPasswordImpl;

    /// Function that asks the user for the passphrase for a private SSH key.
    fu2::unique_function<bool(const QString&, const QString&, QString&)> _askUserForKeyPassphraseImpl;

    /// Function that asks the user for the answer to a keyboard-interactive question sent by the SSH server.
    fu2::unique_function<bool(const QString&, const QString&, const QString&, const QString&, bool, QString&)> _askUserForKbiResponseImpl;

    /// Function that asks the user for PKCS#11 smartcard credentials.
    fu2::unique_function<bool(const QString&, const QString&, QString&, QString&)> _askUserForPKCS11CredentialsImpl;

    /// Function that informs the user about an unknown SSH host.
    fu2::unique_function<bool(const QString&, const QString&, const QString&)> _detectedUnknownSshServerImpl;
#endif

    friend class DownloadRemoteFileJob;
};

}   // End of namespace
