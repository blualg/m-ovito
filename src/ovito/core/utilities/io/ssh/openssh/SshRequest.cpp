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
#include "SshRequest.h"
#include "OpensshConnection.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
SshRequest::SshRequest(OpensshConnection* connection) : QObject(connection)
{
}

/******************************************************************************
* Initiate the request.
******************************************************************************/
void SshRequest::submit()
{
    static_cast<OpensshConnection*>(parent())->processRequests();
}

static bool isSpecialCharUnix(ushort c)
{
    // Chars that should be quoted (TM). This includes:
    static const uchar iqm[] = {
        0xff, 0xff, 0xff, 0xff, 0xdf, 0x07, 0x00, 0xd8,
        0x00, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x78
    }; // 0-32 \'"$`<>|;&(){}*?#!~[]

    return (c < sizeof(iqm) * 8) && (iqm[c / 8] & (1 << (c & 7)));
}

static bool hasSpecialCharsUnix(const QString& arg)
{
    for(int x = arg.length() - 1; x >= 0; --x)
        if(isSpecialCharUnix(arg.unicode()[x].unicode()))
            return true;
    return false;
}

/******************************************************************************
* Puts a path argument in quotes and escapes special characters.
******************************************************************************/
QByteArray SshRequest::quoteAgument(const QString& arg)
{
    if(arg.isEmpty())
        return QByteArrayLiteral("''");

    QByteArray ret = arg.toUtf8();
    if(hasSpecialCharsUnix(arg)) {
        ret.replace('\'', "'\\''");
        ret.prepend('\'');
        ret.append('\'');
    }
    return ret;
}

} // End of namespace
