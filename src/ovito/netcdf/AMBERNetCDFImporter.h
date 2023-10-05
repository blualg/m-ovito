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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>

namespace Ovito {

/**
 * \brief File parser for NetCDF simulation files.
 */
class OVITO_NETCDFPLUGIN_EXPORT AMBERNetCDFImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual Ovito::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("NetCDF/AMBER Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(AMBERNetCDFImporter, OOMetaClass)

public:

    /// \brief Constructs a new instance of this class.
    Q_INVOKABLE AMBERNetCDFImporter(ObjectInitializationFlags flags) : ParticleImporter(flags), _useCustomColumnMapping(false) {
        setMultiTimestepFile(true);
    }

    /// Returns the title of this object.
    virtual QString objectTitle() const override { return tr("NetCDF"); }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_shared<FrameLoader>(request, sortParticles(), useCustomColumnMapping(), customColumnMapping());
    }

    /// Creates an asynchronous frame discovery object that scans the input file for contained animation frames.
    virtual std::shared_ptr<FileSourceImporter::FrameFinder> createFrameFinder(const FileHandle& file) override {
        return std::make_shared<FrameFinder>(file);
    }

    /// Inspects the header of the given file and returns the number of file columns.
    Future<ParticleInputColumnMapping> inspectFileHeader(const Frame& frame);

private:

    class NetCDFFile
    {
    public:

        /// Destructor.
        ~NetCDFFile() { close(); }

        /// Open NetCDF file for reading.
        QString open(const QString& filename);

        /// Close the current NetCDF file.
        void close();

        /// Scans the NetCDF file and determines the set of particle properties it contains.
        ParticleInputColumnMapping detectColumnMapping(size_t movieFrame = 0);

        /// Map dimensions from NetCDF file to internal representation.
        bool detectDims(int movieFrame, int particleCount, int nDims, int *dimIds, int& nDimsDetected, size_t& componentCount, size_t& particleCountDim, size_t *startp, size_t *countp);

        bool _ncIsOpen = false;
        int _ncid = -1;
        int _root_ncid = -1;
        int _frame_dim, _atom_dim, _spatial_dim, _sph_dim, _dem_dim;
        int _cell_origin_var, _cell_lengths_var, _cell_angles_var;
        int _shear_dx_var;
        int _coordinatesVar = -1;
    };

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request,
                bool sortParticles,
                bool useCustomColumnMapping, const ParticleInputColumnMapping& customColumnMapping)
            : ParticleImporter::FrameLoader(request),
            _sortParticles(sortParticles),
            _useCustomColumnMapping(useCustomColumnMapping),
            _customColumnMapping(customColumnMapping) {}

        /// Returns the file column mapping used to load the file.
        const ParticleInputColumnMapping& columnMapping() const { return _customColumnMapping; }

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        bool _sortParticles;
        bool _useCustomColumnMapping;
        ParticleInputColumnMapping _customColumnMapping;
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

protected:

    /// \brief Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// \brief Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// \brief Guesses the mapping of an input file field to one of OVITO's internal particle properties.
    static InputColumnInfo mapVariableToColumn(const QString& name, int dataType, size_t componentCount);

private:

    /// Controls whether the mapping between input file columns and particle
    /// properties is done automatically or by the user.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, useCustomColumnMapping, setUseCustomColumnMapping);

    /// The user-defined mapping of input file columns to OVITO's particle properties.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticleInputColumnMapping, customColumnMapping, setCustomColumnMapping);
};

}   // End of namespace
