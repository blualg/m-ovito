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


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/io/PropertyOutputWriter.h>
#include "ParticleExporter.h"

namespace Ovito {

/**
 * \brief Abstract base class for export services that can export an arbitrary list of particle properties.
 */
class OVITO_PARTICLES_EXPORT FileColumnParticleExporter : public ParticleExporter
{
    OVITO_CLASS(FileColumnParticleExporter)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// \brief Returns the mapping of particle properties to output file columns.
    const OutputColumnMapping& columnMapping() const { return _columnMapping; }

    /// \brief Sets the mapping of particle properties to output file columns.
    void setColumnMapping(const OutputColumnMapping& mapping) { _columnMapping = mapping; }

public:

    Q_PROPERTY(Ovito::OutputColumnMapping columnMapping READ columnMapping WRITE setColumnMapping)

private:

    /// The mapping of particle properties to output file columns.
    OutputColumnMapping _columnMapping;
};

}   // End of namespace
