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

#pragma once

#include <ovito/core/Core.h>
#include "ProcessChannel.h"

namespace Ovito {

class ScpChannel : public ProcessChannel
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ScpChannel(LibsshConnection* connection, const QString& location);

    /// Sets the destination buffer for the received file data.
    void setDestinationBuffer(char* buffer) {
        _dataBuffer = buffer;
        processData();
    }

Q_SIGNALS:

    /// This signal is generated before transmission of a file begins.
    void receivingFile(qint64 fileSize);

    /// This signal is generated during data transmission.
    void receivedData(qint64 totalReceivedBytes);

    /// This signal is generated after a file has been fully transmitted.
    void receivedFileComplete(std::unique_ptr<QTemporaryFile>* localFile);

private:

    enum State {
        StateClosed,
        StateConnected,
        StateReceivingFile,
        StateFileComplete
    };

    /// Part of the state machine implementation.
    void setState(State state) { _state = state; }

    /// Returns the current state of the channel.
    State state() const { return _state; }

private Q_SLOTS:

    /// Is called whenever data arrives from the remote process.
    void processData();

private:

    State _state = StateClosed;
    char* _dataBuffer = nullptr;
    qint64 _bytesReceived = 0;
    qint64 _fileSize = 0;

    /// The local destination file during download.
    std::unique_ptr<QTemporaryFile> _localFile;

    /// The memory-mapped destination file.
    uchar* _fileMapping = nullptr;
};

} // End of namespace
