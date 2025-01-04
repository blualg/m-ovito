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
#include <boost/container/stable_vector.hpp>
#include <QReadWriteLock>

#ifdef OVITO_ZSTD_SUPPORT
    #include <zstd_zlibwrapper.h>
#else
    #include <zlib.h>
#endif

namespace Ovito {

using ZlibByte = Bytef;
using ZlibSize = uInt;

/**
 * \brief An in-memory index of seek points that is progressively being built while reading a compressed file for the first time.
 *
 * Subsequently, the index can be used to allow pseudo random access to arbitrary locations in the compressed data stream.
 */
class GzipIndex
{
public:

    struct Entry
    {
        qint64 uncompressedOffset;
        qint64 compressedOffset;
        z_stream zlibStream;
    };

    /// Minimum distance in bytes between generated seek points in the uncompressed stream.
    static constexpr qint64 MinimumSeekPointSpacing = 4 * 1024 * 1024;

    /// Destructor.
    ~GzipIndex() {
        for(Entry& entry : _entries) {
            Q_DECL_UNUSED int status = ::inflateEnd(&entry.zlibStream);
            OVITO_ASSERT(status == Z_OK);
        }
    }

    qint64 indexRange() const { return _entries.empty() ? 0 : _entries.back().uncompressedOffset; }

    /// Adds an entry to the gzip index.
    int addEntryConditional(qint64 uncompressedOffset, qint64 compressedOffset, z_stream& zlibStream) {
        _lock.lockForRead();
        if(uncompressedOffset < indexRange() + MinimumSeekPointSpacing) {
            _lock.unlock();
            return Z_OK;
        }
        _lock.unlock();
        _lock.lockForWrite();
        if(uncompressedOffset < indexRange() + MinimumSeekPointSpacing) {
            _lock.unlock();
            return Z_OK;
        }
        Entry& entry = _entries.emplace_back();
        entry.uncompressedOffset = uncompressedOffset;
        entry.compressedOffset = compressedOffset - zlibStream.avail_in;
        int status = ::inflateCopy(&entry.zlibStream, &zlibStream);
        if(status != Z_OK)
            _entries.pop_back();
        _lock.unlock();
        return status;
    }

    /// Determines the right entry to use for a seek operation.
    const Entry* lookupEntry(qint64 uncompressedOffset) const {
        _lock.lockForRead();
        auto iter = std::upper_bound(_entries.cbegin(), _entries.cend(),
            uncompressedOffset,
            [](qint64 offset, const Entry& entry) { return offset < entry.uncompressedOffset; });
        OVITO_ASSERT(iter == _entries.cend() || uncompressedOffset < iter->uncompressedOffset);
        OVITO_ASSERT(iter == _entries.cbegin() || uncompressedOffset >= std::prev(iter)->uncompressedOffset);
        _lock.unlock();
        if(iter == _entries.cbegin())
            return nullptr;
        else
            return &*std::prev(iter);
    }

private:

    /// All entries of the index. Must use stable_vector, because z_stream structures cannot be relocated.
    boost::container::stable_vector<Entry> _entries;

    /// Protecting from concurrent access to the data structure.
    mutable QReadWriteLock _lock;
};

/**
 * \brief A QIODevice adapter that can compress/uncompress a stream of data on the fly.
 *
 * A GzipIODevice object is constructed with a pointer to an
 * underlying QIODevice.  Data written to the GzipIODevice object
 * will be compressed before it is written to the underlying
 * QIODevice. Similary, if you read from the GzipIODevice object,
 * the data will be read from the underlying device and then
 * decompressed.
 *
 * GzipIODevice is a sequential device, which means that it does
 * not support seeks or random access. Internally, GzipIODevice
 * uses the zlib library to compress and uncompress data.
 */
class OVITO_CORE_EXPORT GzipIODevice : public QIODevice
{
    Q_OBJECT

public:

    /// The compression formats supported by this class.
    enum StreamFormat {
        ZlibFormat,
        GzipFormat,
        RawZipFormat
    };

    /// Constructor.
    ///
    /// The allowed value range for \a compressionLevel is 0 to 9, where 0 means no compression
    /// and 9 means maximum compression. The default value is 6.
    ///
    /// bufferSize specifies the size of the internal buffer used when reading from and writing to the
    /// underlying device. The default value is 65KB. Using a larger value allows for faster compression and
    /// decompression at the expense of memory usage.
    explicit GzipIODevice(QIODevice* device, int bufferSize = 65500, int compressionLevel = 6);

    /// Destructor.
    virtual ~GzipIODevice();

    /// Selects the compression format to read/write.
    void setStreamFormat(StreamFormat format) { _streamFormat = format; }

    /// Returns the compression format being read/written.
    StreamFormat streamFormat() const { return _streamFormat; }

    /// We support seeking in the file.
    bool isSequential() const override { return false; }

    bool open(OpenMode mode) override;
    void close() override;
    void flush();
    qint64 bytesAvailable() const override;
    bool seek(qint64 pos) override;

    /// Makes the device generate an index, which will enable random access to the
    /// compressed data stream in future load operations.
    void recordSeekPoint();

    bool setUnderlyingDevice(QIODevice* device);

    /// Indicates if a compression/decompression error has occurred.
    bool isError() const { return _state == Error; }

    /// Enables/disables the Gzip seek index.
    void setSeekIndexEnabled(bool enabled) { _enableSeekIndex = enabled; }

protected:

    qint64 readData(char * data, qint64 maxSize) override;
    qint64 writeData(const char * data, qint64 maxSize) override;

private:

    // The states this class can be in:
    enum State {
        // Read state
        NotReadFirstByte,
        InStream,
        EndOfStream,
        // Write state
        NoBytesWritten,
        BytesWritten,
        // Common
        Closed,
        Error
    };

    /// Sets the error string to errorMessage + zlib error string for zlibErrorCode
    void setZlibError(const QString& errorMessage, int zlibErrorCode);

    /// Flushes the zlib stream.
    void flushZlib(int flushMode);

    /// Writes outputSize bytes from buffer to the underlying device.
    bool writeBytes(qint64 outputSize);

    /// Looks up the cached index for the current file being uncompressed.
    void lookupGzipIndex(bool createIfNeeded);

    bool _manageDevice = false;
    int _compressionLevel;
    QIODevice* _device;
    State _state = Closed;
    StreamFormat _streamFormat = GzipFormat;
    z_stream _zlibStream;
    qint64 _bufferSize;
    std::unique_ptr<ZlibByte[]> _buffer;
    std::shared_ptr<GzipIndex> _index;
    bool _enableSeekIndex = true;
};

}   // End of namespace
