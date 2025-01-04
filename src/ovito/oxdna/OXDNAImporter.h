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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

/**
 * \brief File parser for data files of the oxDNA code.
 *
 * File format documentation:
 *
 * https://dna.physics.ox.ac.uk/index.php/Documentation#Visualisation_of_structures
 */
class OVITO_OXDNA_EXPORT OXDNAImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("oxDNA Configuration Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(OXDNAImporter, OOMetaClass)

public:

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, topologyFileUrl());
    }

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, const QUrl& userSpecifiedTopologyUrl) :
            ParticleImporter::FrameLoader(request),
            _userSpecifiedTopologyUrl(userSpecifiedTopologyUrl) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// URL of the topology file if explicitly specified by the user.
        QUrl _userSpecifiedTopologyUrl;
    };

private:

    /// oxDNA files come in pairs: a topology file and a configuration file.
    /// The configuration file is the primary file passed to the file importer by the system.
    /// This extra field stores the URL of the oxDNA topology file belonging to the configuration file
    /// if explicitly specified by the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QUrl{}, topologyFileUrl, setTopologyFileUrl);
};

}   // End of namespace
