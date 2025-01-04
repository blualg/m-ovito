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
#include "Exception.h"

namespace Ovito {

Exception::Exception()
{
    _messages.push_back("An exception has occurred.");
}

Exception::Exception(const QString& message)
{
    _messages.push_back(message);
}

Exception::Exception(QStringList errorMessages) : _messages(std::move(errorMessages))
{
}

Exception& Exception::appendDetailMessage(const QString& message)
{
    _messages.push_back(message);
    return *this;
}

Exception& Exception::prependGeneralMessage(const QString& message)
{
    _messages.push_front(message);
    return *this;
}

Exception& Exception::prependToMessage(const QString& text)
{
    if(!_messages.empty())
        _messages.front().prepend(text);
    else
        _messages.push_back(text);
    return *this;
}

void Exception::logError() const
{
    if(!traceback().isEmpty())
        qCritical().noquote() << traceback();
    for(const QString& msg : _messages) {
        qCritical().noquote() << msg;
    }
}

}   // namespace Ovito
