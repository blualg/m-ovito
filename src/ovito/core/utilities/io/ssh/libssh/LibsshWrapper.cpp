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

#include <ovito/core/Core.h>
#include "LibsshWrapper.h"

namespace Ovito {

#ifdef OVITO_LIBSSH_RUNTIME_LINKING
QLibrary LibsshWrapper::libssh;
#endif

/******************************************************************************
* Loads libssh into the process and resolves the function pointers.
******************************************************************************/
void LibsshWrapper::initialize()
{
#ifdef OVITO_LIBSSH_RUNTIME_LINKING
    libssh.setFileName(QDir::cleanPath(QCoreApplication::applicationDirPath() + QChar('/') + QStringLiteral(OVITO_LIBSSH_RELATIVE_PATH) + QStringLiteral("/libssh")));
    if(!libssh.load())
        throw Exception(QStringLiteral("Failed to load dynamic link library %1: %2.\nThis error may be due to missing dependencies. Libssh requires OpenSSL 1.1 to be installed on the system.")
            .arg(libssh.fileName()).arg(libssh.errorString()));
#endif
}

} // End of namespace
