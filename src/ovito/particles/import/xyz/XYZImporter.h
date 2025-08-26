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
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for the text-based XYZ file format.
 */
class OVITO_PARTICLES_EXPORT XYZImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("XYZ Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(XYZImporter, OOMetaClass)

public:

    /// Indicates whether this file importer type loads particle trajectories.
    virtual bool isTrajectoryFormat() const override { return true; }

    /// Guesses the mapping of input file columns to internal particle properties.
    static bool mapVariableToProperty(ParticleInputColumnMapping& columnMapping, int column, QString name, int dataType, int vec);

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_unique<FrameLoader>(request, recenterCell(), generateBoundingBox(), sortParticles(), columnMapping(), autoRescaleCoordinates());
    }

    /// Inspects the header of the given file and returns the number of file columns.
    [[nodiscard]] Future<ParticleInputColumnMapping> inspectFileHeader(const Frame& frame);

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Normal constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell, bool generateBoundingBox, bool sortParticles, const ParticleInputColumnMapping& columnMapping, bool autoRescaleCoordinates)
          : ParticleImporter::FrameLoader(request, recenterCell, generateBoundingBox),
            _sortParticles(sortParticles),
            _columnMapping(columnMapping),
            _autoRescaleCoordinates(autoRescaleCoordinates) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

    private:

        bool _sortParticles;
        bool _autoRescaleCoordinates;
        ParticleInputColumnMapping _columnMapping;
    };

protected:

    /// Scans the data file and builds a list of source frames.
    virtual void discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const override;

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Interprets the comment line of an extended XYZ file.
    static ParticleInputColumnMapping parseExtendedXYZColumnSpecification(const QString& commentLine);

private:

    /// The user-defined mapping of input file columns to OVITO's particle properties.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(ParticleInputColumnMapping{}, columnMapping, setColumnMapping);

    /// Controls the automatic detection of reduced atom coordinates in the input file.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, autoRescaleCoordinates, setAutoRescaleCoordinates);
};

}   // End of namespace
