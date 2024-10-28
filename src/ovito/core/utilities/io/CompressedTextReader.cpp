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
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#include "CompressedTextReader.h"

#ifdef OVITO_ZLIB_SUPPORT
    #include <ovito/core/utilities/io/gzdevice/GzipIODevice.h>
#endif

namespace Ovito {

/******************************************************************************
* Opens the given I/O device for reading.
******************************************************************************/
CompressedTextReader::CompressedTextReader(const FileHandle& input, qint64 byteOffset, int lineNumber) :
    _device(input.createIODevice())
{
    // Try to find out what the filename is.
    if(!input.sourceUrl().isEmpty())
        _filename = input.sourceUrl().fileName();
    else if(QFileDevice* fileDevice = qobject_cast<QFileDevice*>(_device.get()))
        _filename = fileDevice->fileName();

    // Check if file is compressed (i.e. filename ends with .gz or .zstd).
    if(_filename.endsWith(".gz", Qt::CaseInsensitive) || _filename.endsWith(".zst", Qt::CaseInsensitive)) {
#ifndef OVITO_ZSTD_SUPPORT
        if(_filename.endsWith(".zst", Qt::CaseInsensitive))
            throw Exception(FileManager::tr("Cannot open file '%1' for reading. This version of OVITO was built without I/O support for zstandard compressed files (*.zst)."));
#endif
#ifdef OVITO_ZLIB_SUPPORT
        // When reading consecutive frames from the same compressed trajectory file, try to re-use an existing open file stream.
        if(byteOffset != 0) {
            auto [open_uncompressor, open_device] = Application::instance()->fileManager().lookupGzipOpenFile(_device.get());
            if(open_uncompressor) {
                _uncompressor = std::move(open_uncompressor);
                _device = std::move(open_device);
                _uncompressor->setUnderlyingDevice(_device.get());
                OVITO_ASSERT(_uncompressor->isOpen());
            }
        }
        // Open compressed file for reading.
        if(!_uncompressor)
            _uncompressor = std::make_unique<GzipIODevice>(_device.get());
        if(!_uncompressor->isOpen() && !_uncompressor->open(QIODevice::ReadOnly))
            throw Exception(FileManager::tr("Failed to open input file: %1").arg(_uncompressor->errorString()));
        _stream = _uncompressor.get();
        // Enable seek index for .gz files, disable it for .zst files (because the zlibWrapper library does not support the inflateCopy() function).
        _uncompressor->setSeekIndexEnabled(_filename.endsWith(".zst", Qt::CaseInsensitive) == false);
#else
        throw Exception(FileManager::tr("Cannot open file '%1' for reading. This version of OVITO was built without I/O support for gzip compressed files."));
#endif
    }
    else {
        // Open uncompressed file for reading.
        if(!_device->isOpen() && !_device->open(QIODevice::ReadOnly))
            throw Exception(FileManager::tr("Failed to open file for reading: %1").arg(_device->errorString()));
        _stream = _device.get();
    }

    if(byteOffset != 0 || lineNumber != 0)
        seek(byteOffset, lineNumber);
}

/******************************************************************************
* Destructor.
******************************************************************************/
CompressedTextReader::~CompressedTextReader()
{
#ifdef OVITO_ZLIB_SUPPORT
    if(_device) {
        if(_uncompressor) {
            _uncompressor->setUnderlyingDevice(nullptr);
            Application::instance()->fileManager().returnGzipOpenFile(std::move(_uncompressor), std::move(_device));
        }
    }
#endif
}

/******************************************************************************
* Reads in the next line.
******************************************************************************/
const char* CompressedTextReader::readLine(int maxSize)
{
    _lineNumber++;

#ifdef OVITO_ZLIB_SUPPORT
    if(_uncompressor && _uncompressor->isError())
        throw Exception(_uncompressor->errorString());
#endif

    if(_stream->atEnd())
        throw Exception(FileManager::tr("File parsing error. Unexpected end of file after line %1.").arg(_lineNumber));

    qint64 readBytes = 0;
    if(maxSize == 0) {
        if(_line.size() <= 1) {
            _line.resize(1024);
        }
        readBytes = _stream->readLine(_line.data(), _line.size());

        if(readBytes == _line.size() - 1 && _line[readBytes - 1] != '\n') {
            qint64 readResult;
            do {
                _line.resize(_line.size() + 16384);
                readResult = _stream->readLine(_line.data() + readBytes, _line.size() - readBytes);
                if(readResult > 0 || readBytes == 0)
                    readBytes += readResult;
            }
            while(readResult == Q_INT64_C(16384) && _line[readBytes - 1] != '\n');
        }
    }
    else {
        if(maxSize > (int)_line.size()) {
            _line.resize(maxSize + 1);
        }
        readBytes = _stream->readLine(_line.data(), _line.size());
    }

    if(readBytes <= 0)
        _line[0] = '\0';
    else
        _line[readBytes] = '\0';

    return _line.data();
}

/******************************************************************************
* Maps a part of the input file to memory.
******************************************************************************/
std::pair<const char*, const char*> CompressedTextReader::mmap(qint64 offset, qint64 size)
{
    OVITO_ASSERT(_mmapPointer == nullptr);
    if(!isCompressed()) {
        if(QFileDevice* fileDevice = qobject_cast<QFileDevice*>(&device()))
            _mmapPointer = fileDevice->map(underlyingByteOffset(), size);
    }
    return std::make_pair(
            reinterpret_cast<const char*>(_mmapPointer),
            reinterpret_cast<const char*>(_mmapPointer) + size);
}

/******************************************************************************
* Unmaps the file from memory.
******************************************************************************/
void CompressedTextReader::munmap()
{
    OVITO_ASSERT(_mmapPointer != nullptr);
    if(QFileDevice* fileDevice = qobject_cast<QFileDevice*>(&device()))
        fileDevice->unmap(_mmapPointer);
    _mmapPointer = nullptr;
}

/******************************************************************************
* Reads the entire file contents into memory.
******************************************************************************/
QByteArray CompressedTextReader::readAll()
{
    return _stream->readAll();
}

/******************************************************************************
* Asks the file reader to generate a seek index record at the current stream
* position, which will enable random access to the compressed data in subsequent
* load operations.
******************************************************************************/
void CompressedTextReader::recordSeekPoint()
{
#ifdef OVITO_ZLIB_SUPPORT
    if(_uncompressor)
        _uncompressor->recordSeekPoint();
#endif
}

}   // End of namespace
