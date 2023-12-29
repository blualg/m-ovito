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
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for POSCAR files as used by the VASP DFT code.
 */
class OVITO_PARTICLES_EXPORT POSCARImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("POSCAR/CHGCAR Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(POSCARImporter, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "POSCAR");

public:

    /// Constructor.
    using ParticleImporter::ParticleImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        activateCLocale();
        return std::make_shared<FrameLoader>(request, recenterCell(), generateBonds());
    }

    /// Creates an asynchronous frame discovery object that scans the input file for contained animation frames.
    virtual std::shared_ptr<FileSourceImporter::FrameFinder> createFrameFinder(const FileHandle& file) override {
        activateCLocale();
        return std::make_shared<FrameFinder>(file);
    }

protected:

    /// \brief Determines whether the input file should be scanned to discover all contained frames.
    virtual bool shouldScanFileForFrames(const QUrl& sourceUrl) const override;

    /// \brief Parses the list of atom types from the POSCAR file.
    static void parseAtomTypeNamesAndCounts(CompressedTextReader& stream, QStringList& atomTypeNames, QVector<int>& atomCounts);

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBonds) : ParticleImporter::FrameLoader::FrameLoader(request, recenterCell), _generateBonds(generateBonds) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        QString readDensityGrid(CompressedTextReader& stream);
        Property* readFieldQuantity(CompressedTextReader& stream, VoxelGrid* grid, const QString& name);

        /// Controls the generation of ad-hoc bonds during data import.
        bool _generateBonds;
    };

    /// The format-specific task object that is responsible for scanning the input file for animation frames.
    class FrameFinder : public FileSourceImporter::FrameFinder
    {
    public:

        /// Inherit constructor from base class.
        using FileSourceImporter::FrameFinder::FrameFinder;

    protected:

        /// Scans the data file and builds a list of source frames.
        virtual void discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames) override;
    };
};

}   // End of namespace
