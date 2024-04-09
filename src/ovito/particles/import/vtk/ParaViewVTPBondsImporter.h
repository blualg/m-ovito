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
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/mesh/io/ParaViewVTMImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File reader for contact network data in ParaView VTP (vtkPolyData) files written by the Aspherix simulation code.
 */
class OVITO_PARTICLES_EXPORT ParaViewVTPBondsImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTPBondsImporter, OOMetaClass)

public:

    /// Constructor.
    using ParticleImporter::ParticleImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_shared<FrameLoader>(request);
    }

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        using ParticleImporter::FrameLoader::FrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Creates the right kind of OVITO property object that will receive the data read from a <DataArray> element.
        Property* createBondPropertyForDataArray(QXmlStreamReader& xml, int& vectorComponent, bool preserveExistingData);
    };
};


/**
 * \brief Plugin filter used to customize the loading of VTM files referencing a ParaView VTP file
 */
class OVITO_PARTICLES_EXPORT BondsParaViewVTMFileFilter : public ParaViewVTMFileFilter
{
    OVITO_CLASS(BondsParaViewVTMFileFilter)

public:

    /// \brief Is called after all datasets referenced in a multi-block VTM file have been loaded.
    virtual void postprocessDatasets(FileSourceImporter::LoadOperationRequest& request) override;
};

}   // End of namespace
