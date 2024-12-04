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
#include "../ParticleExporter.h"

namespace Ovito {

/**
 * \brief Exporter that writes the particles to a POSCAR file.
 */
class OVITO_PARTICLES_EXPORT POSCARExporter : public ParticleExporter
{
    /// Defines a metaclass specialization for this exporter type.
    class OOMetaClass : public ParticleExporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleExporter::OOMetaClass::OOMetaClass;

        /// Returns the file filter that specifies the extension of files written by this service.
        virtual QString fileFilter() const override { return QStringLiteral("*"); }

        /// Returns the filter description that is displayed in the drop-down box of the file dialog.
        virtual QString fileFilterDescription() const override { return tr("POSCAR"); }
    };

    OVITO_CLASS_META(POSCARExporter, OOMetaClass)

protected:

    /// Creates a worker performing the actual data export.
    virtual OORef<FileExportJob> createExportJob(const QString& filePath, int numberOfFrames) override;

private:

    /// Controls whether atomic coordinates are written in reduced form to the POSCAR file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, writeReducedCoordinates, setWriteReducedCoordinates, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
