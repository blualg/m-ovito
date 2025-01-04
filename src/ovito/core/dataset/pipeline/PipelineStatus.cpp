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
#include "PipelineStatus.h"

namespace Ovito {

/******************************************************************************
* Constructs a status object with error status and a text string taken from
* the given exception object.
******************************************************************************/
PipelineStatus::PipelineStatus(const Exception& exception, const QString& messageSeparator) :
    _type(Error),
    _text(exception.messages().join(messageSeparator))
{
}

/******************************************************************************
* Writes a status object to a file stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const PipelineStatus& s)
{
    stream.beginChunk(0x03);
    stream << s._type;
    stream << s._text;
    stream << s._shortInfo;
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads a status object from a binary input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, PipelineStatus& s)
{
    quint32 version = stream.expectChunkRange(0x0, 0x03);
    stream >> s._type;
    stream >> s._text;
    if(version <= 0x01)
        stream >> s._text;
    else if(version >= 0x03)
        stream >> s._shortInfo;
    stream.closeChunk();
    return stream;
}

/******************************************************************************
* Writes a status object to the log stream.
******************************************************************************/
QDebug operator<<(QDebug debug, const PipelineStatus& s)
{
    switch(s.type()) {
    case PipelineStatus::Success: debug << "Success"; break;
    case PipelineStatus::Warning: debug << "Warning"; break;
    case PipelineStatus::Error: debug << "Error"; break;
    }
    if(s.text().isEmpty() == false)
        debug << s.text();
    if(s.shortInfo().isValid())
        debug << s.shortInfo();
    return debug;
}

/******************************************************************************
* Combines the status with another one.
* This method can be used by modifiers that perform multiple operations in a row
* and want to combine the status of each operation into a single status.
******************************************************************************/
void PipelineStatus::combine(const PipelineStatus& other)
{
    // Error status takes precedence over success status.
    if(type() == PipelineStatus::Success || other.type() == PipelineStatus::Error)
        setType(other.type());

    // Combine the text messages.
    if(!other.text().isEmpty()) {
        if(!text().isEmpty())
            setText(text() + QStringLiteral("\n") + other.text());
        else
            setText(other.text());
    }

    OVITO_ASSERT_MSG(other.shortInfo().isNull(), "PipelineStatus::combine()", "PipelineStatus::combine() does not support combining short info yet");
}


}   // End of namespace
