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


#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "GzipIODevice.h"

namespace Ovito {

OVITO_STATIC_ASSERT((std::is_same_v<ZlibByte, unsigned char>));

/// Constructor
GzipIODevice::GzipIODevice(QIODevice* device, int bufferSize, int compressionLevel) :
    _device(device),
    _compressionLevel(compressionLevel),
    _bufferSize(bufferSize)
{
    std::memset(&_zlibStream, 0, sizeof(_zlibStream));
}

/// Destructor.
GzipIODevice::~GzipIODevice()
{
    GzipIODevice::close();
}

/// Looks up the cached index for the current file being uncompressed.
void GzipIODevice::lookupGzipIndex(bool createIfNeeded)
{
    OVITO_ASSERT(!_index);
    if(_enableSeekIndex)
        _index = Application::instance()->fileManager().lookupGzipIndex(_device, createIfNeeded);
}

/// Makes the device generate an index, which will enable random access to the
/// compressed data stream in future load operations.
void GzipIODevice::recordSeekPoint()
{
    if(_state != InStream)
        return;

    if(!_index)
        lookupGzipIndex(true);

    // Take a snapshot of the zlib stream from time to time to build the index while reading the file.
    if(_index) {
        int status = _index->addEntryConditional(_zlibStream.total_out, _device->pos(), _zlibStream);
        if(status != Z_OK) {
            _state = Error;
            setZlibError(tr("Internal zlib error when decompressing: "), status);
        }
    }
}

bool GzipIODevice::setUnderlyingDevice(QIODevice* device)
{
    _device = device;
    return true;
}

/*!
    Opens the GzipIODevice in \a mode. Only ReadOnly and WriteOnly is supported.
    This function will return false if you try to open in other modes.

    If the underlying device is not opened, this function will open it in a suitable mode. If this happens
    the device will also be closed when close() is called.

    If the underlying device is already opened, its openmode must be compatable with \a mode.

    Returns true on success, false on error.
*/
bool GzipIODevice::open(OpenMode mode)
{
    if(isOpen()) {
        qWarning("GzipIODevice::open: device already open");
        return false;
    }

    // Check for correct mode: ReadOnly xor WriteOnly
    const bool read = (bool)(mode & ReadOnly);
    const bool write = (bool)(mode & WriteOnly);
    const bool both = (read && write);
    const bool neither = !(read || write);
    if(both || neither) {
        qWarning("GzipIODevice::open: GzipIODevice can only be opened in the ReadOnly or WriteOnly modes");
        return false;
    }

    // If the underlying device is open, check that is it opened in a compatible mode.
    if(_device->isOpen()) {
        _manageDevice = false;
        const OpenMode deviceMode = _device->openMode();
        if(read && !(deviceMode & ReadOnly)) {
            qWarning("GzipIODevice::open: underlying device must be open in one of the ReadOnly or WriteOnly modes");
            return false;
        }
        if(write && !(deviceMode & WriteOnly)) {
            qWarning("GzipIODevice::open: underlying device must be open in one of the ReadOnly or WriteOnly modes");
            return false;
        }
    }
    else { // If the underlying device is closed, open it.
        _manageDevice = true;
        if(!_device->open(mode)) {
            setErrorString(tr("Error opening underlying device: %1").arg(_device->errorString()));
            return false;
        }
    }

    // Allocate read/write buffer.
    _buffer = std::make_unique<ZlibByte[]>(_bufferSize);

    // The second argument to inflate/deflateInit2 is the windowBits parameter,
    // which also controls what kind of compression stream headers to use.
    // The default value for this is 15. Passing a value greater than 15
    // enables gzip headers and then subtracts 16 form the windowBits value.
    // (So passing 31 gives gzip headers and 15 windowBits). Passing a negative
    // value selects no headers hand then negates the windowBits argument.
    int windowBits;
    switch(streamFormat()) {
    case GzipFormat:
        windowBits = 31;
        break;
    case RawZipFormat:
        windowBits = -15;
        break;
    default:
        windowBits = 15;
    }

    int status;
    if(read) {
        _state = NotReadFirstByte;
        _zlibStream.next_in = nullptr;
        _zlibStream.avail_in = 0;
        if(streamFormat() == ZlibFormat) {
            status = ::inflateInit(&_zlibStream);
        }
        else {
            status = ::inflateInit2(&_zlibStream, windowBits);
        }
        lookupGzipIndex(false);
    }
    else {
        _state = NoBytesWritten;
        if(streamFormat() == ZlibFormat)
            status = ::deflateInit(&_zlibStream, _compressionLevel);
        else
            status = ::deflateInit2(&_zlibStream, _compressionLevel, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY);
    }

    // Handle error.
    if(status != Z_OK) {
        setZlibError(tr("Internal zlib error: "), status);
        return false;
    }

    return QIODevice::open(mode);
}

/// Closes the GzipIODevice, and also the underlying device if it was opened by GzipIODevice.
void GzipIODevice::close()
{
    if(!isOpen())
        return;

    // Flush and close the zlib stream.
    if(openMode() & ReadOnly) {
        _state = NotReadFirstByte;
        Q_DECL_UNUSED int status = ::inflateEnd(&_zlibStream);
        OVITO_ASSERT(status == Z_OK);
    }
    else {
        if(_state == BytesWritten) { // Only flush if we have written anything.
            _state = NoBytesWritten;
            flushZlib(Z_FINISH);
        }
        Q_DECL_UNUSED int status = ::deflateEnd(&_zlibStream);
        OVITO_ASSERT(status == Z_OK);
    }

    // Close the underlying device if we are managing it.
    if(_manageDevice && _device)
        _device->close();

    _zlibStream.next_in = nullptr;
    _zlibStream.avail_in = 0;
    _zlibStream.next_out = nullptr;
    _zlibStream.avail_out = 0;
    _state = Closed;
    _buffer.reset();
    _index.reset();

    QIODevice::close();
}

/// Flushes the zlib stream.
void GzipIODevice::flushZlib(int flushMode)
{
    // No input.
    _zlibStream.next_in = nullptr;
    _zlibStream.avail_in = 0;
    int status;
    do {
        _zlibStream.next_out = _buffer.get();
        _zlibStream.avail_out = _bufferSize;
        status = ::deflate(&_zlibStream, flushMode);
        if(status != Z_OK && status != Z_STREAM_END) {
            _state = Error;
            setZlibError(tr("Internal zlib error when compressing: "), status);
            return;
        }

        ZlibSize outputSize = _bufferSize - _zlibStream.avail_out;

        // Try to write data from the buffer to to the underlying device, return on failure.
        if(!writeBytes(outputSize))
            return;

        // If the mode is Z_FNISH we must loop until we get Z_STREAM_END,
        // else we loop as long as zlib is able to fill the output buffer.
    }
    while((flushMode == Z_FINISH && status != Z_STREAM_END) || (flushMode != Z_FINISH && _zlibStream.avail_out == 0));

    if(flushMode == Z_FINISH)
        OVITO_ASSERT(status == Z_STREAM_END);
    else
        OVITO_ASSERT(status == Z_OK);
}

// Writes outputSize bytes from buffer to the underlying device.
bool GzipIODevice::writeBytes(qint64 outputSize)
{
    ZlibSize totalBytesWritten = 0;
    // Loop until all bytes are written to the underlying device.
    do {
        const qint64 bytesWritten = _device->write(reinterpret_cast<char*>(_buffer.get()), outputSize);
        if(bytesWritten == -1) {
            setErrorString(tr("Error writing to underlying I/O device: %1").arg(_device->errorString()));
            return false;
        }
        totalBytesWritten += bytesWritten;
    }
    while(totalBytesWritten != outputSize);

    // Put up a flag so that the device will be flushed on close.
    _state = BytesWritten;
    return true;
}

// Sets the error string to errorMessage + zlib error string for zlibErrorCode
void GzipIODevice::setZlibError(const QString& errorMessage, int zlibErrorCode)
{
    // Watch out, zlibErrorString may be null.
    const char* const zlibErrorString = ::zError(zlibErrorCode);
    QString errorString;
    if(zlibErrorString)
        errorString = errorMessage + zlibErrorString;
    else
        errorString = tr("%1 - Unknown error (code %2)").arg(errorMessage).arg(zlibErrorCode);

    setErrorString(errorString);
}

/*!
    Flushes the internal buffer.

    Each time you call flush, all data written to the GzipIODevice is compressed and written to the
    underlying device. Calling this function can reduce the compression ratio. The underlying device
    is not flushed.

    Calling this function when GzipIODevice is in ReadOnly mode has no effect.
*/
void GzipIODevice::flush()
{
    if(!isOpen() || openMode() & ReadOnly)
        return;

    flushZlib(Z_SYNC_FLUSH);
}

/*!
    Returns 1 if there might be data available for reading, or 0 if there is no data available.

    There is unfortunately no way of knowing how much data there is available when dealing with compressed streams.

    Also, since the remaining compressed data might be a part of the meta-data that ends the compressed stream (and
    therefore will yield no uncompressed data), you cannot assume that a read after getting a 1 from this function will return data.
*/
qint64 GzipIODevice::bytesAvailable() const
{
    if(!(openMode() & ReadOnly))
        return 0;

    qint64 numBytes = 0;

    switch(_state) {
        case NotReadFirstByte:
            numBytes = _device->bytesAvailable();
            break;
        case InStream:
            numBytes = 1;
            break;
        case EndOfStream:
        case Error:
        default:
            numBytes = 0;
            break;
    };

    numBytes += QIODevice::bytesAvailable();

    return (numBytes > 0) ? 1 : 0;
}

/*!
    Reads and decompresses data from the underlying device.
*/
qint64 GzipIODevice::readData(char* data, qint64 maxSize)
{
    if(_state == EndOfStream)
        return 0;

    if(_state == Error)
        return -1;

    if(maxSize <= 0)
        return 0;

    // We will to try to fill the data buffer
    _zlibStream.next_out = reinterpret_cast<ZlibByte*>(data);
    _zlibStream.avail_out = maxSize;

    int status;
    bool reachedEnd = false;
    do {
        // Read data if the input buffer is empty. There could be data in the buffer
        // from a previous readData call.
        if(_zlibStream.avail_in == 0) {
            qint64 bytesAvailable = _device->read(reinterpret_cast<char*>(_buffer.get()), _bufferSize);
            _zlibStream.next_in = _buffer.get();
            _zlibStream.avail_in = bytesAvailable;

            if(bytesAvailable == -1) {
                _state = Error;
                setErrorString(tr("Error reading data from underlying device: %1").arg(_device->errorString()));
                return -1;
            }

            // If the underlying file stream is still opened for writing by the simulation code, QIODevice::read() does not return -1 when the end of the file is reached.
            // To not wait for more data indefinitely, we should check if the underlying device is at the end of the file.
            // This explicit check seems to be necessary only for zstd streams.
            if(bytesAvailable == 0 && _device->atEnd()) {
                reachedEnd = true;
            }

            if(_state != InStream) {
                // If we are not in a stream and get 0 bytes, we are probably trying to read from an empty device.
                if(bytesAvailable == 0)
                    return 0;
                if(bytesAvailable > 0)
                    _state = InStream;
            }
        }

        // Decompress.
        status = ::inflate(&_zlibStream, Z_SYNC_FLUSH);
        switch(status) {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                _state = Error;
                setZlibError(tr("Internal zlib error when decompressing: "), status);
                return -1;
            case Z_BUF_ERROR: // No more input and zlib can not provide more output - Not an error, we can try to read again when we have more input.
                return 0;
        }
        // Loop until data buffer is full or we reach the end of the input stream.
    }
    while(_zlibStream.avail_out != 0 && status != Z_STREAM_END && !reachedEnd);

    if(status == Z_STREAM_END) {
        _state = EndOfStream;
        // Unget any data left in the read buffer.
        for(int i = _zlibStream.avail_in; i >= 0; --i)
            _device->ungetChar(*reinterpret_cast<const char*>(_zlibStream.next_in + i));
    }
    else if(reachedEnd) {
        // Reached premature end of compressed data stream. The file may be truncated.
        _state = EndOfStream;
    }

    return maxSize - _zlibStream.avail_out;
}

bool GzipIODevice::seek(qint64 pos)
{
    if(isWritable())
        return false;

    qint64 offset = pos - this->pos();
    if(offset == 0)
        return true;

    if(const GzipIndex::Entry* indexEntry = _index ? _index->lookupEntry(pos) : nullptr) {
        OVITO_ASSERT(pos >= indexEntry->uncompressedOffset);
        if(offset < 0 || offset > pos - indexEntry->uncompressedOffset) {
            // Reposition underlying I/O device.
            if(!_device->seek(indexEntry->compressedOffset)) {
                _state = Error;
                setErrorString(tr("I/O error when seeking in compressed file: %1").arg(_device->errorString()));
                return false;
            }
            // Close old zlib stream.
            _state = NotReadFirstByte;
            int status = ::inflateEnd(&_zlibStream);
            if(status != Z_OK) {
                _state = Error;
                setZlibError(tr("Internal zlib error when seeking in compressed file: "), status);
                return false;
            }
            // Restore saved stream.
            status = ::inflateCopy(&_zlibStream, const_cast<z_stream*>(&indexEntry->zlibStream));
            if(status != Z_OK) {
                _state = Error;
                setZlibError(tr("Internal zlib error when seeking in compressed file: "), status);
                return false;
            }
            _zlibStream.avail_in = 0;
            _state = InStream;
            if(!QIODevice::seek(indexEntry->uncompressedOffset))
                return false;

            offset = pos - indexEntry->uncompressedOffset;
        }
    }

    if(offset < 0) { // Seeking backward? Close and restart file and start decompressing it from the beginning.
        OpenMode mode = openMode();
        close();
        if(_device->isOpen()) {
            if(!_device->reset())
                return false;
        }
        if(!open(mode))
            return false;

        char buffer[0x10000];
        while(pos > 0) {
            qint64 s = read(buffer, std::min(pos, (qint64)sizeof(buffer)));
            if(s <= 0)
                return false;
            pos -= s;
        }
    }
    else { // Seeking forward? Simply read (then discard) bytes starting from the current file position.
        char buffer[0x10000];
        while(offset > 0) {
            qint64 s = read(buffer, std::min(offset, (qint64)sizeof(buffer)));
            if(s <= 0)
                return false;
            offset -= s;
        }
    }

    return true;
}

/*!
    Compresses and writes data to the underlying device.
*/
qint64 GzipIODevice::writeData(const char* data, qint64 maxSize)
{
    if(maxSize < 1)
        return 0;
    _zlibStream.next_in = reinterpret_cast<ZlibByte*>(const_cast<char*>(data));
    _zlibStream.avail_in = maxSize;

    if(_state == Error)
        return -1;

    do {
        _zlibStream.next_out = _buffer.get();
        _zlibStream.avail_out = _bufferSize;
        const int status = ::deflate(&_zlibStream, Z_NO_FLUSH);
        if(status != Z_OK) {
            _state = Error;
            setZlibError(tr("Internal zlib error when compressing: "), status);
            return -1;
        }

        ZlibSize outputSize = _bufferSize - _zlibStream.avail_out;

        // Try to write data from the buffer to to the underlying device, return -1 on failure.
        if(!writeBytes(outputSize))
            return -1;
    }
    while(!_zlibStream.avail_out); // run until output is not full.
    OVITO_ASSERT(!_zlibStream.avail_in);

    return maxSize;
}

}   // End of namespace
