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

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/DataSet.h>
#include "FileColumnParticleExporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FileColumnParticleExporter);

/******************************************************************************
* Constructor.
*****************************************************************************/
void FileColumnParticleExporter::initializeObject(ObjectInitializationFlags flags)
{
    ParticleExporter::initializeObject(flags);

#ifndef OVITO_DISABLE_QSETTINGS
    if(this_task::isInteractive()) {
        // Restore last output column mapping.
        QSettings settings;
        settings.beginGroup("exporter/particles/");
        if(settings.contains("columnmapping")) {
            try {
                _columnMapping.fromByteArray(settings.value("columnmapping").toByteArray());
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(tr("Failed to load previous output column mapping from application settings store."));
                ex.logError();
            }
        }
        settings.endGroup();
    }
#endif
}

}   // End of namespace
