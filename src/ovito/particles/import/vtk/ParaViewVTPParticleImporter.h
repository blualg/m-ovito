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
 * \brief File reader for point-like particle data from a ParaView VTP (PolyData) file as written by the Aspherix simulation code.
 */
class OVITO_PARTICLES_EXPORT ParaViewVTPParticleImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.vtp"), tr("Aspherix Particles Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTPParticleImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_shared<FrameLoader>(request, std::move(_particleShapeFiles), std::move(_bodiesFiles));
    }

    /// Stores the list of particle type names and corresponding shape file URLs to be loaded.
    void setParticleShapeFileList(std::vector<ParaViewVTMBlockInfo>&& particleShapeFiles) {
        _particleShapeFiles = std::move(particleShapeFiles);
    }

    /// Stores the list of particle type names and corresponding shape file URLs to be loaded.
    void setBodiesFileList(std::vector<ParaViewVTMBlockInfo>&& bodiesFiles) { _bodiesFiles = std::move(bodiesFiles); }

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, std::vector<ParaViewVTMBlockInfo>&& particleShapeFiles,
                    std::vector<ParaViewVTMBlockInfo>&& bodesFiles)
            : ParticleImporter::FrameLoader(request),
              _particleShapeFiles(std::move(particleShapeFiles)),
              _bodiesFiles(std::move(bodesFiles))
        {
        }

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Creates the right kind of OVITO property object that will receive the data read from a <DataArray> element.
        Property* createParticlePropertyForDataArray(QXmlStreamReader& xml, int& vectorComponent, bool preserveExistingData);

        /// Helper method that loads the shape of a particle type from an external geometry file.
        void loadParticleShape(ParticleType* particleType);

        /// Helper method that loads all bodies from the bodies files.
        void loadBodies();

        /// The list of particle type names and corresponding files containing the particle shapes.
        /// This list is extracted by the ParticlesParaViewVTMFileFilter class from the VTM multi-block structure.
        std::vector<ParaViewVTMBlockInfo> _particleShapeFiles;

        /// The list of particle type names and corresponding files containing the bodies.
        /// This list is extracted by the ParticlesParaViewVTMFileFilter class from the VTM multi-block structure.
        std::vector<ParaViewVTMBlockInfo> _bodiesFiles;
    };

    /// The list of particle type names and corresponding files containing the particle shapes.
    /// This list is extracted by the ParticlesParaViewVTMFileFilter class from the VTM multi-block structure.
    std::vector<ParaViewVTMBlockInfo> _particleShapeFiles;

    /// The list of particle type names and corresponding files containing the bodies.
    /// This list is extracted by the ParticlesParaViewVTMFileFilter class from the VTM multi-block structure.
    std::vector<ParaViewVTMBlockInfo> _bodiesFiles;
};

/**
 * \brief Plugin filter used to customize the loading of VTM files referencing a ParaView VTP file.
 *        This filter is needed to correctly load VTM/VTP file combinations written by the Aspherix simulation code.
 */
class OVITO_PARTICLES_EXPORT ParticlesParaViewVTMFileFilter : public ParaViewVTMFileFilter
{
    OVITO_CLASS(ParticlesParaViewVTMFileFilter)

public:

    /// \brief Is called once before the datasets referenced in a multi-block VTM file will be loaded.
    virtual void preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets, FileSourceImporter::LoadOperationRequest& request, const ParaViewVTMImporter& vtmImporter) override;

    /// Is called before parsing of a dataset reference in a multi-block VTM file begins.
    virtual void configureImporter(const ParaViewVTMBlockInfo& blockInfo, FileSourceImporter::LoadOperationRequest& loadRequest, FileSourceImporter* importer) override;

private:

    /// The list of shape files for particle types.
    std::vector<ParaViewVTMBlockInfo> _particleShapeFiles;

    /// The list of bodies files for body types
    std::vector<ParaViewVTMBlockInfo> _bodiesFiles;
};

}   // End of namespace
